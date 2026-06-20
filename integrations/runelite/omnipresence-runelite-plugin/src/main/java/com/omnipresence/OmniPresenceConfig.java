package com.omnipresence;

import net.runelite.client.config.Config;
import net.runelite.client.config.ConfigGroup;
import net.runelite.client.config.ConfigItem;
import net.runelite.client.config.Range;

@ConfigGroup("omnipresence")
public interface OmniPresenceConfig extends Config {

    @ConfigItem(
        keyName = "enabled",
        name = "Enable OmniPresence",
        description = "Toggle activity reporting on or off without uninstalling the plugin.",
        position = 0
    )
    default boolean enabled() {
        return true;
    }

    @ConfigItem(
        keyName = "endpointUrl",
        name = "Endpoint URL",
        description = "OmniPresence context server URL. Leave as-is unless you changed the default port.",
        position = 1
    )
    default String endpointUrl() {
        return "http://127.0.0.1:47831/integrations/runelite/context";
    }

    @ConfigItem(
        keyName = "shareAccountName",
        name = "Share account name",
        description = "Include your OSRS character name in the payload. Disabled by default for privacy.",
        position = 2
    )
    default boolean shareAccountName() {
        return false;
    }

    @ConfigItem(
        keyName = "minConfidence",
        name = "Minimum confidence",
        description = "Only send updates when the inferred activity confidence is at or above this value (0–100).",
        position = 3
    )
    @Range(min = 0, max = 100)
    default int minConfidencePercent() {
        return 60;
    }

    @ConfigItem(
        keyName = "pollIntervalSeconds",
        name = "Poll interval (seconds)",
        description = "Minimum seconds between context updates sent to OmniPresence.",
        position = 4
    )
    @Range(min = 2, max = 60)
    default int pollIntervalSeconds() {
        return 5;
    }
}
