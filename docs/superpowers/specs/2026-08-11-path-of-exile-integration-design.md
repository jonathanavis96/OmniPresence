# Path of Exile presence integration — design

Status: draft for review
Date: 2026-08-11

## Problem

OmniPresence publishes rich Discord presence for Old School RuneScape by
intercepting the Discord IPC named pipe that RuneLite's own Discord plugin
writes to (`NamedPipeInterceptor`). Path of Exile has no equivalent: no PoE
client_id was ever observed handshaking on the pipe while the game was running,
so there is nothing to intercept.

Throughout this spec and in all user-facing strings the game is called simply
**Path of Exile** — never "PoE 1" or "Path of Exile 1". The integration `source`
string is `poe`.

PoE does, however, write a detailed plain-text activity log. This spec covers
reading that log and feeding the existing integration pipeline, so PoE gets the
same quality of presence RuneLite has.

## What the log actually provides

Verified against a live 18 MB `Client.txt` on this machine
(`C:\Program Files (x86)\Steam\steamapps\common\Path of Exile\logs\`), last
written 2026-08-11 18:50.

**Tail `LatestClient.txt`, not `Client.txt`.** The install writes both.
`Client.txt` is append-only and unbounded (18 MB live, an 79 MB `.bak` beside
it). `LatestClient.txt` is truncated at every launch, holds only the current
session, and was 32 KB. It opens with a `***** LOG FILE OPENING *****` marker,
which doubles as an unambiguous "new session started" signal. Fall back to
`Client.txt` only if `LatestClient.txt` is absent.

Format is ASCII with CRLF line endings:

```
2026/08/11 18:50:59 24067703 cffb065b [INFO Client 7272] : You have entered Cartographer's Hideout.
```

Signals we consume, all confirmed present in real data:

| Signal | Line form | Use |
|---|---|---|
| Zone change | `: You have entered <Zone>.` | Primary location |
| Zone change (dup) | `[SCENE] Set Source [<Zone>]` | Cross-check only; ignore |
| Level / class | `: <Char> (<Class>) is now level <N>` | Character identity — **but see below** |
| Death | `: <Char> has been slain.` | Death count — **but see below** |
| Focus | `[WINDOW] Gained focus` / `Lost focus` | Alt-tabbed detection |
| Instance change | `Connecting to instance server at <ip>:<port>` | New map instance |
| Load time | `[LOADING SCREEN] (<Zone>) Duration = <s> seconds` | Zone-entry timestamp |
| AFK / DND | `: AFK mode is now ON/OFF.` | Idle state |
| Session start | `***** LOG FILE OPENING *****` | Reset session state |

### Level-up and death lines are NOT the local player

This is the single biggest trap in the log, and an earlier draft of this spec
got it wrong.

`: <Char> (<Class>) is now level <N>` and `: <Char> has been slain.` fire for
**every player in your area or party**, not just you. The live log contains 15
distinct names in level-up lines and **49** distinct names in death lines.
Picking the most frequent name is not safe either — it happens to work only
because the local player levels most often.

**The log contains no login line, no character-select line, and no other
reliable local-character identifier.** Verified by searching for login,
character-select and session-start markers; none exist.

Therefore:

- The user configures their character name(s) in config (`poe.characters`).
  Only lines matching a configured name update character state or the death
  count.
- With no configured name, character/level/class and deaths are simply
  **omitted** from the presence. The zone-based presence still works fully.
- A "most frequent name this session" heuristic is explicitly rejected: it
  silently publishes a stranger's name as yours, which is worse than showing
  nothing.

### What the log does NOT provide

Stated plainly so the design doesn't promise it: **no map tier, no league, no
build, no character level except at the moment of a level-up, and no class until
the first level-up of that character.** Anything shown for those must be
inferred or persisted, and must degrade gracefully to omitted rather than wrong.

### Privacy: the log contains chat

The log carries whispers and public chat in full:

```
@From <PUPsÍK> VilochkaSDrugom: gl
@To Tauniz: GOLD
```

**The parser must operate on a strict allowlist of the line forms in the table
above.** Chat lines are never parsed, stored, forwarded, or logged. This mirrors
the RuneLite plugin, which subscribes to `onChatMessage` purely to document that
it ignores it. Character name is treated like RuneLite's `account` field —
omitted unless the user opts in.

## Approach

Three options were considered.

**Chosen: an in-app `PoeLogWatcher` component** that tails the log and writes
into `IntegrationContext` under source `poe`, exactly as the RuneLite
interceptor writes under `runelite`.

Rejected — **an external sidecar that POSTs to the local context server**: the
HTTP endpoint exists and would work, but it means a second process to install,
launch, update and debug, for zero benefit when both halves run on the same
machine as the same user. The sidecar shape only earns its keep when the data
source is another machine or another runtime (as with the browser extension).

Rejected — **extending `NamedPipeInterceptor`**: nothing to intercept.

This needs no new plugin interface. A "game" in this codebase is already just a
`source` string plus a `Rule` with `matchIntegrationSource` and
`{{source.field}}` templates, per `docs/INTEGRATIONS.md` §6.

## Components

Each unit is separately testable, following the existing `tests/` pattern where
pure logic is unit-tested with QtTest and no Windows/Discord dependency.

### 1. `PoeLogWatcher` — file tailing only

Owns *reading*, knows nothing about presence.

- Resolves the log path: config override, else the Steam default, else scan
  common install roots. Emits a clear "PoE log not found" state rather than
  failing silently.
- Opens read-only with sharing permitted — the game holds the file open, so the
  handle must not request exclusive access.
- **Polls on a 1 s `QTimer`, seeking to the last byte offset and reading only
  the delta.** `QFileSystemWatcher` is not used: on Windows it coalesces and
  misses rapid appends to a file held open by another process.
- Starts at end-of-file, so a 32 KB backlog never replays as presence history.
- Handles truncation: if the file is now smaller than the stored offset, or a
  `LOG FILE OPENING` marker appears, reset the offset to 0 and clear session
  state.
- Emits one signal per complete line: `lineRead(QString)`. Partial trailing
  lines are buffered until their newline arrives.

### 2. `PoeActivityInferencer` — pure parsing and inference

Mirrors `ActivityInferencer` in the RuneLite plugin: no I/O, no Qt GUI, fully
unit-testable.

- Matches each line against the allowlist of patterns. Everything else is
  discarded immediately — this is the privacy boundary.
- Maintains session state: current zone, zone-entry time, character name/class/
  level, death count, focus, AFK.
- Classifies the zone into an activity via the zone table (below).
- Produces a `PoeContext` value: `{ zone, zoneCategory, activity, character,
  characterClass, level, deaths, zoneEnteredAt, afk }`.

### 3. Zone classification table — data, not code

352 distinct zone names appear in one player's log alone, and GGG adds more
every league. Hardcoding them in C++ guarantees staleness.

Ship `config/poe-zones.json`, loaded at startup:

```json
{
  "patterns": [ { "match": "Hideout$", "category": "hideout" } ],
  "exact":    { "Lioneye's Watch": "town" },
  "ambiguous": ["Dunes", "Strand", "Jungle Valley"]
}
```

Ordered patterns, first match wins, then exact lookups, then `unknown`.
Suffix patterns carry most of the weight (`Hideout$` alone covers a large share
of real traffic — `Cartographer's Hideout` was the single most-visited zone).

**Known limitation, deliberately accepted:** PoE reuses campaign zone names for
Atlas maps — `Dunes`, `Strand` and `Jungle Valley` are both. The log gives no
way to tell them apart. These are classified by their more likely endgame
meaning and listed in `ambiguous`. The presence may occasionally say "Mapping"
during the campaign. The alternative — inferring from whether the previous zone
was a hideout — is fragile and not worth the complexity.

Categories: `hideout`, `town`, `map`, `campaign`, `delve`, `labyrinth`,
`sanctum`, `heist`, `menagerie`, `boss`, `memory`, `simulacrum`, `other`.

The table is checked in at `config/poe-zones.json`, built from the 352 zone names
in the live log and independently verified: **all 352 classified, none dropped,
none left unclassified.**

| Category | Count | | Category | Count |
|---|---|---|---|---|
| `map` | 125 | | `delve` | 10 |
| `campaign` | 107 | | `town` | 7 |
| `hideout` | 44 | | `boss` | 4 |
| `sanctum` | 26 | | `memory` | 4 |
| `other` | 19 | | `labyrinth` | 3 |
| | | | `simulacrum` | 2 |
| | | | `menagerie` | 1 |

The 23 hardest names were resolved against PoEDB with quoted evidence per zone
(`Eye of the Storm` → Sirus arena, `Seething Chyme` → The Infinite Hunger,
`Hive Colony` → Breach-lord fight — all `boss` because they are fragment-gated
instances with no map tier; the four `Distant Memory` variants → `memory`).

Remaining caveats, both honest rather than blocking:

- The `sanctum` bucket (26 zones) was assigned from the Sanctum room-naming
  convention at roughly 70% confidence, not from a verified room list. Worth a
  spot-check.
- `Monastery of the Keepers` is classified `other` on the strength of a search
  snippet quoting a PoE dev post, not a fully fetched page. It is a league
  hub area, not a tiered map.
- `Dunes`, `Jungle Valley` and `Strand` remain genuinely ambiguous: PoE reuses
  these names for both a campaign zone and an Atlas map, and the log cannot
  distinguish them. Classified as `map`.

### 4. Wiring

`AppController` owns a `PoeLogWatcher`, feeds its lines to
`PoeActivityInferencer`, and writes the resulting JSON into `IntegrationContext`
under source `poe` — the same slot the RuneLite interceptor uses. Everything
downstream (rules, templates, privacy, publishing) is unchanged.

Freshness: the watcher re-stamps via `IntegrationContext::refresh("poe")` on a
timer while the PoE process is alive, because zone changes can be many minutes
apart and the payload would otherwise expire mid-map. This is the same
keep-alive the RuneLite path needs.

## Presence output

Display follows the house Option A convention (`StatusDisplayType::Details`):

| Field | Content | Example |
|---|---|---|
| name | Game | `Path of Exile` |
| details | What | `Mapping` / `In Hideout` / `Running Delve` |
| state | Where + who | `Dunes — Level 92 Occultist` |
| large image | Zone category icon | `poe_map` |
| small image | Game icon | `poe` |
| timestamp | Zone entry time | elapsed in zone |

Character name is omitted unless opted in; class and level are shown when known.
Before the first level-up of a session the state degrades to just the zone.

Session stats (deaths, zones cleared) are held in memory and reset on
`LOG FILE OPENING`. They are surfaced as template variables so the user chooses
whether to show them, rather than being forced into the default layout.

## Icons

New asset keys needed: `poe`, plus one per category shown
(`poe_hideout`, `poe_map`, `poe_town`, `poe_campaign`, `poe_delve`,
`poe_labyrinth`). These follow the existing external-URL convention and, per the
fix shipped alongside this spec, must be square PNGs of at least 512×512 or
Discord renders a white box.

This overlaps the separate icon backlog drawn from `app-coverage.log`, which
already lists `PathOfExileSteam.exe`, `Path of Building.exe` and
`Awakened PoE Trade.exe` as uncovered.

## Testing

Following `tests/` conventions, one QtTest executable per unit:

- `test_poe_inferencer` — feed recorded log lines, assert the inferred context.
  Includes an explicit case asserting that chat/whisper lines produce **no**
  state change and are never echoed, since that is the privacy guarantee.
- `test_poe_zones` — every zone in the shipped table resolves to a category;
  patterns are ordered correctly; unknown zones degrade to `unknown`.
- `PoeLogWatcher` gets a test against a temp file exercising append, partial
  trailing line, and truncation-resets-offset.

A real `LatestClient.txt` sample is checked in as a fixture, scrubbed of chat
lines and character names.

## Out of scope

- Path of Exile 2. Same log format and the install is present on this machine,
  but the zone table is entirely different; it is a follow-up, not a variant.
- Map tier, league and build. Not derivable from the log; would need the
  official API or client memory reading, both of which are a different project
  with different risk.
- Anything that reads game memory. Log tailing is passive and TOS-safe; memory
  reading is not, and OmniPresence's whole positioning is "not a selfbot".
