# OmniPresence — Discord Setup Guide

This guide covers everything needed to connect OmniPresence to Discord: creating the application, configuring the Social SDK, uploading Rich Presence assets, and understanding how the display fields map to what Discord shows.

---

## 1. Create a Discord Application

1. Go to the [Discord Developer Portal](https://discord.com/developers/applications).
2. Click **New Application**.
3. Name it (e.g. `OmniPresence`). This name is NOT what appears in Rich Presence — the name shown in Discord is controlled by the **Activity name** field your rules produce.
4. On the application page, copy the **Application ID** (also called Client ID). You will need this.

---

## 2. Enable Rich Presence

1. In the left sidebar of your application, click **Rich Presence**.
2. No additional toggle is needed — Rich Presence is available to all applications by default.
3. Note: Discord displays Rich Presence only when the local Discord desktop client is running and the Social SDK is successfully connected to it.

---

## 3. Upload Rich Presence Assets

Discord Rich Presence images (`large_image`, `small_image`) are referenced by **asset key names** that you define. The images themselves must be uploaded in the Developer Portal.

**Steps:**

1. In your application, go to **Rich Presence → Art Assets**.
2. Click **Add Image(s)**.
3. Upload an image and give it a key name (lowercase, no spaces — e.g. `osrs`, `code`, `terminal`).
4. Save.

The key name you enter here is what you put in the `large_image_key` / `small_image_key` fields in OmniPresence rules.

### Recommended Asset Keys

| Key | Used for | Suggested image |
|---|---|---|
| `osrs` | Old School RuneScape (RuneLite) | OSRS logo / character |
| `code` | Code editor / terminal development | Code brackets or VS Code icon |
| `terminal` | Terminal / command line (non-dev) | Terminal prompt icon |
| `youtube` | YouTube activity | YouTube play button |
| `reddit` | Reddit activity | Reddit alien logo |
| `pihole` | Pi-hole dashboard | Pi-hole logo or network icon |
| `dashboard` | Generic custom dashboard | Grid / monitor icon |

> **Note:** Discord has image size and format requirements (PNG recommended, minimum 512×512 px). Check the Rich Presence documentation for current limits.

---

## 4. Record Your Application ID

Open `config/omnipresence.example.json` (or create your `config/omnipresence.json`) and set:

```json
{
  "discord": {
    "application_id": "YOUR_APPLICATION_ID_HERE"
  }
}
```

The Application ID is a large integer (e.g. `1234567890123456789`). Do not commit your real `config/omnipresence.json` — it is gitignored. Only `omnipresence.example.json` is committed.

---

## 5. Discord Social SDK — Setup

OmniPresence uses the [Discord Social SDK](https://docs.discord.com/developers/discord-social-sdk/getting-started/using-c%2B%2B) (C++) to set Rich Presence. This is the official, Terms of Service-compliant path.

**References:**
- [Getting started with C++](https://docs.discord.com/developers/discord-social-sdk/getting-started/using-c%2B%2B)
- [Setting Rich Presence](https://docs.discord.com/developers/discord-social-sdk/development-guides/setting-rich-presence)
- [Platform compatibility](https://docs.discord.com/developers/discord-social-sdk/core-concepts/platform-compatibility)
- [Release notes](https://discord.com/developers/docs/social-sdk/release_notes.html)

### 5.1 Download the SDK

The Discord Social SDK is not committed to this repository (it would be a large binary). A setup script will fetch it:

```powershell
# On Windows (PowerShell):
scripts\setup.ps1
```

Until that script exists, download the SDK manually from the Discord Developer Portal (Developer Resources → Social SDK) and extract it to:

```
third_party/discord_social_sdk/
```

The `third_party/discord_social_sdk/` directory is gitignored.

### 5.2 CMake Integration

The CMakeLists.txt will add the SDK as a library target. Approximate pattern:

```cmake
add_library(discord_social_sdk SHARED IMPORTED)
set_target_properties(discord_social_sdk PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/third_party/discord_social_sdk/lib/discord_social_sdk.dll"
    IMPORTED_IMPLIB   "${CMAKE_SOURCE_DIR}/third_party/discord_social_sdk/lib/discord_social_sdk.lib"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/third_party/discord_social_sdk/include"
)
target_link_libraries(omnipresence_core PRIVATE discord_social_sdk)
```

Exact paths depend on the SDK archive layout — adjust to match the downloaded version.

### 5.3 Runtime Requirement

The Discord desktop client must be running for the Social SDK to connect. OmniPresence gracefully handles the case where Discord is not running: the `PresenceManager` retries connection on a timer and shows "Disconnected" in the Dashboard until it succeeds.

---

## 6. StatusDisplayType::Details

By default, Discord shows the activity name in the member list (e.g. "Playing Old School RuneScape"). OmniPresence sets `StatusDisplayType::Details` so that the **details** field is displayed in the member list instead.

This produces cleaner output:

| Without `StatusDisplayType::Details` | With `StatusDisplayType::Details` |
|---|---|
| "Playing Old School RuneScape" | "Training Slayer" |
| "Playing Code" | "Working on ArchiveBox" |
| "Playing Computer" | "Working privately" |

**Reference:** [discordpp::Activity class](https://discord.com/developers/docs/social-sdk/classdiscordpp_1_1Activity.html)

In C++, set this on the `discordpp::Activity` object before passing it to the client:

```cpp
activity.SetStatusDisplayType(discordpp::StatusDisplayType::Details);
```

---

## 7. Rich Presence Fields Reference

| Discord field | OmniPresence mapping | Example |
|---|---|---|
| Activity type | Rule `activity_type` | Playing / Watching |
| Activity name | Rule `activity_name` template | "Old School RuneScape" |
| Details | Rule `details` template | "Training Slayer" |
| State | Rule `state` template | "Attacking Skeletal Wyvern" |
| Large image | Rule `large_image_key` | `osrs` |
| Large image tooltip | Rule `large_image_text` | "Old School RuneScape" |
| Small image | Rule `small_image_key` | `slayer` |
| Small image tooltip | Rule `small_image_text` | "Slayer" |
| Start timestamp | Rule `timestamp_mode` | Since activity category change |

---

## 8. Testing Your Setup

Once the core is built and running:

1. Open Discord (desktop client, not browser).
2. Launch OmniPresence — the Dashboard should show "Connected" within a few seconds.
3. Your Discord profile should show the Rich Presence matching the current foreground window.
4. Use the Dashboard's live preview to see the generated presence before it reaches Discord.

If the Dashboard shows "Disconnected":
- Confirm the Discord desktop client is running.
- Confirm the Application ID in `config/omnipresence.json` is correct.
- Check that the Discord Social SDK DLL is present in `third_party/discord_social_sdk/lib/`.
- Check that `discord_social_sdk.dll` is alongside the OmniPresence executable at runtime (copy it during build or add to PATH).

## Rich Presence art assets (why your icon shows the app logo)

Discord only renders a presence image if its `largeImageKey` / `smallImageKey`
exists under **Developer Portal → your app → Rich Presence → Art Assets**. For any
key it doesn't recognise, Discord silently falls back to the **application icon**
(the OmniPresence "OP" logo). This is not an app bug.

OmniPresence stores generated/imported tiles locally (`%APPDATA%\OmniPresence\art\<key>.png`)
so the in-app preview is correct immediately, but **there is no API to upload art
assets** — it's a manual step. When you Generate or Add-photo, the app reveals the
PNG in Explorer and opens the Art Assets page; drag the file in, name the asset
**exactly** the key shown in the upload hint (e.g. `code`, `youtube`), and Save.
Allow a minute for Discord to propagate before the icon appears on your presence.
