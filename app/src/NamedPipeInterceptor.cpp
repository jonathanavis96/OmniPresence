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

#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <tlhelp32.h>

namespace OmniPresence {

// ── Faithful Discord READY payload ───────────────────────────────────────────
// Legacy discord-rpc (RuneLite) reads data.user fields; keep the object
// complete or it may refuse to treat the pipe as a real Discord client.
static constexpr const char* READY_JSON =
    R"({"cmd":"DISPATCH","data":{"v":1,"config":{"cdn_host":"cdn.discordapp.com","api_endpoint":"//discord.com/api","environment":"production"},"user":{"id":"1045800378228281345","username":"omnipresence","discriminator":"0","global_name":"OmniPresence","avatar":null,"avatar_decoration_data":null,"bot":false,"flags":0,"premium_type":0}},"evt":"READY","nonce":null})";

// RuneLite's built-in Discord plugin client_id.  We only emit activityCaptured
// for this client; we still ACK all others (e.g. the Social SDK join session).
static constexpr const char* RUNELITE_CLIENT_ID = "409416265891971072";

static std::wstring discordPipeName(int idx)
{
    return L"\\\\.\\pipe\\discord-ipc-" + std::to_wstring(idx);
}

// Probe whether \\.\pipe\discord-ipc-<idx> already has a server (the real Discord).
// We cannot learn this from CreateNamedPipeW: with PIPE_UNLIMITED_INSTANCES it
// SUCCEEDS as an extra instance when Discord already owns the name, so it never
// reports ERROR_PIPE_BUSY / ERROR_ACCESS_DENIED in the Discord-first case.  Ask
// as a client instead:
//   • opens                 -> a server exists (Discord); close, send no handshake.
//   • ERROR_PIPE_BUSY       -> exists but all instances busy -> a server exists.
//   • ERROR_FILE_NOT_FOUND  -> nobody serves it yet -> we are first, no bounce.
static bool discordPipeAlreadyServed(int idx)
{
    HANDLE h = CreateFileW(discordPipeName(idx).c_str(),
                           GENERIC_READ | GENERIC_WRITE,
                           0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
        return true;
    }
    return GetLastError() == ERROR_PIPE_BUSY;
}

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
    // Auto-bounce Discord to claim the pipes unless explicitly disabled.
    m_autoBounce = !qEnvironmentVariableIsSet("OMNIPRESENCE_NO_AUTO_BOUNCE");
    m_bounceTried.store(false);
    m_killDone.store(false);
    m_claimedCount.store(0);
    m_relaunchDiscordPending.store(false);
    // Own BOTH discord-ipc-0 AND discord-ipc-1 so RuneLite's built-in plugin
    // (which scans ipc-0 → ipc-1 → …) can never fall through to the real Discord;
    // Discord is bounced to ipc-2, which its own clients still find (our presence
    // uses Discord's cloud SDK, not the local pipe).  Thread 0 is the leader (does
    // the one-time Discord kill); thread 1 waits for that kill, then claims ipc-1.
    m_threads[0] = std::thread(&NamedPipeInterceptor::workerLoop, this, 0);
    m_threads[1] = std::thread(&NamedPipeInterceptor::workerLoop, this, 1);
}

// ── Discord bounce helpers ─────────────────────────────────────────────────────

bool NamedPipeInterceptor::isDiscordProcessRunning()
{
    bool found = false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        qWarning() << "[NamedPipeInterceptor] CreateToolhelp32Snapshot (probe) failed:"
                   << GetLastError();
        return false;
    }
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"Discord.exe") == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return found;
}

bool NamedPipeInterceptor::killDiscord()
{
    bool killedAny = false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        qWarning() << "[NamedPipeInterceptor] CreateToolhelp32Snapshot failed:" << GetLastError();
        return false;
    }
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"Discord.exe") == 0) {
                HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (h) {
                    if (TerminateProcess(h, 0)) {
                        killedAny = true;
                    }
                    CloseHandle(h);
                }
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return killedAny;
}

void NamedPipeInterceptor::relaunchDiscord()
{
    // Discord's stable launcher is %LOCALAPPDATA%\Discord\Update.exe; it resolves
    // the latest app-<version> and relaunches.  Since we now own ipc-0 AND ipc-1,
    // the fresh Discord falls back to ipc-2 automatically.
    wchar_t* localAppData = nullptr;
    size_t   len          = 0;
    if (_wdupenv_s(&localAppData, &len, L"LOCALAPPDATA") != 0 || !localAppData) {
        qWarning() << "[NamedPipeInterceptor] LOCALAPPDATA unset; relaunch Discord manually.";
        return;
    }
    const std::wstring updater = std::wstring(localAppData) + L"\\Discord\\Update.exe";
    free(localAppData);

    if (GetFileAttributesW(updater.c_str()) == INVALID_FILE_ATTRIBUTES) {
        qWarning() << "[NamedPipeInterceptor] Discord Update.exe not found at expected path; "
                      "relaunch Discord manually.";
        return;
    }

    std::wstring cmd = L"\"" + updater + L"\" --processStart Discord.exe";
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0'); // CreateProcessW requires a mutable, NUL-terminated buffer

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, cmdBuf.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        qDebug() << "[NamedPipeInterceptor] Relaunched Discord (will use ipc-2).";
    } else {
        qWarning() << "[NamedPipeInterceptor] CreateProcessW(Discord) failed:" << GetLastError()
                   << "— relaunch Discord manually.";
    }
}

void NamedPipeInterceptor::stop()
{
    if (!m_running.exchange(false)) {
        return; // already stopped
    }

    // Unblock everyone, but do NOT close client handles here — each serviceClient
    // thread owns and closes its own handle on exit.  We only:
    //   • close the acceptor's pending instance (unblocks ConnectNamedPipe), and
    //   • CancelIoEx + DisconnectNamedPipe each live client (unblocks its ReadFile
    //     so the servicing thread falls out of its frame loop and cleans up).
    // The lock is released before joining so the servicing threads can re-acquire
    // it to remove themselves from m_clientPipes without deadlocking.
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        for (int i = 0; i < 2; ++i) {
            if (m_acceptPipe[i] != INVALID_HANDLE_VALUE) {
                DisconnectNamedPipe(m_acceptPipe[i]);
                CloseHandle(m_acceptPipe[i]);
                m_acceptPipe[i] = INVALID_HANDLE_VALUE;
            }
        }
        for (HANDLE h : m_clientPipes) {
            CancelIoEx(h, nullptr);
            DisconnectNamedPipe(h);
        }
    }

    for (int i = 0; i < 2; ++i) {
        if (m_threads[i].joinable()) {
            m_threads[i].join();
        }
    }

    // Acceptor has exited and stopped spawning; join any remaining client threads.
    // Move the vector out under the lock so we don't hold it across join().
    std::vector<std::thread> threads;
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        threads = std::move(m_clientThreads);
        m_clientThreads.clear();
    }
    for (std::thread& t : threads) {
        if (t.joinable()) {
            t.join();
        }
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

void NamedPipeInterceptor::workerLoop(int pipeIndex)
{
    const std::wstring pname = discordPipeName(pipeIndex);
    bool counted = false; // has THIS thread counted its first claim yet?

    // ── One-time coordinated Discord bounce (leader = pipe 0) ────────────────
    // We must own ipc-0 AND ipc-1 as the SOLE server on each.  If Discord already
    // serves either (it holds ipc-1 from a prior run, or ipc-0 if it launched
    // first), the leader kills it so both names are free; both threads then claim
    // their pipe and, once BOTH are claimed, Discord is relaunched onto ipc-2.
    // CreateNamedPipeW can't tell us Discord is there (PIPE_UNLIMITED_INSTANCES
    // makes it succeed as a dead extra instance), so we probe as a client.  The
    // follower waits for the leader's kill before creating its instance so it
    // never coexists with Discord on ipc-1.
    if (pipeIndex == 0) {
        // Decide the one-time Discord bounce, logging every input so the decision
        // is never silent again — a silent skip here hid a whole-session steal:
        // Discord coexisted on ipc-0 as an extra PIPE_UNLIMITED_INSTANCES server
        // and RuneLite's plugin raced onto it. Bounce on DISCORD PROCESS PRESENCE,
        // not the pipe probe: discordPipeAlreadyServed() false-negatives (timing +
        // Discord's extra ipc-0 instance is invisible as "served"), whereas a
        // running Discord.exe reliably means it will claim ipc-0. The pipe probe is
        // kept for diagnostics only.
        const bool served0   = discordPipeAlreadyServed(0);
        const bool served1   = discordPipeAlreadyServed(1);
        const bool discordUp = isDiscordProcessRunning();
        qInfo().nospace()
            << "[NamedPipeInterceptor] bounce probe: autoBounce=" << m_autoBounce
            << " served(ipc-0)=" << served0 << " served(ipc-1)=" << served1
            << " discordProcRunning=" << discordUp;
        if (m_autoBounce && discordUp && !m_bounceTried.exchange(true)) {
            if (killDiscord()) {
                qWarning() << "[NamedPipeInterceptor] Discord running; bounced it to claim "
                              "ipc-0/ipc-1 as the SOLE server (Discord returns on ipc-2) "
                              "(set OMNIPRESENCE_NO_AUTO_BOUNCE=1 to disable).";
                m_relaunchDiscordPending.store(true);
                Sleep(1500); // let Windows release Discord's pipe instances
            } else {
                qWarning() << "[NamedPipeInterceptor] Discord process seen but kill failed; "
                              "RuneLite may reach real Discord — check permissions.";
            }
        } else if (m_autoBounce && !discordUp) {
            qInfo() << "[NamedPipeInterceptor] no Discord process at startup; nothing to bounce "
                       "yet (self-heal re-bounce will handle a later Discord launch).";
        }
        m_killDone.store(true); // release the follower
    } else {
        // Follower: wait (bounded) until the leader has finished any kill so
        // Discord has released ipc-1 before we create our instance.
        for (int i = 0; i < 100 && !m_killDone.load() && m_running.load(); ++i) {
            Sleep(50);
        }
    }

    while (m_running.load()) {
        // ── Create a fresh server-side pipe INSTANCE ─────────────────────────
        // PIPE_ACCESS_DUPLEX: we both read (frames) and write (ACKs / READY).
        // PIPE_UNLIMITED_INSTANCES is the heart of the multi-client fix: each
        //   accepted client is serviced on its OWN serviceClient() thread, so
        //   RuneLite's built-in Discord plugin and OmniPresence's own Social SDK
        //   probe can be connected simultaneously.  The old single-instance
        //   server let whichever client connected first squat the only slot
        //   forever — which is exactly how RuneLite's plugin got locked out and
        //   the captured presence froze on stale data.
        // CRITICAL: pass explicit 64 KB in/out buffer sizes.  With a 0
        //   out-buffer, a named-pipe write becomes a synchronous rendezvous
        //   that blocks until the client reads — which froze us on the ACK
        //   write because RuneLite only reads on its callback timer.
        //   A real out-buffer makes writes return immediately (buffered).
        HANDLE pipe = CreateNamedPipeW(
            pname.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES, // concurrent clients (RuneLite + SDK + …)
            65536,  // out-buffer (see CRITICAL note above)
            65536,  // in-buffer
            0,      // default timeout
            nullptr // default security
        );

        if (pipe == INVALID_HANDLE_VALUE) {
            // We hold both ipc-0 and ipc-1 continuously (Discord was bounced to
            // ipc-2), so Discord can't steal a name mid-session; a failure here is
            // transient. Back off and retry.
            qWarning() << "[NamedPipeInterceptor] CreateNamedPipeW(discord-ipc-" << pipeIndex
                       << ") failed, error:" << GetLastError();
            Sleep(2000);
            continue;
        }

        // Publish this instance as the pending acceptor handle so stop() can
        // close it to unblock the ConnectNamedPipe below.
        {
            std::lock_guard<std::mutex> lock(m_clientsMutex);
            if (!m_running.load()) {
                CloseHandle(pipe);
                break;
            }
            m_acceptPipe[pipeIndex] = pipe;
        }

        // Count this thread's FIRST claim.  Once BOTH pipes are held, bring
        // Discord back — it takes ipc-2 since we now own ipc-0 and ipc-1.
        // Relaunching only after both are claimed stops Discord from grabbing the
        // pipe we haven't claimed yet.
        if (!counted) {
            counted = true;
            if (m_claimedCount.fetch_add(1) + 1 == 2 &&
                m_relaunchDiscordPending.exchange(false)) {
                relaunchDiscord();
            }
        }

        qDebug() << "[NamedPipeInterceptor] Listening on \\\\.\\pipe\\discord-ipc-"
                 << pipeIndex << "(awaiting next client) ...";

        // ── Wait for a client on THIS instance ───────────────────────────────
        const BOOL connected = ConnectNamedPipe(pipe, nullptr)
            ? TRUE
            : (GetLastError() == ERROR_PIPE_CONNECTED ? TRUE : FALSE);

        // Reclaim ownership from m_acceptPipe and decide its fate.  stop() may
        // have already closed+cleared it to unblock us, in which case it's no
        // longer ours to touch.
        {
            std::lock_guard<std::mutex> lock(m_clientsMutex);
            const bool stillOurs = (m_acceptPipe[pipeIndex] == pipe);
            if (stillOurs) {
                m_acceptPipe[pipeIndex] = INVALID_HANDLE_VALUE;
            }

            if (!m_running.load()) {
                if (stillOurs) CloseHandle(pipe);
                break;
            }
            if (!connected) {
                qWarning() << "[NamedPipeInterceptor] ConnectNamedPipe failed, error:"
                           << GetLastError();
                if (stillOurs) CloseHandle(pipe);
                continue;
            }

            // Hand the live connection to its own servicing thread, then loop
            // back immediately to create+accept the NEXT instance.  This is what
            // lets RuneLite's plugin and the Social SDK be connected at once.
            m_clientPipes.push_back(pipe);
            m_clientThreads.emplace_back(&NamedPipeInterceptor::serviceClient, this, pipe);
        }
    } // acceptor while
}

// ── Per-client servicing ───────────────────────────────────────────────────────
// Runs on its own thread for the lifetime of one connected client.  Owns \p pipe:
// closes it and removes itself from m_clientPipes on exit.  stop() unblocks a
// thread parked in readExact() via CancelIoEx()/DisconnectNamedPipe() on the
// handle; the thread then falls through to the disconnect cleanup below.

void NamedPipeInterceptor::serviceClient(HANDLE pipe)
{
    qDebug() << "[NamedPipeInterceptor] CLIENT CONNECTED.";

    // Track the client_id from the handshake for this connection lifetime.
    QString connectionClientId;

    // ── Frame loop ────────────────────────────────────────────────────────────
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

        // ── Dispatch by opcode ────────────────────────────────────────────────
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
                goto disconnect; // break out of switch AND frame loop
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

                    // Only emit for RuneLite; still ACK everything (incl. our own
                    // Social SDK probe on its own concurrent instance).
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
    } // frame loop

disconnect:
    qDebug() << "[NamedPipeInterceptor] CLIENT DISCONNECTED.";
    // Close our handle and de-register under the lock so stop() never CancelIoEx's
    // a handle we've already closed (both touch m_clientPipes under m_clientsMutex).
    {
        std::lock_guard<std::mutex> lock(m_clientsMutex);
        DisconnectNamedPipe(pipe);
        CloseHandle(pipe);
        m_clientPipes.erase(
            std::remove(m_clientPipes.begin(), m_clientPipes.end(), pipe),
            m_clientPipes.end());
    }
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
