#Requires -Version 5.1
<#
.SYNOPSIS
    OmniPresence terminal integration for PowerShell / Windows Terminal.

.DESCRIPTION
    Reports sanitized terminal context (cwd, git repo/branch, command summary)
    to OmniPresence's localhost context server after each command, enabling
    Discord Rich Presence to reflect what you are working on in the terminal.

    Privacy guarantees:
      - Raw command lines are NEVER sent.  A sanitized summary is derived instead.
      - Arguments that look like secrets, tokens, passwords or API keys are redacted.
      - File contents are never read or forwarded.
      - All traffic stays on 127.0.0.1 — no external network calls.

.NOTES
    Import in $PROFILE with:
        Import-Module "path\to\OmniPresence.Terminal.psm1"
        Register-OmniPresenceHook
#>

Set-StrictMode -Version Latest

# ---------------------------------------------------------------------------
# Module-level configuration (overridable before calling Register-OmniPresenceHook)
# ---------------------------------------------------------------------------

$script:Config = @{
    EndpointUrl   = 'http://127.0.0.1:47831/integrations/terminal/context'
    # Minimum milliseconds between POSTs (debounce)
    DebounceMs    = 3000
    Enabled       = $true
}

$script:LastSentTime = [datetime]::MinValue
$script:LastActivity = $null

# Shared HttpClient for non-blocking fire-and-forget POSTs. We must NOT use a raw
# ThreadPool thread to run Invoke-RestMethod: a ThreadPool thread has no
# PowerShell runspace, so the cmdlet cannot execute and the resulting unhandled
# exception terminates the whole shell process. HttpClient.PostAsync is pure .NET
# async I/O — it returns immediately and needs no runspace.
Add-Type -AssemblyName System.Net.Http -ErrorAction SilentlyContinue
$script:HttpClient = [System.Net.Http.HttpClient]::new()
$script:HttpClient.Timeout = [TimeSpan]::FromSeconds(3)
# Send the body immediately instead of waiting for a "100 Continue" from the
# server (pointless round-trip on loopback; also simplifies the receiver).
$script:HttpClient.DefaultRequestHeaders.ExpectContinue = $false

# ---------------------------------------------------------------------------
# Secret-pattern redaction
# ---------------------------------------------------------------------------

# Patterns that suggest an argument is a credential or secret.
# Applied to each token in the raw command; matching tokens are replaced.
$script:SecretPatterns = @(
    '^--?(api[_-]?key|apikey|token|secret|password|passwd|pwd|auth|credential|cred|key)=?.+$',
    '^[A-Za-z0-9+/]{32,}={0,2}$',      # long base64 blob (likely a token)
    '^sk-[A-Za-z0-9]{20,}$',            # OpenAI-style key
    '^ghp_[A-Za-z0-9]{36,}$',           # GitHub personal access token
    '^xox[baprs]-[A-Za-z0-9\-]{10,}$',  # Slack token
    '^[A-Fa-f0-9]{40,}$'                # long hex string (API key / hash)
)

# Tools whose first positional token is a sub-command (safe to surface) rather
# than a private argument. For anything not in this set we report the verb alone.
$script:SubcommandVerbs = @(
    'git','docker','docker-compose','kubectl','helm','npm','yarn','pnpm','bun',
    'cargo','go','rustup','gh','glab','az','aws','gcloud','heroku','flyctl','wrangler',
    'pip','pip3','poetry','uv','conda','python','python3','dotnet','mvn','gradle','sbt',
    'archivebox','terraform','pulumi','ansible','systemctl','service','brew','apt',
    'apt-get','dnf','yum','pacman','choco','winget','scoop','make','just','task',
    'composer','rails','bundle','php','node','deno','npx','rake','dvc','pre-commit'
)

<#
.SYNOPSIS
    Derive a short, privacy-safe command summary from a raw command string.

.DESCRIPTION
    Splits the raw command on whitespace, keeps only the verb (first token)
    and the first non-flag, non-secret positional argument (if any), and
    redacts anything that looks like a credential.

.PARAMETER RawCommand
    The raw command string as entered (e.g. from PSReadLine history).

.EXAMPLE
    ConvertTo-SafeCommandSummary "archivebox add https://example.com --api-key=supersecret"
    # Returns: "Running archivebox add"

.EXAMPLE
    ConvertTo-SafeCommandSummary "git commit -m 'fix bug'"
    # Returns: "Running git commit"
#>
function ConvertTo-SafeCommandSummary {
    [CmdletBinding()]
    [OutputType([string])]
    param(
        [Parameter(Mandatory)]
        [AllowEmptyString()]
        [string]$RawCommand
    )

    if ([string]::IsNullOrWhiteSpace($RawCommand)) {
        return $null
    }

    # Tokenise on whitespace (simple; doesn't parse quotes, but safe enough).
    # Wrap in @() so a single-token command stays an array under StrictMode.
    $tokens = @($RawCommand.Trim() -split '\s+' | Where-Object { $_ -ne '' })

    if ($tokens.Count -eq 0) {
        return $null
    }

    $verb = $tokens[0]

    # Keep ONLY the verb plus, for known sub-command-style tools, the first clean
    # sub-command word (e.g. "git commit", "archivebox add"). We deliberately do
    # NOT include arbitrary positional arguments because they routinely carry
    # private data: URLs, file paths, commit messages, hostnames. For tools whose
    # first positional is an argument rather than a sub-command (ssh <host>,
    # cat <file>, cd <dir>), we report the verb alone.
    $subcommand = $null
    $verbKey = (Split-Path -Leaf $verb).ToLowerInvariant()
    if ($script:SubcommandVerbs -contains $verbKey) {
        $prevWasFlag = $false
        foreach ($tok in ($tokens | Select-Object -Skip 1)) {
            # Flags: skip, and remember so we can drop the flag's value next.
            if ($tok -match '^-') { $prevWasFlag = $true; continue }
            # Token directly after a flag is that flag's value — never include it.
            if ($prevWasFlag) { $prevWasFlag = $false; continue }

            # First positional decides the summary. Keep only a plain sub-command
            # word; anything else (URL, path, message, secret) is dropped + stop.
            $looksSecret = $false
            foreach ($pattern in $script:SecretPatterns) {
                if ($tok -match $pattern) { $looksSecret = $true; break }
            }
            if (-not $looksSecret -and
                $tok.Length -le 32 -and
                $tok -match '^[A-Za-z][A-Za-z0-9_:-]*$') {
                $subcommand = $tok
            }
            break
        }
    }

    $parts = if ($subcommand) { @($verb, $subcommand) } else { @($verb) }
    return "Running $($parts -join ' ')"
}

# ---------------------------------------------------------------------------
# Context gathering
# ---------------------------------------------------------------------------

<#
.SYNOPSIS
    Gather current terminal context: cwd, git repo, branch, and command summary.

.PARAMETER LastCommand
    The last command string (will be sanitized before inclusion).
#>
function Get-OmniContext {
    [CmdletBinding()]
    [OutputType([hashtable])]
    param(
        [string]$LastCommand = ''
    )

    $cwd = (Get-Location).Path

    # Git context — run git commands; ignore errors if not in a repo
    $repo   = $null
    $branch = $null
    try {
        $topLevel = git rev-parse --show-toplevel 2>$null
        if ($LASTEXITCODE -eq 0 -and $topLevel) {
            $repo = Split-Path -Leaf $topLevel.Trim()
        }

        $branchRaw = git rev-parse --abbrev-ref HEAD 2>$null
        if ($LASTEXITCODE -eq 0 -and $branchRaw) {
            $branch = $branchRaw.Trim()
        }
    }
    catch {
        # Not in a git repo or git not available — silently ignore
    }

    $commandSummary = ConvertTo-SafeCommandSummary -RawCommand $LastCommand

    return @{
        source          = 'terminal'
        cwd             = $cwd
        repo            = $repo
        branch          = $branch
        command_summary = $commandSummary
        privacy_safe    = $true
    }
}

# ---------------------------------------------------------------------------
# HTTP publish
# ---------------------------------------------------------------------------

<#
.SYNOPSIS
    POST a context hashtable to the OmniPresence context server.

.DESCRIPTION
    Fires-and-forgets via a background job. Fails silently if OmniPresence
    is not running.

.PARAMETER Context
    Hashtable produced by Get-OmniContext.
#>
function Send-OmniContext {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [hashtable]$Context
    )

    if (-not $script:Config.Enabled) { return }

    # Debounce: skip if we sent within the debounce window
    $now = [datetime]::UtcNow
    if (($now - $script:LastSentTime).TotalMilliseconds -lt $script:Config.DebounceMs) {
        return
    }
    $script:LastSentTime = $now

    $endpointUrl = $script:Config.EndpointUrl
    $json        = $Context | ConvertTo-Json -Compress -Depth 3

    # Fire-and-forget via HttpClient.PostAsync. PostAsync returns immediately so
    # the prompt never blocks; ContinueWith observes (and swallows) any failure
    # so an unobserved task exception can never surface. If OmniPresence is not
    # running the connection simply fails silently.
    try {
        $content = [System.Net.Http.StringContent]::new(
            $json, [System.Text.Encoding]::UTF8, 'application/json')
        $task = $script:HttpClient.PostAsync($endpointUrl, $content)
        $null = $task.ContinueWith([Action[System.Threading.Tasks.Task[System.Net.Http.HttpResponseMessage]]]{
            param($t)
            if ($t.Exception) { $t.Exception | Out-Null }  # observe to avoid UnobservedTaskException
        })
    }
    catch {
        # Building/dispatching the request failed — ignore silently.
    }
}

# ---------------------------------------------------------------------------
# Hook installation
# ---------------------------------------------------------------------------

<#
.SYNOPSIS
    Wire OmniPresence into the PowerShell prompt so context is reported after
    each command.

.DESCRIPTION
    Wraps the existing `prompt` function to capture the last command (via
    PSReadLine history), gather context, and call Send-OmniContext.
    The original prompt output is preserved.

.EXAMPLE
    Register-OmniPresenceHook
#>
function Register-OmniPresenceHook {
    [CmdletBinding()]
    param()

    # Capture original prompt function (may be Oh-My-Posh, Starship wrapper, etc.)
    $originalPrompt = if (Test-Path Function:\prompt) {
        $function:prompt
    }
    else {
        { "PS $($executionContext.SessionState.Path.CurrentLocation)$('>' * ($nestedPromptLevel + 1)) " }
    }

    # Also wire PSReadLine AddToHistoryHandler to capture each submitted command
    # before the prompt fires (more reliable than $MyInvocation in prompt).
    $script:LastSubmittedCommand = ''

    if (Get-Module -ListAvailable PSReadLine) {
        try {
            Set-PSReadLineOption -AddToHistoryHandler {
                param([string]$line)
                $script:LastSubmittedCommand = $line
                return $true  # always add to history
            }
        }
        catch {
            # PSReadLine might not support this in older versions — gracefully degrade
        }
    }

    # Replace the prompt function
    $function:prompt = {
        # Run OmniPresence context report (non-blocking)
        try {
            $ctx = Get-OmniContext -LastCommand $script:LastSubmittedCommand
            Send-OmniContext -Context $ctx
        }
        catch {
            # Never let OmniPresence errors bubble up to the user's prompt
        }

        # Delegate to original prompt
        & $originalPrompt
    }

    Write-Host '[OmniPresence] Terminal hook registered.' -ForegroundColor DarkGray
}

<#
.SYNOPSIS
    Remove the OmniPresence prompt hook and restore the original prompt.
#>
function Unregister-OmniPresenceHook {
    [CmdletBinding()]
    param()

    # Re-importing the module or restarting the shell also clears the hook.
    # For an in-session unregister, restore to a clean default.
    $function:prompt = { "PS $($executionContext.SessionState.Path.CurrentLocation)$('>' * ($nestedPromptLevel + 1)) " }

    if (Get-Module -ListAvailable PSReadLine) {
        try {
            Set-PSReadLineOption -AddToHistoryHandler $null
        }
        catch {}
    }

    Write-Host '[OmniPresence] Terminal hook removed.' -ForegroundColor DarkGray
}

# ---------------------------------------------------------------------------
# Exports
# ---------------------------------------------------------------------------

Export-ModuleMember -Function @(
    'Get-OmniContext',
    'Send-OmniContext',
    'Register-OmniPresenceHook',
    'Unregister-OmniPresenceHook',
    'ConvertTo-SafeCommandSummary'
)
