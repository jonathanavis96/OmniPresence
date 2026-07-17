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
//
// IMPORTANT — self-heal (mid-session):
//   The startup bounce above only fires once.  If Discord is not running yet at
//   startup, or the user quits/relaunches it later, Discord will re-claim
//   discord-ipc-0 as an extra PIPE_UNLIMITED_INSTANCES server and RuneLite's
//   built-in plugin can race onto it again — owning the pipe NAME does not evict
//   a rival SERVER process on that name.  A background watchdog thread (see
//   watchdogLoop()) re-checks every ~10 s and re-bounces (cooldown ~30 s) if
//   Discord has reappeared and is coexisting on ipc-0.
#pragma once

#include <QObject>
#include <QString>

#ifdef Q_OS_WIN
#include <atomic>
#include <chrono>
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
    /// Acceptor loop for one pipe name (discord-ipc-<pipeIndex>).  Repeatedly
    /// creates a fresh pipe INSTANCE and waits for a client.  Each accepted client
    /// is handed to its own serviceClient() thread so multiple Discord clients
    /// (e.g. RuneLite's built-in plugin AND our own Social SDK probe) can be
    /// connected simultaneously.  Two of these run — pipeIndex 0 and 1 — so we own
    /// both discord-ipc-0 and discord-ipc-1 and RuneLite (which scans ipc-0 →
    /// ipc-1 → …) can never fall through to the real Discord (bounced to ipc-2).
    void workerLoop(int pipeIndex);

    /// Self-heal watchdog: periodically re-checks (every ~10 s, in 500 ms steps
    /// so stop() is responsive) whether Discord has (re)appeared and reclaimed
    /// discord-ipc-0 as a coexisting server. Owning the pipe NAME does not evict
    /// a rival SERVER on that name — only killing the rival process does — so if
    /// Discord starts/restarts after us, RuneLite can race onto it again exactly
    /// like the original Task 1.2 bug. The startup bounce (m_bounceTried) is a
    /// one-shot; this loop is what handles every re-appearance for the rest of
    /// the session, gated only by its own cooldown (m_lastRebounceTicks).
    void watchdogLoop();

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

    /// True if any Discord.exe process is currently running.  A running Discord is
    /// the reliable signal that it will (re)claim discord-ipc-0 — the pipe probe
    /// (discordPipeAlreadyServed) false-negatives because PIPE_UNLIMITED_INSTANCES
    /// lets Discord coexist as an invisible extra ipc-0 instance, which let
    /// RuneLite's connections race onto Discord and steal the presence.
    static bool isDiscordProcessRunning();

    /// Relaunch Discord via its updater (it falls back to ipc-1 since we now own
    /// ipc-0).  Best-effort; logs on failure.
    static void relaunchDiscord();

    std::thread          m_threads[2];         // acceptor threads: ipc-0, ipc-1
    std::thread          m_watchdog;           // self-heal re-bounce thread (Task 1.3)
    std::atomic<bool>    m_running{false};

    // Concurrency: the acceptor owns m_acceptPipe (the instance currently waiting
    // in ConnectNamedPipe); once a client connects, ownership transfers to a
    // serviceClient thread and the handle is tracked in m_clientPipes.  All three
    // are guarded by m_clientsMutex.  stop() closes m_acceptPipe to unblock the
    // acceptor and CancelIoEx()es each client handle to unblock its reader, then
    // joins every thread (each serviceClient closes its own handle on exit).
    std::mutex                m_clientsMutex;
    HANDLE                    m_acceptPipe[2]{INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE};
    std::vector<HANDLE>       m_clientPipes;
    std::vector<std::thread>  m_clientThreads;

    bool                 m_autoBounce{true};   // gated off by OMNIPRESENCE_NO_AUTO_BOUNCE
    std::atomic<bool>    m_bounceTried{false};  // one-shot: never loop-kills Discord
    std::atomic<bool>    m_killDone{false};     // leader signals follower Discord is dead
    std::atomic<int>     m_claimedCount{0};     // #pipes claimed; relaunch Discord at 2
    std::atomic<bool>    m_relaunchDiscordPending{false}; // relaunch Discord onto ipc-2

    // Watchdog re-bounce cooldown (Task 1.3), stored as raw steady_clock ticks
    // (std::chrono::steady_clock::rep) so it fits in a lock-free atomic; 0 means
    // "never re-bounced yet" and is far enough in the past that the first
    // watchdog re-bounce is never blocked by the cooldown.
    std::atomic<std::chrono::steady_clock::rep> m_lastRebounceTicks{0};
#endif
};

} // namespace OmniPresence
