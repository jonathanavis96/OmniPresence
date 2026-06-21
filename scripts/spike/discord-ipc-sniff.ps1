# discord-ipc-sniff.ps1 — OmniPresence research spike (v2, instrumented)
# ----------------------------------------------------------------------
# Proves we can intercept RuneLite's built-in Discord plugin WITHOUT a sideloaded
# RuneLite plugin (so it works under the Jagex Launcher).
#
# How it works: RuneLite's Discord plugin uses the legacy discord-rpc library,
# which connects to the Windows named pipe \\.\pipe\discord-ipc-0 and streams
# SET_ACTIVITY frames containing the rich data (details/state/region/assets).
# This script impersonates the Discord client on that pipe: it ACKs the handshake
# (so RuneLite believes it is connected to Discord and keeps sending) and prints
# every activity update. No forwarding yet — this is a capture proof only.
#
# Frame wire format (per Discord RPC IPC):
#   [opcode : int32 little-endian][length : int32 little-endian][payload : UTF-8 JSON]
#   opcodes: 0=Handshake 1=Frame 2=Close 3=Ping 4=Pong
#
# v2 changes vs v1: explicit READY-write success/failure logging; logs EVERY
# frame (incl. ping/pong/unknown) with timestamps; faithful Discord READY
# payload (legacy discord-rpc is pickier than the modern lib); and the server
# now LOOPS to accept reconnections so you can toggle the RuneLite Discord
# plugin off/on to force a fresh presence push without restarting the script.
#
# RUNBOOK:
#   0. Make sure the OmniPresence app is CLOSED (it squats discord-ipc-0 with its
#      own client_id 1517890711218028544 — if you see that id, OmniPresence is up).
#   1. Fully QUIT Discord (tray -> Quit Discord) so discord-ipc-0 is free.
#   2. In RuneLite, make sure the built-in **Discord** plugin is ON.
#   3. Run this script. Wait for "Listening ... waiting".
#   4. Launch RuneLite and log in. Confirm the handshake shows client_id 409416265891971072.
#   5. If no ACTIVITY appears within ~15s: in RuneLite, toggle the Discord plugin
#      OFF then ON. That forces DiscordPlugin.startUp() to re-push presence onto
#      our (now reconnected) pipe. Watch for the yellow ACTIVITY block.
#   Ctrl+C to stop. (Re-launch Discord afterwards to restore normal presence.)

$ErrorActionPreference = 'Stop'
$PipeName = 'discord-ipc-0'

function Log([string]$msg, [string]$color = 'Gray') {
    Write-Host ("[{0}] {1}" -f (Get-Date -Format HH:mm:ss.fff), $msg) -ForegroundColor $color
}

# Write a whole frame as ONE buffer via the raw stream. CRITICAL: do NOT call
# Flush()/FlushFileBuffers on a named pipe — it blocks until the client drains
# the pipe, which legacy discord-rpc (RuneLite) does not do promptly, hanging us
# forever. Stream.Write already puts the bytes on the wire.
function New-Frame([int]$op, [string]$json, [System.IO.Stream]$s) {
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($json)
    $frame = New-Object byte[] (8 + $bytes.Length)
    [System.BitConverter]::GetBytes([int]$op).CopyTo($frame, 0)
    [System.BitConverter]::GetBytes([int]$bytes.Length).CopyTo($frame, 4)
    if ($bytes.Length -gt 0) { $bytes.CopyTo($frame, 8) }
    $s.Write($frame, 0, $frame.Length)
}

# Read exactly $count bytes (named pipes can return short reads).
function Read-Exact([System.IO.Stream]$s, [int]$count) {
    $buf = New-Object byte[] $count
    $off = 0
    while ($off -lt $count) {
        $n = $s.Read($buf, $off, $count - $off)
        if ($n -le 0) { throw [System.IO.EndOfStreamException]::new() }
        $off += $n
    }
    return $buf
}

# Faithful Discord READY (legacy discord-rpc reads data.user; keep it complete).
$READY = '{"cmd":"DISPATCH","data":{"v":1,"config":{"cdn_host":"cdn.discordapp.com","api_endpoint":"//discord.com/api","environment":"production"},"user":{"id":"1045800378228281345","username":"omnipresence","discriminator":"0","global_name":"OmniPresence","avatar":null,"avatar_decoration_data":null,"bot":false,"flags":0,"premium_type":0}},"evt":"READY","nonce":null}'

Log "Sniffer v2 starting. discord-ipc-0 proxy. Ctrl+C to stop." Cyan
Log "(If 'access denied' on create: Discord or OmniPresence still owns the pipe — quit them.)" DarkYellow

# Outer loop: accept reconnections so plugin toggle / RuneLite restart is captured.
while ($true) {
    Log "Creating pipe \\.\pipe\$PipeName and listening ..." Cyan
    # CRITICAL: pass explicit in/out buffer sizes. With a 0 out-buffer, a named-pipe
    # write becomes a synchronous rendezvous that BLOCKS until the client reads — which
    # froze us on the ack write (RuneLite only reads on its callback timer, not
    # continuously). A real out-buffer makes writes return immediately (buffered).
    $server = New-Object System.IO.Pipes.NamedPipeServerStream(
        $PipeName,
        [System.IO.Pipes.PipeDirection]::InOut,
        1,
        [System.IO.Pipes.PipeTransmissionMode]::Byte,
        [System.IO.Pipes.PipeOptions]::None,
        65536,
        65536)

    try {
        $server.WaitForConnection()
        Log "CLIENT CONNECTED." Green

        while ($true) {
            $hdr = Read-Exact $server 8
            $op  = [System.BitConverter]::ToInt32($hdr, 0)
            $len = [System.BitConverter]::ToInt32($hdr, 4)
            $payload = ''
            if ($len -gt 0) {
                $body = Read-Exact $server $len
                $payload = [System.Text.Encoding]::UTF8.GetString($body)
            }
            # Always log the raw frame so nothing is invisible.
            Log ("<- op={0} len={1} {2}" -f $op, $len, $payload) DarkGray

            switch ($op) {
                0 {   # Handshake -> reply with READY so the client stays connected
                    Log "HANDSHAKE: $payload" DarkCyan
                    try {
                        New-Frame 1 $READY $server
                        Log "-> READY sent OK ($($READY.Length) bytes)" Green
                    } catch {
                        Log "!! READY WRITE FAILED: $($_.Exception.Message)" Red
                    }
                }
                1 {   # Frame — SET_ACTIVITY is the one we want
                    try { $obj = $payload | ConvertFrom-Json } catch { $obj = $null }
                    if ($obj -and $obj.cmd -eq 'SET_ACTIVITY') {
                        $a = $obj.args.activity
                        Log "===== ACTIVITY =====" Yellow
                        if ($a) {
                            Write-Host ("  details : {0}" -f $a.details)
                            Write-Host ("  state   : {0}" -f $a.state)
                            if ($a.assets) {
                                Write-Host ("  large   : {0}  ({1})" -f $a.assets.large_image, $a.assets.large_text)
                                Write-Host ("  small   : {0}  ({1})" -f $a.assets.small_image, $a.assets.small_text)
                            }
                            if ($a.timestamps) { Write-Host ("  start   : {0}" -f $a.timestamps.start) }
                        } else {
                            Write-Host "  (activity cleared)"
                        }
                        $nonce = if ($obj.nonce) { $obj.nonce } else { '' }
                        New-Frame 1 ('{"cmd":"SET_ACTIVITY","data":null,"evt":null,"nonce":"' + $nonce + '"}') $server
                        Log "-> SET_ACTIVITY ack sent (nonce $nonce)" DarkGray
                    } else {
                        $nonce = if ($obj -and $obj.nonce) { $obj.nonce } else { '' }
                        $cmd   = if ($obj) { $obj.cmd } else { 'UNKNOWN' }
                        New-Frame 1 ('{"cmd":"' + $cmd + '","data":null,"evt":null,"nonce":"' + $nonce + '"}') $server
                        Log "-> ack '$cmd' (nonce $nonce)" DarkGray
                    }
                }
                3 {   # Ping -> Pong
                    New-Frame 4 $payload $server
                    Log "-> PONG" DarkGray
                }
                2 {   # Close
                    Log "CLIENT sent CLOSE." DarkYellow
                    break
                }
                default {
                    Log "?? unknown op=$op len=$len payload=$payload" DarkYellow
                }
            }
        }
    }
    catch [System.IO.EndOfStreamException] {
        Log "CLIENT DISCONNECTED." DarkYellow
    }
    catch {
        Log "!! ERROR: $($_.Exception.Message)" Red
    }
    finally {
        $server.Dispose()
    }
    Log "Re-listening for next connection (toggle the RuneLite Discord plugin to reconnect)..." Cyan
}
