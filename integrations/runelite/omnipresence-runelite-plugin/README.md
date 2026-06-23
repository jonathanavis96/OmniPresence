# OmniPresence RuneLite Plugin

> ## ⚠️ DEPRECATED — NOT THE METHOD IN USE (2026-06-23)
> OmniPresence **no longer uses this custom plugin / HTTP-POST method**. It now runs
> on the **real RuneLite client** and intercepts RuneLite's **built-in Discord
> plugin** over the `\\.\pipe\discord-ipc-0` named pipe (see
> `app/src/NamedPipeInterceptor.cpp` and `docs/DECISIONS.md`).
>
> This means we **no longer sideload a standalone developer Java RuneLite client**
> and **no longer need** `--developer-mode` / `--insecure-write-credentials` or a
> `~/.runelite/credentials.properties` file.
>
> **The code in this directory is retained on purpose** (reference / possible future
> use) but is not part of the live pipeline. Do not delete it.

A RuneLite external plugin that infers your current OSRS activity from in-game events and reports it to the **OmniPresence** context server running locally on your PC. OmniPresence then uses this context to drive Discord Rich Presence.

> **Status: unbuilt scaffold.** The code compiles against the RuneLite API but has not been built or tested against a live client on this machine (no JDK present in the build environment). It follows the standard RuneLite external-plugin layout and should build cleanly with a JDK 11+ and Gradle.

---

## What it tracks

| Field | Source |
|---|---|
| `activity` | Inferred from NPC interaction + animation + recent XP |
| `target` | Interacting NPC name (e.g. `Skeletal Wyvern`) |
| `skill` | Skill that received XP most recently |
| `location` | Region ID mapped to a human-readable name |
| `confidence` | 0.0–1.0 score for the inference |
| `account` | **Disabled by default** — opt in per config |

Activities inferred include: Training Slayer, Bossing, In Combat, Woodcutting, Fishing, Mining, Cooking, Fletching, Crafting, Smithing, Herblore, Banking, At the Grand Exchange, Idle, Logged out.

---

## Endpoint & payload

**POST** `http://127.0.0.1:47831/integrations/runelite/context`

```json
{
  "source": "runelite",
  "game": "Old School RuneScape",
  "account": "WongBater",
  "activity": "Training Slayer",
  "target": "Skeletal Wyvern",
  "skill": "Slayer",
  "location": "Asgarnian Ice Dungeon",
  "confidence": 0.95,
  "timestamp": "2026-06-20T12:34:56Z"
}
```

The `account` field is omitted unless **Share account name** is enabled.

---

## Privacy

- **Account name off by default.** Enable it in the plugin config only if you want it in your Rich Presence.
- **No chat content is ever forwarded.** The `ChatMessage` subscription exists solely to detect system-level game-state transitions (e.g. login messages); raw message text is never stored or sent.
- **Localhost only.** The endpoint is `127.0.0.1`; no data leaves your machine.
- **No external dependencies.** OkHttp and Gson are provided by the RuneLite client at runtime.

---

## Build

Requires JDK 11+ and internet access to fetch RuneLite Maven artifacts.

```bash
cd integrations/runelite/omnipresence-runelite-plugin
./gradlew shadowJar
```

Output: `build/libs/omnipresence-runelite-plugin-1.0.0.jar`

---

## Install (sideload as external plugin)

1. Build the shadow JAR above.
2. In RuneLite: **Configuration → Plugin Hub → ⚙ → Load plugin from file**
   (or use the `--plugin-dir` JVM flag pointing to the JAR's parent directory).
3. Enable **OmniPresence** in the Plugin Hub list.
4. Configure the endpoint URL if you changed OmniPresence's default port.

---

## Config options

| Setting | Default | Description |
|---|---|---|
| Enable OmniPresence | `true` | Toggle without uninstalling |
| Endpoint URL | `http://127.0.0.1:47831/integrations/runelite/context` | OmniPresence server |
| Share account name | `false` | Include OSRS character name in payload |
| Minimum confidence | `60` | Skip inferences below this % |
| Poll interval | `5 s` | Minimum seconds between context updates |

---

## Source layout

```
src/main/java/com/omnipresence/
  OmniPresencePlugin.java       ← @PluginDescriptor entry point
  OmniPresenceConfig.java       ← @ConfigGroup settings
  ActivityInferencer.java       ← pure inference logic (unit-testable)
  ContextPublisher.java         ← OkHttp POST on background thread
  dto/RuneLiteContext.java      ← payload POJO

src/test/java/com/omnipresence/
  ActivityInferencerTest.java   ← JUnit 4 tests for the inferencer
```
