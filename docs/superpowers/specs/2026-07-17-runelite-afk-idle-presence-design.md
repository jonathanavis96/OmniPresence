# RuneLite pipe integrity + skill wording + idle/AFK presence

**Date:** 2026-07-17
**Branch:** `omnipresence-work`
**Status:** Approved design — ready for implementation plan

## Problem

Three defects/gaps in the RuneLite → Discord presence path, all on the **current
IPC-interception architecture** (real Jagex-launched RuneLite; OmniPresence
impersonates Discord on `discord-ipc-0`/`ipc-1` and captures the built-in Discord
plugin's `details`/`state`). The deprecated custom Java inference plugin stays dead
(never got RuneLite Plugin Hub approval).

1. **RuneLite steals the pipe.** RuneLite's built-in Discord plugin sometimes reaches
   *real* Discord, so Discord shows RuneLite's own presence ("RuneLite" + RuneLite
   logo) instead of OmniPresence's. This is the top-priority bug.
2. **Blank / bare "OSRS".** When the intercept feed goes stale (RuneLite only sends
   `SET_ACTIVITY` on change; the keep-alive only re-stamps *while focused*), the rule
   renders `"OSRS – {{empty}}"` → tidied to bare "OSRS", losing the skill/location
   detail. Refocusing after tabbing away reproduces it.
3. **No idle/AFK concept.** There is no in-game-AFK or away-from-computer state. There
   is no system-input idle detection anywhere in the app.

## Non-goals

- Reviving the custom RuneLite inference plugin (skill/XP/target inference, true
  game-state idle). Deliberately deprecated 2026-06-23; not in scope.
- True in-game idle detection (idle animation / no-XP). We use system input-idle as
  the proxy instead — see Part 3.
- Changing OmniPresence's own presence transport (stays on the Discord cloud Social
  SDK; the local pipes exist only to capture RuneLite).

## Architecture facts (verified, for the implementer)

- Interceptor parses a captured `SET_ACTIVITY` and forwards **only** two strings:
  `activity.details` → `runelite.activity`, `activity.state` → `runelite.location`
  (`NamedPipeInterceptor.cpp:482-495` → `AppController.cpp:233-236`). No skill/target
  fields exist on this path.
- OmniPresence owns `ipc-0`+`ipc-1` (two acceptor threads, `PIPE_UNLIMITED_INSTANCES`),
  kills real Discord and relaunches it onto `ipc-2` (`NamedPipeInterceptor.cpp:85-96,
  123, 355-357`).
- The interceptor is **fully stopped** during OmniPresence's own OAuth authorize
  (`AppController.cpp:130-133`) and only started after SDK Connected (`:120-122`).
- Integration freshness window = 120 s (`IntegrationContext.h:18`). Focus-gated
  keep-alive re-stamps every 30 s **only while RuneLite is focused**
  (`AppController.cpp:155-158, 244-263`).
- Single presence choke point: `AppController::evaluateAndPublish()`
  (`AppController.cpp:267-290`) → `RuleEngine::evaluate` → `resolveRule` (renders
  name/details/state, picks `largeImageKey`, `RuleEngine.cpp:238-308`) →
  `DiscordPresenceClient::publishActivity` (`:283-361`, `SetLargeImage` at `:314`).
- No `GetLastInputInfo` / input hook exists (`Win32ActiveWindowWatcher.cpp` polls
  foreground window only).

---

## Part 1 — RuneLite can never steal the pipe (prerequisite)

**Invariant:** RuneLite's built-in Discord plugin must never establish a live
`SET_ACTIVITY` session with real Discord. OmniPresence must hold at least one pipe
slot below real Discord's slot at *all times*, with no unguarded window.

**Confirmed root cause (live-diagnosed 2026-07-17).** Discord's registered-game
auto-detection is **ruled out** — the user has RuneLite activity display disabled in
Discord ("Added Games" shows the display-off icon). The steal is the **built-in
RuneLite Discord plugin reaching real Discord over the pipe**: the on-screen presence
was rich ("Playing RuneLite / Training: Runecraft / Dark Altar"), which only the plugin
produces. The interceptor logged **exactly two lines all session** (`Listening on
ipc-0` + `ipc-1` at 20:41:16) — no kill, no relaunch, zero captures — while OmniPresence
published the generic `RuneLite`/`Active` fallback.

The bug is in the **probe-gated, one-shot Discord bounce** (`NamedPipeInterceptor.cpp:
279-293`): the kill only runs when `discordPipeAlreadyServed(0) || (1)` is true *at
thread-0 startup*. At 20:41:16 that probe returned false, so the kill was skipped and
OmniPresence created its own `ipc-0`/`ipc-1` instances. Because `CreateNamedPipeW` uses
`PIPE_UNLIMITED_INSTANCES`, real Discord coexists as an **additional server instance**
on `ipc-0`; RuneLite's connections are then distributed by the OS between Discord's
instance and OmniPresence's — mostly Discord → steal. The `m_bounceTried` one-shot guard
means OmniPresence never re-bounces once Discord appears/returns later.

**Fix direction (validate the exact shape during implementation):**
- Make ownership of `ipc-0`/`ipc-1` **exclusive and self-healing**, not a one-shot
  startup probe: when `autoBounce`, reliably kill Discord → claim both pipes as sole
  server → relaunch Discord to `ipc-2`, and **re-detect coexistence during the session**
  (periodic probe) so a Discord that starts/restarts after OmniPresence is re-bounced
  rather than left squatting `ipc-0`. Drop or re-arm the `m_bounceTried` one-shot.
- Fix the startup probe timing/reliability (it false-negatived a running Discord).
- Add explicit logging at each bounce decision so the log always shows why the kill
  did/didn't run (the current silent skip is what hid this for a whole session).

**Related defects observed same session (fold in where cheap):**
- **Intercept rarely captures** even when it owns the pipe — same on-change/staleness
  root as Part 2; the dominant reason the screen shows generic `RuneLite/Active`.
- **Presence flip-flop** RuneLite↔Claude↔Browsing↔Discord every 2-4 s (the "keeps
  flickering" report) — check whether the Discord kill/relaunch or window-watcher
  transients drive it; add stability/debounce if so.

**Lower-probability leaks to still rule out:** OAuth gap (both pipes freed during
OmniPresence authorize), handshake reject → scan-upward to real Discord on `ipc-2`.

**Work:** This part is investigative first. Reproduce the steal across all three
launch orderings (RuneLite-first / Discord-first / OmniPresence-first), read the
pipe-claim timeline from `omnipresence-debug.log`, identify the exact leak, then
harden. Likely fixes (apply what the evidence supports):
- Never leave *zero* owned slots during OmniPresence's OAuth — hold `ipc-1` while
  temporarily freeing only `ipc-0` for the authorize, or serialize authorize before
  the interceptor claims pipes.
- If scan-upward is real, own `ipc-0..2` and pin Discord to `ipc-3`.
- Ensure both pipes are claimed *before* relaunching Discord.

**Acceptance:** In each launch ordering, and across an OmniPresence re-auth while
RuneLite runs, Discord never shows RuneLite's own presence; OmniPresence captures the
activity every time (verified in the debug log + on-screen Discord).

---

## Part 2 — Skill wording + no blank / bare "OSRS"

**Persist last-good capture.** Retain the last *non-empty* `runelite.activity` and
`runelite.location` so a refocus or on-change gap never wipes detail to bare "OSRS".
Self-corrects on the next capture; overridden by the idle tiers in Part 3. Keep the
existing empty-name guard.

**Wording.** Map the captured activity to `Training {skill}` **only** when it matches a
known OSRS skill name (fixed 23-skill list: Attack, Strength, Defence, Ranged, Prayer,
Magic, Runecraft, Construction, Hitpoints, Agility, Herblore, Thieving, Crafting,
Fletching, Slayer, Hunter, Mining, Smithing, Fishing, Cooking, Firemaking, Woodcutting,
Farming). Otherwise pass the activity through verbatim, so combat/bosses/minigames read
faithfully ("Fighting: Zulrah", never "Training Zulrah"). Note RuneLite reports
"Runecraft" as the skill; render "Training Runecrafting" for that one (map skill →
display name).

**Graceful degrade.** `name = "OSRS"` always (osrs logo). `details` = the
skill/activity text (or, if absent, the location). `state` = location. Never emit a
dangling "OSRS – ".

**Acceptance:** Training a skill shows `OSRS / Training {Skill} / {Location}`; tab away
and back keeps the last-known skill/location (until the next capture or an idle tier
takes over); a boss fight shows the faithful activity, not "Training …".

---

## Part 3 — Idle/AFK state machine

**Signal.** A new input-idle monitor using Win32 `GetLastInputInfo` → system-wide idle
seconds (`(GetTickCount - dwTime)/1000`). Duration only — no key content, no hook; safe
by construction. Poll on a periodic tick (~5 s); may piggyback an existing timer or add
a dedicated one.

**Override.** Add a highest-priority override in `RuleEngine::evaluate` (alongside the
pause/private override), fed the current idle seconds and the focused window. Because
transitions are time-based with no external event, the ~5 s tick calls
`evaluateAndPublish`; the existing `isSamePresence` change-gate prevents re-publish
spam. When input resumes, the next tick restores normal presence.

**Tiers (evaluate higher threshold first):**

| Condition | Presence |
|---|---|
| `idle ≥ awaySeconds` (default 600 = 10 min), **any** focused app | **Away**: `large_image = "away"`, name/details/state = "Away from computer". Drops app identity. |
| `idle ≥ afkSeconds` (default 120) **and** RuneLite focused | **AFK**: keep OSRS identity (`large_image = "osrs"`), details/state = "AFK". |
| otherwise | Normal presence (Parts 1–2 / other rules). |

Non-RuneLite apps skip the AFK tier (no in-game concept): normal until `awaySeconds`,
then Away.

**Config.** `afkSeconds` (120), `awaySeconds` (600 = 10 min), the AFK label ("AFK"),
the Away label ("Away from computer"), the away asset key ("away"), and a master enable
for the idle system. Persisted to `config/omnipresence.json`, **and editable from the
app GUI** — add controls (at minimum the AFK and Away minute thresholds + master
enable) to a QML settings surface so the user tunes them without touching JSON. The GUI
writes back through the normal config-save path so changes take effect live.

**Asset.** `zzz-pc.png` — a sleeping monitor (closed eyes) with three rising Z's,
transparent, 1254×1254 — is in Windows Downloads
(`/mnt/c/Users/grafe.MASTERRIG/Downloads/zzz-pc.png`). Add it to the repo
(`assets/icons/`) and **upload it to the Discord app's Art Assets portal under key
`away`** (same manual portal step as the app icons). AFK/normal tiers reuse the
existing `osrs` asset.

**Acceptance:** In RuneLite, 2 min without mouse/keyboard shows OSRS + "AFK"; 10 min
shows the sleeping-computer card + "Away from computer"; in any other app, 10 min shows
the Away card; any input immediately restores normal presence on the next tick.
Thresholds set from the GUI take effect without an app restart.

---

## Sequencing

1. **Part 1** (pipe integrity) — top priority; the rest is moot if RuneLite steals the pipe.
2. **Part 2** (skill wording + staleness).
3. **Part 3** (idle/AFK + away asset).

Each part is independently testable on the Windows build.

## Verification

Live on the real Jagex client + Discord (per `verify` skill): drive each acceptance
case above and observe actual Discord presence, not just unit tests. Reproduce Part 1
across all launch orderings.
