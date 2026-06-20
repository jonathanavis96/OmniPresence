# tools/graphify — Graphify integration

OmniPresence is a **Graphify-first** repo. The primary docs live in
[`../../docs/GRAPHIFY.md`](../../docs/GRAPHIFY.md); this folder holds the
integration notes and serves as the fallback if the `graphify` CLI is missing.

## Status in this repo

Graphify **is** wired up. The graph build was run during project scaffolding:

- Build CLI: `graphify` (semantic backend: Gemini via `GEMINI_API_KEY`)
- Graph output: `graphify-out/graph.json` (+ `report.md`), committed.
- Wrapper scripts: `scripts/graphify-build`, `scripts/graphify-query`,
  `scripts/graphify-refresh`.

See `BUILD_STATUS.md` in this folder (written by the build step) for the exact
outcome of the most recent build on this machine.

## If `graphify` is NOT installed (placeholder path)

The repo still works without Graphify — the scripts fail gracefully with a clear
message. To enable graph navigation:

1. **Install the CLI.** Graphify is an external tool (not bundled here). On the
   scaffolding machine it was installed at `~/.local/bin/graphify`
   (version 0.8.42). Install/update it per your Graphify distribution, then
   confirm `graphify --version`.
2. **Provide an LLM backend key** for the semantic pass. Default backend is
   Gemini (`GEMINI_API_KEY`). Other supported backends: `kimi`, `claude`,
   `openai`, `deepseek`, `ollama` — pass `--backend` to `graphify extract`.
3. **Build:** `scripts/graphify-build` (full) or `scripts/graphify-build --ast`
   (structural-only, no key needed).
4. **Query:** `scripts/graphify-query "..."`.

### What would be missing without it

- `graphify` binary on PATH — required for build/query/refresh.
- An LLM API key (e.g. `GEMINI_API_KEY`) — required only for the *semantic*
  pass; the free AST-only build (`--ast` / `graphify update`) needs no key.

Until the CLI is present, treat `docs/GRAPHIFY.md` as the navigation contract and
fall back to targeted (non-recursive) file reads.
