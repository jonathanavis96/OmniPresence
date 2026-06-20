package com.omnipresence;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.inject.Provides;
import lombok.extern.slf4j.Slf4j;
import net.runelite.api.Client;
import net.runelite.api.GameState;
import net.runelite.api.NPC;
import net.runelite.api.Player;
import net.runelite.api.Skill;
import net.runelite.api.coords.WorldPoint;
import net.runelite.api.events.AnimationChanged;
import net.runelite.api.events.ChatMessage;
import net.runelite.api.events.GameStateChanged;
import net.runelite.api.events.GameTick;
import net.runelite.api.events.InteractingChanged;
import net.runelite.api.events.NpcDespawned;
import net.runelite.api.events.NpcSpawned;
import net.runelite.api.events.StatChanged;
import net.runelite.api.events.VarbitChanged;
import net.runelite.client.config.ConfigManager;
import net.runelite.client.eventbus.Subscribe;
import net.runelite.client.plugins.Plugin;
import net.runelite.client.plugins.PluginDescriptor;
import okhttp3.OkHttpClient;

import javax.inject.Inject;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;

/**
 * OmniPresence RuneLite plugin.
 *
 * Subscribes to key game events, maintains a lightweight inference state
 * machine, and periodically publishes sanitized context to OmniPresence's
 * localhost context server.
 *
 * Privacy rules enforced here:
 *   - Account name is only included when the user explicitly enables it.
 *   - Chat messages are observed ONLY to detect bank/interface states via
 *     system-message patterns; raw chat text is NEVER forwarded.
 *   - Payloads stay on 127.0.0.1 — no external network calls.
 */
@Slf4j
@PluginDescriptor(
    name = "OmniPresence",
    description = "Reports your OSRS activity to OmniPresence for Discord Rich Presence",
    tags = {"discord", "rich presence", "status", "activity"}
)
public class OmniPresencePlugin extends Plugin {

    // Varbit that tracks whether the bank interface is open.
    // Varbit 5570 = 1 when a bank/deposit box is open.
    private static final int VARBIT_BANK_OPEN = 5570;

    @Inject
    private Client client;

    @Inject
    private OmniPresenceConfig config;

    @Inject
    private OkHttpClient okHttpClient;

    private ActivityInferencer inferencer;
    private ContextPublisher publisher;
    private ScheduledExecutorService scheduler;
    private ScheduledFuture<?> scheduledTask;

    // ---------------------------------------------------------------------------
    // Observed state (updated by event subscribers, read on scheduler tick)
    // ---------------------------------------------------------------------------

    private volatile String interactingNpcName = null;
    private volatile int currentAnimation = -1;
    private volatile int recentXpSkillIndex = -1;
    private volatile int currentRegionId = -1;
    private volatile boolean bankOpen = false;
    private volatile boolean loggedIn = false;

    /** Track last published context to avoid redundant POSTs. */
    private volatile String lastPublishedActivity = null;

    @Override
    protected void startUp() {
        Gson gson = new GsonBuilder().serializeNulls().create();
        inferencer = new ActivityInferencer();
        publisher = new ContextPublisher(okHttpClient, gson);
        scheduler = Executors.newSingleThreadScheduledExecutor(r -> {
            Thread t = new Thread(r, "omnipresence-scheduler");
            t.setDaemon(true);
            return t;
        });

        reschedule();
        log.info("OmniPresence plugin started");
    }

    @Override
    protected void shutDown() {
        if (scheduledTask != null) {
            scheduledTask.cancel(false);
        }
        scheduler.shutdown();
        publisher.shutdown();
        log.info("OmniPresence plugin stopped");
    }

    @Provides
    OmniPresenceConfig provideConfig(ConfigManager configManager) {
        return configManager.getConfig(OmniPresenceConfig.class);
    }

    // ---------------------------------------------------------------------------
    // Event subscribers
    // ---------------------------------------------------------------------------

    @Subscribe
    public void onGameStateChanged(GameStateChanged event) {
        loggedIn = event.getGameState() == GameState.LOGGED_IN;
        if (!loggedIn) {
            interactingNpcName = null;
            currentAnimation = -1;
            recentXpSkillIndex = -1;
            bankOpen = false;
        }
    }

    @Subscribe
    public void onGameTick(GameTick tick) {
        // Refresh region on each tick (cheap; WorldPoint is a value object).
        Player local = client.getLocalPlayer();
        if (local != null) {
            WorldPoint wp = local.getWorldLocation();
            currentRegionId = wp.getRegionID();
        }
    }

    @Subscribe
    public void onInteractingChanged(InteractingChanged event) {
        if (event.getSource() != client.getLocalPlayer()) {
            return;
        }
        if (event.getTarget() instanceof NPC) {
            interactingNpcName = ((NPC) event.getTarget()).getName();
        } else {
            interactingNpcName = null;
        }
    }

    @Subscribe
    public void onAnimationChanged(AnimationChanged event) {
        if (event.getActor() == client.getLocalPlayer()) {
            currentAnimation = event.getActor().getAnimation();
        }
    }

    @Subscribe
    public void onStatChanged(StatChanged event) {
        // Record the skill that just gained XP (ordinal matches our SKILL_NAMES map).
        recentXpSkillIndex = event.getSkill().ordinal();
        // Decay: clear after a few ticks via the scheduler tick, not here, to
        // avoid competing writes. The scheduler reads this within the poll window
        // and then resets it to -1 after each publish cycle.
    }

    @Subscribe
    public void onVarbitChanged(VarbitChanged event) {
        if (event.getVarbitId() == VARBIT_BANK_OPEN) {
            bankOpen = event.getValue() == 1;
        }
    }

    @Subscribe
    public void onNpcSpawned(NpcSpawned event) {
        // Reserved for area-based inference (e.g. boss rooms).
    }

    @Subscribe
    public void onNpcDespawned(NpcDespawned event) {
        // If the NPC we were tracking despawns, clear the interaction.
        if (event.getNpc().getName() != null
                && event.getNpc().getName().equals(interactingNpcName)) {
            interactingNpcName = null;
        }
    }

    /**
     * ChatMessage is subscribed to ONLY for detecting game-state transitions
     * (e.g. "Welcome to Old School RuneScape" on login). Raw text is never
     * forwarded or stored beyond this check.
     */
    @Subscribe
    public void onChatMessage(ChatMessage event) {
        // (No chat content is currently used — placeholder for safe system-message checks.)
    }

    // ---------------------------------------------------------------------------
    // Scheduled publish tick
    // ---------------------------------------------------------------------------

    private void reschedule() {
        if (scheduledTask != null) {
            scheduledTask.cancel(false);
        }
        int intervalSec = Math.max(2, config.pollIntervalSeconds());
        scheduledTask = scheduler.scheduleAtFixedRate(
            this::maybPublish,
            intervalSec,
            intervalSec,
            TimeUnit.SECONDS
        );
    }

    private void maybPublish() {
        if (!config.enabled()) {
            return;
        }

        ActivityInferencer.InferenceResult result = inferencer.infer(
            interactingNpcName,
            currentAnimation,
            recentXpSkillIndex,
            currentRegionId,
            bankOpen,
            loggedIn
        );

        // Debounce: skip if the derived activity hasn't meaningfully changed.
        if (result.getActivity().equals(lastPublishedActivity)
                && result.getConfidence() < 0.99) {
            return;
        }
        lastPublishedActivity = result.getActivity();

        // Decay the recent XP signal after each publish cycle.
        recentXpSkillIndex = -1;

        String accountName = config.shareAccountName()
            ? (client.getLocalPlayer() != null ? client.getLocalPlayer().getName() : null)
            : null;

        double minConfidence = config.minConfidencePercent() / 100.0;

        publisher.publish(config.endpointUrl(), result, accountName, minConfidence);
    }
}
