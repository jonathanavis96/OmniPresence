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
#include <cstring>
#include <iterator>

namespace OmniPresence {

namespace {

// UWP host resolution: a UWP app (Calculator, Photos, Store, Settings, …) shows
// a foreground "ApplicationFrameWindow" owned by ApplicationFrameHost.exe, while
// the real app lives in a child "Windows.UI.Core.CoreWindow" owned by its OWN
// process.  We enumerate the frame's children to find that CoreWindow so we can
// report Calculator.exe instead of ApplicationFrameHost.exe.
struct UwpChildSearch {
    DWORD hostPid;   // ApplicationFrameHost.exe pid — the child we want differs
    HWND  found;
};

BOOL CALLBACK findUwpAppChild(HWND child, LPARAM lp) {
    auto* s = reinterpret_cast<UwpChildSearch*>(lp);
    DWORD childPid = 0;
    ::GetWindowThreadProcessId(child, &childPid);
    if (childPid != 0 && childPid != s->hostPid) {
        wchar_t cls[256] = {};
        ::GetClassNameW(child, cls, static_cast<int>(std::size(cls)));
        if (wcscmp(cls, L"Windows.UI.Core.CoreWindow") == 0) {
            s->found = child;
            return FALSE; // stop enumerating
        }
    }
    return TRUE; // keep looking
}

} // namespace

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

    // Ignore our own window. Otherwise, every time the user alt-tabs back to
    // OmniPresence the presence would flip to "OmniPresence", and a manual
    // capture would be clobbered the moment focus returns to the app.
    if (candidate.pid == static_cast<uint32_t>(::GetCurrentProcessId())) {
        return;
    }

    m_debouncer.feed(candidate, [this](const WindowInfo& committed, bool categoryChanged) {
        Q_UNUSED(categoryChanged)
        emit activeWindowChanged(committed);
    });
}

WindowInfo Win32ActiveWindowWatcher::currentForegroundWindow() const {
    return queryForegroundWindow();
}

WindowInfo Win32ActiveWindowWatcher::queryForegroundWindow() const {
    HWND hwnd = ::GetForegroundWindow();
    if (!hwnd) return {};

    WindowInfo info;

    // ── Redirect UWP host frames to the hosted app's process ──────────────────
    // For a UWP "ApplicationFrameWindow", pid/exe must come from the hosted
    // CoreWindow child (procHwnd); the human-readable title stays on the frame
    // (hwnd) and is read below.
    HWND procHwnd = hwnd;
    {
        wchar_t frameCls[256] = {};
        if (::GetClassNameW(hwnd, frameCls, static_cast<int>(std::size(frameCls))) &&
            wcscmp(frameCls, L"ApplicationFrameWindow") == 0) {
            DWORD hostPid = 0;
            ::GetWindowThreadProcessId(hwnd, &hostPid);
            UwpChildSearch search{ hostPid, nullptr };
            ::EnumChildWindows(hwnd, &findUwpAppChild, reinterpret_cast<LPARAM>(&search));
            if (search.found) {
                procHwnd = search.found;
            }
        }
    }

    // ── PID ───────────────────────────────────────────────────────────────────
    DWORD pid = 0;
    ::GetWindowThreadProcessId(procHwnd, &pid);
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
