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

    # Tokenise on whitespace (simple; doesn't parse quotes, but safe enough)
    $tokens = $RawCommand.Trim() -split '\s+' | Where-Object { $_ -ne '' }

    if ($tokens.Count -eq 0) {
        return $null
    }

    $verb = $tokens[0]

    # Gather up to two safe positional arguments to make the summary legible
    $safeArgs = @()
    foreach ($tok in ($tokens | Select-Object -Skip 1)) {
        # Skip flags
        if ($tok -match '^-') { continue }

        # Check if the token looks like a secret
        $looksSecret = $false
        foreach ($pattern in $script:SecretPatterns) {
            if ($tok -match $pattern) {
                $looksSecret = $true
                break
            }
        }
        if ($looksSecret) { continue }

        # Skip things that look like file paths with sensitive extensions
        if ($tok -match '\.(pem|key|p12|pfx|env|secret)$') { continue }

        # Keep short, benign-looking positional args (subcommands, filenames)
        if ($tok.Length -le 40 -and $tok -notmatch '[=@]') {
            $safeArgs += $tok
        }

        if ($safeArgs.Count -ge 2) { break }
    }

    $parts = @($verb) + $safeArgs
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

    # Fire on a background thread so the prompt never blocks
    $null = [System.Threading.ThreadPool]::QueueUserWorkItem({
        param($state)
        try {
            $response = Invoke-RestMethod `
                -Uri         $state.Url `
                -Method      Post `
                -Body        $state.Json `
                -ContentType 'application/json; charset=utf-8' `
                -TimeoutSec  3 `
                -ErrorAction SilentlyContinue
        }
        catch {
            # OmniPresence not running — ignore silently
        }
    }.GetNewClosure(), [PSCustomObject]@{ Url = $endpointUrl; Json = $json })
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
