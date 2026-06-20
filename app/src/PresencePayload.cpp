// PresencePayload.cpp — Method implementations for PresencePayload.
// This file was not listed in the original spec but is required so
// isSamePresence() and debugSummary() have a translation unit.
// CMakeLists.txt includes it in OMNIPRESENCE_SOURCES.
#include "PresencePayload.h"

namespace OmniPresence {

bool PresencePayload::isSamePresence(const PresencePayload& other) const noexcept {
    return activityType    == other.activityType
        && name            == other.name
        && details         == other.details
        && state           == other.state
        && largeImageKey   == other.largeImageKey
        && largeImageText  == other.largeImageText
        && smallImageKey   == other.smallImageKey
        && smallImageText  == other.smallImageText
        && isPrivateFallback == other.isPrivateFallback;
    // Intentionally ignores timestamps and matchedRuleName so we don't
    // re-push presence just because the rule name string changed.
}

QString PresencePayload::debugSummary() const {
    return QStringLiteral("[%1] name=%2 | details=%3 | state=%4%5")
        .arg(matchedRuleName)
        .arg(name)
        .arg(details)
        .arg(state)
        .arg(isPrivateFallback ? QStringLiteral(" (private)") : QString{});
}

} // namespace OmniPresence
