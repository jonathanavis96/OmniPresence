// InputIdleMonitor.cpp — Win32 GetLastInputInfo wrapper.
//
// Privacy constraint (see header): duration only, no input hook, no key
// content ever read.
#include "InputIdleMonitor.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace OmniPresence {

InputIdleMonitor::InputIdleMonitor(QObject* parent) : QObject(parent) {}

quint64 InputIdleMonitor::idleSeconds() const {
#ifdef _WIN32
    LASTINPUTINFO lii;
    lii.cbSize = sizeof(LASTINPUTINFO);
    if (!::GetLastInputInfo(&lii)) {
        return 0;
    }
    // Both GetTickCount() and dwTime are ms-since-boot DWORDs; unsigned
    // subtraction wraps correctly across the ~49.7-day GetTickCount rollover.
    const DWORD idleMs = ::GetTickCount() - lii.dwTime;
    return static_cast<quint64>(idleMs) / 1000;
#else
    // Non-Windows stub — no idle detection available on this platform; the
    // idle-tier override in RuleEngine simply never fires (idleSeconds stays
    // 0 < any configured threshold).
    return 0;
#endif
}

} // namespace OmniPresence
