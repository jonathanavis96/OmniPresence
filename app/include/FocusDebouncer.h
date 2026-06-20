// FocusDebouncer.h — Debounce helper that gates rapid focus changes.
//
// Design rules (per spec):
//  - Commit a new window only after it has been stable for `stabilityMs`.
//  - Do NOT reset the elapsed/timestamp timer unless the activity *category*
//    changes (i.e. a different process, not just a new title within the same app).
//  - Skip the Discord update if the generated presence would be identical.
#pragma once

#include "WindowInfo.h"
#include <QElapsedTimer>
#include <functional>

namespace OmniPresence {

class FocusDebouncer {
public:
    /// Callback type: (committed WindowInfo, bool categoryChanged)
    using CommitCallback = std::function<void(const WindowInfo&, bool categoryChanged)>;

    /// @param stabilityMs   How long the same window must be held before commit (default 2500 ms).
    explicit FocusDebouncer(int stabilityMs = 2500);

    /// Feed a newly detected candidate window.  Call this on every poll tick.
    /// Invokes `onCommit` when the window has been stable long enough.
    void feed(const WindowInfo& candidate, const CommitCallback& onCommit);

    /// Reset internal state (e.g. on explicit user override).
    void reset();

    [[nodiscard]] int stabilityMs() const noexcept { return m_stabilityMs; }
    void setStabilityMs(int ms) noexcept { m_stabilityMs = ms; }

private:
    int             m_stabilityMs;
    WindowInfo      m_candidate;      ///< Window we're currently observing.
    WindowInfo      m_committed;      ///< Last window we committed.
    QElapsedTimer   m_stableTimer;    ///< How long candidate has been stable.
    bool            m_timerRunning{false};
};

} // namespace OmniPresence
