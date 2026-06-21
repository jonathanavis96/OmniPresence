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
        keyName = "port",
        name = "OmniPresence port",
        description = "Local port of the OmniPresence app on THIS computer (127.0.0.1 only). "
            + "Your activity is sent only to the OmniPresence desktop app running locally — "
            + "never to the internet or any third-party server. Leave as-is unless you changed "
            + "the app's default port.",
        position = 1
    )
    @Range(min = 1, max = 65535)
    default int port() {
        return 47831;
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

    @ConfigItem(
        keyName = "houseLabel",
        name = "House label",
        description = "Text shown when you are idle inside your Player-Owned House "
            + "(e.g. \"Chilling at Home\"). Player-owned houses are instanced, so this "
            + "is detected from the house's map template rather than a fixed region. "
            + "Active fighting or skilling in the house still shows that activity instead.",
        position = 5
    )
    default String houseLabel() {
        return "Chilling at Home";
    }
}
