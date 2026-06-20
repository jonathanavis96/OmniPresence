package com.omnipresence.dto;

import com.google.gson.annotations.SerializedName;
import lombok.Builder;
import lombok.Data;

/**
 * Payload POJO sent to OmniPresence's context server.
 * Gson serializes field names as-is (camelCase matches the schema).
 */
@Data
@Builder
public class RuneLiteContext {

    /** Always "runelite". */
    private final String source;

    /** Always "Old School RuneScape". */
    private final String game;

    /**
     * Local player name — only populated when the user has enabled
     * "Share account name" in the plugin config (default: off).
     * Null / omitted when disabled.
     */
    private final String account;

    /** Human-readable activity label, e.g. "Training Slayer", "Bossing", "Banking". */
    private final String activity;

    /**
     * Primary target (NPC or skill target), e.g. "Skeletal Wyvern".
     * Null when not applicable.
     */
    private final String target;

    /**
     * Skill being trained, e.g. "Slayer".
     * Null when not deterministic.
     */
    private final String skill;

    /** Plain-English location label derived from region/map chunk. */
    private final String location;

    /**
     * 0.0–1.0 confidence that the inferred context is correct.
     * Low-confidence payloads can be filtered by the plugin config threshold.
     */
    private final double confidence;

    /** ISO-8601 UTC timestamp when this context was captured. */
    private final String timestamp;
}
