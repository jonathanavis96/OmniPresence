package com.omnipresence;

import com.google.gson.Gson;
import com.google.inject.Provides;
import lombok.extern.slf4j.Slf4j;
import net.runelite.api.Actor;
import net.runelite.api.Client;
import net.runelite.api.GameState;
import net.runelite.api.NPC;
import net.runelite.api.Player;
import net.runelite.api.Skill;
import net.runelite.api.coords.LocalPoint;
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
    description = "Sends your in-game activity to the OmniPresence desktop app on this same "
        + "computer (127.0.0.1 only) for Discord Rich Presence. No data leaves your machine; "
        + "requires the OmniPresence app to be running.",
    tags = {"discord", "rich presence", "status", "activity"}
)
public class OmniPresencePlugin extends Plugin {

    // Varbit that tracks whether the bank interface is open.
    // Varbit 5570 = 1 when a bank/deposit box is open.
    private static final int VARBIT_BANK_OPEN = 5570;

    // Endpoint is hard-locked to loopback — the host can never be changed, only
    // the port. This guarantees the plugin can only ever talk to the OmniPresence
    // app on this same machine, never a third-party server.
    private static final String ENDPOINT_HOST = "http://127.0.0.1:";
    private static final String ENDPOINT_PATH = "/integrations/runelite/context";

    @Inject
    private Client client;

    @Inject
    private OmniPresenceConfig config;

    @Inject
    private OkHttpClient okHttpClient;

    @Inject
    private Gson gson;

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
    /** Stable map-template region (POH and other instances resolve to a fixed
     *  template even though their live region id shifts each visit). -1 outside
     *  an instanced region. Surfaced in the presence log as tpl: for diagnosis. */
    private volatile int currentTemplateRegionId = -1;
    /** True when the player is inside their Player-Owned House (derived from the
     *  template position, not the shifting instanced region id). */
    private volatile boolean inPoh = false;
    private volatile boolean bankOpen = false;
    private volatile boolean loggedIn = false;

    /** Tick counter + last tick we saw an NPC interaction — used to decay the
     *  interacting NPC a few ticks after combat ends (OSRS keeps "interacting"
     *  set briefly after the last attack, and despawn/clear events are unreliable). */
    private volatile int gameTickCount = 0;
    private volatile int lastInteractTick = -100;

    /** Track last published context to avoid redundant POSTs. */
    private volatile String lastPublishedActivity = null;

    /** Wall-clock of the last POST. Used for the freshness heartbeat: OmniPresence
     *  treats integration data as stale after ~120s, so even when the activity is
     *  unchanged we must re-POST periodically — otherwise an idle/repetitive
     *  session stops POSTing, the context expires, and OmniPresence falls back to
     *  the raw window ("Java" for the dev client). Must be < that 120s window. */
    private volatile long lastPublishMs = 0L;
    /** Slack subtracted from the poll interval when deciding if an unchanged
     *  state is due for a heartbeat re-POST, so the fixed-rate scheduler
     *  reliably crosses the threshold each tick despite timing jitter. The
     *  effective max staleness is therefore ~the configured poll interval
     *  (default 5s, user-tunable 2–60s) — i.e. the log/presence refreshes on a
     *  steady cadence rather than only when the activity changes. */
    private static final long HEARTBEAT_SLACK_MS = 500L;

    @Override
    protected void startUp() {
        // Derive from the client's injected Gson (Plugin Hub forbids fresh Gson instances).
        Gson serializingGson = gson.newBuilder().serializeNulls().create();
        inferencer = new ActivityInferencer();
        publisher = new ContextPublisher(okHttpClient, serializingGson);
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
        gameTickCount++;
        // Refresh region on each tick (cheap; WorldPoint is a value object).
        Player local = client.getLocalPlayer();
        if (local != null) {
            WorldPoint wp = local.getWorldLocation();
            if (wp != null) {
                currentRegionId = wp.getRegionID();
            }
            // POH (and other instances) shift their live region id every visit, so
            // resolve the stable map *template* position and recognise the house by
            // its template bounds rather than a fixed region number.
            int tpl = -1;
            boolean poh = false;
            if (client.isInInstancedRegion()) {
                LocalPoint lp = local.getLocalLocation();
                if (lp != null) {
                    WorldPoint tw = WorldPoint.fromLocalInstance(client, lp);
                    if (tw != null) {
                        tpl = tw.getRegionID();
                        poh = isPohTemplate(tw.getX(), tw.getY());
                    }
                }
            }
            currentTemplateRegionId = tpl;
            inPoh = poh;
            // Read the live interaction target each tick — more reliable than the
            // InteractingChanged event, which often misses the "combat ended"
            // transition. Decay ~5 ticks (~3s) after the last interaction so a
            // finished fight stops showing "Fighting/Slaying …".
            Actor target = local.getInteracting();
            if (target instanceof NPC) {
                interactingNpcName = ((NPC) target).getName();
                lastInteractTick = gameTickCount;
            } else if (gameTickCount - lastInteractTick > 5) {
                interactingNpcName = null;
            }
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

    /**
     * Recognise the Player-Owned House from a resolved map-template world point.
     *
     * The POH is an instanced area built from the Construction template block
     * around base region 7513 (world x≈1856, y≈5696). The live region id differs
     * every visit, but the template position is stable, so we match on a generous
     * bounding box covering the whole template block (any house size/room). The
     * tpl: value logged in presence-events.log lets these bounds be tightened from
     * real readings if ever needed.
     */
    private static boolean isPohTemplate(int x, int y) {
        return x >= 1850 && x <= 2115 && y >= 5630 && y <= 5765;
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

        // Snapshot the raw signals BEFORE inference decays them, so the diagnostic
        // trail in presence-events.log reflects exactly what infer() saw this cycle.
        final String signals = String.format(
            "npc:%s xp:%s region:%d tpl:%d poh:%b anim:%d bank:%b",
            interactingNpcName != null ? interactingNpcName : "(none)",
            ActivityInferencer.skillName(recentXpSkillIndex),
            currentRegionId, currentTemplateRegionId, inPoh, currentAnimation, bankOpen);

        ActivityInferencer.InferenceResult result = inferencer.infer(
            interactingNpcName,
            currentAnimation,
            recentXpSkillIndex,
            currentRegionId,
            bankOpen,
            loggedIn,
            inPoh,
            config.houseLabel()
        );

        // Decay the recent XP signal EVERY cycle (before the debounce), so a stale
        // reading can't keep a finished activity (e.g. "Training Ranged") alive
        // while standing still. Active training re-arms it via StatChanged before
        // the next poll.
        recentXpSkillIndex = -1;

        // Debounce: skip if the derived activity hasn't meaningfully changed —
        // UNLESS a heartbeat is due, so the integration stays fresh and
        // OmniPresence never reverts to the raw "Java" window. The heartbeat is
        // tied to the poll interval (default 5s), so an unchanged state is
        // re-POSTed on essentially every tick → the log/presence refreshes at
        // least every poll interval rather than only on change.
        final long nowMs = System.currentTimeMillis();
        final long heartbeatMs =
            Math.max(2000L, config.pollIntervalSeconds() * 1000L - HEARTBEAT_SLACK_MS);
        final boolean unchanged = result.getActivity().equals(lastPublishedActivity)
                && result.getConfidence() < 0.99;
        if (unchanged && (nowMs - lastPublishMs) < heartbeatMs) {
            return;
        }
        lastPublishedActivity = result.getActivity();
        lastPublishMs = nowMs;

        String accountName = config.shareAccountName()
            ? (client.getLocalPlayer() != null ? client.getLocalPlayer().getName() : null)
            : null;

        double minConfidence = config.minConfidencePercent() / 100.0;

        String endpoint = ENDPOINT_HOST + config.port() + ENDPOINT_PATH;
        publisher.publish(endpoint, result, accountName, minConfidence, signals);
    }
}
