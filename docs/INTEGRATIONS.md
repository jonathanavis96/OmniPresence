# OmniPresence — Integrations Guide

OmniPresence supports four deep-context integrations. Each runs in its host environment (Java, JavaScript, PowerShell, TypeScript) and POSTs a structured JSON payload to the local context server at `http://127.0.0.1:47831/integrations/<source>/context`.

All integration traffic is **localhost-only**. The context server never exposes a port to the network.

Integration context is consumed by the rule engine at **priority 3** — higher than user-created rules, lower than manual overrides. See [SPEC.md §4](SPEC.md) for the full priority stack.

---

## How the Rule Engine Consumes Integration Context

When an integration posts a payload, OmniPresence:

1. Stores the latest payload per source (e.g. latest RuneLite context, latest browser context).
2. When the active window matches the integration's source app (e.g. `RuneLite.exe` is the foreground process), the rule engine checks if a valid, recent integration payload is available.
3. If yes: template variables are populated from the payload fields and the matching integration rule (priority 3) is applied.
4. If no current integration payload: falls through to priority 4+ (user rules, generic process rules).

Integration payloads expire after 30 seconds of no update (the integration is considered disconnected or the relevant app is closed).

---

## 1. RuneLite Plugin

**What it tracks:** Real-time Old School RuneScape game state — the player's current activity (combat, skilling, idle, bossing, banking, GE), the specific NPC or target being interacted with, the skill being trained, the in-game location, and a confidence score for the inferred activity.

**Endpoint:** `POST http://127.0.0.1:47831/integrations/runelite/context`

### Payload Schema

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

| Field | Type | Required | Description |
|---|---|---|---|
| `source` | string | Yes | Always `"runelite"` |
| `game` | string | Yes | Always `"Old School RuneScape"` |
| `account` | string | No | Local player name. Omit if privacy is enabled in plugin settings |
| `activity` | string | Yes | Inferred activity label (e.g. `Training Slayer`, `Bossing`, `Idle at GE`) |
| `target` | string | No | Current NPC/target (from `InteractingChanged`) |
| `skill` | string | No | Primary skill being trained (from `StatChanged` recency) |
| `location` | string | No | In-game location name |
| `confidence` | float | Yes | 0.0–1.0; rule engine may suppress display below 0.5 |
| `timestamp` | ISO 8601 | Yes | UTC timestamp of last update |

### Events Subscribed

| Event | Purpose |
|---|---|
| `GameStateChanged` | Login/logout, loading state detection |
| `GameTick` | Heartbeat for idle detection and state refresh |
| `InteractingChanged` | Current NPC or player target |
| `AnimationChanged` | Distinguish combat / skilling / idle from animation ID |
| `StatChanged` | Recent XP gains; infer skill being trained |
| `NpcSpawned` / `NpcDespawned` | Bossing and slayer task context |
| `VarbitChanged` / `VarPlayerChanged` | Quest state, slayer task flags, activity flags |
| `ChatMessage` | Game messages only (e.g. "You've completed a slayer task") — **chat content is never forwarded** |

### Activity Inference Logic

| Signal | Inferred activity |
|---|---|
| `InteractingChanged` → NPC with combat animations | "Training [skill]" or "Bossing" |
| Slayer task VarBit active + combat | "Training Slayer" |
| Bank interface open (VarBit) | "At the bank" |
| Grand Exchange interface open | "At the Grand Exchange" |
| Skilling animation (fishing/woodcutting/etc.) | "Training [skill]" |
| No interaction, no XP gain, no animation | "Idle" |
| Died / respawned | "Died" (short-lived) |

### Privacy Handling

- `account` is **optional** and defaults to omitted. User must explicitly enable account name sharing in the plugin's RuneLite settings panel.
- Chat message content is never included in the payload. The plugin only uses `ChatMessage` to detect game events (e.g. task completion messages) through pattern matching — the raw message text is discarded.
- Bank contents, inventory, and equipment are never read or forwarded.

### Build

The plugin is a standard RuneLite plugin built against the RuneLite parent POM. Requires JDK 11+.

```
integrations/runelite/
├── pom.xml
└── src/main/java/com/omnipresence/runelite/
    ├── OmniPresencePlugin.java
    ├── OmniPresenceConfig.java
    └── ActivityInferrer.java
```

**References:**
- [RuneLite Developer Guide](https://github.com/runelite/runelite/wiki/Developer-Guide)
- [RuneLite API Events](https://static.runelite.net/runelite-api/apidocs/net/runelite/api/events/package-summary.html)
- [AnimationChanged](https://static.runelite.net/runelite-api/apidocs/net/runelite/api/events/AnimationChanged.html)
- [InteractingChanged](https://static.runelite.net/runelite-api/apidocs/net/runelite/api/events/InteractingChanged.html)
- [StatChanged](https://static.runelite.net/runelite-api/apidocs/net/runelite/api/events/StatChanged.html)

---

## 2. Browser Extension (Chrome / Edge MV3)

**What it tracks:** The domain of the active browser tab, a human-readable category label for that domain, and — only when the user has enabled the relevant whitelist toggle — the page title. Also detects local dashboard URLs (Pi-hole, router, localhost services).

**Endpoint:** `POST http://127.0.0.1:47831/integrations/browser/context`

### Payload Schema

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

| Field | Type | Required | Description |
|---|---|---|---|
| `source` | string | Yes | Always `"browser"` |
| `browser` | string | Yes | `"chrome"` or `"edge"` |
| `domain` | string | Yes | Registered domain (e.g. `reddit.com`, `github.com`) |
| `category` | string | Yes | Human-readable activity label for this domain |
| `title_allowed` | bool | Yes | True only if user has enabled exact title for this domain |
| `safe_title` | string or null | No | Page title, only present when `title_allowed: true` |
| `dashboard_label` | string or null | No | Set when URL is a local dashboard (see below) |

### Domain → Category Mapping (Defaults)

| Domain | `category` |
|---|---|
| `youtube.com` | `Watching YouTube` |
| `reddit.com` | `Browsing Reddit` |
| `github.com` | `Browsing GitHub` |
| `chatgpt.com` | `Using ChatGPT` |
| `pi.hole` | `Checking Pi-hole` |
| `192.168.x.x` | `Custom Dashboard` |
| `localhost:xxxx` | `Local Dashboard` |
| Other | `Browsing privately` |

### Dashboard Detection Logic

| URL pattern | `dashboard_label` |
|---|---|
| `pi.hole/admin*` | `Checking Pi-hole` |
| `192.168.x.x/admin*` | `Custom Dashboard` |
| `localhost:<port>` | `Local Dashboard` |
| Router login page patterns | `Router Dashboard` |

### Privacy Handling

- Domain is always sent (domain alone carries minimal privacy risk and enables category matching).
- Page title is **never sent** unless `title_allowed: true`.
- `title_allowed` is set per-domain based on user whitelist toggles in OmniPresence Privacy Settings.
- ChatGPT conversation titles remain off by default even when other whitelist toggles are enabled.

### Build

A Manifest V3 browser extension. Load unpacked in Chrome/Edge developer mode.

```
integrations/browser-extension/
├── manifest.json
├── background.js       (service worker)
├── popup.html          (optional settings UI)
└── popup.js
```

**Reference:** [Chrome Tabs API](https://developer.chrome.com/docs/extensions/reference/api/tabs)

---

## 3. Terminal Hook (PowerShell / Windows Terminal)

**What it tracks:** The current working directory, the inferred git repository name, the current branch, and a sanitized summary of the most recently run command.

**Endpoint:** `POST http://127.0.0.1:47831/integrations/terminal/context`

### Payload Schema

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

| Field | Type | Required | Description |
|---|---|---|---|
| `source` | string | Yes | Always `"terminal"` |
| `cwd` | string | Yes | Current working directory |
| `repo` | string | No | Git repository name inferred from `git rev-parse --show-toplevel` |
| `branch` | string | No | Current git branch |
| `command_summary` | string | No | Sanitized last-command summary. Omitted if `privacy_safe: false` |
| `privacy_safe` | bool | Yes | `true` only when command has been fully sanitized |

### Command Sanitization Rules

The hook must apply the following filters before generating `command_summary`:

1. Strip all credential-looking arguments: `--token`, `--key`, `--password`, `--secret`, `--api-key`, `-p <value>`, `VARIABLE=value` assignments where the value is high-entropy.
2. Reduce to `"Running <base-command>"` — arguments are only preserved if they are safe (e.g. subcommand names like `add`, `push`, `build`).
3. Never include piped input content.
4. If any sanitization uncertainty exists, set `privacy_safe: false` and omit `command_summary`.

**Examples:**

| Raw command | `command_summary` | `privacy_safe` |
|---|---|---|
| `archivebox add https://example.com` | `Running archivebox add` | `true` |
| `git commit -m "fix auth bug"` | `Running git commit` | `true` |
| `curl -H "Authorization: Bearer sk-abc" api.example.com` | `Running curl` | `true` |
| `export DISCORD_TOKEN=abc123` | *(omitted)* | `false` |
| `npm run build` | `Running npm run build` | `true` |

### Installation

Add the PowerShell module to `$PROFILE`:

```powershell
Import-Module "C:\path\to\OmniPresence\integrations\terminal\OmniPresence.psm1"
```

The module hooks into `PSReadLine`'s `OnIdle` event and the prompt function to detect directory changes and completed commands.

---

## 4. VS Code Extension

**What it tracks:** The current workspace or folder name, the git repository name and branch, the current activity (editing code, running debugger, viewing diff, etc.), and — only if explicitly enabled — the active file name.

**Endpoint:** `POST http://127.0.0.1:47831/integrations/vscode/context`

### Payload Schema

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

| Field | Type | Required | Description |
|---|---|---|---|
| `source` | string | Yes | Always `"vscode"` |
| `workspace` | string | Yes | Workspace or folder name |
| `repo` | string | No | Git repository name (from VS Code git extension) |
| `branch` | string | No | Current git branch |
| `activity` | string | Yes | Current editor activity (see table below) |
| `file_title_allowed` | bool | Yes | Reflects the VS Code setting `omniPresence.allowFileTitle` (default: `false`) |

### Activity Labels

| VS Code state | `activity` value |
|---|---|
| Editing a file | `Editing code` |
| Running a debug session | `Debugging` |
| Viewing git diff | `Reviewing changes` |
| Running a terminal task | `Running a task` |
| No file open / idle | `Browsing files` |

### Privacy Handling

- Active file name is never included unless `omniPresence.allowFileTitle` is enabled in VS Code settings (`false` by default).
- Workspace name is considered safe (it is the folder/project name, not a file path).
- No file contents are ever read or forwarded.

### Build

A standard VS Code extension. Can be installed as a local VSIX or published to the VS Code Marketplace.

```
integrations/vscode/
├── package.json
├── tsconfig.json
└── src/
    └── extension.ts
```

---

## 5. Manual Context / Pinned Presence

There is no external integration for manual overrides — these are set directly in the OmniPresence GUI:

- **Pause:** Stops all updates. Access from system tray right-click.
- **Pin presence:** Lock a specific presence from the Dashboard. Any forwarded integration context is ignored while pinned.
- **Private mode:** Emits the private fallback (`name="Computer", details="Working privately", state="Private"`) regardless of what any integration reports.

---

## 6. Adding a New Integration

To add a new integration:

1. Choose a source name (lowercase, no spaces, e.g. `spotify`).
2. POST to `http://127.0.0.1:47831/integrations/<source>/context` with a JSON body containing at minimum `"source": "<source>"` and a `"timestamp"`.
3. Add the new source's template variables to SPEC.md §5.
4. Add a default mapping to SPEC.md §7.
5. Add an ADR entry in DECISIONS.md explaining any non-obvious design choices for that integration.
6. Document the payload schema in this file.
