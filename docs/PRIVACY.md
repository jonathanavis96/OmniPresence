# OmniPresence — Privacy Model

OmniPresence is designed with privacy as the default. This document defines what data is used, how it flows, what is never exposed, and what the user controls.

---

## 1. Data Flow — Localhost Only

```
RuneLite plugin  ──┐
Browser extension ─┼──▶  POST http://127.0.0.1:47831/integrations/<source>/context
Terminal hook    ──┤
VS Code extension ─┘
                          │
                          ▼
                   OmniPresence core
                   (rule engine + template renderer)
                          │
                          ▼
                   Discord Social SDK
                   (local Discord client only)
```

- All integration traffic is to `127.0.0.1` (loopback). The context server **never binds to `0.0.0.0`** and must reject any request not originating from localhost.
- The Discord Social SDK communicates with the local Discord desktop client via a local IPC mechanism (Discord-managed). OmniPresence does not make any outbound network connections.
- No data leaves the machine. No telemetry. No analytics. No remote logging.

---

## 2. Browser Titles — Private by Default

The browser extension tracks the **domain only**. Page titles are **never sent** unless explicitly permitted by a whitelist toggle.

### Default behaviour

| Domain | What is sent | What is never sent |
|---|---|---|
| youtube.com | `"category": "Watching YouTube"` | Video title |
| reddit.com | `"category": "Browsing Reddit"` | Post title, subreddit |
| github.com | `"category": "Browsing GitHub"` | Repo name, file path, PR title |
| chatgpt.com | `"category": "Using ChatGPT"` | Conversation title or content |
| pi.hole / local dash | `"dashboard_label": "Checking Pi-hole"` | Any page content |
| Unknown domain | `"category": "Browsing privately"` | Domain name |

### Whitelist toggles (opt-in per user)

| Toggle | Default | What becomes visible when enabled |
|---|---|---|
| Allow exact localhost dashboard titles | Off | Local dashboard page title in state field |
| Allow exact YouTube video titles | Off | Video title in details field |
| Allow exact GitHub repo names | Off | Repo name in details field |
| Allow exact ChatGPT conversation titles | **Off — intentionally never defaulted on** | Conversation title |

ChatGPT conversation titles must never be enabled by default because they frequently contain sensitive personal context.

---

## 3. Terminal Command Sanitization

The terminal hook (`integrations/terminal/`) runs a sanitization pass before forwarding any command summary.

### What is stripped / never forwarded

- Any argument that looks like a credential: matches `--token`, `--key`, `--password`, `--secret`, `--api-key`, `-p <value>`, etc.
- Any argument that is a secret-shaped string (length > 20, high entropy, alphanumeric+symbols).
- Full paths to private files (paths outside standard working directories).
- Full command history — only the **most recent command** (sanitized) is forwarded.
- Any content of piped input (e.g. `echo "secret" | my-tool` is reduced to `Running my-tool`).

### Sanitized example

| Raw command | Forwarded as `command_summary` |
|---|---|
| `archivebox add https://example.com` | `Running archivebox add` |
| `git commit -m "fix auth bug"` | `Running git commit` |
| `curl -H "Authorization: Bearer sk-abc123" api.example.com` | `Running curl` |
| `export DISCORD_TOKEN=abc123` | *(not forwarded — credential export)* |

### `privacy_safe` field

The payload includes `"privacy_safe": true` only when the sanitization routine confirms no sensitive content is present. If uncertain, the hook omits `command_summary` entirely and sets `"privacy_safe": false`. The rule engine treats `privacy_safe: false` context as unavailable.

---

## 4. Exact File Names — Private by Default

Neither the VS Code extension nor the terminal hook exposes exact file names unless explicitly permitted.

- VS Code: `"file_title_allowed": false` by default. The active file name is never included in the payload unless the user enables `omniPresence.allowFileTitle` in VS Code settings.
- Terminal: Only the **repository name** (top-level git folder name) is forwarded, not the path of files being edited.

---

## 5. Window Titles

The raw window title is read by the Win32 watcher (`GetWindowText`). It is used internally for rule matching but is:

- **Not sent to Discord** unless the matching rule explicitly uses `{{window.title}}` in a template.
- **Not logged to disk** in normal operation.
- Treated with the same caution as browser titles: if a rule uses `{{window.title}}` in a public-facing template, the user should be warned in the GUI that raw window titles may contain document names.

---

## 6. Global Pause / Private Mode

Two user-controlled overrides suppress all Rich Presence updates:

### Global Pause

- OmniPresence stops calling the Discord SDK entirely.
- Discord Rich Presence falls back to whatever state Discord itself sets (typically nothing / no activity shown).
- Useful for temporary suppression (e.g. screen sharing, confidential work).

### Private Mode

- OmniPresence continues running and monitoring.
- All rules are bypassed; the **private fallback** presence is emitted instead:

```
Activity name : Computer
Details       : Working privately
State         : Private
```

- The user appears active on Discord without revealing which application is in focus.

Both modes are accessible from the system tray context menu (single click) and from the Dashboard screen.

---

## 7. What Is NEVER Logged

The following are never written to any log file, config file, or sent anywhere:

- Discord user tokens or OAuth tokens
- Browser page titles (unless user explicitly enables a whitelist toggle — and even then, titles are only forwarded to the local context server, not logged)
- Terminal commands containing credential flags or high-entropy arguments
- Private file paths or file contents
- Full command history
- Private URLs (passwords in URLs, session tokens in query strings)
- RuneLite chat message content
- Any secret, API key, or password that appears in a terminal or editor window

---

## 8. RuneLite Privacy

The RuneLite plugin follows the same principle:

- Account name (`"account"`) is optional and can be disabled in the plugin configuration.
- Chat message events (`ChatMessage`) are subscribed to only for activity inference (e.g. detecting a completed slayer task from the game message). Chat content is never forwarded to OmniPresence.
- No player interaction history, bank contents, or inventory data is forwarded.

---

## 9. No Discord User Token

OmniPresence does not, and will never, use a Discord user token. Only the official Discord Social SDK is used, which authenticates via the locally running Discord desktop client. This means:

- OmniPresence can only set Rich Presence, not send messages, read channels, or perform any other Discord action.
- There is no risk of account termination from Terms of Service violations related to self-bots.
