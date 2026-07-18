# OmniPresence

Windows 11 C++20 / Qt6 / QML / CMake desktop app that auto-updates **Discord Rich
Presence** from the active focused window + per-app context. **NOT a selfbot** — uses the
official Discord Social SDK, no user token. Strong privacy defaults (browser titles / file
names private unless whitelisted; global pause). Work on the **`omnipresence-work`** branch.

Integrations feed context over **localhost only** at
`http://127.0.0.1:47831/integrations/<source>/context` (RuneLite plugin, Chrome/Edge MV3
extension, PowerShell hook, VS Code extension). Display = Option A (name = broad label,
details = what, state = specific), `StatusDisplayType::Details`.

**RuneLite (current):** uses the **real RuneLite client + its built-in Discord plugin**,
captured via IPC interception (`NamedPipeInterceptor` on `discord-ipc-0`). The old
standalone-dev-Java + custom-HTTP-plugin method is **DEPRECATED** (kept in
`integrations/runelite/` with a banner; ADR in `docs/DECISIONS.md`).
- **Gotcha:** the built-in plugin only sends `SET_ACTIVITY` on change (no heartbeat) →
  needs the focus-gated keep-alive re-stamp + empty-name guard, or it publishes a blank
  name ("OmniPresence").

Scaffolded but NOT yet compiled on WSL (no Qt6/JDK there); Discord SDK calls are
preview/no-op TODOs pending symbol verification + a Windows build. See `docs/PLAN.md` for
implemented-vs-stubbed. **Live state** → vault `Projects/OmniPresence.md`.

## graphify

This project has a knowledge graph at graphify-out/ with god nodes, community structure, and cross-file relationships.

Rules:
- For codebase questions, first run `graphify query "<question>"` when graphify-out/graph.json exists. Use `graphify path "<A>" "<B>"` for relationships and `graphify explain "<concept>"` for focused concepts. These return a scoped subgraph, usually much smaller than GRAPH_REPORT.md or raw grep output.
- If graphify-out/wiki/index.md exists, use it for broad navigation instead of raw source browsing.
- Read graphify-out/GRAPH_REPORT.md only for broad architecture review or when query/path/explain do not surface enough context.
- After modifying code, run `graphify update .` to keep the graph current (AST-only, no API cost).
