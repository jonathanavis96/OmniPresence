// FocusDebouncer.cpp — Implements the focus-stability debounce logic.
#include "FocusDebouncer.h"
#include <QDebug>

namespace OmniPresence {

FocusDebouncer::FocusDebouncer(int stabilityMs)
    : m_stabilityMs(stabilityMs)
{}

void FocusDebouncer::feed(const WindowInfo& candidate, const CommitCallback& onCommit) {
    // Same candidate as we're already tracking?
    if (candidate.sameActivity(m_candidate)) {
        // Already stable — check whether enough time has elapsed.
        if (m_timerRunning && m_stableTimer.elapsed() >= m_stabilityMs) {
            // Only commit if it differs from what we last committed.
            if (!candidate.sameActivity(m_committed)) {
                const bool categoryChanged = (candidate.processName != m_committed.processName);
                m_committed     = candidate;
                m_timerRunning  = false;   // Arm again on next new candidate.
                onCommit(m_committed, categoryChanged);
            } else {
                // Same as last committed — nothing to do; reset timer to avoid repeated fires.
                m_timerRunning = false;
            }
        }
        return;
    }

    // New candidate — restart the stability timer.
    m_candidate    = candidate;
    m_timerRunning = true;
    m_stableTimer.restart();
}

void FocusDebouncer::reset() {
    m_candidate    = {};
    m_committed    = {};
    m_timerRunning = false;
}

} // namespace OmniPresence
