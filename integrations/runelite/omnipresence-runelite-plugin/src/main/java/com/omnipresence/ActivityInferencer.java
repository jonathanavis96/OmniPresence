package com.omnipresence;

import lombok.Value;

import java.util.Map;
import java.util.Set;
import java.util.function.LongSupplier;

/**
 * Pure-ish activity inference from observed RuneLite signals.
 *
 * Design intent: keep this free of RuneLite API imports so it can be
 * unit-tested without a game client on the classpath. Callers convert
 * RuneLite types to the plain primitives this class consumes.
 */
public class ActivityInferencer {

    // ---------------------------------------------------------------------------
    // Region IDs — RuneLite uses 16-bit packed region identifiers.
    // Format: (mapX >> 6) << 8 | (mapY >> 6) — i.e. floor(worldX / 64) combined.
    // ---------------------------------------------------------------------------

    /** Grand Exchange region. */
    private static final int REGION_GE = 12598;

    /** Lumbridge region (banking). */
    private static final int REGION_LUMBRIDGE = 12850;

    /** Edgeville region. */
    private static final int REGION_EDGEVILLE = 12342;

    /** Varrock West Bank region. */
    private static final int REGION_VARROCK_WEST = 12853;

    /** Asgarnian Ice Dungeon region. */
    private static final int REGION_ASGARNIAN_ICE_DUNGEON = 11672;

    /** Slayer Tower region. */
    private static final int REGION_SLAYER_TOWER = 13623;

    /** Catacombs of Kourend. */
    private static final int REGION_CATACOMBS = 6557;

    /** God Wars Dungeon lobby. */
    private static final int REGION_GWD = 11578;

    /** Corporeal Beast. */
    private static final int REGION_CORP = 11842;

    /** Theatre of Blood (entrance region). */
    private static final int REGION_TOB = 12613;

    /** Chambers of Xeric (raid start). */
    private static final int REGION_COX = 12889;

    /** Nightmare of Ashihama. */
    private static final int REGION_NIGHTMARE = 15515;

    /** Fossil Island birdhouse trap regions (the 4 spots a run cycles through).
     *  Hunter/Crafting XP in any of these is a birdhouse run, not standalone
     *  skill training — see the birdhouse branch in {@link #infer0}. */
    private static final Set<Integer> BIRDHOUSE_REGIONS = Set.of(14651, 14652, 14906, 14908);

    /** Skill indices that drop XP during a birdhouse run: Crafting (12, building
     *  the birdhouse) and Hunter (21, dismantling/collecting). */
    private static final int SKILL_CRAFTING = 12;
    private static final int SKILL_HUNTER = 21;

    // ---------------------------------------------------------------------------
    // Animation IDs (sampled from the OSRS animation list; extend as needed).
    // ---------------------------------------------------------------------------

    private static final Set<Integer> IDLE_ANIMATIONS = Set.of(808, 813);
    private static final Set<Integer> FISHING_ANIMATIONS = Set.of(621, 622, 623, 624, 625);
    private static final Set<Integer> WOODCUTTING_ANIMATIONS = Set.of(867, 868, 2846, 2847, 6782, 7651);
    private static final Set<Integer> MINING_ANIMATIONS = Set.of(624, 625, 628, 629, 630, 6747, 6748, 7139, 7150, 7155);
    private static final Set<Integer> COOKING_ANIMATIONS = Set.of(883, 897);
    private static final Set<Integer> FLETCHING_ANIMATIONS = Set.of(1248, 6702);
    private static final Set<Integer> CRAFTING_ANIMATIONS = Set.of(1249, 1251, 1252, 7531);
    private static final Set<Integer> HERBLORE_ANIMATIONS = Set.of(363, 364);
    private static final Set<Integer> SMITHING_ANIMATIONS = Set.of(898, 899, 900, 901, 902);
    private static final Set<Integer> PRAYER_ANIMATIONS = Set.of(645);
    private static final Set<Integer> BANKING_ANIMATIONS = Set.of(832);

    // ---------------------------------------------------------------------------
    // NPC names associated with Slayer tasks (partial list; RuneLite has more).
    // ---------------------------------------------------------------------------

    private static final Set<String> SLAYER_NPCS = Set.of(
        "Aberrant Spectre", "Abyssal Demon", "Adamant Dragon", "Ankou",
        "Aviansie", "Banshee", "Basilisk", "Basilisk Knight",
        "Black Demon", "Black Dragon", "Bloodveld", "Blue Dragon",
        "Brine Rat", "Bronze Dragon", "Cave Bug", "Cave Crawler",
        "Cave Horror", "Cave Slime", "Cockatrice", "Crawling Hand",
        "Dagannoth", "Dark Beast", "Desert Lizard", "Dust Devil",
        "Dwarf", "Earth Warrior", "Elves", "Fever Spider",
        "Fire Giant", "Fossil Island Wyvern", "Gargoyle", "Ghoul",
        "Greater Demon", "Green Dragon", "Harpie Bug Swarm", "Hellhound",
        "Hill Giant", "Hobgoblin", "Ice Giant", "Ice Spider",
        "Ice Troll", "Ice Warrior", "Infernal Mage", "Iron Dragon",
        "Jungle Horror", "Kalphite", "Killerwatt", "Kurask",
        "Lavender Dragon", "Lesser Demon", "Locust Rider", "Mogre",
        "Molanisk", "Moss Giant", "Mutated Zygomite", "Nechryael",
        "Ogre", "Otherworldly Being", "Pyrefiend", "Rock Slug",
        "Rockslugs", "Rogue", "Ruins Guard", "Sea Snake",
        "Skeletal Wyvern", "Smoke Devil", "Spiritual Warrior",
        "Steel Dragon", "Suqah", "Terrorbird", "Troll",
        "Turoth", "Tzhaar", "Vampire", "Wall Beast",
        "Waterfiend", "Werewolf", "Wyrm"
    );

    // ---------------------------------------------------------------------------
    // Boss NPC names (subset; extend freely).
    // ---------------------------------------------------------------------------

    private static final Set<String> BOSS_NPCS = Set.of(
        "Abyssal Sire", "Alchemical Hydra", "Barrows Brother",
        "Bryophyta", "Callisto", "Cerberus", "Chaos Elemental",
        "Chaos Fanatic", "Commander Zilyana", "Corporeal Beast",
        "Crazy Archaeologist", "Dagannoth Prime", "Dagannoth Rex",
        "Dagannoth Supreme", "Demonic Gorilla", "Duke Sucellus",
        "General Graardor", "Giant Mole", "Grotesque Guardians",
        "Hespori", "Kalphite Queen", "K'ril Tsutsaroth", "Kree'arra",
        "Kraken", "Leviathan", "Lunar Chest", "Phantom Muspah",
        "Sarachnis", "Scorpia", "Skotizo", "Tempoross",
        "Thermonuclear Smoke Devil", "The Leviathan", "The Mimic",
        "The Nightmare", "Venenatis", "Vet'ion", "Vardorvis",
        "Vorkath", "Whisperer", "Wintertodt", "Zalcano",
        "Zulrah"
    );

    // ---------------------------------------------------------------------------
    // Skill index → name (mirrors net.runelite.api.Skill ordinal order).
    // ---------------------------------------------------------------------------

    private static final Map<Integer, String> SKILL_NAMES = Map.ofEntries(
        Map.entry(0,  "Attack"),
        Map.entry(1,  "Defence"),
        Map.entry(2,  "Strength"),
        Map.entry(3,  "Hitpoints"),
        Map.entry(4,  "Ranged"),
        Map.entry(5,  "Prayer"),
        Map.entry(6,  "Magic"),
        Map.entry(7,  "Cooking"),
        Map.entry(8,  "Woodcutting"),
        Map.entry(9,  "Fletching"),
        Map.entry(10, "Fishing"),
        Map.entry(11, "Firemaking"),
        Map.entry(12, "Crafting"),
        Map.entry(13, "Smithing"),
        Map.entry(14, "Mining"),
        Map.entry(15, "Herblore"),
        Map.entry(16, "Agility"),
        Map.entry(17, "Thieving"),
        Map.entry(18, "Slayer"),
        Map.entry(19, "Farming"),
        Map.entry(20, "Runecraft"),
        Map.entry(21, "Hunter"),
        Map.entry(22, "Construction")
    );

    // ---------------------------------------------------------------------------
    // Combat skills — XP in these is a byproduct of fighting, not a standalone
    // "training" activity. A bare XP drop in one of these (no animation, no NPC)
    // is almost always combat residue between hits, so it must NOT produce a
    // "Training Prayer"/"Training Hitpoints"/"Training Attack" fallback. Real
    // altar prayer is caught earlier by PRAYER_ANIMATIONS ("Using Altar").
    // (Attack 0, Defence 1, Strength 2, Hitpoints 3, Ranged 4, Prayer 5,
    //  Magic 6, Slayer 18.)
    // ---------------------------------------------------------------------------

    private static final Set<Integer> COMBAT_SKILL_INDICES =
        Set.of(0, 1, 2, 3, 4, 5, 6, 18);

    /** Fallback label shown when inside the Player-Owned House and otherwise idle,
     *  if the user hasn't set a custom one in the plugin config. */
    static final String DEFAULT_HOUSE_LABEL = "Chilling at Home";

    /** Wall-clock window a finished activity is held through idle gaps before the
     *  player is surfaced as AFK. Until this elapses the last real activity keeps
     *  showing ("hold the last info until there's an update"); after it, "AFK".
     *  Generalises the old 1-poll combat bridge and is poll-interval-independent. */
    private static final long AFK_HOLD_MS = 120_000L; // 2 minutes

    // AFK-hold state (the only mutable state; isolated to the public infer()
    // wrapper, leaving infer0() pure for unit testing).
    private InferenceResult lastRealActivity = null;
    private long lastRealActivityMs = 0L;

    /** Wall-clock source, injectable so timing behaviour is unit-testable. */
    private LongSupplier clock = System::currentTimeMillis;

    /** Test seam: override the wall-clock used for the AFK hold. */
    void setClock(LongSupplier clock) {
        this.clock = clock;
    }

    // ---------------------------------------------------------------------------
    // Public result type
    // ---------------------------------------------------------------------------

    @Value
    public static class InferenceResult {
        String activity;
        String target;
        String skill;
        String location;
        double confidence;
    }

    /** Human-readable skill name for a skill index, or "(none)" for -1 / unknown.
     *  Used to render the diagnostic signal trail. */
    public static String skillName(int skillIndex) {
        if (skillIndex < 0) {
            return "(none)";
        }
        return SKILL_NAMES.getOrDefault(skillIndex, "Unknown");
    }

    // ---------------------------------------------------------------------------
    // Inference entry point
    // ---------------------------------------------------------------------------

    /**
     * Infer activity from the observed signals.
     *
     * @param interactingNpcName  Name of the NPC the player is currently
     *                            interacting with, or null.
     * @param currentAnimation    Current player animation ID, or -1.
     * @param recentXpSkillIndex  Skill index that received XP most recently,
     *                            or -1 if none in the observation window.
     * @param regionId            Current map region ID (16-bit packed).
     * @param isInBank            True when the bank interface is open.
     * @param isLoggedIn          True when the player is in-game.
     */
    public InferenceResult infer(
        String interactingNpcName,
        int currentAnimation,
        int recentXpSkillIndex,
        int regionId,
        boolean isInBank,
        boolean isLoggedIn
    ) {
        return infer(interactingNpcName, currentAnimation, recentXpSkillIndex,
            regionId, isInBank, isLoggedIn, false, null);
    }

    /**
     * Inference with Player-Owned House awareness.
     *
     * @param inPoh      True when the player is inside their POH. POH regions are
     *                   instanced (the raw region id shifts every visit), so this
     *                   flag is derived plugin-side from the stable instance
     *                   template, not from {@code regionId}.
     * @param houseLabel User-configured label to show when idle in the POH; falls
     *                   back to {@link #DEFAULT_HOUSE_LABEL} when null/empty. Only
     *                   the Idle fallback is overridden — active combat/skilling
     *                   still win.
     */
    public InferenceResult infer(
        String interactingNpcName,
        int currentAnimation,
        int recentXpSkillIndex,
        int regionId,
        boolean isInBank,
        boolean isLoggedIn,
        boolean inPoh,
        String houseLabel
    ) {
        InferenceResult result = infer0(
            interactingNpcName, currentAnimation, recentXpSkillIndex,
            regionId, isInBank, isLoggedIn, inPoh, houseLabel);
        return applyAfkHold(result);
    }

    /**
     * Hold the last real activity through idle gaps; surface AFK only after a
     * sustained pause.
     *
     * {@link #infer0} emits the internal "Idle" sentinel whenever nothing
     * concrete is observed — the brief gap between kills, running between
     * birdhouses, or a genuine pause. Rather than flicker to idle instantly, we
     * keep showing the last concrete reading until {@link #AFK_HOLD_MS} has
     * elapsed since it was last seen, then release to "AFK". Any concrete reading
     * (combat, skilling, birdhouse run, banking, GE, POH label, logged out)
     * refreshes the hold. The internal "Idle" string therefore never escapes —
     * callers see either a held activity or "AFK".
     */
    private InferenceResult applyAfkHold(InferenceResult result) {
        final long now = clock.getAsLong();
        if (!"Idle".equals(result.getActivity())) {
            lastRealActivity = result;
            lastRealActivityMs = now;
            return result;
        }
        // Reading degraded to idle. Hold the last real activity until the window
        // expires, then surface AFK (keeping the idle reading's location).
        if (lastRealActivity != null && (now - lastRealActivityMs) < AFK_HOLD_MS) {
            return lastRealActivity;
        }
        lastRealActivity = null;
        return new InferenceResult("AFK", null, null, result.getLocation(), result.getConfidence());
    }

    /** Pure inference from observed signals (no cross-call state). */
    private InferenceResult infer0(
        String interactingNpcName,
        int currentAnimation,
        int recentXpSkillIndex,
        int regionId,
        boolean isInBank,
        boolean isLoggedIn,
        boolean inPoh,
        String houseLabel
    ) {
        if (!isLoggedIn) {
            return new InferenceResult("Logged out", null, null, null, 1.0);
        }

        // 1. Banking / GE
        if (isInBank) {
            return new InferenceResult("Banking", null, null, locationFromRegion(regionId), 0.95);
        }
        if (regionId == REGION_GE) {
            return new InferenceResult("At the Grand Exchange", null, null, "Grand Exchange", 0.90);
        }

        // 2-4. Combat — interacting with any NPC. We build a descriptive main line
        //      "Slaying Tormented Demons" / "Fighting Zulrah" rather than a vague
        //      "In Combat" / "Bossing", because the Discord rule only renders the
        //      activity string. Slayer is detected by recent Slayer XP (the curated
        //      SLAYER_NPCS list is incomplete — e.g. it misses Tormented Demons —
        //      so the live XP signal is the source of truth, with the list as a
        //      fallback). Bosses keep their singular proper name.
        if (interactingNpcName != null) {
            final boolean isBoss      = BOSS_NPCS.contains(interactingNpcName);
            final boolean slayerXp    = recentXpSkillIndex == 18; // Slayer
            final boolean knownSlayer = SLAYER_NPCS.contains(interactingNpcName);

            if (isBoss) {
                return new InferenceResult(
                    "Fighting " + interactingNpcName,
                    interactingNpcName,
                    combatSkillFromXpOrAnimation(recentXpSkillIndex, currentAnimation),
                    locationFromRegion(regionId),
                    0.93
                );
            }
            if (slayerXp || knownSlayer) {
                return new InferenceResult(
                    "Slaying " + pluralize(interactingNpcName),
                    interactingNpcName,
                    "Slayer",
                    locationFromRegion(regionId),
                    slayerXp ? 0.95 : 0.82
                );
            }
            return new InferenceResult(
                "Fighting " + pluralize(interactingNpcName),
                interactingNpcName,
                combatSkillFromXpOrAnimation(recentXpSkillIndex, currentAnimation),
                locationFromRegion(regionId),
                0.80
            );
        }

        // 4.5 Birdhouse run (Fossil Island). A run cycles through 4 birdhouse
        //     spots dropping Crafting XP (building) then Hunter XP (collecting),
        //     which would otherwise flip between "Training Crafting" and
        //     "Training Hunter" every poll. In a known birdhouse region with
        //     either of those XP drops, collapse it to a single "Birdhouse run";
        //     the AFK hold then bridges the running-between-houses gaps so it
        //     shows continuously rather than flickering to idle.
        if (BIRDHOUSE_REGIONS.contains(regionId)
                && (recentXpSkillIndex == SKILL_HUNTER || recentXpSkillIndex == SKILL_CRAFTING)) {
            return new InferenceResult("Birdhouse run", null, "Hunter", "Fossil Island", 0.85);
        }

        // 5. Skilling — animation-based
        if (FISHING_ANIMATIONS.contains(currentAnimation)) {
            return new InferenceResult("Fishing", null, "Fishing", locationFromRegion(regionId), 0.85);
        }
        if (WOODCUTTING_ANIMATIONS.contains(currentAnimation)) {
            return new InferenceResult("Woodcutting", null, "Woodcutting", locationFromRegion(regionId), 0.85);
        }
        if (MINING_ANIMATIONS.contains(currentAnimation)) {
            return new InferenceResult("Mining", null, "Mining", locationFromRegion(regionId), 0.85);
        }
        if (COOKING_ANIMATIONS.contains(currentAnimation)) {
            return new InferenceResult("Cooking", null, "Cooking", locationFromRegion(regionId), 0.85);
        }
        if (FLETCHING_ANIMATIONS.contains(currentAnimation)) {
            return new InferenceResult("Fletching", null, "Fletching", locationFromRegion(regionId), 0.85);
        }
        if (CRAFTING_ANIMATIONS.contains(currentAnimation)) {
            return new InferenceResult("Crafting", null, "Crafting", locationFromRegion(regionId), 0.85);
        }
        if (HERBLORE_ANIMATIONS.contains(currentAnimation)) {
            return new InferenceResult("Herblore", null, "Herblore", locationFromRegion(regionId), 0.85);
        }
        if (SMITHING_ANIMATIONS.contains(currentAnimation)) {
            return new InferenceResult("Smithing", null, "Smithing", locationFromRegion(regionId), 0.85);
        }
        if (PRAYER_ANIMATIONS.contains(currentAnimation)) {
            return new InferenceResult("Using Altar", null, "Prayer", locationFromRegion(regionId), 0.75);
        }

        // 6. Recent XP but no animation match — use skill index as a fallback.
        //    Combat skills are excluded: a bare combat-skill XP drop with no NPC
        //    and no animation is fight residue between hits (the cause of the old
        //    "Training Prayer" misfire mid-Slayer), not a training session. Those
        //    fall through to combat-stickiness / Idle instead.
        if (recentXpSkillIndex >= 0 && !COMBAT_SKILL_INDICES.contains(recentXpSkillIndex)) {
            String skillName = SKILL_NAMES.getOrDefault(recentXpSkillIndex, "Unknown");
            return new InferenceResult(
                "Training " + skillName,
                null,
                skillName,
                locationFromRegion(regionId),
                0.65
            );
        }

        // 7a. Idle inside the Player-Owned House → show the house label instead of a
        //     bare "Idle". POH is instanced (region id shifts each visit) so this is
        //     keyed off the plugin-supplied inPoh flag, not a region number.
        if (inPoh) {
            final String label =
                (houseLabel != null && !houseLabel.isEmpty()) ? houseLabel : DEFAULT_HOUSE_LABEL;
            return new InferenceResult(label, null, null, "Player-Owned House", 0.70);
        }

        // 7b. Idle / unknown. Confidence sits at/above the default 0.60 filter so an
        //    idle reading is actually sent — otherwise a stale prior activity (e.g.
        //    "Fighting …") lingers on Discord long after the player stopped.
        return new InferenceResult("Idle", null, null, locationFromRegion(regionId), 0.60);
    }

    /**
     * Naive English pluralisation for monster names (OSRS names pluralise with -s
     * almost universally; -y → -ies; already-plural / awkward endings are left
     * as-is). "Tormented Demon" → "Tormented Demons", "Fire Giant" → "Fire Giants".
     */
    static String pluralize(String name) {
        if (name == null || name.isEmpty()) {
            return name;
        }
        final String lower = name.toLowerCase();
        if (lower.endsWith("s") || lower.endsWith("x") || lower.endsWith("z")
                || lower.endsWith("sh") || lower.endsWith("ch")) {
            return name; // already plural-ish or awkward to pluralise
        }
        if (lower.endsWith("y") && name.length() > 1) {
            final char before = Character.toLowerCase(name.charAt(name.length() - 2));
            if ("aeiou".indexOf(before) < 0) {
                return name.substring(0, name.length() - 1) + "ies";
            }
        }
        return name + "s";
    }

    // ---------------------------------------------------------------------------
    // Helpers
    // ---------------------------------------------------------------------------

    /**
     * Map a packed region ID to a human-readable location string.
     * Falls back to a "Region <id>" placeholder for unmapped regions.
     */
    public static String locationFromRegion(int regionId) {
        // Classic switch (Java 11) — RuneLite plugins target Java 11, which does
        // not support switch expressions (`case X ->`).
        switch (regionId) {
            case REGION_GE:                    return "Grand Exchange";
            case REGION_LUMBRIDGE:             return "Lumbridge";
            case REGION_EDGEVILLE:             return "Edgeville";
            case REGION_VARROCK_WEST:          return "Varrock";
            case REGION_ASGARNIAN_ICE_DUNGEON: return "Asgarnian Ice Dungeon";
            case REGION_SLAYER_TOWER:          return "Slayer Tower";
            case REGION_CATACOMBS:             return "Catacombs of Kourend";
            case REGION_GWD:                   return "God Wars Dungeon";
            case REGION_CORP:                  return "Corporeal Beast";
            case REGION_TOB:                   return "Theatre of Blood";
            case REGION_COX:                   return "Chambers of Xeric";
            case REGION_NIGHTMARE:             return "Nightmare of Ashihama";
            // Unmapped region → return null rather than a meaningless "Region 16453"
            // number. The location line is then simply omitted.
            default:                           return null;
        }
    }

    /**
     * Best-effort combat skill from recent XP gain; fallback to animation heuristic.
     */
    private static String combatSkillFromXpOrAnimation(int xpSkillIndex, int animation) {
        // XP skill is more reliable when available
        if (xpSkillIndex >= 0) {
            switch (xpSkillIndex) {
                case 4:  return "Ranged";
                case 6:  return "Magic";
                case 18: return "Slayer";
                default: return "Melee";
            }
        }
        return combatSkillFromAnimation(animation);
    }

    private static String combatSkillFromAnimation(int animation) {
        // Heuristic: distinguish melee/range/magic by animation ranges.
        // These spans are approximations — exact IDs vary by weapon style.
        if (animation >= 390 && animation <= 440) return "Ranged";  // bow/crossbow
        if (animation >= 710 && animation <= 730) return "Magic";   // spell cast
        return "Melee";
    }
}
