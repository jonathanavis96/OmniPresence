@echo off
REM ============================================================================
REM  launch-stack.bat — start the OmniPresence presence stack in the ONE order
REM  that lets OmniPresence intercept RuneLite's Discord presence.
REM
REM  WHY THE ORDER MATTERS:
REM  RuneLite's built-in Discord plugin talks to the FIRST owner of the named
REM  pipe \\.\pipe\discord-ipc-0. Whoever creates that pipe first wins it.
REM   - If Discord starts first, Discord owns ipc-0, RuneLite talks to the real
REM     Discord, and you see "Playing RuneLite".
REM   - If OmniPresence starts first, OmniPresence owns ipc-0, intercepts
REM     RuneLite's activity, and re-publishes it under your own identity so the
REM     sidebar reads "OSRS - <activity>". The real Discord falls back to ipc-1.
REM
REM  So: close Discord, start OmniPresence (grabs ipc-0), THEN start Discord.
REM  Run THIS once at the start of a session instead of launching them manually.
REM ============================================================================

echo [launch-stack] Closing Discord so OmniPresence can claim discord-ipc-0...
taskkill /IM Discord.exe /F >nul 2>&1
REM Give Windows a moment to release the pipe handle.
ping -n 3 127.0.0.1 >nul

echo [launch-stack] Starting OmniPresence (claims discord-ipc-0)...
start "" /D "C:\dev\OmniPresence\build-discord\app" omnipresence.exe
REM Let the interceptor's CreateNamedPipeW win ipc-0 before Discord comes back.
ping -n 4 127.0.0.1 >nul

echo [launch-stack] Starting Discord (falls back to ipc-1)...
start "" "%LOCALAPPDATA%\Discord\Update.exe" --processStart Discord.exe

echo [launch-stack] Done. Launch RuneLite normally; keep its built-in Discord plugin ON.
exit /b 0
