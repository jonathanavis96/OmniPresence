# app/resources/

## UI assets (QML)

Place icon and image files used by the QML UI here, for example:
- `icons/omnipresence.ico` — application tray icon (Windows .ico, 256×256 recommended)
- `icons/omnipresence.png` — fallback PNG icon

Add entries to `app/CMakeLists.txt` under `qt_add_qml_module(...  RESOURCES ...)` to bundle them
into the Qt resource system, then reference them in QML as `qrc:/OmniPresence/resources/<file>`.

## Discord Rich Presence art assets

Discord Rich Presence image assets are **NOT bundled with the application binary**.
They must be uploaded directly to the Discord Developer Portal:

1. Open https://discord.com/developers/applications
2. Select your application.
3. Go to **Rich Presence → Art Assets**.
4. Upload images (PNG, recommended 512×512 or 1024×1024).
5. Note the **key name** assigned to each asset.
6. Enter those key names in the **Asset Manager** screen inside OmniPresence, or
   directly in the `largeImageKey` / `smallImageKey` fields of your rules.

The key names must match exactly (case-sensitive) what you enter in the Discord portal.

### Suggested asset keys

| Key name      | Suggested artwork                        |
|---------------|------------------------------------------|
| `osrs`        | OSRS logo or game screenshot             |
| `vscode`      | VS Code icon                             |
| `terminal`    | Terminal / command prompt icon           |
| `youtube`     | YouTube logo                             |
| `reddit`      | Reddit alien / logo                      |
| `pihole`      | Pi-hole logo                             |
| `dashboard`   | Generic grid / dashboard icon            |
| `discord`     | Discord logo (check ToS for usage)       |
| `computer`    | Computer icon (used for private fallback)|
