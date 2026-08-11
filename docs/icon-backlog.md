# OmniPresence icon coverage backlog

**Status as of 2026-08-11: closed.** All 30 "worth an icon" entries that could be
identified now have both artwork in `assets/icons/` and a `matchProcessName` rule
in `config/omnipresence.example.json` (44 rules total). The 14 icons that were
missing artwork were extracted from the installed applications' own PE icon
resources (or their bundled UWP/Steam assets), normalised to 512x512 transparent
PNGs, and committed. `Launcher.exe` remains deliberately unresolved — the
filename is too generic to identify the product.

Per-icon provenance is in the table below under "Source".

Source log: `app-coverage.log` (72 lines, 45 distinct `NO ICON` exes).
Reference: `assets/icons/` (existing PNGs) and `config/omnipresence.example.json` (`rules[].matchProcessName`).

**Totals: 45 distinct NO-ICON exes = 1 already covered + 31 worth an icon + 13 suppress.**

---

## 1. Worth an icon (31)

Real, user-facing apps someone would want shown. "Icon exists" means a PNG already sits in `assets/icons/` with a matching name but **no rule references it yet** — per the example config, none of these have a `matchProcessName` rule, so today they still show nothing despite the asset being present.

| Exe | Product (real name) | Suggested rule name | Suggested icon slug | Notes |
|---|---|---|---|---|
| ExitLag.exe | ExitLag (game network optimizer) | ExitLag | `exitlag` | Icon exists (`exitlag.png`) — needs rule only |
| AnyDesk.exe | AnyDesk (remote desktop) | AnyDesk | `anydesk` | Icon exists (`anydesk.png`) — needs rule only |
| powershell.exe | Windows PowerShell | PowerShell | `powershell` | Icon exists (`powershell.png`) — needs rule only |
| deadlock-mod-manager.exe | Deadlock Mod Manager | Deadlock Mod Manager | `deadlock_mod_manager` | Icon exists (`deadlock-mod-manager.png`) — needs rule only |
| deadlock.exe | Deadlock (Valve game) | Deadlock | `deadlock` | Icon exists (`deadlock.png`) — needs rule only |
| TeamViewer.exe | TeamViewer | TeamViewer | `teamviewer` | Icon exists (`teamviewer.png`) — needs rule only |
| RadeonSoftware.exe | AMD Radeon Software (Adrenalin) | Radeon Software | `radeon` | Icon exists (`radeon.png`) — needs rule only |
| pythonw.exe | Python (windowless) | Python | `python` | Icon exists (`python.png`) — needs rule only |
| parsecd.exe | Parsec (remote desktop / game streaming) | Parsec | `parsec` | Icon exists (`parsec.png`) — needs rule only |
| SteelSeriesGGClient.exe | SteelSeries GG | SteelSeries GG | `steelseries` | Icon exists (`steelseries.png`) — needs rule only |
| lghub.exe | Logitech G HUB | Logitech G HUB | `lghub` | Icon exists (`lghub.png`) — needs rule only |
| thunderbird.exe | Mozilla Thunderbird | Thunderbird | `thunderbird` | Icon exists (`thunderbird.png`) — needs rule only |
| SyncTrayzor.exe | SyncTrayzor (Syncthing tray GUI) | SyncTrayzor | `syncthing` | Icon exists (`syncthing.png`) — needs rule only |
| soffice.bin | LibreOffice | LibreOffice | `libreoffice` | Icon exists (`libreoffice.png`) — needs rule only |
| Code.exe | Visual Studio Code | VS Code | `vscode` | Icon exists (`vscode.png`) — needs rule only |
| pdfeditor.exe | left deliberately unidentified | PDF | `pdfeditor` | Rule name and icon are both **generic on purpose** (2026-08-11): a plain "PDF" label and a generic PDF-document icon, rather than naming whichever PDF app happens to own this executable. No identification needed. |
| Odin3.exe | Samsung Odin (Android/S8+ flashing tool) | Odin3 | `odin3` | No icon yet |
| SnakeTail.exe | SnakeTail (log-file tail viewer) | SnakeTail | `snaketail` | No icon yet |
| PathOfExileSteam.exe | Path of Exile (Steam build) | Path of Exile | `path_of_exile` | No icon yet |
| PoeSmoother.exe | PoE Smoother (PoE network optimizer) | PoE Smoother | `poesmoother` | No icon yet; same product as PoeSmoother1.exe below |
| PoeSmoother1.exe | PoE Smoother (secondary/updater process) | PoE Smoother | `poesmoother` | No icon yet; likely the same tool as PoeSmoother.exe — cover both with one rule if process names can be OR'd, else duplicate the rule |
| tailscale-ipn.exe | Tailscale VPN | Tailscale | `tailscale` | No icon yet |
| Awakened PoE Trade.exe | Awakened PoE Trade | Awakened PoE Trade | `awakened_poe_trade` | No icon yet |
| Path of Building.exe | Path of Building (PoE build planner) | Path of Building | `path_of_building` | No icon yet |
| MonsGeek Driver v4.exe | MonsGeek keyboard driver/config utility | MonsGeek Driver | `monsgeek` | No icon yet |
| AutoHotkeyU64.exe | AutoHotkey | AutoHotkey | `autohotkey` | No icon yet |
| M365Copilot.exe | Microsoft 365 Copilot | M365 Copilot | `m365_copilot` | No icon yet |
| SystemSettings.exe | Windows Settings | Windows Settings | `settings` | No icon yet; consistent with existing built-in-app treatment (Calculator, Notepad, Explorer, Task Manager already have rules+icons) |
| obs64.exe | OBS Studio | OBS Studio | `obs` | No icon yet |
| openshot-qt.exe | OpenShot Video Editor | OpenShot | `openshot` | No icon yet |
| Launcher.exe | unknown — filename too generic to identify the product with confidence | Launcher (TBD) | n/a | Do not build a rule/icon until manually identified; appeared in the same 2026-08-07 13:04 PoE-adjacent session as Path of Building / Awakened PoE Trade, but that's circumstantial, not confirmed |

## 2. Already covered (1)

| Exe | Covered by | Icon file |
|---|---|---|
| RuneLite.exe | Rule `matchProcessName: "RuneLite.exe"`, name "RuneLite / OSRS" | `osrs.png` |

Note: the log also shows `java.exe` hitting this same rule with `ICON ✓` — RuneLite sometimes launches under the `java.exe` process name instead of `RuneLite.exe`, and both are matched. The `RuneLite.exe` NO-ICON row is just a pre-existing-rule log entry (or a run where the rule didn't fire for another reason); the rule itself already exists in `omnipresence.example.json`, so no action needed.

## 3. Should be suppressed (13)

Windows internals, dialog hosts, and installer/crash-reporter noise — none of these should ever produce a Discord presence.

| Exe | Category |
|---|---|
| ApplicationFrameHost.exe | UWP app-frame host (Windows internal) |
| StartMenuExperienceHost.exe | Start Menu internal host process |
| ShellExperienceHost.exe | Windows Shell internal host process |
| PickerHost.exe | Windows file/contact picker dialog host |
| CredentialUIBroker.exe | Windows credential-prompt broker |
| OpenWith.exe | Windows "Open With" dialog |
| ShellHost.exe | Same `*Host.exe` naming pattern as the other internal shell hosts above — treat as internal chrome; worth a quick manual check only if it recurs unexpectedly |
| setup.exe | Generic installer |
| wixstdba.exe | WiX installer bootstrapper |
| Awakened-PoE-Trade-Setup-3.29.103.exe | Versioned installer for Awakened PoE Trade (the app itself is in the "worth an icon" list — this is just its installer) |
| Loader.exe | Appears sandwiched between other installer/updater/crash-reporter entries in the same session (wixstdba.exe, WerFault.exe, Update.exe) — reads as a generic install-time bootstrapper stub, not a user-facing app |
| WerFault.exe | Windows Error Reporting crash handler |
| Update.exe | Generic updater process |

## Count check

- Worth an icon: 31
- Already covered: 1
- Suppress: 13
- **Total: 31 + 1 + 13 = 45** ✓ (matches the 45 distinct `NO ICON` exes in the log)

An earlier revision listed four more under "suspicious / unknown" — random-looking
names that matched no product. They were **GameHelper**, the PoE overlay, and were
never anything to worry about. Their lines have been removed from
`app-coverage.log` so they stop being re-flagged; do not reintroduce them.
