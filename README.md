# OmniPresence

**Status:** First-pass scaffold — see [docs/PLAN.md](docs/PLAN.md) for what is implemented vs stubbed.

Auto-updates Discord Rich Presence based on the active focused window and deeper app context, using the official **Discord Social SDK** (no selfbot, no user token, no Discord Terms of Service violations).

---

## What It Does

OmniPresence watches your foreground window, applies a priority-ordered rule engine, and sets your Discord Rich Presence accordingly. Deeper integrations (RuneLite plugin, browser extension, terminal hook, VS Code extension) POST structured context to a local HTTP endpoint so the rule engine can show richer, accurate status.

### Behaviour Examples

| Focused app | Activity name | Details | State |
|---|---|---|---|
| RuneLite (Slayer task) | Old School RuneScape | Training Slayer | Attacking Skeletal Wyvern |
| Terminal in ArchiveBox repo | Code | Working on ArchiveBox | Running archivebox add |
| YouTube in browser | YouTube | Watching YouTube | *(private)* |
| Reddit in browser | Reddit | Browsing Reddit | *(private)* |
| Pi-hole dashboard | Custom Dashboard | Checking Pi-hole | Network monitoring |

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        OmniPresence Core                        │
│                                                                 │
│  ┌──────────────────┐   ┌──────────────────┐                   │
│  │  Active Window   │   │   Rule Engine    │                   │
│  │  Watcher         │──▶│  + Template      │                   │
│  │  (Win32 API)     │   │    Renderer      │                   │
│  └──────────────────┘   └────────┬─────────┘                   │
│                                  │                             │
│  ┌──────────────────┐            │  ┌──────────────────────┐   │
│  │  Local Context   │            │  │  Discord Social SDK  │   │
│  │  HTTP Server     │──▶context  └─▶│  Rich Presence       │   │
│  │  :47831          │               │  Client              │   │
│  └──────────────────┘               └──────────────────────┘   │
│           ▲                                                     │
│           │  POST /integrations/<source>/context                │
│  ┌────────┴────────────────────────────────────────────────┐   │
│  │                    Integrations                         │   │
│  │                                                         │   │
│  │  RuneLite Plugin │ Browser MV3 Ext │ Terminal Hook      │   │
│  │  (Java)          │ (Chrome/Edge)   │ (PowerShell/WT)    │   │
│  │                  │                 │                    │   │
│  │  VS Code Ext     │ Manual Override │                    │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │  GUI (Qt 6 / QML)  —  system tray + settings window     │  │
│  └──────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
```

All integration traffic is **localhost-only**. No external servers. No Discord user token.

---

## Repository Layout

```
OmniPresence/
├── README.md
├── LICENSE
├── CMakeLists.txt               # Top-level build (Windows target)
├── config/
│   ├── omnipresence.example.json  # Committed sample config
│   └── .gitignore               # Excludes omnipresence.json (real config)
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── WindowWatcher.*      # Win32 foreground window detection
│   │   ├── RuleEngine.*         # Priority-ordered rule matching
│   │   ├── TemplateRenderer.*   # {{variable}} substitution
│   │   └── PresenceManager.*    # Discord Social SDK wrapper
│   ├── integrations/
│   │   └── ContextServer.*      # Local HTTP server (:47831)
│   └── gui/
│       ├── MainWindow.*
│       └── qml/                 # Qt Quick / QML screens
├── integrations/
│   ├── runelite/                # Java RuneLite plugin
│   ├── browser-extension/       # Chrome/Edge MV3 extension
│   ├── terminal/                # PowerShell / Windows Terminal hook
│   └── vscode/                  # VS Code extension or workspace watcher
├── third_party/
│   └── discord_social_sdk/      # Fetched by setup script; gitignored
├── assets/
│   └── discord/                 # Reference asset names for Developer Portal
└── docs/
    ├── SPEC.md                  # Full functional specification
    ├── PLAN.md                  # Phased implementation plan + status
    ├── DECISIONS.md             # Architecture decision records
    ├── PRIVACY.md               # Privacy model
    ├── DISCORD_SETUP.md         # Discord app + SDK setup guide
    ├── INTEGRATIONS.md          # Per-integration payload schemas
    └── GRAPHIFY.md              # Graph query guide for contributors
```

---

## Quick-Start Pointers

**Building requires a Windows 11 machine.** The core app targets Win32 APIs and the Discord Social SDK; it cannot be compiled on Linux/WSL without significant shims that are out of scope.

Prerequisites (Windows):
- Visual Studio 2022 (C++20 support)
- [Qt 6](https://doc.qt.io/qt-6/qmlapplications.html) installed and `Qt6_DIR` set
- CMake 3.25+
- Discord Social SDK placed in `third_party/discord_social_sdk/` (see [docs/DISCORD_SETUP.md](docs/DISCORD_SETUP.md))
- A Discord application with Rich Presence enabled (see [docs/DISCORD_SETUP.md](docs/DISCORD_SETUP.md))

The RuneLite integration is a separate Java plugin built with the RuneLite parent POM (JDK 11+ required). The browser extension is loaded unpacked in Chrome/Edge developer mode. See [docs/INTEGRATIONS.md](docs/INTEGRATIONS.md) for each integration.

---

## Graphify-First Note for Contributors

This repository is indexed with [graphify](docs/GRAPHIFY.md). Before doing broad file searches or reading files speculatively, query the knowledge graph:

```bash
graphify query "where is rule matching implemented" --budget 1200
graphify explain "RuleEngine"
graphify path "WindowWatcher" "PresenceManager"
```

If the graph is stale after changes, run `graphify update .` (free AST rebuild) before querying. See [docs/GRAPHIFY.md](docs/GRAPHIFY.md) for the full query reference.

---

## Documentation Index

| File | Purpose |
|---|---|
| [docs/SPEC.md](docs/SPEC.md) | Full functional specification (canonical) |
| [docs/PLAN.md](docs/PLAN.md) | Phased plan + implementation status |
| [docs/DECISIONS.md](docs/DECISIONS.md) | Architecture decision records |
| [docs/PRIVACY.md](docs/PRIVACY.md) | Privacy model and guarantees |
| [docs/DISCORD_SETUP.md](docs/DISCORD_SETUP.md) | Discord app + SDK setup |
| [docs/INTEGRATIONS.md](docs/INTEGRATIONS.md) | Integration payload schemas |

---

## What This Is Not

- Not a selfbot. No Discord user token is used.
- Not a custom-status automator. Rich Presence is set via the official Discord Social SDK through the local Discord client.
- Not a cloud service. All data stays on the local machine.
