// PoeLogWatcher.h — Tails Path of Exile's Client.txt / LatestClient.txt.
//
// Owns *reading* only — no parsing (see PoeActivityInferencer) and no
// knowledge of presence. Polls on a 1 s timer rather than QFileSystemWatcher:
// on Windows that coalesces and misses rapid appends to a file held open by
// another process (see the design doc).
#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

namespace OmniPresence {

class PoeLogWatcher : public QObject {
    Q_OBJECT
public:
    explicit PoeLogWatcher(QObject* parent = nullptr);

    /// Start polling. Resolves the log path (unless pinned via
    /// setLogPathForTest) each tick until found, then seeks to end-of-file so
    /// a pre-existing backlog never replays as presence history, and begins
    /// emitting lineRead() for everything appended from that point on.
    void start();
    /// Stop polling. Safe to call multiple times / before start().
    void stop();

    /// Optional config override for the log file or its containing directory;
    /// empty = auto-resolve (Steam default, then other common install roots).
    void setConfiguredPath(const QString& path) { m_configuredPath = path; }

    /// Test seam: pin the watcher directly at a file, bypassing path
    /// resolution/scanning entirely.
    void setLogPathForTest(const QString& path);

    /// Test seam: run one poll cycle synchronously instead of waiting for the
    /// 1 s timer, so tests aren't tied to real wall-clock delay.
    void pollOnceForTest() { poll(); }

    [[nodiscard]] QString resolvedPath() const noexcept { return m_resolvedPath; }

signals:
    /// One complete line (CR/LF already stripped), in the order it appeared.
    void lineRead(const QString& line);
    /// Emitted once when a path is found and opened (including on
    /// re-resolution after the log disappears and comes back).
    void logOpened(const QString& path);
    /// Emitted while no log file can be found — a clear "not found" state
    /// rather than silent failure. Repeats every poll while still missing so
    /// a late-arriving install (game launched after OmniPresence) is
    /// eventually picked up.
    void logNotFound(const QString& reason);

private:
    void poll();
    [[nodiscard]] QString resolveLogPath() const;
    void openAndSeekToEnd(const QString& path);
    void readDelta();

    QTimer   m_timer;
    QString  m_configuredPath;
    QString  m_testPath;
    bool     m_useTestPath{false};
    QString  m_resolvedPath;
    qint64   m_offset{0};
    QString  m_pendingPartialLine;
    bool     m_open{false};
};

} // namespace OmniPresence
