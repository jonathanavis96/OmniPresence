// WindowInfo.h — Plain data struct describing the currently focused window.
// Header-only; no Qt dependencies so it can be used from any translation unit.
#pragma once

#include <QString>
#include <cstdint>

namespace OmniPresence {

/// Snapshot of a focused window as detected by the active-window watcher.
struct WindowInfo {
    QString processName;      ///< e.g. "chrome.exe"
    QString executablePath;   ///< Full path, e.g. "C:/Program Files/Google/Chrome/..."
    QString windowTitle;      ///< Text returned by GetWindowText / XGetWindowProperty
    QString windowClass;      ///< Win32 class name (WNDCLASSEX.lpszClassName)
    uint32_t pid{0};          ///< OS process identifier

    /// True when none of the meaningful fields are populated.
    [[nodiscard]] bool isNull() const noexcept {
        return processName.isEmpty() && windowTitle.isEmpty() && pid == 0;
    }

    /// Equality ignores pid so watcher can detect title-only changes.
    [[nodiscard]] bool sameActivity(const WindowInfo& other) const noexcept {
        return processName == other.processName
            && windowTitle == other.windowTitle
            && executablePath == other.executablePath;
    }
};

} // namespace OmniPresence
