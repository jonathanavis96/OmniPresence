// SkillLabel.h — Maps a captured RuneLite skill/activity string to a friendly
// "Training {Skill}" label for Discord Rich Presence.
//
// The built-in RuneLite Discord plugin emits `details` values like
// "Training: Mining" / "Training: Crafting" (skill currently being trained)
// and occasionally a bare skill token like "Runecraft". skillLabel()
// recognises the fixed 23-skill OSRS skill set (case-insensitive, with or
// without a leading "Training:"/"Training " prefix), applies the display
// name (Runecraft -> "Runecrafting"), and returns "Training {DisplayName}".
// Anything that isn't a recognised skill (bosses, minigames, e.g.
// "Fighting: Zulrah") passes through verbatim so real activity is never
// mislabelled as training.
#pragma once

#include <QString>

namespace omni {

/// Returns a friendly "Training {Skill}" label when `activity` names (or is
/// prefixed with "Training:"/"Training ") one of the 23 OSRS skills;
/// otherwise returns `activity` unchanged (trimmed). Empty input -> empty
/// output.
QString skillLabel(const QString& activity);

} // namespace omni
