# Rule-editor & presence polish — design spec

**Date:** 2026-06-21
**Status:** Approved (brainstorm)
**Repo:** `~/code/OmniPresence` (branch `omnipresence-work`)
**Builds on:** `2026-06-21-presence-ux-simplification-design.md`

## Problem

Live use of the simplified rule editor surfaced six gaps:

1. Rules feel **unnamed** — the only editable name is buried under **Advanced**, so there's
   no obvious way to rename a rule.
2. The Public/Private toggle text is unclear ("shows the private placeholder").
3. There's **no way to generate** a rule image in-app — only pick an existing key or
   import a file. The user wants to mint new monogram tiles in the same style as the
   bundled `osrs` / `code` icons.
4. A Windows-Terminal coding session (tab titled e.g. "RAM") shows the main line as
   "code" instead of "Coding – RAM".
5. Generated/keyed art doesn't appear on Discord — the window shows the OmniPresence app
   icon — because the art key was never uploaded to the Discord portal.
6. "RuneLight" shows on the Discord profile even with the plugin off; it's almost
   certainly Discord's own game detection, separate from our SDK presence.

**Out of scope / corrected:** The user does **not** use VS Code — coding happens in
**Windows Terminal**. No VS Code rule, match criteria, or icon. Item 4/5 are about the
terminal coding presence only.

## Approach (overview)

Small, focused changes to the existing simplified editor (`RulesPage.qml`), the art
plumbing (`ArtStore`, `AppController`), one live rule template, plus one research task.
Items with logic get tests in the existing QtTest harness under `tests/`.

## Components & changes

### 1. Surface the rule name
- `RulesPage.qml`: add a **"Rule name"** `TextField` as the **first** field of the editor
  (above "Main line"), bound to `current.name`, writing via `setField("name", text)`.
  The left-hand list already renders `name`, so it updates live.
- Remove the now-redundant "Name (internal)" row from the **Advanced** section.
- No backend change — `updateRuleField(index, "name", …)` already exists.

### 2. Clearer Public/Private wording
- `RulesPage.qml` (the toggle Text): track state plainly —
  - Public → `"Public — this window shows its details"`
  - Private → `"Private — this window will not show its details"`
- Cosmetic only; no logic change.

### 3. In-app monogram generator
- **Refactor for DRY:** extract the monogram-rendering core from
  `scripts/make-placeholder-art.cpp` into a reusable static on `ArtStore`:
  `static bool renderMonogram(const QString& outPath, const QString& monogram,
  const QColor& accent, QString* err)`. The standalone tool then calls it (keeps the CLI
  working); the app calls it too. (Rendering needs a live `QGuiApplication` for fonts —
  satisfied: the tool already creates one, and the app is a running GUI process.)
- **Controller bridge:** `AppController::generateArt(int ruleIndex, const QString& monogram,
  const QString& accentHex)` → derives the art key from the rule name
  (`ArtStore::slugify`), renders `artDir/<key>.png` via `renderMonogram`, sets the rule's
  `largeImageKey`, persists, and runs the **same portal hand-off as `importPhoto`**
  (reveal file + open the Art Assets page). Returns the key ("" on failure).
- **UI:** in the Image row add a **"Generate"** button beside "Add photo…". On click it
  opens a small inline popup with:
  - a monogram `TextField` (default = up-to-2-char initials derived from the rule name),
  - a colour choice (a handful of preset swatches incl. the cyan `#22d3ee` default;
    "Other" lets a hex be typed).
  On confirm it calls `generateArt(...)`, lights up the preview, and shows the existing
  upload hint.

### 4. Terminal coding main line → "Coding – {tab}"
- **Investigate first:** check the live config at
  `%APPDATA%\OmniPresence\config.json` (and `config/omnipresence.example.json`) for the
  Windows-Terminal rule and the terminal integration's title field. Hypothesis: the
  window is hitting `genericPresence()` ("code") rather than a real coding rule, or the
  rule's `activityNameTemplate` is wrong.
- **Fix:** the terminal/coding rule's `activityNameTemplate` becomes
  `"Coding – {{terminal.title or window.title}}"` (exact integration token confirmed
  against the example config / PS hook payload during implementation), so a tab titled
  "RAM" renders **"Coding – RAM"**. The engine already supports `{{a or b}}` and trims a
  dangling separator when both are empty (→ plain "Coding").
- Update `config/omnipresence.example.json` for fresh installs **and** patch the live
  config (app stopped) so the running app reflects it.
- Test: extend `tests/test_rule_engine_render.cpp` — terminal rule with
  `terminal.title="RAM"` → "Coding – RAM"; with neither field → "Coding".

### 5. Art not showing on Discord (app icon instead)
- Root cause (documented, not a code bug): Discord falls back to the **app icon** for any
  `largeImageKey` not present in the portal's **Rich Presence → Art Assets**. Keys like
  `code`/`osrs` were generated locally but never uploaded.
- The generate/import flows (items 3 + existing import) already reveal the PNG + open the
  Art Assets page for a one-drag upload. Spec records that **portal upload is the required
  manual step**, and we verify the icon appears after uploading the coding tile.

### 6. RuneLight — Discord game-detection override (research)
- **Research task:** determine whether our Social-SDK presence can suppress or override
  Discord's own detected-game label (e.g. activity type, an SDK flag, or simply that two
  activities can't coexist and the registered game wins). Check the Social SDK headers/docs
  and the `reference_discord_social_sdk_cpp` notes.
- **If override is possible:** wire it (likely in `DiscordPresenceClient` /
  `PresencePayload`).
- **If not:** document the exact Discord client setting that stops the auto-label
  (Settings → Registered Games / Activity Privacy) and verify only OmniPresence's presence
  shows once disabled.
- Outcome documented either way; no speculative code left behind.

## Testing

- TDD where logic exists: `renderMonogram` (writes a valid 1024² PNG; deterministic),
  `generateArt` key derivation, and the terminal-template render case extend the existing
  `tests/` QtTest harness.
- Items 1, 2, 5 are UI/wording/doc — verified by click-through on the live
  `build-discord` build (re-run `windeployqt --qmldir app\qml` after any new QML import).
- Item 6 verified live on Discord.

## Build / deploy notes

- Live behaviour is governed by the compiled exe **+** `%APPDATA%\OmniPresence\config.json`
  (two synced copies) — independent of the git branch. Editing
  `config/omnipresence.example.json` only affects fresh installs.
- After a clean rebuild, re-run `windeployqt --qmldir app\qml
  build-discord\app\omnipresence.exe`; incremental relinks keep the Qt runtime.
- `graphify update .` after code changes.

## Risks

- `renderMonogram` extraction must keep the standalone `make-placeholder-art` CLI working
  (shared core, thin `main`).
- Item 6 may conclude "not overridable from the app" — that's an acceptable outcome
  (fallback to the Discord setting), not a failure.
