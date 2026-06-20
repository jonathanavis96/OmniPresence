# OmniPresence Terminal Integration

A PowerShell module for Windows Terminal / PowerShell 5.1+ that reports sanitized terminal context to OmniPresence after each command, enabling Discord Rich Presence to reflect what you're working on.

---

## What's reported

| Field | Source |
|---|---|
| `cwd` | Current working directory (`(Get-Location).Path`) |
| `repo` | Git repository name (`git rev-parse --show-toplevel`) |
| `branch` | Current branch (`git rev-parse --abbrev-ref HEAD`) |
| `command_summary` | Sanitized short summary derived from the last command |
| `privacy_safe` | Always `true` — confirms sanitization ran |

**Example payload:**
```json
{
  "source": "terminal",
  "cwd": "C:\\code\\ArchiveBox",
  "repo": "ArchiveBox",
  "branch": "main",
  "command_summary": "Running archivebox add",
  "privacy_safe": true
}
```

---

## Sanitization rules

The `command_summary` is **derived**, never the raw command line:

1. The verb (first token) is kept.
2. Up to two short positional arguments are included (subcommands, filenames).
3. **Anything matching these patterns is silently dropped:**
   - Flags like `--token=`, `--api-key=`, `--password=`, `--secret=`, `--auth=`, etc.
   - Long base64 blobs (`[A-Za-z0-9+/]{32,}`)
   - OpenAI-style keys (`sk-...`), GitHub PATs (`ghp_...`), Slack tokens (`xox...`)
   - Long hex strings (40+ hex chars)
   - Files with sensitive extensions (`.pem`, `.key`, `.p12`, `.pfx`, `.env`, `.secret`)
4. Full command arguments, file contents, and environment variables are never forwarded.

---

## Endpoint

**POST** `http://127.0.0.1:47831/integrations/terminal/context`

Traffic never leaves `127.0.0.1`. If OmniPresence is not running, the POST fails silently — your prompt is never blocked.

---

## Install

### 1. Find your PowerShell profile

```powershell
$PROFILE
# e.g. C:\Users\you\Documents\PowerShell\Microsoft.PowerShell_profile.ps1
```

If it doesn't exist yet:
```powershell
New-Item -Path $PROFILE -ItemType File -Force
```

### 2. Append the profile snippet

Copy the contents of `profile-snippet.ps1` and paste them at the end of your `$PROFILE`, adjusting `$OmniPresenceModulePath` to match where you cloned OmniPresence.

```powershell
# Example — run in PowerShell:
$snippet = Get-Content "C:\code\OmniPresence\integrations\terminal\profile-snippet.ps1" -Raw
Add-Content -Path $PROFILE -Value "`n$snippet"
```

### 3. Reload your profile

```powershell
. $PROFILE
```

You should see `[OmniPresence] Terminal hook registered.` in dark grey.

---

## Uninstall

```powershell
Unregister-OmniPresenceHook
```

Or simply remove the snippet from `$PROFILE` and restart PowerShell.

---

## Configuration

After importing the module but before calling `Register-OmniPresenceHook`, override defaults in `$script:Config`:

```powershell
# Inside your $PROFILE, after Import-Module:
$script:Config.EndpointUrl = 'http://127.0.0.1:47831/integrations/terminal/context'
$script:Config.DebounceMs  = 5000   # only POST every 5 s minimum
$script:Config.Enabled     = $true  # set $false to pause without uninstalling
```

---

## Compatibility

- PowerShell 5.1 (Windows PowerShell) and PowerShell 7+
- Works with Oh-My-Posh, Starship, and other custom `prompt` functions — the original prompt is preserved and still runs.
- PSReadLine is used when available to capture commands before the prompt fires; gracefully degrades without it.

---

## Module exports

| Function | Purpose |
|---|---|
| `Register-OmniPresenceHook` | Wire the prompt hook |
| `Unregister-OmniPresenceHook` | Remove the hook |
| `Get-OmniContext` | Gather context (cwd/repo/branch/summary) |
| `Send-OmniContext` | POST to OmniPresence (fire-and-forget) |
| `ConvertTo-SafeCommandSummary` | Sanitize a raw command string → short summary |
