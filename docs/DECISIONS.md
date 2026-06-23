# OmniPresence — Architecture Decision Records

> **Rule:** If a strong technical reason requires changing part of the stack, document it here BEFORE changing it. This file is append-only — do not edit previous decisions.

---

## ADR-001 — Core Language: C++20

**Decision:** Write the core application in C++20.

**Rationale:**
- The Discord Social SDK is distributed as a C++ library with a first-class C++ API.
- Win32 APIs are natively C/C++; calling them from C++ requires no FFI layer.
- Qt 6 (the chosen GUI framework) is a C++ library with native C++ bindings; its QML engine is embedded in a C++ host.
- C++20 features (concepts, ranges, `std::format`, coroutines) improve code clarity without runtime overhead.

**Rejected alternatives:**
- **Rust:** Excellent for systems work, but the Discord Social SDK has no official Rust binding; a C FFI wrapper would add fragility and maintenance burden.
- **C#/.NET:** WinForms/WPF are Windows-only, which fits, but the Discord Social SDK C++ integration is simpler than a P/Invoke bridge, and Qt is cross-platform for dev convenience.
- **Python:** Too slow for a polling event loop; Discord SDK binding would be unofficial.

---

## ADR-002 — GUI: Qt 6 / QML

**Decision:** Use Qt 6 with QML for the GUI.

**Rationale:**
- Qt 6 provides a mature, cross-platform widget system that can be compiled on Windows (the runtime target) while allowing development on Linux/WSL with the same codebase.
- QML enables declarative, animated UI with hot-reload during development.
- Qt 6 integrates tightly with the C++ core via `Q_PROPERTY`, signals/slots, and `QAbstractListModel` for rules display.
- Qt's system tray API (`QSystemTrayIcon`) is well-supported on Windows 11.

**Rejected alternatives:**
- **WinUI 3 / XAML:** Windows-only; prevents any Linux dev-machine prototyping of UI logic.
- **Electron:** Too heavy for a tray utility; adds Node.js + Chromium overhead; complicates Discord SDK integration.
- **ImGui:** Suitable for debug overlays but not for a polished settings UI with form editors.

---

## ADR-003 — Build System: CMake

**Decision:** Use CMake as the build system.

**Rationale:**
- Qt 6 ships CMake config files and is the officially recommended build system for Qt 6 projects (`qt_add_executable`, `qt_add_qml_module`).
- CMake works on both Windows (MSVC) and Linux (GCC/Clang) for development.
- The Discord Social SDK can be included as a CMake `find_package` or `add_subdirectory` target.
- Industry standard; CI/CD integrations (GitHub Actions, etc.) have mature CMake support.

**Rejected alternatives:**
- **qmake:** Qt's legacy build system; being phased out in Qt 6.
- **Meson:** Good system but less common in Qt/Win32 projects; fewer examples for the SDK integration.

---

## ADR-004 — Window Detection: Win32 APIs

**Decision:** Use Win32 APIs directly for foreground window detection.

**APIs:** `GetForegroundWindow`, `GetWindowThreadProcessId`, `OpenProcess`, `QueryFullProcessImageName`, `GetWindowText`.

**Reference:** [GetForegroundWindow — Microsoft Docs](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getforegroundwindow)

**Rationale:**
- These are stable, well-documented, low-overhead Win32 APIs.
- No elevated privileges required for querying process names of standard applications.
- Direct polling at 500–1000 ms is sufficient for this use case; no window hooks (`SetWinEventHook`) needed unless latency requirements change.

**Trade-off noted:** `SetWinEventHook` (event-driven) would be more efficient than polling. Polling is chosen for simplicity in the initial implementation; can be upgraded to event-driven in Phase 1 if CPU overhead is observed.

---

## ADR-005 — Integration Transport: HTTP Loopback (Port 47831)

**Decision:** Integrations send context to the core via `POST http://127.0.0.1:47831/integrations/<source>/context`.

**Rationale:**
- HTTP is language-agnostic: the RuneLite plugin (Java), browser extension (JavaScript), terminal hook (PowerShell), and VS Code extension (TypeScript) can all use their native HTTP clients without special libraries.
- Easy to debug with `curl` during development.
- JSON payloads are human-readable and schema-checkable.
- Port 47831 is unassigned in the IANA registry and unlikely to conflict.

**Rejected alternative — Named Pipes:**

| Aspect | HTTP Loopback | Named Pipe |
|---|---|---|
| Language support | Universal (any HTTP client) | Win32-specific; Java/JS need wrappers |
| Debuggability | `curl`-testable | Requires pipe client tooling |
| Security | Bound to 127.0.0.1 only | Process ACL (Windows-native) |
| Complexity | Slightly higher (HTTP server in core) | Lower overhead per-message |
| Cross-platform dev | Works on Linux for integration unit tests | Windows-only |

**Security note:** The HTTP server MUST bind to `127.0.0.1` only, never `0.0.0.0`. All incoming requests must be verified to originate from localhost; any non-loopback source must be rejected.

---

## ADR-006 — Config Format: Human-Readable JSON

**Decision:** Store configuration as JSON at `%APPDATA%\OmniPresence\config.json` (installed) or `config/omnipresence.json` (dev).

**Rationale:**
- Human-readable: users can inspect and hand-edit rules without a GUI.
- Easy to serialize/deserialize in C++ (nlohmann/json or similar).
- Git-diffable for version-controlled rule sets.
- Committed sample (`omnipresence.example.json`) serves as documentation.

**Rejected alternatives:**
- **SQLite:** Overkill for a flat list of rules; not human-readable.
- **TOML/YAML:** Either would work, but JSON has better C++ library support and is consistent with the integration payloads.

---

## ADR-007 — StatusDisplayType::Details

**Decision:** Use `StatusDisplayType::Details` when setting Rich Presence via the Discord Social SDK.

**Rationale:**
- The Discord member list shows a condensed status line next to the user's name. Without `StatusDisplayType::Details`, this line defaults to the activity name only (e.g. "Playing Old School RuneScape"), which is often redundant.
- With `StatusDisplayType::Details`, the member-list status shows the **details** field (e.g. "Training Slayer"), which is the meaningful, actionable information.
- Avoids awkward combinations like "Playing Working on code" that arise when the activity name is a verb phrase.

**Reference:** [Activity class — discordpp::Activity](https://discord.com/developers/docs/social-sdk/classdiscordpp_1_1Activity.html)

---

## ADR-008 — Rule Priority Design

**Decision:** Use a fixed 7-level priority stack (see SPEC §4) evaluated deterministically in descending order.

**Rationale:**
- A flat numeric priority across all user rules creates ambiguity when the same window matches multiple user rules. The 7-level stack makes the winner deterministic without requiring the user to understand numeric weights.
- Manual overrides (pause, pin) always win, regardless of what else matches.
- Integration context (priority 3) is inherently richer than a generic rule match, so it outranks user-created rules. Users who want to override an integration match can use the manual pin (priority 2).
- The private fallback always emits rather than leaving presence stale, preventing accidental disclosure of an old presence.

---

## ADR-009 — Dev Environment: Linux/WSL Scaffold

**Decision:** The initial scaffold (Phase 0 documentation and directory structure) is being authored on a Linux/WSL machine. No C++ compilation or Java build will be performed in this environment.

**Constraint documented here:**
- Qt 6 is **not installed** on the Linux/WSL dev machine used for this scaffold.
- A JDK is **not installed** on the Linux/WSL dev machine used for this scaffold.
- The Discord Social SDK targets Windows; its headers and libraries are not available in this environment.

**Consequence:** The C++ core application, the RuneLite plugin, and the browser/terminal/VS Code integrations were scaffolded (directory structure + documentation) but **not compiled or built** during Phase 0. Building requires a Windows 11 machine with:
- Visual Studio 2022 (C++20)
- Qt 6 installed and `Qt6_DIR` configured
- CMake 3.25+
- Discord Social SDK placed in `third_party/discord_social_sdk/`
- JDK 11+ (for RuneLite plugin only)

Any contributor working on the build system should follow the instructions in [DISCORD_SETUP.md](DISCORD_SETUP.md) and [PLAN.md](PLAN.md) Phase 1.

## RuneLight game-detection vs our SDK presence (2026-06-21)

"RuneLight" on the profile comes from **Discord's own game detection** (its
registered-game database matching the RuneLite executable), independent of our
Social-SDK Rich Presence — it shows even with our RuneLite plugin off.

**Finding: no SDK-side override exists.** `third_party/discord_social_sdk/include/discordpp.h`
exposes only `UserHandle::GameActivity()` (a *read* accessor for a user's detected
game) — there is no API to suppress, deprioritise, or opt out of Discord's
registered-game detection from the application/SDK side. So the app cannot stop the
"RuneLight" label.

**Resolution (client-side only):** in the Discord desktop client, **User Settings →
Activity Settings → Registered Games** — toggle detection off for RuneLite (or remove
it from the list); and/or **Activity Privacy → "Display currently running game as a
status message"** off. With Discord's auto-detection disabled, the profile shows only
OmniPresence's published presence (our set name/line). No code change shipped.

## RuneLite presence source: built-in Discord plugin via IPC interception, NOT our custom plugin (2026-06-23)

**Decision:** OmniPresence captures RuneLite activity by **intercepting RuneLite's
built-in Discord plugin** on the `\\.\pipe\discord-ipc-0` named pipe
(`app/src/NamedPipeInterceptor.cpp`). We run the **real, official RuneLite client**.

**Deprecated (code retained, not deleted):** the earlier method — a **standalone
developer Java RuneLite** client sideloading our custom
`integrations/runelite/omnipresence-runelite-plugin` which POSTed inferred context
to the local HTTP server (`/integrations/runelite/context`). That required
`--developer-mode` + `--insecure-write-credentials` (the Jagex Launcher can't pass
those), which is why a locally-built dev client was needed.

**Why switched:** the real client + built-in plugin removes the dev-client sideload,
the `~/.runelite/credentials.properties` bypass file, and the maintenance of our own
inference plugin. The plugin source stays in `integrations/runelite/` for reference.

**Removed as part of this switch (2026-06-23):**
- the standalone developer Java RuneLite clone at `C:\dev\runelite`
- `C:\Users\<user>\.runelite\credentials.properties` (dev `--insecure-write-credentials` file)

**Trade-off / follow-on:** the built-in Discord plugin only sends `SET_ACTIVITY`
**on change** (no heartbeat), unlike our old plugin's ~5 s re-POST. With the 120 s
freshness window this caused steady-state sessions (e.g. training one skill) to go
stale and publish a **blank name** → Discord showed the bare "OmniPresence".
Fixed by (a) a focus-gated RuneLite keep-alive that re-stamps the payload every 30 s
while RuneLite is the focused window, and (b) an empty-name guard in `RuleEngine`
that never lets a rule publish a blank activity name.
