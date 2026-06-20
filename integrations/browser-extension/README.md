# OmniPresence Browser Bridge

A Manifest V3 browser extension (Chrome / Edge) that reports your active browsing context to the local [OmniPresence](https://github.com/grafe/OmniPresence) service for Discord Rich Presence.

## Privacy model

| What gets sent | When |
|---|---|
| Domain (e.g. `github.com`) | Always (when enabled) |
| Category (e.g. `Browsing GitHub`) | Always (when enabled) |
| Dashboard label (e.g. `Checking Pi-hole`) | When a local dashboard is detected |
| Exact page title | **Only** if the domain is in your whitelist |

No data is ever sent to any external server. All traffic goes exclusively to `http://127.0.0.1:47831` — your local OmniPresence instance.

## Loading in Chrome or Edge

1. Open `chrome://extensions` (Chrome) or `edge://extensions` (Edge).
2. Enable **Developer mode** (toggle in the top-right corner).
3. Click **Load unpacked**.
4. Select the `integrations/browser-extension/` folder from this repo.
5. The extension icon should appear in your toolbar.

> **Note:** The extension requires PNG icons at `icons/icon16.png`, `icons/icon48.png`, and `icons/icon128.png`. You can use any 16×16, 48×48, and 128×128 PNG images (or generate placeholders) — the extension will load without them but Chrome may show a warning.

## Payload schema

POST `http://127.0.0.1:47831/integrations/browser/context`

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

| Field | Type | Description |
|---|---|---|
| `source` | `"browser"` | Always `"browser"` |
| `browser` | string | `"chrome"`, `"edge"`, `"firefox"`, `"safari"`, or `"unknown"` |
| `domain` | string | Bare domain, no `www.` prefix |
| `category` | string | Human-readable activity label |
| `title_allowed` | boolean | Whether the domain is whitelisted for title sharing |
| `safe_title` | string \| null | Exact page title (trimmed), or `null` if not whitelisted |
| `dashboard_label` | string \| null | `"Checking Pi-hole"`, `"Router Dashboard"`, `"Local Dashboard"`, `"Custom Dashboard"`, or `null` |

## Built-in domain categories

| Domain | Category |
|---|---|
| youtube.com | Watching YouTube |
| reddit.com | Browsing Reddit |
| github.com | Browsing GitHub |
| chatgpt.com / chat.openai.com | Using ChatGPT |
| twitter.com / x.com | Browsing Twitter / X |
| linkedin.com | Browsing LinkedIn |
| stackoverflow.com | Reading Stack Overflow |
| notion.so | Working in Notion |
| figma.com | Designing in Figma |
| (unknown) | Browsing privately |

ChatGPT exact titles are **off by default** — add `chatgpt.com` to your whitelist only if you want titles reported.

## Dashboard detection

| Pattern | Label |
|---|---|
| `pi.hole` hostname | Checking Pi-hole |
| LAN IP + `/admin` path | Checking Pi-hole / Custom Dashboard |
| Known router IPs (192.168.0.1, etc.) | Router Dashboard |
| `localhost:PORT` | Local Dashboard |

## Whitelist management

Open the extension popup (toolbar icon) or the full Options page (`chrome://extensions` → Details → Extension options) to add or remove domains from the title whitelist.

## Files

```
browser-extension/
├── manifest.json      MV3 manifest
├── background.js      Service worker — event listeners, debounce, POST
├── categories.js      Domain→category map + dashboard detection (ES module)
├── popup.html/.js     Toolbar popup UI
├── options.html/.js   Full options page
└── icons/             PNG icons (16, 48, 128 — provide your own)
```
