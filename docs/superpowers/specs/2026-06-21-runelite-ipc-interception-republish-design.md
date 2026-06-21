# RuneLite Discord-IPC interception → republish as OmniPresence — design

Date: 2026-06-21
Status: approved (brainstormed with Jonathan; spike proven + live-confirmed)

## Problem

OmniPresence wants to show live OSRS activity (region / skill) as part of its single
unified Discord presence. The originally-planned data source — a **sideloaded RuneLite
plugin** — is **inert under the Jagex Launcher** (RuneLite only loads sideloaded plugins in
`--developer-mode`, which the Jagex Launcher cannot pass; see
`reference_discord_ipc_interception`). We need a data source that works under the Jagex
Launcher with no RuneLite-side install.

## Approach (Option C — proven)

Intercept RuneLite's **built-in Discord plugin** on the Windows named pipe
`\\.\pipe\discord-ipc-0`. RuneLite's `DiscordService` uses the legacy discord-rpc library,
which streams `SET_ACTIVITY` frames (`[op int32-LE][len int32-LE][UTF-8 JSON]`) carrying the
live activity (`details`, `state`/region, assets, timestamps). OmniPresence impersonates the
Discord client on that pipe, ACKs the handshake so RuneLite keeps streaming, and feeds the
captured fields into its existing pipeline.

Validated by `scripts/spike/discord-ipc-sniff.ps1` (2026-06-21): captured live region
changes (Grand Exchange → Rimmington → Player Owned House) with no sideloaded plugin, under
the Jagex Launcher. Two pipe gotchas were found and must be carried into the C++ port:
- **Never `Flush()` a named pipe** — `FlushFileBuffers` blocks until the client drains it;
  legacy discord-rpc doesn't drain promptly → hang. Write the frame, don't flush.
- **Construct the pipe with a real out-buffer** (e.g. 64 KB) — a 0 out-buffer makes writes a
  synchronous rendezvous that blocks until the client reads → hang on the ack write.

## Chosen model: republish under OmniPresence (not transparent forward)

Captured RuneLite data is **swallowed** (not forwarded to the real Discord) and re-published
through OmniPresence's existing rule engine + Social SDK, so OSRS appears as part of the one
unified OmniPresence presence and the duplicate "RuneLite" Discord entry disappears. Jonathan
has already disabled Discord's built-in RuneLite game-detection.

The interception **replaces the sideloaded plugin as the `runelite` integration source** —
everything downstream is unchanged.

## Gating question — RESOLVED

Does OmniPresence's own Social SDK use `discord-ipc-0` (conflict) or the network (no conflict)?
**Network.** The Social SDK header (`third_party/discord_social_sdk/include/discordpp.h`,
12.5k lines) has zero named-pipe references and an entirely HTTP/Gateway-based error model
(`HTTPError`, `HttpStatusCode`, `BadGateway`, `HttpWait`); presence publishes to Discord's
backend via the OAuth token. The SDK's *optional* local pipe use is only for join/invite
features (the earlier `ACTIVITY_JOIN`/`SET_SUPPRESS_NOTIFICATIONS` traffic from client_id
`1517890711218028544`), which OmniPresence does not use. **Therefore the interceptor can own
`discord-ipc-0` and the unified presence still publishes over the network — no transparent
relay needed.**

## Data mapping

`SET_ACTIVITY.args.activity` → `runelite` integration context:
- `.state`   → `runelite.location`  (region, e.g. "Grand Exchange")
- `.details` → `runelite.activity`  (skill/activity; may be empty depending on RuneLite's
  Discord-plugin config — richer text is a RuneLite-side toggle, not a capture limit)

Existing RuneScape rule (priority 10, `matchProcessName: RuneLite.exe`,
`matchIntegrationSource: runelite`): main line now **`OSRS – {{runelite.activity}}`** (was
"RuneLight –"; changed 2026-06-21 in example + both live configs), state `{{runelite.location}}`.
`resolveRule` already trims a dangling separator, so an empty activity renders just "OSRS".

## Operational requirement

For interception, OmniPresence must own `discord-ipc-0` **before RuneLite connects**, which
means Discord must not already hold it (Discord then falls back to ipc-1). Practical model:
OmniPresence's interceptor grabs ipc-0 on app start; if Discord already owns it, detect and
surface guidance (restart Discord) rather than failing silently. RuneLite's built-in Discord
plugin must remain **ON** (it's the source we intercept).

## Non-goals
- Transparent forwarding to the real Discord (rejected in favour of republish).
- Forcing richer RuneLite `details` (separate optional RuneLite-config follow-up).
- Cross-platform: interceptor is Windows-only (`#ifdef Q_OS_WIN`).
