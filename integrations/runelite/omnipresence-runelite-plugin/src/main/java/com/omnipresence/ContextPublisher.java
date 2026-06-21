package com.omnipresence;

import com.google.gson.Gson;
import com.omnipresence.dto.RuneLiteContext;
import lombok.extern.slf4j.Slf4j;
import okhttp3.MediaType;
import okhttp3.OkHttpClient;
import okhttp3.Request;
import okhttp3.RequestBody;
import okhttp3.Response;

import java.io.IOException;
import java.time.Instant;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

/**
 * Builds a {@link RuneLiteContext} payload and POSTs it to OmniPresence on a
 * background thread.  Failures are swallowed silently so a downed OmniPresence
 * process never disrupts the game.
 */
@Slf4j
public class ContextPublisher {

    private static final MediaType JSON = MediaType.get("application/json; charset=utf-8");

    private final OkHttpClient http;
    private final Gson gson;
    private final ExecutorService executor;

    public ContextPublisher(OkHttpClient http, Gson gson) {
        this.http = http;
        this.gson = gson;
        this.executor = Executors.newSingleThreadExecutor(r -> {
            Thread t = new Thread(r, "omnipresence-publisher");
            t.setDaemon(true);
            return t;
        });
    }

    /**
     * Asynchronously publish context.  Returns immediately; the POST is sent on
     * the background thread.
     *
     * @param endpointUrl       Configured endpoint (from {@link OmniPresenceConfig}).
     * @param inference         Inferred activity/skill/target.
     * @param accountName       Null unless the user enabled shareAccountName.
     * @param minConfidence     Minimum confidence (0.0–1.0) below which we skip.
     * @param signals           Diagnostic trail of the raw signals behind this
     *                          inference (for OmniPresence's presence-events.log).
     */
    public void publish(
        String endpointUrl,
        ActivityInferencer.InferenceResult inference,
        String accountName,
        double minConfidence,
        String signals
    ) {
        if (inference.getConfidence() < minConfidence) {
            log.debug("OmniPresence: skipping low-confidence context ({:.2f} < {:.2f})",
                inference.getConfidence(), minConfidence);
            return;
        }

        RuneLiteContext ctx = RuneLiteContext.builder()
            .source("runelite")
            .game("Old School RuneScape")
            .account(accountName) // null when sharing is off; Gson omits null fields
            .activity(inference.getActivity())
            .target(inference.getTarget())
            .skill(inference.getSkill())
            .location(inference.getLocation())
            .confidence(inference.getConfidence())
            .timestamp(Instant.now().toString())
            .signals(signals)
            .build();

        String body = gson.toJson(ctx);

        executor.submit(() -> {
            try {
                Request request = new Request.Builder()
                    .url(endpointUrl)
                    // okhttp 3.x argument order (MediaType, String) — RuneLite ships
                    // okhttp 3.14.x at runtime; this call is also valid (deprecated) on 4.x.
                    .post(RequestBody.create(JSON, body))
                    .build();

                try (Response response = http.newCall(request).execute()) {
                    if (!response.isSuccessful()) {
                        log.debug("OmniPresence: non-200 response {} from context server", response.code());
                    }
                }
            } catch (IOException e) {
                // OmniPresence is likely not running — ignore silently.
                log.trace("OmniPresence: context server unreachable: {}", e.getMessage());
            }
        });
    }

    /** Call on plugin shutdown to drain any in-flight request gracefully. */
    public void shutdown() {
        executor.shutdown();
        try {
            executor.awaitTermination(2, TimeUnit.SECONDS);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        }
    }
}
