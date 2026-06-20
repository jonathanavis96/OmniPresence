# Graphify — knowledge graph for OmniPresence

This repo is **Graphify-first**. Before broadly reading files or grepping the
tree to answer a "where / how / what-calls" question, **query the graph**. It is
faster, cheaper, and points you at exact `file:line` locations to open.

> Rule of thumb: *query the graph to find where to look, then open only those
> exact locations.* Do not fan out `grep -r` / `rg` across the repo when a graph
> query can answer the question.

## Where the graph files live

```
graphify-out/
├─ graph.json          # the graph: nodes (symbols/files/communities) + edges  ← query this
├─ report.md           # human-readable community map ("god nodes", clusters)
├─ graph.html          # interactive visualisation (gitignored — regenerate locally)
└─ memory/             # saved Q&A feedback (gitignored)
```

`graphify-out/` is **not committed** — it is gitignored globally (regenerable,
and it churns on every commit). The repo's git hooks rebuild it automatically:
`post-commit` refreshes after code changes and `post-checkout` rebuilds on branch
switch. **On a fresh clone, run `scripts/graphify-build` once** to generate the
graph before querying (or just make a commit / checkout to trigger the hook).

## How to query (do this FIRST)

Use the wrapper script (or the `graphify` CLI directly):

```bash
# Natural-language traversal — returns src=path loc=Lnn pointers
scripts/graphify-query "where is the rule priority evaluated?"
scripts/graphify-query "how does integration context reach Discord presence?" --budget 1500

# Explain a symbol and its neighbours
scripts/graphify-query --explain RuleEngine

# Shortest path between two symbols (how does A reach B?)
scripts/graphify-query --path "LocalContextServer" "DiscordPresenceClient"

# Reverse impact — what breaks if I change X?
scripts/graphify-query --affected "PresencePayload"
```

Raw CLI equivalents: `graphify query "..."`, `graphify explain "X"`,
`graphify path "A" "B"`, `graphify affected "X"`.

### Navigation style — prefer query/path/explain over grep

| Question | Do this | Not this |
|---|---|---|
| "Where is X handled?" | `graphify-query "X..."` → open the cited lines | `rg X` across the repo |
| "What does this symbol relate to?" | `graphify-query --explain X` | reading the whole file |
| "How does A reach B?" | `graphify-query --path A B` | tracing calls by hand |
| "What's impacted if I change X?" | `graphify-query --affected X` | guessing |

A repo-level hook may **hard-block** recursive grep (`grep -r`, `rg`, `ack`,
`ag`) in graphed repos. If you genuinely need raw recursive grep (e.g. editing
every call site), opt out per your harness's documented escape hatch.

## How to (re)build the graph

```bash
# Full STRUCTURAL + SEMANTIC build (AST + LLM community naming). Run occasionally.
scripts/graphify-build

# Fast FREE structural refresh after edits (AST only, no LLM). Run often.
scripts/graphify-refresh

# Structural-only one-off
scripts/graphify-build --ast
```

`graphify-build` uses the Gemini backend (`GEMINI_API_KEY`) for the semantic
pass and a flash-lite relabel pass for real community names. After meaningful
code changes, run `scripts/graphify-refresh` so pointers stay accurate; do a
full `scripts/graphify-build` after large refactors that add/remove modules.

## For future agents working on this repo

1. **Check `graphify-out/graph.json` exists.** If the code changed since it was
   built, run `scripts/graphify-refresh` (free, AST) first.
2. **Query before reading.** Treat any where/how/what-calls question as a graph
   query. Open only the `file:line` locations it returns.
3. **Don't broad-read.** Reading large parts of the repo to orient yourself is
   exactly what the graph is for — use it.

If Graphify is not installed in your environment, see
[`../tools/graphify/README.md`](../tools/graphify/README.md) for the placeholder
integration and install notes.
