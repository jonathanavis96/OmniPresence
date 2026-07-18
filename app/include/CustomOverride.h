// CustomOverride.h — User-defined manual presence override (the "Custom" tab).
//
// A library of saved Presets — each a complete presence (name/details/state/
// icon) — that the user either pins one of (Single mode) or cycles through at a
// fixed interval (Cycle mode). When enabled the override sits at priority 0 in
// RuleEngine, above idle/AFK, pause, pin, integrations and every user rule.
//
// Persisted as a top-level "custom" object in config.json (see ConfigStore).
// AppController owns the cycle frame index and resolves the active preset into a
// PresencePayload; RuleEngine stays a pure selector and just returns it.
#pragma once

#include "PresencePayload.h"   // ActivityType (via Rule.h), PresencePayload
#include <QString>
#include <QList>

namespace OmniPresence {

/// One saved presence in the override library.
struct CustomPreset {
    QString      label{QStringLiteral("Custom")};   ///< List label (not published).
    QString      name;                              ///< Discord activity name.
    QString      details;
    QString      state;
    ActivityType activityType{ActivityType::Playing};
    QString      largeImageKey;                     ///< Public image URL.
    QString      largeImageText;
    QString      smallImageKey;
    QString      smallImageText;
    bool         includeInCycle{true};              ///< Part of the Cycle sequence.
};

/// A reusable image URL (uploaded via catbox or pasted), offered as a dropdown
/// so a preset's icon can be re-selected without re-hosting.
struct CustomImageAsset {
    QString label;   ///< Friendly name (e.g. original filename).
    QString url;     ///< Public direct URL.
};

/// How the override chooses which preset to publish.
enum class CustomMode {
    Single,   ///< Statically show presets[activeIndex]. No timer.
    Cycle,    ///< Step through includeInCycle presets, one every intervalSeconds.
};

/// The whole feature's persisted state.
struct CustomOverrideConfig {
    bool                    enabled{false};        ///< Master toggle (independent of pause).
    CustomMode              mode{CustomMode::Single};
    int                     activeIndex{0};        ///< Single-mode selection.
    int                     intervalSeconds{4};    ///< Cycle-mode step (min 1).
    QList<CustomPreset>     presets;
    QList<CustomImageAsset> imageLibrary;

    /// Resolve a preset into a publishable PresencePayload. Returns nullopt when
    /// the index is out of range or the preset's name is blank (Discord shows an
    /// empty name as the bare app name — never publish that).
    [[nodiscard]] std::optional<PresencePayload> resolve(int index) const;

    /// Indices of presets flagged includeInCycle, in list order. The cycle steps
    /// over exactly these; empty means the override publishes nothing.
    [[nodiscard]] QList<int> cycleIndices() const;
};

} // namespace OmniPresence
