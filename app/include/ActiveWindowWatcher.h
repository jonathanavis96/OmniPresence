// ActiveWindowWatcher.h — Abstract interface + factory for active-window detection.
// Concrete implementations: Win32ActiveWindowWatcher (Windows) / StubActiveWindowWatcher (dev).
#pragma once

#include "WindowInfo.h"
#include <QObject>
#include <memory>

namespace OmniPresence {

/// Abstract base class.  Emit activeWindowChanged() whenever the focused
/// window has been stable for the debounce window (implemented via
/// FocusDebouncer internally or in the concrete class).
class ActiveWindowWatcher : public QObject {
    Q_OBJECT
public:
    explicit ActiveWindowWatcher(QObject* parent = nullptr) : QObject(parent) {}
    ~ActiveWindowWatcher() override = default;

    /// Start polling / hooking.
    virtual void start() = 0;
    /// Stop polling / hooking.
    virtual void stop()  = 0;
    /// Whether the watcher is currently running.
    [[nodiscard]] virtual bool isRunning() const = 0;

    /// Read the foreground window right now, bypassing the debounce window.
    /// Used by the "Capture next window" countdown so the snapshot reflects
    /// whatever the user focused, without waiting for the stability period.
    [[nodiscard]] virtual WindowInfo currentForegroundWindow() const = 0;

signals:
    /// Emitted after the debounce window has elapsed and the focused window
    /// has been stable for at least the required stability period.
    void activeWindowChanged(const OmniPresence::WindowInfo& info);

    /// Emitted when an error prevents window detection (e.g. access denied).
    void watcherError(const QString& message);
};

/// Factory — picks Win32 implementation on Windows, Stub on every other platform.
std::unique_ptr<ActiveWindowWatcher> createActiveWindowWatcher(QObject* parent = nullptr);

} // namespace OmniPresence
