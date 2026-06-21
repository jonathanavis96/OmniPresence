// NamedPipeInterceptor.cpp — Discord IPC named-pipe impersonator.
// Ports scripts/spike/discord-ipc-sniff.ps1 (v2/v3) to C++/Win32.
//
// Thread model: start() launches workerLoop() on std::thread.  The worker
// never touches Qt objects or IntegrationContext directly — it only calls
// emit activityCaptured(), which is delivered to the main thread via a
// queued connection because the emitter lives on a non-Qt thread.
#include "NamedPipeInterceptor.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

#ifdef Q_OS_WIN

#include <cstring>

namespace OmniPresence {

// ── Faithful Discord READY payload ───────────────────────────────────────────
// Legacy discord-rpc (RuneLite) reads data.user fields; keep the object
// complete or it may refuse to treat the pipe as a real Discord client.
static constexpr const char* READY_JSON =
    R"({"cmd":"DISPATCH","data":{"v":1,"config":{"cdn_host":"cdn.discordapp.com","api_endpoint":"//discord.com/api","environment":"production"},"user":{"id":"1045800378228281345","username":"omnipresence","discriminator":"0","global_name":"OmniPresence","avatar":null,"avatar_decoration_data":null,"bot":false,"flags":0,"premium_type":0}},"evt":"READY","nonce":null})";

// RuneLite's built-in Discord plugin client_id.  We only emit activityCaptured
// for this client; we still ACK all others (e.g. the Social SDK join session).
static constexpr const char* RUNELITE_CLIENT_ID = "409416265891971072";

// ── Constructor / destructor ──────────────────────────────────────────────────

NamedPipeInterceptor::NamedPipeInterceptor(QObject* parent)
    : QObject(parent)
{
}

NamedPipeInterceptor::~NamedPipeInterceptor()
{
    stop();
}

// ── Public API ────────────────────────────────────────────────────────────────

void NamedPipeInterceptor::start()
{
    if (m_running.exchange(true)) {
        return; // already running
    }
    m_thread = std::thread(&NamedPipeInterceptor::workerLoop, this);
}

void NamedPipeInterceptor::stop()
{
    if (!m_running.exchange(false)) {
        return; // already stopped
    }

    // Close the pipe handle to unblock any pending ConnectNamedPipe or ReadFile.
    // The worker checks m_running after each blocking call and exits cleanly.
    HANDLE h = m_pipe;
    if (h != INVALID_HANDLE_VALUE) {
        // Disconnect first so the client gets a broken-pipe error rather than
        // lingering in a half-open state.
        DisconnectNamedPipe(h);
        CloseHandle(h);
        m_pipe = INVALID_HANDLE_VALUE;
    }

    if (m_thread.joinable()) {
        m_thread.join();
    }
}

// ── I/O helpers ───────────────────────────────────────────────────────────────

bool NamedPipeInterceptor::readExact(HANDLE pipe, void* buf, DWORD count)
{
    auto* ptr = static_cast<BYTE*>(buf);
    DWORD remaining = count;
    while (remaining > 0) {
        DWORD read = 0;
        if (!ReadFile(pipe, ptr, remaining, &read, nullptr) || read == 0) {
            return false; // EOF or error — caller treats as disconnect
        }
        ptr       += read;
        remaining -= read;
    }
    return true;
}

bool NamedPipeInterceptor::writeFrame(HANDLE pipe, int opcode, const QString& json)
{
    // Build ONE contiguous buffer: [op int32-LE][len int32-LE][UTF-8 payload].
    // CRITICAL: do NOT call FlushFileBuffers after writing.
    //   FlushFileBuffers on a named pipe blocks until the CLIENT drains the
    //   buffer.  Legacy discord-rpc (RuneLite) only reads on its callback timer,
    //   not continuously — so Flush would hang us permanently.
    //   The 64 KB output buffer set in CreateNamedPipeW is the other half of
    //   the fix: writes return immediately (buffered) instead of blocking for a
    //   synchronous rendezvous with the reader.
    const QByteArray payload = json.toUtf8();
    const DWORD payloadLen   = static_cast<DWORD>(payload.size());

    QByteArray frame(8 + static_cast<int>(payloadLen), Qt::Uninitialized);
    BYTE* data = reinterpret_cast<BYTE*>(frame.data());

    // Little-endian int32 opcode
    data[0] = static_cast<BYTE>( opcode        & 0xFF);
    data[1] = static_cast<BYTE>((opcode >>  8) & 0xFF);
    data[2] = static_cast<BYTE>((opcode >> 16) & 0xFF);
    data[3] = static_cast<BYTE>((opcode >> 24) & 0xFF);
    // Little-endian int32 length
    data[4] = static_cast<BYTE>( payloadLen        & 0xFF);
    data[5] = static_cast<BYTE>((payloadLen >>  8) & 0xFF);
    data[6] = static_cast<BYTE>((payloadLen >> 16) & 0xFF);
    data[7] = static_cast<BYTE>((payloadLen >> 24) & 0xFF);

    if (payloadLen > 0) {
        std::memcpy(data + 8, payload.constData(), payloadLen);
    }

    DWORD written = 0;
    return WriteFile(pipe, frame.constData(), static_cast<DWORD>(frame.size()),
                     &written, nullptr) && written == static_cast<DWORD>(frame.size());
}

// ── Worker loop ───────────────────────────────────────────────────────────────

void NamedPipeInterceptor::workerLoop()
{
    while (m_running.load()) {
        // ── Create the server-side pipe end ──────────────────────────────────
        // PIPE_ACCESS_DUPLEX: we both read (frames) and write (ACKs / READY).
        // CRITICAL: pass explicit 64 KB in/out buffer sizes.  With a 0
        //   out-buffer, a named-pipe write becomes a synchronous rendezvous
        //   that blocks until the client reads — which froze us on the ACK
        //   write because RuneLite only reads on its callback timer.
        //   A real out-buffer makes writes return immediately (buffered).
        HANDLE pipe = CreateNamedPipeW(
            L"\\\\.\\pipe\\discord-ipc-0",
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            1,      // max instances — we own the only slot
            65536,  // out-buffer (see CRITICAL note above)
            65536,  // in-buffer
            0,      // default timeout
            nullptr // default security
        );

        if (pipe == INVALID_HANDLE_VALUE) {
            const DWORD err = GetLastError();
            if (err == ERROR_PIPE_BUSY || err == ERROR_ACCESS_DENIED) {
                qWarning() << "[NamedPipeInterceptor] discord-ipc-0 is already owned by another"
                           << "process (Discord or OmniPresence).  Error:" << err
                           << "— quit Discord and start OmniPresence first.";
            } else {
                qWarning() << "[NamedPipeInterceptor] CreateNamedPipeW failed, error:" << err;
            }
            // Do not busy-spin.  Back off briefly and try again so that if the
            // user quits Discord while OmniPresence is running we pick it up.
            Sleep(2000);
            continue;
        }

        m_pipe = pipe;
        qDebug() << "[NamedPipeInterceptor] Listening on \\\\.\\pipe\\discord-ipc-0 ...";

        // ── Wait for a client ─────────────────────────────────────────────────
        const BOOL connected = ConnectNamedPipe(pipe, nullptr)
            ? TRUE
            : (GetLastError() == ERROR_PIPE_CONNECTED ? TRUE : FALSE);

        if (!m_running.load()) {
            // stop() was called while we were waiting
            CloseHandle(pipe);
            m_pipe = INVALID_HANDLE_VALUE;
            break;
        }

        if (!connected) {
            qWarning() << "[NamedPipeInterceptor] ConnectNamedPipe failed, error:" << GetLastError();
            CloseHandle(pipe);
            m_pipe = INVALID_HANDLE_VALUE;
            continue;
        }

        qDebug() << "[NamedPipeInterceptor] CLIENT CONNECTED.";

        // Track the client_id from the handshake for this connection lifetime.
        QString connectionClientId;

        // ── Inner frame loop ──────────────────────────────────────────────────
        while (m_running.load()) {
            // Read 8-byte header
            BYTE header[8];
            if (!readExact(pipe, header, 8)) {
                break; // disconnect
            }

            const int opcode = static_cast<int>(
                static_cast<DWORD>(header[0])
                | (static_cast<DWORD>(header[1]) <<  8)
                | (static_cast<DWORD>(header[2]) << 16)
                | (static_cast<DWORD>(header[3]) << 24));
            const DWORD payloadLen =
                static_cast<DWORD>(header[4])
                | (static_cast<DWORD>(header[5]) <<  8)
                | (static_cast<DWORD>(header[6]) << 16)
                | (static_cast<DWORD>(header[7]) << 24);

            // Read payload
            QString payloadStr;
            if (payloadLen > 0) {
                QByteArray body(static_cast<int>(payloadLen), Qt::Uninitialized);
                if (!readExact(pipe, body.data(), payloadLen)) {
                    break;
                }
                payloadStr = QString::fromUtf8(body);
            }

            // ── Dispatch by opcode ────────────────────────────────────────────
            switch (opcode) {
            case 0: { // Handshake → reply READY
                qDebug() << "[NamedPipeInterceptor] HANDSHAKE:" << payloadStr;

                // Extract client_id for optional RuneLite gating.
                QJsonParseError parseErr;
                const QJsonDocument doc = QJsonDocument::fromJson(
                    payloadStr.toUtf8(), &parseErr);
                if (parseErr.error == QJsonParseError::NoError && doc.isObject()) {
                    connectionClientId = doc.object()
                        .value(QStringLiteral("client_id")).toString();
                }

                if (!writeFrame(pipe, 1, QString::fromUtf8(READY_JSON))) {
                    qWarning() << "[NamedPipeInterceptor] READY write failed, error:"
                               << GetLastError();
                    goto disconnect; // break out of switch AND inner while
                }
                qDebug() << "[NamedPipeInterceptor] READY sent.";
                break;
            }

            case 1: { // Frame
                QJsonParseError parseErr;
                const QJsonDocument doc = QJsonDocument::fromJson(
                    payloadStr.toUtf8(), &parseErr);

                QString cmd, nonce;
                if (parseErr.error == QJsonParseError::NoError && doc.isObject()) {
                    const QJsonObject obj = doc.object();
                    cmd   = obj.value(QStringLiteral("cmd")).toString();
                    nonce = obj.value(QStringLiteral("nonce")).toString();

                    if (cmd == QLatin1String("SET_ACTIVITY")) {
                        const QJsonObject activityObj =
                            obj.value(QStringLiteral("args"))
                               .toObject()
                               .value(QStringLiteral("activity"))
                               .toObject();

                        const QString details =
                            activityObj.value(QStringLiteral("details")).toString();
                        const QString state =
                            activityObj.value(QStringLiteral("state")).toString();

                        // Only emit for RuneLite; still ACK everything.
                        if (connectionClientId == QLatin1String(RUNELITE_CLIENT_ID)) {
                            qDebug() << "[NamedPipeInterceptor] ACTIVITY"
                                     << "activity=" << details
                                     << "location=" << state;
                            // emit is thread-safe across Qt's queued connection;
                            // the worker never touches IntegrationContext directly.
                            emit activityCaptured(details, state);
                        }

                        const QString ack = QStringLiteral(
                            R"({"cmd":"SET_ACTIVITY","data":null,"evt":null,"nonce":"%1"})")
                            .arg(nonce);
                        writeFrame(pipe, 1, ack);
                    } else {
                        // Generic ACK for any other cmd (SUBSCRIBE, etc.)
                        const QString ack = QStringLiteral(
                            R"({"cmd":"%1","data":null,"evt":null,"nonce":"%2"})")
                            .arg(cmd, nonce);
                        writeFrame(pipe, 1, ack);
                    }
                }
                break;
            }

            case 2: // Close
                qDebug() << "[NamedPipeInterceptor] CLIENT sent CLOSE.";
                goto disconnect;

            case 3: // Ping → Pong
                writeFrame(pipe, 4, payloadStr);
                qDebug() << "[NamedPipeInterceptor] PONG sent.";
                break;

            default:
                qDebug() << "[NamedPipeInterceptor] Unknown op=" << opcode
                         << "len=" << payloadLen;
                break;
            }
        } // inner while

        disconnect:
        qDebug() << "[NamedPipeInterceptor] CLIENT DISCONNECTED.";
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        m_pipe = INVALID_HANDLE_VALUE;

        if (m_running.load()) {
            qDebug() << "[NamedPipeInterceptor] re-listening for next connection"
                     << "(toggle the RuneLite Discord plugin to reconnect).";
        }
    } // outer while
}

} // namespace OmniPresence

#else // !Q_OS_WIN

namespace OmniPresence {

NamedPipeInterceptor::NamedPipeInterceptor(QObject* parent)
    : QObject(parent)
{
}

NamedPipeInterceptor::~NamedPipeInterceptor() = default;

void NamedPipeInterceptor::start()
{
    qWarning() << "[NamedPipeInterceptor] Named-pipe interception is Windows-only; start() is a no-op.";
}

void NamedPipeInterceptor::stop()
{
    // no-op on non-Windows
}

} // namespace OmniPresence

#endif // Q_OS_WIN
