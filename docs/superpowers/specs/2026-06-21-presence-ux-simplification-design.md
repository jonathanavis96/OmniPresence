# Presence UX simplification — design

Date: 2026-06-21
Branch: `omnipresence-work`
Status: approved (brainstorming complete)

## Problem

OmniPresence works end-to-end (auth, publish, art-asset resilience) but the
day-to-day UX has six concrete gaps Jonathan hit in live use:

1. **Member-list line is just the app name.** Discord shows "Playing RuneLight";
   the activity ("training crafting") is only visible after clicking the profile.
   Wanted: the compact line reads **"RuneLight – training crafting"**, side panel
   updates with location.
2. **The in-app Preview lies.** `PreviewPage.qml` is a hard-coded mock (literal
   `"PLAYING A GAME"`, 🎮 large emoji, 💻 small emoji) and its name line can differ
   from the real payload — it showed "Old School RuneScape" while Discord correctly
   showed RuneLight + Grand Exchange. The preview must mirror what is actually
   published.
3. **"Create Rule from This Window" is a stub.** `CapturePage.qml:87-89` only
   navigates to the Rules page (`// TODO: Pre-populate…`) — captured data is
   dropped, so clicking it appears to do nothing.
4. **The rule editor is overwhelming.** `RulesPage.qml` exposes ~20 raw backend
   fields (process name, exe path, regex, browser domain/category, integration
   source, four image fields, timestamp mode, privacy level, raw templates). Wanted:
   a simple form — pick a window, public/private, optionally include extra info
   (with a clear statement of what that info is), pick/add an image.
5. **No easy "Add photo".** Adding art today means manually setting an image key
   + hover text in the Asset Manager, then uploading to the Discord portal
   separately. Wanted: one "Add photo" button that saves the image locally and
   opens the Discord Art Assets page so it can be dropped in — no Playwright.
6. **Terminal title not pulled through.** The Windows Terminal rule renders
   `Working on {{terminal.repo or vscode.workspace}}`; when the PowerShell hook
   isn't running both are empty, so a terminal titled "RAM" shows "Working on "
   (blank) instead of the actual window title.

## Non-goals (YAGNI)

- Extracting the Claude desktop-app chat title (already proven impossible — the UI
  Automation tree is opaque; see vault note 2026-06-21).
- Automating the Discord portal upload (Playwright / reverse-engineered API).
  Decided against: fragile. The local-save + open-portal-for-drop flow is the chosen
  trade-off.
- Re-theming the app or unrelated refactors.

## Current architecture (relevant)

- **C++/Qt6/QML.** `AppController` exposes `Q_PROPERTY`s + invokables to QML.
- `ConfigStore` reads/writes a flat JSON config (`rules`, `assetKeys`,
  `discordApplicationId`, …).
- `RuleEngine` matches the active window (7-level priority) and renders a
  `PresencePayload` via `TemplateEngine`.
- `DiscordPresenceClient` publishes the payload (Social SDK), with retry-without-
  images on art-asset failure, and honours `PresencePayload::statusDisplay`.
- **Gap:** `RulesPage.qml` and `AssetManager.qml` use throwaway local `ListModel`s
  **not wired to the backend** — "Save" does not persist real rule/asset edits.
  Real C++↔QML model wiring is part of this work.

## Design

### A. Backend wiring (foundation for B–F)

- Expose a real **rule list model** from `ConfigStore` to QML with CRUD:
  `addRule(draft)`, `updateRule(index, field, value)`, `deleteRule(index)`,
  `saveRules()`. Either a `QAbstractListModel` or an invokable JSON-array bridge on
  `AppController`; pick whichever fits the existing `AppController` pattern best
  during planning. The Rules page binds to this instead of a placeholder model.
- Add a **local art store**: a user directory (`QStandardPaths` AppData →
  `art/`). Each entry = `{ key, localPath, hoverText }`. Used to resolve an image
  key to a local file for the preview, and to hold photos added via "Add photo".
  Persist the key→metadata map in config (`assetKeys` extended).
- New `AppController` surface:
  - `seedRuleFromCapture()` → builds a draft rule from the current captured window.
  - `addPhoto()` → file picker → normalize → store → open portal (see F).
  - `presenceLargeImageSource` / `presenceSmallImageSource` (string URLs) — resolved
    local/bundled image paths for the live payload.
  - `availableContextFields` → list of `{ token, label }` describing what extra
    info exists for the current window (drives the Q4 dropdown).
  - Ensure `presenceName/Details/State` always reflect the **exact** rendered payload
    that is (or would be) published — single source of truth shared with the preview.

### B. Main line rendering (Q1a)

- The simplified form's **"Main line" field = the Discord activity Name**
  (`activityNameTemplate`), and the member-list display is set to **Name** for
  every rule (`PresencePayload::statusDisplay = Name`). Whatever is on the main line
  is exactly the compact member-list line.
- Update the RuneScape rule: main line `RuneLight – {{runelite.activity}}`
  → "RuneLight – training crafting"; `stateTemplate = {{runelite.location}}`
  → "Grand Exchange"; leave `detailsTemplate` empty (no redundant line).
- Note: this changes the earlier "sidebar shows Details" behaviour to a consistent
  "main line = Name = compact line" model, which matches the simplified mental model
  (one main line you control). The previous Details-display logic is superseded.

### C. Preview fidelity (Q2)

- Replace the hard-coded mock in `PreviewPage.qml`:
  - Remove `"PLAYING A GAME"`, 🎮, 💻.
  - Big image ← `AppController.presenceLargeImageSource` (actual art; neutral
    placeholder only when none). Small image ← `presenceSmallImageSource`.
  - Bold name ← `presenceName`, details ← `presenceDetails`, state ← `presenceState`.
  - Bind to the same payload object the client publishes, so preview == reality.
  - Updates live as the active window changes.

### D. Capture → rule (Q3a)

- `CapturePage.qml` "Create Rule from This Window" calls `seedRuleFromCapture()`:
  build a draft rule with match criteria auto-filled (process name, exe path; browser
  domain / integration source if detected), main line pre-filled from the friendly
  app name, then navigate to the **simplified form** bound to that draft, ready to
  tweak and save. No more dropped data.

### E. Simplified rule form (Q4)

Rewrite the visible part of `RulesPage.qml` to show only:

1. **Main line** — pre-filled, editable (maps to `activityNameTemplate`).
2. **Public / Private** toggle (maps to `privacyLevel`: Public ↔ Private). Private
   → that window uses the private fallback.
3. **Include extra detail?** toggle → when on, a plain-language dropdown sourced from
   `availableContextFields` ("the window/tab title", "the website domain",
   "RuneScape activity / location", "terminal repo", …). The chosen field fills the
   **state** (side-panel) line. Always shows the user what the extra info will be.
4. **Image** — choose from uploaded art (the art store) or **Add photo** (F).
5. **Advanced** (collapsed by default): priority, regex, raw match criteria, raw
   templates, timestamp mode, small image. Existing fields move here, not deleted.

Match criteria are hidden in the main flow because they're seeded from the captured
window (D). A manual "Add Rule" with no capture uses the Advanced section to set the
match criteria.

### F. Add photo (Q5a)

- **Add photo** → native Qt `FileDialog` (PNG/JPG).
- Image copied into the local art store, **normalized to a Discord-valid 1024×1024
  PNG** via `QImage` (no Python at runtime), assigned a lowercase key (slug from
  filename or rule name; deduped).
- Local preview lights up immediately (preview + asset list + the rule's image).
- Then open the Discord Art Assets portal page for this app
  (`https://discord.com/developers/applications/<appId>/rich-presence/assets`) in the
  default browser, reveal the saved file (open its folder and/or copy its path to the
  clipboard), and show an in-app note: *"Drop this file into the Art Assets page that
  just opened — key `<key>`."* One manual drag-drop, no Playwright.
- Until uploaded, the existing retry-without-images path keeps text presence working.
- Bonus: this `QImage` normalizer is committed and reusable, replacing the lost
  Pillow placeholder-art script (closes the "how were the images made" gap — they
  were one-off Pillow placeholders whose script was never committed).

### G. Terminal title fallback (bug 6)

- Update the Windows Terminal rule templates to fall back to the real window title:
  `detailsTemplate: "Working on {{terminal.repo or vscode.workspace or window.title}}"`
  (and similarly let the state fall back sensibly). Verify `TemplateEngine`'s
  `{{a or b or c}}` chain supports 3+ operands; extend if it only handles two.
- A terminal titled "RAM" with no PowerShell hook then shows "Working on RAM".

## Error handling

- Add photo: reject unreadable/oversized files with an inline message; never crash
  the picker. If the portal URL can't open, still save locally and show the path.
- Rule CRUD: saving writes through `ConfigStore`; a failed write surfaces a status
  message, config is not left half-written (write-to-temp-then-rename if not already).
- Missing art for a key → neutral placeholder in preview, and presence still
  publishes via retry-without-images.

## Testing

- **RuleEngine / TemplateEngine** (existing test harness): main-line = Name rendering;
  3-operand `{{a or b or c}}` fallback; terminal-title fallback ("RAM" case);
  RuneScape combined main line + location state.
- **ConfigStore**: round-trip of the extended `assetKeys` (key→localPath→hoverText)
  and rule CRUD save/load.
- **Image normalizer**: arbitrary input → 1024×1024 PNG, key slugged + deduped.
- **Manual / live on Windows**: capture → pre-filled simplified form → save →
  presence matches; preview mirrors Discord; Add photo saves + opens portal + drop
  works; terminal "RAM" shows through.

## Build / deploy notes

- Windows 11 build (`build-discord.bat` / `rebuild-inc.bat`); after any *clean*
  rebuild re-run `windeployqt --qmldir app\qml`. Prefer incremental relink.
- `graphify update .` after code changes.
