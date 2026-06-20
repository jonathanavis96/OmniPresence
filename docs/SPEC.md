# OmniPresence — Functional Specification

> This is the canonical specification. Changes to product behaviour must be reflected here first.

---

## 1. Goals

- Auto-update Discord Rich Presence based on the currently focused Windows application and deeper context from optional integrations.
- Use the official **Discord Social SDK** exclusively — no Discord user token, no selfbot, no custom-status automation.
- Respect user privacy by default: browser page titles, exact file names, and terminal commands are sanitized before any use.
- Run entirely locally — no external servers, no cloud relay, no telemetry.
- Provide a clean Qt 6 / QML GUI for rule management and real-time status visibility.

---

## 2. Display Format — Option A

Discord Rich Presence has three primary text fields:

| Field | Purpose in OmniPresence |
|---|---|
| **Activity name** | Broad app or activity label (e.g. "Old School RuneScape", "Code", "YouTube") |
| **Details** | What the user is doing (e.g. "Training Slayer", "Working on ArchiveBox", "Watching YouTube") |
| **State** | Specific context (e.g. "Attacking Skeletal Wyvern", "Running archivebox add", "Private") |

`StatusDisplayType::Details` is used where the Discord Social SDK supports it, so the abbreviated member-list status shows the **details** field rather than only the activity name. This avoids awkward output like "Playing Working on code" and instead shows the meaningful line.

**References:**
- [Discord Rich Presence](https://docs.discord.com/developers/platform/rich-presence)
- [Social SDK — Setting Rich Presence](https://docs.discord.com/developers/discord-social-sdk/development-guides/setting-rich-presence)
- [Activity class](https://discord.com/developers/docs/social-sdk/classdiscordpp_1_1Activity.html)

---

## 3. Behaviour Examples

| Focused app | Activity name | Details | State |
|---|---|---|---|
| RuneLite (Slayer task) | Old School RuneScape | Training Slayer | Attacking Skeletal Wyvern |
| Terminal — ArchiveBox repo | Code | Working on ArchiveBox | Running archivebox add |
| YouTube in browser | YouTube | Watching YouTube | *(private — omitted or blank)* |
| Reddit in browser | Reddit | Browsing Reddit | *(private)* |
| Pi-hole dashboard | Custom Dashboard | Checking Pi-hole | Network monitoring |

---

## 4. Rule Priority (Deterministic)

Rules are evaluated in strict descending priority. The first match wins.

| Priority | Source |
|---|---|
| 1 | **Manual pause / private override** — user has toggled global privacy mode or paused OmniPresence; emit private fallback |
| 2 | **Manual pinned presence** — user has pinned a specific presence in the GUI; emit that |
| 3 | **Deep integration context** for the currently active app (RuneLite, browser ext, terminal, VS Code) — posted to local context API |
| 4 | **Specific user-created rule** matching process name, exe path, window title, browser domain, or integration source |
| 5 | **Browser sanitized domain / category rule** — generic domain-level match, no page title exposed |
| 6 | **Generic process rule** — process name / exe match with no deeper context |
| 7 | **Private fallback** |

### Private Fallback Presence

```
Activity name : Computer
Details       : Working privately
State         : Private
```

---

## 5. Template Variables

Template variables can be used in rule fields `activity_name`, `details`, and `state`.

| Variable | Source | Description |
|---|---|---|
| `{{app.name}}` | Window watcher | Friendly app name resolved from process |
| `{{window.title}}` | Window watcher | Raw window title (sanitized per privacy settings) |
| `{{process.name}}` | Window watcher | Process executable name (e.g. `RuneLite.exe`) |
| `{{browser.domain}}` | Browser extension | Current tab domain (e.g. `reddit.com`) |
| `{{browser.category}}` | Browser extension | Human label for domain (e.g. `Browsing Reddit`) |
| `{{terminal.cwd}}` | Terminal hook | Current working directory |
| `{{terminal.repo}}` | Terminal hook | Git repository name inferred from cwd |
| `{{terminal.command_summary}}` | Terminal hook | Sanitized last-command summary (no secrets) |
| `{{vscode.workspace}}` | VS Code ext | Workspace/folder name |
| `{{runelite.activity}}` | RuneLite plugin | Inferred activity (e.g. `Training Slayer`) |
| `{{runelite.target}}` | RuneLite plugin | Current NPC/target (e.g. `Skeletal Wyvern`) |
| `{{runelite.skill}}` | RuneLite plugin | Primary skill being trained (e.g. `Slayer`) |
| `{{runelite.location}}` | RuneLite plugin | In-game location (e.g. `Asgarnian Ice Dungeon`) |
| `{{runelite.confidence}}` | RuneLite plugin | Confidence score 0.0–1.0 |

Variables that resolve to empty/null are replaced with an empty string. Rules may chain fallbacks using `{{terminal.repo or vscode.workspace}}` syntax (evaluated left-to-right, first non-empty value used).

---

## 6. Active Window Detection

**Win32 APIs used:**

```cpp
HWND hwnd = GetForegroundWindow();
// https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getforegroundwindow

DWORD pid;
GetWindowThreadProcessId(hwnd, &pid);

HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
QueryFullProcessImageName(proc, 0, exePath, &size);

GetWindowText(hwnd, titleBuf, sizeof(titleBuf));
```

**Debounce and stability requirements:**

- Poll interval: 500–1000 ms.
- Only consider a window change "stable" after the same window has been foreground for 2–3 seconds (avoids Alt-Tab flicker).
- Do not call the Discord SDK update if the generated presence is identical to the last-sent presence.
- Do not reset the elapsed/timestamp timer unless the activity **category** changes (e.g. switching from RuneLite to a browser). Switching between two browser tabs in the same category should not reset elapsed time.

---

## 7. Discord Default Mappings

These are the built-in rules applied when no user-created rule overrides a match.

| App | Activity type | Name | Details | State |
|---|---|---|---|---|
| RuneLite | Playing | Old School RuneScape | `{{runelite.activity}}` | `{{runelite.target}}` |
| YouTube | Watching | YouTube | Watching YouTube | Private |
| Reddit | Playing | Reddit | Browsing Reddit | Private |
| Code (terminal/VS Code) | Playing | Code | `Working on {{terminal.repo or vscode.workspace}}` | `{{terminal.command_summary or vscode.current_task}}` |
| Custom dashboards | Playing | Custom Dashboard | `{{browser.dashboard_label}}` | Monitoring |

---

## 8. Browser Privacy Defaults

Browser page titles are **never exposed by default**. Only the domain-level category is used.

| Domain | Activity name | Details |
|---|---|---|
| youtube.com | YouTube | Watching YouTube |
| reddit.com | Reddit | Browsing Reddit |
| github.com | GitHub | Browsing GitHub |
| chatgpt.com | ChatGPT | Using ChatGPT |
| pi.hole / local dashboard | Custom Dashboard | Checking Pi-hole |
| Unknown domain | Browser | Browsing privately |

### Whitelist Toggles (per Privacy Settings screen)

| Toggle | Default | Effect when enabled |
|---|---|---|
| Allow exact localhost dashboard titles | No | Dashboard page titles shown in state |
| Allow exact YouTube video titles | No | Video title shown in details |
| Allow exact GitHub repo names | No | Repo name shown in details |
| Allow exact ChatGPT conversation titles | **NO** | Conversation title shown — disabled by default |

---

## 9. Configuration Storage

| Environment | Path |
|---|---|
| Development / portable | `config/omnipresence.json` (relative to working dir) |
| Installed (Windows) | `%APPDATA%\OmniPresence\config.json` |

- The file is human-readable JSON.
- `config/omnipresence.example.json` is committed to the repo as a safe reference.
- `config/omnipresence.json` (the real config, which may contain application IDs) is gitignored.
- Secrets (Discord Application ID) are stored in the config, never hardcoded. The config is local-only.

---

## 10. GUI Screens

### 10.1 Dashboard

Real-time overview. Fields displayed:

- Currently active window (process name, window title, exe path)
- Current matched rule (rule name / source)
- Currently generated presence (activity name, details, state, images)
- Discord connection status (connected / disconnected / error)
- Last update time
- Privacy mode status (on/off indicator)
- **[Capture Current Window]** button → opens Rule Creation wizard (see 10.2)

### 10.2 Rule Creation Wizard (Capture Current Window)

Triggered from the Dashboard. Detects and displays:

- Process name
- Window title
- Exe full path
- Window class name
- Current integration data (if available from context server)

User is offered a pre-filled rule with the detected values and can edit before saving.

### 10.3 Rule Editor

Fields per rule:

| Field | Type | Description |
|---|---|---|
| enabled | bool | Whether this rule is active |
| priority | int | User-assigned priority within user rules |
| match_process_name | string / glob | e.g. `RuneLite.exe` |
| match_exe_path | string / glob | Full exe path match |
| match_window_title_contains | string | Substring match |
| match_window_title_regex | string | Regex match (mutually exclusive with contains) |
| match_browser_domain | string | e.g. `reddit.com` |
| match_browser_category | string | e.g. `Browsing Reddit` |
| match_integration_source | string | e.g. `runelite`, `browser`, `terminal`, `vscode` |
| activity_type | enum | Playing / Watching / Listening / Competing |
| activity_name | template string | Uses `{{variables}}` |
| details | template string | Uses `{{variables}}` |
| state | template string | Uses `{{variables}}` |
| large_image_key | string | Asset key from Discord Developer Portal |
| large_image_text | string | Tooltip on large image |
| small_image_key | string | Asset key |
| small_image_text | string | Tooltip on small image |
| timestamp_mode | enum | None / Since-start / Since-category-change |
| privacy_level | enum | Public / Private (emit fallback instead) |

### 10.4 Privacy Settings

- Browser generic by default (radio/toggle)
- Whitelist toggles as defined in Section 8
- Global pause toggle (halts all Discord updates, emits private fallback)
- Private mode toggle (emits private fallback for all rules)

### 10.5 Asset Manager

Map Discord asset keys to human-readable labels. Assets must be uploaded in the Discord Developer Portal (see [DISCORD_SETUP.md](DISCORD_SETUP.md)).

Recommended keys:

| Key | Status |
|---|---|
| `osrs` | Old School RuneScape |
| `code` | Code / terminal work |
| `terminal` | Terminal (non-code) |
| `youtube` | YouTube |
| `reddit` | Reddit |
| `pihole` | Pi-hole dashboard |
| `dashboard` | Generic custom dashboard |

---

## 11. Per-Integration Payload Schemas

All integrations POST to `http://127.0.0.1:47831/integrations/<source>/context`. Localhost only.

### 11.1 RuneLite

**Endpoint:** `POST /integrations/runelite/context`

```json
{
  "source": "runelite",
  "game": "Old School RuneScape",
  "account": "WongBater",
  "activity": "Training Slayer",
  "target": "Skeletal Wyvern",
  "skill": "Slayer",
  "location": "Asgarnian Ice Dungeon",
  "confidence": 0.91,
  "timestamp": "2026-06-20T00:00:00Z"
}
```

**Tracked events:**
- `GameStateChanged` — login/logout, loading state
- `GameTick` — heartbeat for state inference
- `InteractingChanged` — current NPC/player target
- `AnimationChanged` — combat vs. skilling vs. idle
- `StatChanged` — recent XP gains, skill trained
- `NpcSpawned` / `NpcDespawned` — boss/slayer context
- `VarbitChanged` / `VarPlayerChanged` — quest/activity flags
- `ChatMessage` — limited use; never expose sensitive content

**Inferred fields:** world, region, in-game location, combat vs. skilling vs. idle, slayer task inference, bossing inference, bankstanding/GE inference, confidence score.

**Privacy:** Account name is sent only if the user has permitted it in plugin settings. Chat content is never forwarded.

**References:**
- [RuneLite Developer Guide](https://github.com/runelite/runelite/wiki/Developer-Guide)
- [RuneLite API Events](https://static.runelite.net/runelite-api/apidocs/net/runelite/api/events/package-summary.html)

---

### 11.2 Browser Extension

**Endpoint:** `POST /integrations/browser/context`

```json
{
  "source": "browser",
  "browser": "chrome",
  "domain": "reddit.com",
  "category": "Browsing Reddit",
  "title_allowed": false,
  "safe_title": null,
  "dashboard_label": null
}
```

**Dashboard detection logic:**

| URL pattern | `dashboard_label` |
|---|---|
| `pi.hole/admin` | Checking Pi-hole |
| `192.168.x.x/admin` | Custom Dashboard |
| `localhost:xxxx` | Local Dashboard |
| Router/login page patterns | Router Dashboard |

**References:**
- [Chrome Tabs API](https://developer.chrome.com/docs/extensions/reference/api/tabs)

---

### 11.3 Terminal

**Endpoint:** `POST /integrations/terminal/context`

```json
{
  "source": "terminal",
  "cwd": "C:\\code\\ArchiveBox",
  "repo": "ArchiveBox",
  "branch": "main",
  "command_summary": "Running archivebox add",
  "privacy_safe": true
}
```

**`privacy_safe`:** `true` only when the command summary has been sanitized (no secrets, tokens, credential flags, private paths). If sanitization cannot be guaranteed, the integration omits `command_summary` and sets `privacy_safe: false`.

---

### 11.4 VS Code

**Endpoint:** `POST /integrations/vscode/context`

```json
{
  "source": "vscode",
  "workspace": "RankSentinel",
  "repo": "rank-sentinel",
  "branch": "main",
  "activity": "Editing code",
  "file_title_allowed": false
}
```

**`file_title_allowed`:** controlled by VS Code extension setting; defaults to `false`. When `false`, the active file name is never forwarded.
