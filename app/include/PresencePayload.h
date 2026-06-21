// PresencePayload.h — The resolved Discord presence to be sent to the SDK.
// Produced by RuleEngine; consumed by DiscordPresenceClient.
#pragma once

#include "Rule.h"
#include <QString>
#include <QDateTime>

namespace OmniPresence {

/// Which field Discord shows in the compact member-list / sidebar status.
/// Mirrors discordpp::StatusDisplayTypes without depending on the SDK header.
enum class StatusDisplay {
    Name,     ///< Show the activity name, e.g. "Claude" (normal Discord behaviour).
    State,    ///< Show the state line.
    Details,  ///< Show the details line, e.g. "Training Slayer".
};

/// Fully-resolved Discord Rich Presence payload.
struct PresencePayload {
    ActivityType activityType{ActivityType::Playing};

    // Discord fields (post-template resolution)
    QString name;           ///< Broad activity label shown on the profile card.
    QString details;        ///< "What I'm doing" line.
    QString state;          ///< Specific context line.

    /// Which line Discord surfaces in the compact sidebar status.
    StatusDisplay statusDisplay{StatusDisplay::Name};
    QString largeImageKey;
    QString largeImageText;
    QString smallImageKey;
    QString smallImageText;

    // Timestamp control
    TimestampMode timestampMode{TimestampMode::CategoryChange};
    QDateTime     activityStartedAt;   ///< Set by RuleEngine when category changes.

    // Meta
    QString       matchedRuleName;    ///< Which rule produced this (empty = fallback).
    PrivacyLevel  privacyLevel{PrivacyLevel::Public};
    bool          isPrivateFallback{false};

    /// True when all meaningful fields are identical to `other`.
    /// Used to skip redundant Discord API calls.
    [[nodiscard]] bool isSamePresence(const PresencePayload& other) const noexcept;

    /// Human-readable summary for debugging / UI preview.
    [[nodiscard]] QString debugSummary() const;
};

} // namespace OmniPresence
