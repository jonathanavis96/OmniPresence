// NamedPipeInterceptor.h — Windows named-pipe impersonator for Discord IPC.
//
// Squats \\.\pipe\discord-ipc-0 and captures RuneLite SET_ACTIVITY frames so
// OmniPresence can republish OSRS rich presence under its own Discord identity.
//
// The Discord RPC wire format:
//   [opcode : int32-LE][length : int32-LE][payload : UTF-8 JSON]
//   opcodes: 0=Handshake 1=Frame 2=Close 3=Ping 4=Pong
//
// IMPORTANT — startup ordering:
//   OmniPresence must own discord-ipc-0 BEFORE Discord does.  If Discord already
//   owns it, CreateNamedPipeW returns ERROR_PIPE_BUSY / ERROR_ACCESS_DENIED.  The
//   interceptor then performs a ONE-TIME auto-bounce: it kills Discord, claims the
//   pipe, and relaunches Discord (which falls back to ipc-1).  This removes the
//   manual "start OmniPresence before Discord" requirement.  Disable by setting
//   the env var OMNIPRESENCE_NO_AUTO_BOUNCE=1 (then it just logs guidance).
#pragma once

#include <QObject>
#include <QString>

#ifdef Q_OS_WIN
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <windows.h>
#endif

namespace OmniPresence {

/// Impersonates the Discord client on \\.\pipe\discord-ipc-0 to capture
/// RuneLite SET_ACTIVITY frames.  All blocking I/O runs on a background thread.
/// Emits activityCaptured() on the Qt event loop via a queued connection.
class NamedPipeInterceptor : public QObject {
    Q_OBJECT
public:
    explicit NamedPipeInterceptor(QObject* parent = nullptr);
    ~NamedPipeInterceptor() override;

    /// Create the pipe and start the background worker thread.
    void start();

    /// Signal the worker to exit, close the pipe handle to unblock any pending
    /// I/O, and join the thread.  Safe to call multiple times.
    void stop();

signals:
    /// Emitted (via queued connection) whenever a RuneLite SET_ACTIVITY frame
    /// arrives.  \p activity maps to args.activity.details (the main label,
    /// e.g. "Killing Cows"), \p location maps to args.activity.state (region).
    void activityCaptured(const QString& activity, const QString& location);

#ifdef Q_OS_WIN
private:
    /// Acceptor loop: repeatedly creates a fresh pipe INSTANCE and waits for a
    /// client.  Each accepted client is handed to its own serviceClient() thread
    /// so multiple Discord clients (e.g. RuneLite's built-in plugin AND our own
    /// Social SDK probe) can be connected simultaneously — the single-instance
    /// server used to let the first client squat the only slot forever.
    void workerLoop();

    /// Service one connected client (handshake/frames/ping/close) until it
    /// disconnects.  Owns \p pipe: closes it and removes itself from the live
    /// client list on exit.  Runs on its own std::thread.
    void serviceClient(HANDLE pipe);

    /// Read exactly \p count bytes from \p pipe into \p buf.
    /// Returns false on EOF or error (treat as disconnect).
    static bool readExact(HANDLE pipe, void* buf, DWORD count);

    /// Write \p opcode + \p json as one contiguous frame buffer.
    /// CRITICAL: does NOT call FlushFileBuffers — see implementation comment.
    static bool writeFrame(HANDLE pipe, int opcode, const QString& json);

    /// Kill any running Discord so it releases discord-ipc-0.  Returns true if a
    /// Discord process was actually terminated (i.e. a relaunch is warranted).
    static bool killDiscord();

    /// Relaunch Discord via its updater (it falls back to ipc-1 since we now own
    /// ipc-0).  Best-effort; logs on failure.
    static void relaunchDiscord();

    std::thread          m_thread;             // acceptor thread (workerLoop)
    std::atomic<bool>    m_running{false};

    // Concurrency: the acceptor owns m_acceptPipe (the instance currently waiting
    // in ConnectNamedPipe); once a client connects, ownership transfers to a
    // serviceClient thread and the handle is tracked in m_clientPipes.  All three
    // are guarded by m_clientsMutex.  stop() closes m_acceptPipe to unblock the
    // acceptor and CancelIoEx()es each client handle to unblock its reader, then
    // joins every thread (each serviceClient closes its own handle on exit).
    std::mutex                m_clientsMutex;
    HANDLE                    m_acceptPipe{INVALID_HANDLE_VALUE};
    std::vector<HANDLE>       m_clientPipes;
    std::vector<std::thread>  m_clientThreads;

    bool                 m_autoBounce{true};   // gated off by OMNIPRESENCE_NO_AUTO_BOUNCE
    bool                 m_bounceTried{false};  // one-shot: never loop-kills Discord
    bool                 m_relaunchDiscordPending{false}; // restart after we claim ipc-0
#endif
};

} // namespace OmniPresence
