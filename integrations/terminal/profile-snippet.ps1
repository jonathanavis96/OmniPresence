# ---------------------------------------------------------------------------
# OmniPresence terminal integration — append this block to your $PROFILE
# ---------------------------------------------------------------------------
#
# To find your profile path, run:
#   $PROFILE
# Typical location:
#   C:\Users\<you>\Documents\PowerShell\Microsoft.PowerShell_profile.ps1
#
# Then open it and paste this block at the end.
# ---------------------------------------------------------------------------

# Path to the OmniPresence terminal module.
# Adjust this path to wherever you cloned OmniPresence.
$OmniPresenceModulePath = "$env:USERPROFILE\code\OmniPresence\integrations\terminal\OmniPresence.Terminal.psm1"

if (Test-Path $OmniPresenceModulePath) {
    Import-Module $OmniPresenceModulePath -Force -ErrorAction SilentlyContinue

    # Optional: override the default endpoint or debounce
    # $script:Config.EndpointUrl = 'http://127.0.0.1:47831/integrations/terminal/context'
    # $script:Config.DebounceMs  = 5000
    # $script:Config.Enabled     = $true

    Register-OmniPresenceHook
}
else {
    Write-Warning "[OmniPresence] Module not found at: $OmniPresenceModulePath"
}
