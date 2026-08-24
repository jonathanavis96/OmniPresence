// PoeLogWatcher.cpp — see PoeLogWatcher.h.
#include "PoeLogWatcher.h"
#include <QFile>
#include <QFileInfo>
#include <QDir>

namespace OmniPresence {

namespace {

// Steam's default install path plus the standalone GGG client's default
// locations — scanned in order after any user-configured override. Each root
// is tried with LatestClient.txt first (current-session only, truncated at
// every launch), falling back to the unbounded Client.txt, per the design.
const QStringList& candidateLogDirs() {
    static const QStringList dirs = {
        QStringLiteral("C:/Program Files (x86)/Steam/steamapps/common/Path of Exile/logs"),
        QStringLiteral("C:/Program Files (x86)/Grinding Gear Games/Path of Exile/logs"),
        QStringLiteral("C:/Program Files/Grinding Gear Games/Path of Exile/logs"),
    };
    return dirs;
}

QString firstExistingLog(const QString& dir) {
    for (const QString& name : {QStringLiteral("LatestClient.txt"), QStringLiteral("Client.txt")}) {
        const QString candidate = QDir(dir).filePath(name);
        if (QFileInfo::exists(candidate)) return candidate;
    }
    return {};
}

} // namespace

PoeLogWatcher::PoeLogWatcher(QObject* parent) : QObject(parent) {
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &PoeLogWatcher::poll);
}

void PoeLogWatcher::setLogPathForTest(const QString& path) {
    m_testPath    = path;
    m_useTestPath = true;
}

QString PoeLogWatcher::resolveLogPath() const {
    if (m_useTestPath) return m_testPath;

    if (!m_configuredPath.isEmpty()) {
        const QFileInfo fi(m_configuredPath);
        if (fi.isDir()) {
            const QString found = firstExistingLog(m_configuredPath);
            if (!found.isEmpty()) return found;
        } else if (fi.exists()) {
            return m_configuredPath;
        }
    }

    for (const QString& dir : candidateLogDirs()) {
        const QString found = firstExistingLog(dir);
        if (!found.isEmpty()) return found;
    }
    return {};
}

void PoeLogWatcher::start() {
    m_offset = 0;
    m_pendingPartialLine.clear();
    m_open = false;
    m_resolvedPath.clear();
    poll();          // resolve immediately rather than waiting a full second
    m_timer.start();
}

void PoeLogWatcher::stop() {
    m_timer.stop();
    m_open = false;
}

void PoeLogWatcher::openAndSeekToEnd(const QString& path) {
    if (!QFileInfo::exists(path)) {
        emit logNotFound(QStringLiteral(
            "No Path of Exile log found (checked configured path and common install roots)."));
        return;
    }
    QFile file(path);
    // Read-only, sharing permitted: the game holds this file open for
    // writing the whole time it runs, so an exclusive-access open would fail.
    if (!file.open(QIODevice::ReadOnly)) {
        emit logNotFound(QStringLiteral("PoE log found but unreadable: %1").arg(path));
        return;
    }
    m_resolvedPath = path;
    m_offset       = file.size();   // start at EOF — never replay backlog as history
    m_open         = true;
    emit logOpened(path);
}

void PoeLogWatcher::poll() {
    if (!m_open) {
        const QString path = resolveLogPath();
        if (path.isEmpty()) {
            emit logNotFound(QStringLiteral(
                "No Path of Exile log found (checked configured path and common install roots)."));
            return;
        }
        openAndSeekToEnd(path);
        if (!m_open) return;
    }
    readDelta();
}

void PoeLogWatcher::readDelta() {
    QFile file(m_resolvedPath);
    if (!file.exists()) {
        // Log vanished (uninstall, drive unmount) — go back to searching.
        m_open = false;
        emit logNotFound(QStringLiteral("PoE log no longer exists: %1").arg(m_resolvedPath));
        return;
    }
    if (!file.open(QIODevice::ReadOnly)) return;

    const qint64 size = file.size();
    if (size < m_offset) {
        // Truncated — either a fresh LatestClient.txt from a relaunch, or the
        // file was rotated out from under us. Reset and replay this (small)
        // new session from the start rather than skipping to its end: unlike
        // the very first launch, this content IS the new session.
        m_offset = 0;
        m_pendingPartialLine.clear();
    }

    if (size == m_offset) return;   // nothing new

    file.seek(m_offset);
    const QByteArray chunk = file.read(size - m_offset);
    m_offset = file.pos();

    QString text = m_pendingPartialLine + QString::fromUtf8(chunk);
    // CRLF line endings per the design; split on \n and strip a trailing \r.
    QStringList lines = text.split(QLatin1Char('\n'));
    m_pendingPartialLine = lines.isEmpty() ? QString() : lines.takeLast();

    for (QString line : lines) {
        if (line.endsWith(QLatin1Char('\r'))) line.chop(1);
        emit lineRead(line);
    }
}

} // namespace OmniPresence
