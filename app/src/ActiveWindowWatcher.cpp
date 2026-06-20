// ActiveWindowWatcher.cpp — Factory implementation.
// Returns the correct concrete watcher for the current platform.
#include "ActiveWindowWatcher.h"

#ifdef _WIN32
#  include "Win32ActiveWindowWatcher.h"
#else
#  include "StubActiveWindowWatcher.h"
#endif

namespace OmniPresence {

std::unique_ptr<ActiveWindowWatcher> createActiveWindowWatcher(QObject* parent) {
#ifdef _WIN32
    return std::make_unique<Win32ActiveWindowWatcher>(750, parent);
#else
    return std::make_unique<StubActiveWindowWatcher>(parent);
#endif
}

} // namespace OmniPresence
