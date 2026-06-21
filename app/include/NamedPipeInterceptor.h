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
//   Quit Discord BEFORE launching OmniPresence.  If Discord already owns
//   discord-ipc-0, CreateNamedPipeW returns ERROR_PIPE_BUSY / ERROR_ACCESS_DENIED
//   and the interceptor logs guidance instead of crashing.
#pragma once

#include <QObject>
#include <QString>

#ifdef Q_OS_WIN
#include <atomic>
#include <thread>
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
    void workerLoop();

    /// Read exactly \p count bytes from \p pipe into \p buf.
    /// Returns false on EOF or error (treat as disconnect).
    static bool readExact(HANDLE pipe, void* buf, DWORD count);

    /// Write \p opcode + \p json as one contiguous frame buffer.
    /// CRITICAL: does NOT call FlushFileBuffers — see implementation comment.
    static bool writeFrame(HANDLE pipe, int opcode, const QString& json);

    std::thread          m_thread;
    std::atomic<bool>    m_running{false};
    HANDLE               m_pipe{INVALID_HANDLE_VALUE};
#endif
};

} // namespace OmniPresence
