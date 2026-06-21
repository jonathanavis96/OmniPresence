package com.omnipresence;

import org.junit.Before;
import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

/**
 * Unit tests for {@link ActivityInferencer}.
 *
 * These run without a RuneLite client; all inputs are plain primitives.
 */
public class ActivityInferencerTest {

    private ActivityInferencer inferencer;

    @Before
    public void setUp() {
        inferencer = new ActivityInferencer();
    }

    // ---------------------------------------------------------------------------
    // Slayer inference
    // ---------------------------------------------------------------------------

    @Test
    public void skeletalWyvern_withSlayerXp_isTrainingSlayer() {
        // Interacting with Skeletal Wyvern + recent Slayer XP → high-confidence Slayer
        ActivityInferencer.InferenceResult result = inferencer.infer(
            "Skeletal Wyvern",  // interactingNpcName
            808,                 // idle animation (combat engine handles animation)
            18,                  // recentXpSkillIndex = Slayer
            11672,               // REGION_ASGARNIAN_ICE_DUNGEON
            false,               // not in bank
            true                 // logged in
        );

        assertEquals("Slaying Skeletal Wyverns", result.getActivity());
        assertEquals("Skeletal Wyvern", result.getTarget());
        assertEquals("Slayer", result.getSkill());
        assertEquals("Asgarnian Ice Dungeon", result.getLocation());
        assertTrue("Confidence should be >= 0.90", result.getConfidence() >= 0.90);
    }

    @Test
    public void slayerNpc_withoutSlayerXp_isStillSlayerButLowerConfidence() {
        ActivityInferencer.InferenceResult result = inferencer.infer(
            "Abyssal Demon",
            -1,  // no animation
            0,   // recent Attack XP (not Slayer)
            13623, // REGION_SLAYER_TOWER
            false,
            true
        );

        assertEquals("Slaying Abyssal Demons", result.getActivity());
        assertEquals("Abyssal Demon", result.getTarget());
        assertEquals("Slayer", result.getSkill());
        // Without Slayer XP confirmation, confidence should be lower than 0.90
        assertTrue("Confidence should be < 0.90 without Slayer XP", result.getConfidence() < 0.90);
        assertTrue("Confidence should still be reasonable (>= 0.75)", result.getConfidence() >= 0.75);
    }

    // ---------------------------------------------------------------------------
    // Bossing inference
    // ---------------------------------------------------------------------------

    @Test
    public void killingZulrah_isBossing() {
        ActivityInferencer.InferenceResult result = inferencer.infer(
            "Zulrah",
            710,   // magic animation range
            6,     // Magic XP
            -1,    // unknown region
            false,
            true
        );

        assertEquals("Fighting Zulrah", result.getActivity());
        assertEquals("Zulrah", result.getTarget());
        assertTrue(result.getConfidence() >= 0.90);
    }

    // ---------------------------------------------------------------------------
    // Skilling animations
    // ---------------------------------------------------------------------------

    @Test
    public void woodcuttingAnimation_isWoodcutting() {
        ActivityInferencer.InferenceResult result = inferencer.infer(
            null,  // not interacting with NPC
            867,   // woodcutting animation
            8,     // Woodcutting XP
            -1,
            false,
            true
        );

        assertEquals("Woodcutting", result.getActivity());
        assertEquals("Woodcutting", result.getSkill());
        assertTrue(result.getConfidence() >= 0.80);
    }

    @Test
    public void fishingAnimation_isFishing() {
        ActivityInferencer.InferenceResult result = inferencer.infer(
            null,
            621,  // fishing animation
            10,   // Fishing XP
            -1,
            false,
            true
        );

        assertEquals("Fishing", result.getActivity());
        assertEquals("Fishing", result.getSkill());
    }

    // ---------------------------------------------------------------------------
    // Banking
    // ---------------------------------------------------------------------------

    @Test
    public void bankOpen_isBanking() {
        ActivityInferencer.InferenceResult result = inferencer.infer(
            null,
            -1,
            -1,
            12598, // GE region
            true,  // bank open
            true
        );

        assertEquals("Banking", result.getActivity());
        assertTrue(result.getConfidence() >= 0.90);
    }

    // ---------------------------------------------------------------------------
    // Logged out
    // ---------------------------------------------------------------------------

    @Test
    public void notLoggedIn_isLoggedOut() {
        ActivityInferencer.InferenceResult result = inferencer.infer(
            null, -1, -1, -1, false, false
        );

        assertEquals("Logged out", result.getActivity());
        assertEquals(1.0, result.getConfidence(), 0.001);
    }

    // ---------------------------------------------------------------------------
    // Grand Exchange
    // ---------------------------------------------------------------------------

    @Test
    public void geRegion_noInteraction_isAtGE() {
        ActivityInferencer.InferenceResult result = inferencer.infer(
            null, -1, -1, 12598, false, true
        );

        assertEquals("At the Grand Exchange", result.getActivity());
        assertEquals("Grand Exchange", result.getLocation());
    }

    // ---------------------------------------------------------------------------
    // Location helper
    // ---------------------------------------------------------------------------

    @Test
    public void locationFromRegion_mapsKnownRegions() {
        assertEquals("Lumbridge", ActivityInferencer.locationFromRegion(12850));
        assertEquals("God Wars Dungeon", ActivityInferencer.locationFromRegion(11578));
    }

    @Test
    public void locationFromRegion_unknownRegion_returnsNull() {
        // Unmapped regions return null (location omitted) rather than a useless
        // "Region 9999" number.
        assertNull(ActivityInferencer.locationFromRegion(9999));
    }

    @Test
    public void pluralize_handlesCommonMonsterNames() {
        assertEquals("Tormented Demons", ActivityInferencer.pluralize("Tormented Demon"));
        assertEquals("Fire Giants", ActivityInferencer.pluralize("Fire Giant"));
        assertEquals("Harpie Bug Swarms", ActivityInferencer.pluralize("Harpie Bug Swarm"));
        assertEquals("Elves", ActivityInferencer.pluralize("Elves")); // already plural
    }
}
