# OmniPresence — Implementation Plan

> Status column legend: **Scaffolded** | **Stubbed** | **Implemented** | **Not started**

---

## Phase 0 — Repository Scaffold (this pass)

| Item | Status | Notes |
|---|---|---|
| Repo initialised (git init) | Scaffolded | Directory exists at `/home/grafe/code/OmniPresence/` |
| README.md | Scaffolded | Overview, architecture diagram, quick-start |
| LICENSE (MIT) | Scaffolded | |
| docs/SPEC.md | Scaffolded | Full functional specification |
| docs/PLAN.md | Scaffolded | This file |
| docs/DECISIONS.md | Scaffolded | Architecture decision records |
| docs/PRIVACY.md | Scaffolded | Privacy model |
| docs/DISCORD_SETUP.md | Scaffolded | Discord app + SDK setup guide |
| docs/INTEGRATIONS.md | Scaffolded | Per-integration payload schemas |
| config/omnipresence.example.json | Not started | Needs sample config written |
| config/.gitignore | Not started | Exclude real config |
| Top-level CMakeLists.txt | Not started | |
| src/ directory structure | Not started | |
| integrations/ directory structure | Not started | |
| third_party/.gitignore (discord_social_sdk) | Not started | |

---

## Phase 1 — Win32 Window Watcher

| Item | Status | Notes |
|---|---|---|
| `src/core/WindowWatcher.h` | Not started | Interface definition |
| `src/core/WindowWatcher.cpp` | Not started | `GetForegroundWindow`, `GetWindowThreadProcessId`, `OpenProcess`, `QueryFullProcessImageName`, `GetWindowText` |
| Polling loop (500–1000 ms) | Not started | |
| Stability debounce (2–3 s) | Not started | Ignore transient Alt-Tab switches |
| WindowInfo struct | Not started | pid, exe_path, process_name, window_title, window_class |
| Unit tests (mocked Win32) | Not started | |

---

## Phase 2 — Discord Social SDK Integration

| Item | Status | Notes |
|---|---|---|
| `third_party/discord_social_sdk/` placeholder | Not started | SDK fetched by setup script |
| `src/core/PresenceManager.h` | Not started | Interface: connect, updateActivity, disconnect |
| `src/core/PresenceManager.cpp` | Not started | Wraps `discordpp::Client`, `discordpp::Activity` |
| `StatusDisplayType::Details` wiring | Not started | Set so member-list shows details field |
| Reconnect / error handling | Not started | Handle SDK disconnect gracefully |
| Presence-unchanged guard | Not started | Don't call SDK update if presence identical to last sent |
| Elapsed timer per category | Not started | Reset only on category change, not tab switches |
| References | — | [C++ Getting Started](https://docs.discord.com/developers/discord-social-sdk/getting-started/using-c%2B%2B), [Setting Rich Presence](https://docs.discord.com/developers/discord-social-sdk/development-guides/setting-rich-presence) |

---

## Phase 3 — Rule Engine and Template Renderer

| Item | Status | Notes |
|---|---|---|
| Rule data model (struct/class) | Not started | All fields from SPEC §10.3 |
| `src/core/RuleEngine.h/.cpp` | Not started | Priority-ordered rule evaluation |
| Priority 1: global pause/private | Not started | |
| Priority 2: pinned presence | Not started | |
| Priority 3: integration context match | Not started | |
| Priority 4: user-created specific rules | Not started | |
| Priority 5: browser domain/category | Not started | |
| Priority 6: generic process rule | Not started | |
| Priority 7: private fallback | Not started | Emits name="Computer", details="Working privately", state="Private" |
| `src/core/TemplateRenderer.h/.cpp` | Not started | `{{variable}}` substitution with left-to-right fallback chaining |
| All template variables from SPEC §5 | Not started | |
| Config loader (JSON → Rule objects) | Not started | |
| Rule serialiser (Rule objects → JSON) | Not started | |

---

## Phase 4 — Local Context HTTP Server

| Item | Status | Notes |
|---|---|---|
| `src/integrations/ContextServer.h/.cpp` | Not started | Lightweight HTTP server on `127.0.0.1:47831` |
| `POST /integrations/runelite/context` | Not started | Parse, validate, store latest context |
| `POST /integrations/browser/context` | Not started | |
| `POST /integrations/terminal/context` | Not started | |
| `POST /integrations/vscode/context` | Not started | |
| Context store (per-source, latest-wins) | Not started | Thread-safe; exposed to RuleEngine |
| Reject non-localhost requests | Not started | Check remote IP; reject anything not 127.0.0.1 |
| HTTP library choice | Not started | Decision: cpp-httplib or Boost.Beast (document in DECISIONS.md) |

---

## Phase 5 — GUI (Qt 6 / QML)

| Item | Status | Notes |
|---|---|---|
| Qt6 CMake integration | Not started | Requires Qt6 installed on build machine |
| System tray icon + context menu | Not started | Show/hide window; quick pause toggle |
| `src/gui/MainWindow.*` | Not started | Host for QML engine |
| `src/gui/qml/Dashboard.qml` | Not started | SPEC §10.1 |
| `src/gui/qml/RuleEditor.qml` | Not started | SPEC §10.3 |
| `src/gui/qml/PrivacySettings.qml` | Not started | SPEC §10.4 |
| `src/gui/qml/AssetManager.qml` | Not started | SPEC §10.5 |
| Capture Current Window wizard | Not started | SPEC §10.2; populates rule form |
| Live presence preview in Dashboard | Not started | Polls rule engine output in real time |
| Reference | — | [Qt 6 / Qt Quick](https://doc.qt.io/qt-6/qmlapplications.html) |

---

## Phase 6 — RuneLite Integration

| Item | Status | Notes |
|---|---|---|
| `integrations/runelite/` Maven project scaffold | Not started | |
| Plugin class (implements `Plugin`) | Not started | |
| Event subscriptions (GameStateChanged, GameTick, InteractingChanged, AnimationChanged, StatChanged, NpcSpawned, VarbitChanged, ChatMessage) | Not started | |
| Activity inference logic | Not started | Combat / Slayer / Skilling / Idle / Bossing / Banking / GE |
| Confidence scorer | Not started | |
| HTTP POST to `/integrations/runelite/context` | Not started | |
| Privacy gate (account name opt-in) | Not started | |
| References | — | [RuneLite Developer Guide](https://github.com/runelite/runelite/wiki/Developer-Guide), [Events API](https://static.runelite.net/runelite-api/apidocs/net/runelite/api/events/package-summary.html) |

---

## Phase 7 — Browser Extension

| Item | Status | Notes |
|---|---|---|
| `integrations/browser-extension/manifest.json` (MV3) | Not started | Chrome / Edge |
| Background service worker | Not started | |
| `chrome.tabs` active tab listener | Not started | Domain extraction, title whitelist check |
| Domain → category mapping | Not started | All mappings from SPEC §8 |
| Dashboard URL detection | Not started | Local IPs, pi.hole, localhost |
| POST to `/integrations/browser/context` | Not started | |
| Extension settings UI (whitelist toggles) | Not started | |
| Reference | — | [Chrome Tabs API](https://developer.chrome.com/docs/extensions/reference/api/tabs) |

---

## Phase 8 — Terminal Hook

| Item | Status | Notes |
|---|---|---|
| `integrations/terminal/OmniPresence.psm1` | Not started | PowerShell module |
| `PSReadLine` prompt-function hook | Not started | Hook into prompt to capture cwd + last command |
| Git repo detection from cwd | Not started | `git rev-parse --show-toplevel` |
| Command sanitization | Not started | Strip secrets, tokens, credential-looking args |
| POST to `/integrations/terminal/context` | Not started | |
| Windows Terminal profile install instructions | Not started | Auto-import module in `$PROFILE` |

---

## Phase 9 — VS Code Integration

| Item | Status | Notes |
|---|---|---|
| `integrations/vscode/` extension scaffold | Not started | `package.json`, `extension.ts` |
| Active editor / workspace detection | Not started | `vscode.workspace.name`, `vscode.window.activeTextEditor` |
| File title privacy gate | Not started | Setting: `omniPresence.allowFileTitle` (default: false) |
| POST to `/integrations/vscode/context` | Not started | |
| Publish to VS Code Marketplace | Not started | Optional; can run as unpublished local extension |

---

## Phase 10 — Asset Pipeline and Developer Portal Setup

| Item | Status | Notes |
|---|---|---|
| Recommended asset images created | Not started | osrs, code, terminal, youtube, reddit, pihole, dashboard |
| Upload guide verified against live portal | Not started | See DISCORD_SETUP.md |
| Asset keys wired into default rules | Not started | |

---

## Phase 11 — Packaging and Installer

| Item | Status | Notes |
|---|---|---|
| Windows installer (NSIS or WiX) | Not started | Place config in %APPDATA%\OmniPresence\ |
| Setup script to fetch Discord Social SDK | Not started | `scripts/setup.ps1` |
| Auto-start on login option | Not started | Registry run key or startup shortcut |
| Uninstaller | Not started | Remove config option |

---

## Next Recommended Steps

1. **On a Windows 11 machine:** Install Qt 6, CMake, Visual Studio 2022, download the Discord Social SDK, and create the initial `CMakeLists.txt` to confirm the build toolchain works end-to-end.
2. **Create `config/omnipresence.example.json`** with all default rule mappings from SPEC §7 and a placeholder Application ID.
3. **Implement Phase 1 (Win32 Window Watcher)** as a standalone executable that prints the foreground process name and window title — verify the detection loop works before wiring it to anything else.
4. **Implement Phase 4 (Context Server)** as a minimal HTTP server that accepts POSTs and echoes them back — test with `curl` before wiring integrations.
5. **Implement Phase 3 (Rule Engine)** against the example config and the context server output — unit-testable without the Discord SDK.
6. **Wire Phase 2 (Discord SDK)** once the rule engine produces stable output.
7. **Build integrations** (Phase 6–9) in parallel once the context server is running.
