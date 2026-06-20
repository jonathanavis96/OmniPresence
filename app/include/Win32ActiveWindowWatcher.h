// Win32ActiveWindowWatcher.h — Windows-only active-window watcher.
// Compiled only when _WIN32 is defined.  Uses GetForegroundWindow +
// QueryFullProcessImageName + GetWindowText + GetWindowThreadProcessId.
#pragma once

#ifdef _WIN32

#include "ActiveWindowWatcher.h"
#include "FocusDebouncer.h"
#include <QTimer>

namespace OmniPresence {

class Win32ActiveWindowWatcher : public ActiveWindowWatcher {
    Q_OBJECT
public:
    /// @param pollIntervalMs  How often to check the foreground window (default 750 ms).
    explicit Win32ActiveWindowWatcher(int pollIntervalMs = 750, QObject* parent = nullptr);
    ~Win32ActiveWindowWatcher() override;

    void start()  override;
    void stop()   override;
    [[nodiscard]] bool isRunning() const override;
    [[nodiscard]] WindowInfo currentForegroundWindow() const override;

private slots:
    void onPollTimer();

private:
    /// Read the current foreground window from Win32 APIs.
    WindowInfo queryForegroundWindow() const;

    QTimer          m_pollTimer;
    FocusDebouncer  m_debouncer;
    int             m_pollIntervalMs{750};
};

} // namespace OmniPresence

#endif // _WIN32
