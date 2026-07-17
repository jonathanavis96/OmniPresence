# RuneLite pipe integrity + skill wording + idle/AFK presence — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Stop RuneLite's built-in Discord plugin from stealing the pipe, render the captured skill nicely with a graceful stale fallback, and add a two-tier input-idle → AFK / Away-from-computer presence state machine.

**Architecture:** Current RuneLite path = OmniPresence impersonates Discord on `discord-ipc-0`/`ipc-1` (`NamedPipeInterceptor`), captures the built-in plugin's `details`/`state` → `runelite.activity`/`runelite.location`, and renders via `RuleEngine`→`TemplateEngine`→`PresencePayload`→`DiscordPresenceClient` (own presence goes through Discord's cloud Social SDK, not the pipe). All presence flows through the single choke point `AppController::evaluateAndPublish()`. The idle system adds a Win32 `GetLastInputInfo` monitor and a highest-priority override in `RuleEngine::evaluate`.

**Tech Stack:** C++20, Qt6/QML, CMake, Win32 named pipes + `GetLastInputInfo`, Discord Social SDK. Build on Windows only: `C:\dev\OmniPresence\build-incremental.bat`. Unit tests: `tests/test_*.cpp`.

## Global Constraints

- Work on branch `omnipresence-work`. Commit incrementally (one working step at a time).
- Real, official Jagex-launched RuneLite only. The deprecated Java inference plugin in `integrations/runelite/` stays dead — do NOT revive it.
- OmniPresence's own presence uses the Discord cloud Social SDK; the local pipes exist ONLY to capture RuneLite. Real Discord must live on `ipc-2` (or higher).
- Privacy: idle detection uses `GetLastInputInfo` (idle *duration* only) — never read key/mouse content, never install an input hook.
- Config is JSON at `config/omnipresence.json` and the live copy under `%APPDATA%\OmniPresence\OmniPresence\config.json`. GUI edits must round-trip through the existing config-save path and take effect without a restart.
- Source is edited in the repo (`/home/grafe/code/OmniPresence`) and mirrored to `C:\dev\OmniPresence` for the Windows build (see prior session's `cp` pattern). Commit from the repo.
- Match the repo's commit-trailer convention (Co-Authored-By + Claude-Session trailers).

## Key file/line map (verified 2026-07-17)

- `app/src/NamedPipeInterceptor.cpp`: bounce gate `:279-293`; `killDiscord()` `:96`; `relaunchDiscord()` `:123`; workerLoop `:265-397`; relaunch trigger `:355-358`; `CreateNamedPipeW`/`PIPE_UNLIMITED_INSTANCES` `:317-326`; SET_ACTIVITY parse `details :482` / `state :484`; `emit activityCaptured :495`; RuneLite client_id `409416265891971072 :33`. `discordPipeAlreadyServed()` is the startup probe used in the gate.
- `app/src/AppController.cpp`: `activityCaptured`→`m_integrationContext.update("runelite", …)` `:233-236`; `evaluateAndPublish()` `:267-290`; keep-alive timer setup `:155-158`, `onRuneliteKeepAliveTick()` `:244-263`; interceptor start-after-Connected `:120-122`; interceptor stop-during-OAuth `:130-133`; SDK pump 100 ms `:142-145`; window-change→evaluate `:219`; integration-update→evaluate `:225`; log formatter `:342-345`.
- `app/include/IntegrationContext.h`: `maxAgeSeconds{120}` `:18`; `isFresh()` `:20-22`. `app/src/IntegrationContext.cpp`: `getFresh()` `:36-39`; `refresh()` `:24-29`; skill/target/confidence fields `:72-75`.
- `app/src/RuleEngine.cpp`: `evaluate()` `:82-144` (pause/private override `:89`, integration pass `:113-120`, `getFresh` check `:198`, plain-rule pass `:127-134`, `resolveIfNamed` `:106-110`, `genericPresence` `:143`); `genericPresence()` `:37-78` (details `"Active" :73`); `privateFallback()` `:22-35`; `matchRule()` `:148-176`; `resolveRule()` `:238-308` (name `tidy() :260`, details `:261`, state `:262`, `largeImageKey :263`, `StatusDisplay::Name :283`); `tidy() :249-256`.
- `app/src/TemplateEngine.cpp`: `buildContext()` `:12-66`; runelite tokens `:59-63`; `resolveToken() :70`; `render() :85`.
- `config/omnipresence.json`: RuneLite rule `:25-45` (`matchProcessName "RuneLite.exe"`, `matchIntegrationSource "runelite"`, `activityNameTemplate "OSRS – {{runelite.activity or runelite.location}}"`, `largeImageKey "osrs"`).
- `app/src/DiscordPresenceClient.cpp`: `publishActivity()` `:283-361` (`SetName/Details/State :292-294`, `SetLargeImage :314`, `SetStatusDisplayType :299-310`, `UpdateRichPresence :330`, retry-without-images `:346-354`); `authorizationStarting :171`.
- `app/src/Win32ActiveWindowWatcher.cpp`: `onPollTimer()` `:80` (750 ms foreground poll; NO idle detection anywhere today).

---

## Phase 1 — RuneLite can never steal the pipe (top priority)

**Root cause (confirmed live):** the Discord bounce at `NamedPipeInterceptor.cpp:279-293` is a *one-shot, probe-gated* kill. When `discordPipeAlreadyServed(0)||(1)` false-negatives at thread-0 startup, the kill is skipped; OmniPresence then coexists with real Discord as an extra `PIPE_UNLIMITED_INSTANCES` server on `ipc-0`, and RuneLite's connections race onto Discord's instance. The `m_bounceTried` one-shot means Discord is never re-bounced if it appears/returns later. Whole-session symptom: 2 interceptor log lines, 0 captures, generic `RuneLite/Active` on screen while Discord shows the plugin's real rich presence.

### Task 1.1: Instrument every bounce decision (make the silent skip visible)

**Files:** Modify `app/src/NamedPipeInterceptor.cpp:279-293`.

**Why first:** the bug hid for a whole session because the skip path logs nothing. Logging is the diagnostic backbone for verifying the real fix.

- [ ] **Step 1:** In `workerLoop`, before the gate, log the probe results explicitly:

```cpp
if (pipeIndex == 0) {
    const bool served0 = discordPipeAlreadyServed(0);
    const bool served1 = discordPipeAlreadyServed(1);
    const bool discordRunning = isDiscordProcessRunning(); // new helper, Task 1.2
    qInfo() << "[NamedPipeInterceptor] bounce probe: autoBounce=" << m_autoBounce
            << "served(ipc-0)=" << served0 << "served(ipc-1)=" << served1
            << "discordProcRunning=" << discordRunning;
```

- [ ] **Step 2:** Log the taken branch in each arm (kill fired / kill skipped-no-proc / gate-not-entered), so the log always states why.
- [ ] **Step 3:** Build on Windows (`build-incremental.bat`), run, and confirm the new `bounce probe:` line appears in `%LOCALAPPDATA%\OmniPresence\omnipresence-debug.log` at startup.
- [ ] **Step 4:** Commit.

```bash
git add app/src/NamedPipeInterceptor.cpp
git commit -m "interceptor: log every Discord-bounce decision (skip was silent)"
```

### Task 1.2: Bounce on Discord *process presence*, not the flaky pipe probe

**Files:** Modify `app/src/NamedPipeInterceptor.cpp` (add `isDiscordProcessRunning()`; rewrite the gate `:279-293`). Header `app/include/NamedPipeInterceptor.h` (declare helper).

**Interfaces:**
- Produces: `bool isDiscordProcessRunning();` — true if any `Discord.exe` process exists (reuse the `CreateToolhelp32Snapshot` walk already in `killDiscord()`).

**Rationale:** the pipe probe false-negatives (timing / multi-instance). A running `Discord.exe` is the reliable signal that Discord will (re)claim `ipc-0`. If Discord is running, bounce it so OmniPresence becomes the sole `ipc-0`/`ipc-1` server, then relaunch it to `ipc-2`.

- [ ] **Step 1:** Add `isDiscordProcessRunning()` (factor the snapshot walk out of `killDiscord()` so both share it).
- [ ] **Step 2:** Replace the gate condition so the leader kills Discord when `m_autoBounce && isDiscordProcessRunning()` (drop the `discordPipeAlreadyServed` precondition; keep it only as extra logging). Keep the `Sleep(1500)` release wait and set `m_relaunchDiscordPending`.
- [ ] **Step 3:** Build on Windows. With Discord + RuneLite already running, start OmniPresence and confirm the log shows: `discordProcRunning=true` → kill → `Relaunched Discord (will use ipc-2)`.
- [ ] **Step 4:** In Discord, confirm the app relaunched and `\\.\pipe\discord-ipc-2` exists while OmniPresence owns `ipc-0`/`ipc-1` (PowerShell pipe enumeration).
- [ ] **Step 5:** Confirm RuneLite's activity is now captured: log shows `activityCaptured`, and Discord shows the OmniPresence OSRS card (not RuneLite's own).
- [ ] **Step 6:** Commit.

```bash
git add app/src/NamedPipeInterceptor.cpp app/include/NamedPipeInterceptor.h
git commit -m "interceptor: bounce Discord whenever it is running, not on flaky pipe probe"
```

### Task 1.3: Self-heal — re-bounce a Discord that appears mid-session

**Files:** Modify `app/src/NamedPipeInterceptor.cpp` (remove the permanent `m_bounceTried` latch; add a periodic coexistence re-check). Consider a small watchdog thread or hook onto an existing timer via a signal to `AppController`.

**Rationale:** if Discord starts/restarts after OmniPresence, or the user relaunches it, it must be re-bounced to `ipc-2` or it will squat `ipc-0` again.

- [ ] **Step 1:** Add a periodic check (e.g. a dedicated watchdog `std::thread` sleeping ~10 s, or reuse the app's timer via a Qt signal): if `m_running && m_autoBounce && isDiscordProcessRunning()` AND we detect Discord serving `ipc-0` again (probe as client for a *foreign* server, or simply re-assert by killing+relaunching when a capture hasn't happened and Discord is up), bounce again. Gate re-bounces with a cooldown (≥30 s) so it can't thrash.
- [ ] **Step 2:** Replace `m_bounceTried.exchange(true)` one-shot with a re-armable guard used only for the cooldown, not a permanent latch.
- [ ] **Step 3:** Build on Windows. Repro: with OmniPresence running and owning the pipes, quit and relaunch Discord manually; confirm the log re-bounces it back to `ipc-2` within one cooldown window and RuneLite capture resumes.
- [ ] **Step 4:** Commit.

```bash
git add app/src/NamedPipeInterceptor.cpp app/include/NamedPipeInterceptor.h
git commit -m "interceptor: self-heal pipe ownership when Discord (re)appears mid-session"
```

### Task 1.4: Live acceptance across all launch orderings

**Files:** none (verification). Use the `verify` skill.

- [ ] **Step 1:** Ordering A — Discord first, then OmniPresence, then RuneLite. Expect: Discord bounced to `ipc-2`, RuneLite captured, OmniPresence OSRS card on Discord (no "Playing RuneLite" steal).
- [ ] **Step 2:** Ordering B — OmniPresence first, then Discord, then RuneLite. Expect: Task 1.3 re-bounce fires; capture works.
- [ ] **Step 3:** Ordering C — RuneLite already bound to real Discord, then OmniPresence starts. Expect: bounce + RuneLite reconnect + capture.
- [ ] **Step 4:** Trigger an OmniPresence re-auth while RuneLite runs; confirm no steal window (lower-probability OAuth-gap leak). If a steal appears here, hold `ipc-1` through the authorize instead of stopping the whole interceptor (`AppController.cpp:130-133`).
- [ ] **Step 5:** Ask Jonathan to confirm the Discord card visually for one ordering (his RuneLite/Discord are live). Record results in the vault `Projects/OmniPresence.md`.

---

## Phase 2 — Skill wording + no blank / bare "OSRS"

The built-in plugin already emits rich detail (`details="Training: Runecraft"`, `state="Dark Altar"`). Goal: render it as-is, prefix recognised skills, and never blank on staleness.

### Task 2.1: Skill-name → "Training {Skill}" mapping (unit-tested)

**Files:** Create `app/src/SkillLabel.cpp` + `app/include/SkillLabel.h`. Test: `tests/test_skill_label.cpp`. Wire into CMake test list (follow `tests/test_template_engine.cpp` registration).

**Interfaces:**
- Produces: `QString omni::skillLabel(const QString& activity);` — if `activity` (case-insensitive, trimmed) matches a known OSRS skill or the RuneLite `details` skill form (e.g. `"Runecraft"`, `"Training: Runecraft"`), return `"Training {DisplayName}"` (Runecraft→"Runecrafting"); otherwise return `activity` unchanged.

- [ ] **Step 1:** Write failing tests (follow the assertion style in `tests/test_rule_engine_render.cpp`):

```cpp
// tests/test_skill_label.cpp
#include "SkillLabel.h"
// ... test harness include as in tests/test_template_engine.cpp
void test_skill_label() {
    assertEqual(omni::skillLabel("Runecraft"), QString("Training Runecrafting"));
    assertEqual(omni::skillLabel("Training: Runecraft"), QString("Training Runecrafting"));
    assertEqual(omni::skillLabel("woodcutting"), QString("Training Woodcutting"));
    assertEqual(omni::skillLabel("Fighting: Zulrah"), QString("Fighting: Zulrah")); // not a skill → verbatim
    assertEqual(omni::skillLabel(""), QString(""));
}
```

- [ ] **Step 2:** Run the test; expect link/compile failure (SkillLabel undefined).
- [ ] **Step 3:** Implement `SkillLabel` with the fixed 23-skill set and the Runecraft→Runecrafting display map:

```cpp
// display names keyed by lowercased skill token
static const QMap<QString, QString> kSkills = {
  {"attack","Attack"},{"strength","Strength"},{"defence","Defence"},{"ranged","Ranged"},
  {"prayer","Prayer"},{"magic","Magic"},{"runecraft","Runecrafting"},{"construction","Construction"},
  {"hitpoints","Hitpoints"},{"agility","Agility"},{"herblore","Herblore"},{"thieving","Thieving"},
  {"crafting","Crafting"},{"fletching","Fletching"},{"slayer","Slayer"},{"hunter","Hunter"},
  {"mining","Mining"},{"smithing","Smithing"},{"fishing","Fishing"},{"cooking","Cooking"},
  {"firemaking","Firemaking"},{"woodcutting","Woodcutting"},{"farming","Farming"}
};
QString omni::skillLabel(const QString& activity) {
    QString a = activity.trimmed();
    if (a.isEmpty()) return a;
    QString key = a;
    if (key.startsWith("Training:", Qt::CaseInsensitive)) key = key.mid(9).trimmed();
    auto it = kSkills.find(key.toLower());
    if (it != kSkills.end()) return "Training " + it.value();
    return a; // faithful passthrough for bosses/minigames
}
```

- [ ] **Step 4:** Run tests; expect PASS.
- [ ] **Step 5:** Commit.

```bash
git add app/src/SkillLabel.cpp app/include/SkillLabel.h tests/test_skill_label.cpp CMakeLists.txt
git commit -m "feat: skill-name -> 'Training {Skill}' label mapping (Runecraft->Runecrafting)"
```

### Task 2.2: Apply the label + graceful degrade in rendering

**Files:** Modify `app/src/TemplateEngine.cpp:59-63` (or `RuleEngine::resolveRule`) so `{{runelite.activity}}` renders through `omni::skillLabel`. Modify `config/omnipresence.json:25-45`: set `activityNameTemplate` to a constant `"OSRS"` (large image `osrs`), `detailsTemplate` to `"{{runelite.activity}}"` (skill-labelled), `stateTemplate` to `"{{runelite.location}}"`. Update `tests/test_rule_engine_render.cpp`.

- [ ] **Step 1:** Write a failing render test: given `runelite.activity="Runecraft"`, `runelite.location="Dark Altar"`, expect `name="OSRS"`, `details="Training Runecrafting"`, `state="Dark Altar"`; given both empty, expect `name="OSRS"`, no dangling `"OSRS – "`.
- [ ] **Step 2:** Run; expect FAIL.
- [ ] **Step 3:** Implement: route the activity token through `skillLabel` in `buildContext`/`resolveToken`; change the rule templates in config; ensure `tidy()` never yields `"OSRS –"`.
- [ ] **Step 4:** Run tests; expect PASS.
- [ ] **Step 5:** Commit.

### Task 2.3: Persist last-good capture so refocus/stale never wipes detail

**Files:** Modify `app/src/AppController.cpp:233-263` (retain last non-empty `activity`/`location`); `app/src/IntegrationContext.cpp` `refresh()`/`update()` as needed.

**Rationale:** the built-in plugin only sends on change; keep-alive re-stamps freshness only while focused. On refocus the tokens can be empty → bare "OSRS". Retain the last non-empty capture and re-publish it until the next capture or an idle-tier override.

- [ ] **Step 1:** In the `activityCaptured` handler, if the incoming `activity`/`location` is empty, keep the previously stored non-empty value instead of overwriting with "".
- [ ] **Step 2:** Ensure the focus-gated keep-alive re-stamp (`:244-263`) preserves those retained fields.
- [ ] **Step 3:** Build on Windows. Repro: train a skill → tab away 3 min → tab back. Expect the last skill/location still shows (not bare "OSRS"), self-correcting on the next capture.
- [ ] **Step 4:** Commit.

### Task 2.4: Live verify Phase 2

- [ ] **Step 1:** Train Runecraft at the Dark Altar → Discord shows `OSRS / Training Runecrafting / Dark Altar` with the osrs logo.
- [ ] **Step 2:** Fight a boss → Discord shows the faithful activity (e.g. `Fighting: Zulrah`), NOT "Training Zulrah".
- [ ] **Step 3:** Tab away and back → detail persists. Confirm with Jonathan.

---

## Phase 3 — Idle/AFK state machine

Two tiers driven by system input-idle. RuneLite-focused + idle ≥ `afkSeconds` → "AFK" (keeps osrs logo); any app + idle ≥ `awaySeconds` → "Away from computer" (zzz-pc card, drops app identity).

### Task 3.1: Input-idle monitor (GetLastInputInfo)

**Files:** Create `app/src/InputIdleMonitor.cpp` + `app/include/InputIdleMonitor.h`. Wire into `AppController` construction.

**Interfaces:**
- Produces: `class InputIdleMonitor : QObject { public: quint64 idleSeconds() const; };` — returns `(GetTickCount() - LASTINPUTINFO.dwTime)/1000`. A `QTimer` (~5 s) emits `void tick()` on each poll.

- [ ] **Step 1:** Implement `idleSeconds()` via `GetLastInputInfo` (duration only; no hook, no key content — privacy constraint).
- [ ] **Step 2:** Add a ~5 s `QTimer` in `AppController` that calls `evaluateAndPublish()` so time-based transitions publish without an external event (the `isSamePresence` change-gate at `AppController.cpp:284` prevents spam).
- [ ] **Step 3:** Build on Windows; log `idleSeconds()` each tick and confirm it climbs when untouched and resets to ~0 on input.
- [ ] **Step 4:** Commit.

### Task 3.2: Idle-tier override in RuleEngine (unit-tested)

**Files:** Modify `app/src/RuleEngine.cpp:82-144` (add override near the pause/private check `:89`). Modify `app/include/RuleEngine.h` (extend `evaluate()` signature with idle seconds + focused-process + idle config). Test: `tests/test_rule_engine_render.cpp` (add idle cases).

**Interfaces:**
- Consumes: `quint64 idleSeconds`, `QString focusedProcessName`, and an `IdleConfig{ bool enabled; quint64 afkSeconds; quint64 awaySeconds; QString afkLabel; QString awayLabel; QString awayImageKey; }`.
- Produces: an `awayPresence()` and `afkPresence()` mirroring `privateFallback()` `:22-35`.

- [ ] **Step 1:** Write failing tests for tier selection (higher threshold wins):

```cpp
// idle >= awaySeconds, any app -> Away card, drops app identity
// idle >= afkSeconds AND focused == RuneLite.exe -> AFK, keeps osrs
// idle >= afkSeconds AND focused != RuneLite -> normal (no AFK tier)
// idle < afkSeconds -> normal
// enabled == false -> never overrides
```

- [ ] **Step 2:** Run; expect FAIL.
- [ ] **Step 3:** Implement the override at the top of `evaluate()`:

```cpp
if (idle.enabled) {
    if (idleSeconds >= idle.awaySeconds) {
        PresencePayload p; p.name = idle.awayLabel; p.details = idle.awayLabel;
        p.largeImageKey = idle.awayImageKey; /* drop app identity */ return p;
    }
    if (idleSeconds >= idle.afkSeconds &&
        focusedProcessName.compare("RuneLite.exe", Qt::CaseInsensitive) == 0) {
        PresencePayload p; p.name = "OSRS"; p.details = idle.afkLabel;
        p.largeImageKey = "osrs"; return p;
    }
}
// else fall through to existing pause/private/rule logic
```

- [ ] **Step 4:** Update `AppController::evaluateAndPublish()` to pass `m_inputIdle->idleSeconds()`, the focused process name, and the idle config into `evaluate()`.
- [ ] **Step 5:** Run tests; expect PASS.
- [ ] **Step 6:** Commit.

### Task 3.3: Config fields + defaults

**Files:** Modify `config/omnipresence.json` and `config/omnipresence.example.json` (add an `idle` block); `app/src/ConfigStore.cpp` (parse it); `tests/test_config_assets.cpp` (assert defaults).

- [ ] **Step 1:** Add config block with defaults:

```json
"idle": {
  "enabled": true,
  "afkSeconds": 120,
  "awaySeconds": 600,
  "afkLabel": "AFK",
  "awayLabel": "Away from computer",
  "awayImageKey": "away"
}
```

- [ ] **Step 2:** Parse into the `IdleConfig` struct in `ConfigStore`; default any missing field to the above. Write/extend a test asserting the parsed defaults.
- [ ] **Step 3:** Run tests; expect PASS. Commit.

### Task 3.4: GUI controls for the thresholds

**Files:** Modify a QML settings surface (e.g. `app/qml/PrivacyPage.qml` or `Dashboard.qml` — follow the existing settings-control pattern) and the `AppController` Q_PROPERTY/`Q_INVOKABLE` used to persist config from QML.

- [ ] **Step 1:** Add controls: master enable toggle, AFK minutes, Away minutes (minute inputs → stored as seconds). Bind to `AppController` invokables that write through the existing config-save path.
- [ ] **Step 2:** On save, apply live (no restart) — the ~5 s idle tick re-reads config-backed values.
- [ ] **Step 3:** Build on Windows; change the Away threshold in the GUI, save, and confirm the new value is honoured without restarting (log the effective thresholds each tick).
- [ ] **Step 4:** Commit.

### Task 3.5: Away asset into repo + Discord portal upload

**Files:** Add `assets/icons/away.png` (from `/mnt/c/Users/grafe.MASTERRIG/Downloads/zzz-pc.png`, the sleeping-computer + Zzz image, 1254×1254 transparent).

- [ ] **Step 1:** Copy `zzz-pc.png` → `assets/icons/away.png`.
- [ ] **Step 2:** Upload it to the Discord app's Art Assets portal under key `away` (manual portal step — same as the app-icon uploads). Note: `publishActivity()` retries without images on asset-resolution error (`DiscordPresenceClient.cpp:346-354`), so a missing upload degrades to text-only, not a crash.
- [ ] **Step 3:** Commit the asset.

```bash
git add assets/icons/away.png
git commit -m "assets: away (sleeping-computer + Zzz) card for the away-from-computer state"
```

### Task 3.6: Live verify Phase 3

- [ ] **Step 1:** In RuneLite, don't touch mouse/keyboard 2 min → Discord shows `OSRS / AFK`.
- [ ] **Step 2:** Keep idle to 10 min → Discord switches to the sleeping-computer `away` card + "Away from computer" (app identity dropped).
- [ ] **Step 3:** In a non-RuneLite app, idle 10 min → Away card (no AFK tier at 2 min).
- [ ] **Step 4:** Touch the mouse → normal presence restores within one ~5 s tick.
- [ ] **Step 5:** Change thresholds in the GUI mid-session; confirm they take effect without restart. Confirm the Discord card visually with Jonathan.

---

## Self-review

- **Spec coverage:** Part 1 → Phase 1 (root-cause fix + self-heal + orderings). Part 2 → Phase 2 (skill label, graceful degrade, persistence). Part 3 → Phase 3 (idle monitor, tier override, config, GUI, asset). GUI-configurable thresholds → Task 3.4. Away=zzz-pc drops identity → Task 3.2/3.5. 10-min default → Task 3.3. Flip-flop/OAuth-gap → Task 1.4 (investigated during live acceptance).
- **Placeholders:** none — all code/config/test blocks are concrete. C++ test harness calls follow the existing `tests/test_*.cpp` style (the implementer has those files).
- **Type consistency:** `omni::skillLabel(QString)→QString` used identically in 2.1/2.2; `IdleConfig` fields (`enabled/afkSeconds/awaySeconds/afkLabel/awayLabel/awayImageKey`) identical across 3.2/3.3/3.4; `awayImageKey`=`"away"` matches the asset key in 3.5.

## Verification

Per the `verify` skill: build on Windows and drive each Phase's live acceptance against the real Jagex client + Discord, observing the actual Discord card and the debug log — not just unit tests. Phase 1 must pass all three launch orderings. Final visual confirmation of the Discord card is Jonathan's (his RuneLite/Discord are live).
