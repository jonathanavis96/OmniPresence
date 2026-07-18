// CustomOverride.cpp — preset→payload resolution and cycle sequencing.
#include "CustomOverride.h"

namespace OmniPresence {

std::optional<PresencePayload> CustomOverrideConfig::resolve(int index) const {
    if (index < 0 || index >= presets.size()) return std::nullopt;

    const CustomPreset& preset = presets.at(index);

    // A blank name would render on Discord as the bare application name
    // ("OmniPresence"), so an unnamed preset never publishes — the caller falls
    // through to normal rules instead. Mirrors RuleEngine's empty-name guard.
    if (preset.name.trimmed().isEmpty()) return std::nullopt;

    PresencePayload p;
    p.activityType    = preset.activityType;
    p.name            = preset.name;
    p.details         = preset.details;
    p.state           = preset.state;
    p.largeImageKey   = preset.largeImageKey;
    p.largeImageText  = preset.largeImageText;
    p.smallImageKey   = preset.smallImageKey;
    p.smallImageText  = preset.smallImageText;
    p.matchedRuleName = QStringLiteral("(custom override)");
    p.privacyLevel    = PrivacyLevel::Public;
    return p;
}

QList<int> CustomOverrideConfig::cycleIndices() const {
    QList<int> out;
    for (int i = 0; i < presets.size(); ++i) {
        // A blank-name preset resolve()s to nothing, so including it in the cycle
        // would leave one frame with no override and fall back to normal rules
        // (new presets default includeInCycle=true and start unnamed). Skip them.
        if (presets.at(i).includeInCycle && !presets.at(i).name.trimmed().isEmpty())
            out.append(i);
    }
    return out;
}

} // namespace OmniPresence
