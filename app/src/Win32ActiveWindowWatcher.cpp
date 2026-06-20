// Win32ActiveWindowWatcher.cpp — Windows active-window detection.
// Uses GetForegroundWindow / GetWindowThreadProcessId / OpenProcess /
// QueryFullProcessImageName / GetWindowText.
// Compiled only on _WIN32.
#ifdef _WIN32

#include "Win32ActiveWindowWatcher.h"
#include <QDebug>

// Windows headers — must come after Qt headers to avoid macro collisions.
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>

namespace OmniPresence {

Win32ActiveWindowWatcher::Win32ActiveWindowWatcher(int pollIntervalMs, QObject* parent)
    : ActiveWindowWatcher(parent)
    , m_pollIntervalMs(pollIntervalMs)
    , m_debouncer(2500) // 2.5 s stability window
{
    connect(&m_pollTimer, &QTimer::timeout, this, &Win32ActiveWindowWatcher::onPollTimer);
}

Win32ActiveWindowWatcher::~Win32ActiveWindowWatcher() {
    stop();
}

void Win32ActiveWindowWatcher::start() {
    m_pollTimer.start(m_pollIntervalMs);
}

void Win32ActiveWindowWatcher::stop() {
    m_pollTimer.stop();
}

bool Win32ActiveWindowWatcher::isRunning() const {
    return m_pollTimer.isActive();
}

// ── Polling ───────────────────────────────────────────────────────────────────

void Win32ActiveWindowWatcher::onPollTimer() {
    const WindowInfo candidate = queryForegroundWindow();

    m_debouncer.feed(candidate, [this](const WindowInfo& committed, bool categoryChanged) {
        Q_UNUSED(categoryChanged)
        emit activeWindowChanged(committed);
    });
}

WindowInfo Win32ActiveWindowWatcher::queryForegroundWindow() const {
    HWND hwnd = ::GetForegroundWindow();
    if (!hwnd) return {};

    WindowInfo info;

    // ── PID ───────────────────────────────────────────────────────────────────
    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    info.pid = static_cast<uint32_t>(pid);

    // ── Executable path & process name ────────────────────────────────────────
    HANDLE hProc = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProc) {
        wchar_t exePath[MAX_PATH] = {};
        DWORD   len = MAX_PATH;
        if (::QueryFullProcessImageNameW(hProc, 0, exePath, &len)) {
            info.executablePath = QString::fromWCharArray(exePath, static_cast<int>(len));
            // Extract filename as process name.
            const int sep = info.executablePath.lastIndexOf(QLatin1Char('\\'));
            info.processName = (sep >= 0)
                ? info.executablePath.mid(sep + 1)
                : info.executablePath;
        }
        ::CloseHandle(hProc);
    }

    // ── Window title ──────────────────────────────────────────────────────────
    wchar_t title[1024] = {};
    const int titleLen = ::GetWindowTextW(hwnd, title, static_cast<int>(std::size(title)));
    if (titleLen > 0) {
        info.windowTitle = QString::fromWCharArray(title, titleLen);
    }

    // ── Window class ──────────────────────────────────────────────────────────
    wchar_t cls[256] = {};
    if (::GetClassNameW(hwnd, cls, static_cast<int>(std::size(cls)))) {
        info.windowClass = QString::fromWCharArray(cls);
    }

    return info;
}

} // namespace OmniPresence

#endif // _WIN32
