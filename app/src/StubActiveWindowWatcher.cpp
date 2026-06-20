// StubActiveWindowWatcher.cpp — Synthetic window events for non-Windows dev.
// Rotates through a small set of realistic scenarios every 8 seconds.
#include "StubActiveWindowWatcher.h"
#include <QDebug>

namespace OmniPresence {

namespace {

struct StubScenario {
    const char* processName;
    const char* executablePath;
    const char* windowTitle;
    const char* windowClass;
    uint32_t    pid;
};

static const StubScenario kScenarios[] = {
    { "chrome.exe",        "C:/Program Files/Google/Chrome/Application/chrome.exe",
      "YouTube - Playing: Lo-fi Hip Hop", "Chrome_WidgetWin_1", 1001 },
    { "Code.exe",          "C:/Users/user/AppData/Local/Programs/Microsoft VS Code/Code.exe",
      "AppController.cpp — OmniPresence", "Chrome_WidgetWin_1", 1002 },
    { "RuneLite.exe",      "C:/Users/user/AppData/Local/RuneLite/RuneLite.exe",
      "RuneLite - Username",               "SunAwtFrame",        1003 },
    { "WindowsTerminal.exe","C:/Program Files/WindowsApps/Microsoft.WindowsTerminal/wt.exe",
      "Ubuntu — ~/code/OmniPresence",     "CASCADIA_HOSTING_WINDOW_CLASS", 1004 },
    { "discord.exe",       "C:/Users/user/AppData/Local/Discord/app-1.0.0/discord.exe",
      "Discord",                           "Chrome_WidgetWin_1", 1005 },
};

constexpr int kNumScenarios = static_cast<int>(std::size(kScenarios));

} // anonymous namespace

StubActiveWindowWatcher::StubActiveWindowWatcher(QObject* parent)
    : ActiveWindowWatcher(parent)
{
    connect(&m_timer, &QTimer::timeout, this, &StubActiveWindowWatcher::emitNextStub);
}

void StubActiveWindowWatcher::start() {
    qDebug() << "[StubActiveWindowWatcher] Running in stub mode — no real window detection.";
    m_timer.start(8000);
    // Emit the first scenario immediately.
    emitNextStub();
}

void StubActiveWindowWatcher::stop() {
    m_timer.stop();
}

bool StubActiveWindowWatcher::isRunning() const {
    return m_timer.isActive();
}

WindowInfo StubActiveWindowWatcher::currentForegroundWindow() const {
    return m_lastEmitted;
}

void StubActiveWindowWatcher::emitNextStub() {
    const StubScenario& s = kScenarios[m_cycle % kNumScenarios];
    WindowInfo info;
    info.processName    = QString::fromUtf8(s.processName);
    info.executablePath = QString::fromUtf8(s.executablePath);
    info.windowTitle    = QString::fromUtf8(s.windowTitle);
    info.windowClass    = QString::fromUtf8(s.windowClass);
    info.pid            = s.pid;
    ++m_cycle;
    m_lastEmitted = info;
    emit activeWindowChanged(info);
}

} // namespace OmniPresence
