// InputIdleMonitor.h — System-wide input-idle duration monitor.
//
// Privacy constraint: this reports IDLE *DURATION* ONLY (how long since the
// last keyboard/mouse input, in seconds). It never reads key/mouse content,
// never installs a global input hook, and never observes which key or button
// was pressed — only the timestamp Windows itself already tracks internally
// for screensaver / power-management purposes (GetLastInputInfo).
#pragma once

#include <QObject>

namespace OmniPresence {

/// Wraps Win32 GetLastInputInfo. Deliberately has NO internal QTimer — the
/// AppController owns the polling cadence (a ~5 s tick that re-evaluates and
/// republishes presence), so this class is a pure, cheap, on-demand query.
class InputIdleMonitor : public QObject {
    Q_OBJECT
public:
    explicit InputIdleMonitor(QObject* parent = nullptr);

    /// Seconds since the last system-wide keyboard/mouse input.
    /// Computed as (GetTickCount() - LASTINPUTINFO.dwTime) / 1000.
    /// Returns 0 on non-Windows builds (stub) so the app still compiles/runs.
    [[nodiscard]] quint64 idleSeconds() const;
};

} // namespace OmniPresence
