# RuneLite IPC interception → republish — implementation plan

Date: 2026-06-21
Spec: `docs/superpowers/specs/2026-06-21-runelite-ipc-interception-republish-design.md`

## Done already this session
- [x] **Gating question resolved** — Social SDK presence is network-based (no `discord-ipc-0`
      conflict). See spec.
- [x] **OSRS label** — `activityNameTemplate` "RuneLight –" → **"OSRS –"** in
      `config/omnipresence.example.json` + both live `%APPDATA%` configs. `migrateRuleTemplates`
      only touches terminal rules, so it won't be overwritten.

## Task 1 — `NamedPipeInterceptor` C++ component (Windows-only)
New `app/include/NamedPipeInterceptor.h` + `app/src/NamedPipeInterceptor.cpp`. Ports the proven
`scripts/spike/discord-ipc-sniff.ps1` v3 logic to a Qt/Win32 component.

- `QObject` subclass; owns a background worker (`std::thread` or `QThread`) because Win32 pipe
  reads block. Public: `start()`, `stop()` (signals the thread to exit + closes the pipe handle),
  dtor joins.
- Worker loop: `CreateNamedPipeW(L"\\\\.\\pipe\\discord-ipc-0", PIPE_ACCESS_DUPLEX,
  PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT, 1, 65536, 65536, 0, nullptr)` →
  `ConnectNamedPipe` → serve until disconnect → `DisconnectNamedPipe` → loop (accept reconnects).
- Frame I/O: read `[op int32-LE][len int32-LE][payload]` via a read-exact helper. **Write helper
  must NOT FlushFileBuffers** (carry the two gotchas as comments). Out-buffer 64 KB.
- Handshake (op 0): reply op 1 with the faithful READY JSON (copy from the spike's `$READY`).
- Frame (op 1): parse JSON; on `cmd == "SET_ACTIVITY"` extract `args.activity.state` and
  `args.activity.details`; **emit `activityCaptured(QString activity, QString location)`**
  (Qt::QueuedConnection across the thread boundary — never touch IntegrationContext from the
  worker thread). Send a benign ack frame (`{"cmd":"SET_ACTIVITY","data":null,"evt":null,
  "nonce":"<n>"}`). Optionally gate capture on `client_id == "409416265891971072"` (RuneLite);
  ACK+ignore other client_ids (e.g. the SDK's join connection).
- Ping (op 3) → Pong (op 4, echo). Close (op 2) → end inner loop, re-listen.
- Use `qDebug()` logging mirroring the spike's lines (CONNECTED / HANDSHAKE / READY sent /
  ACTIVITY / DISCONNECTED) so runtime behaviour is observable.
- Entire file guarded `#ifdef Q_OS_WIN` (no-op stub elsewhere so non-Windows still builds).

## Task 2 — wire into AppController
- Add member `std::unique_ptr<NamedPipeInterceptor> m_runeliteInterceptor;` (forward-declared in
  header, like `LocalContextServer`).
- Construct it; `connect(m_runeliteInterceptor, &NamedPipeInterceptor::activityCaptured, this,
  &AppController::onRuneliteActivityCaptured)`.
- New slot `onRuneliteActivityCaptured(QString activity, QString location)`:
  `QJsonObject o; o["activity"]=activity; o["location"]=location;
  m_integrationContext.update("runelite", o); evaluateAndPublish();`
  (mirrors `onIntegrationContextUpdated`). Empty activity is fine — the rule trims the dangling
  separator → "OSRS".
- Start it alongside the others (near `m_contextServer->start();` L127); stop on shutdown.

## Task 3 — CMake
- Add `NamedPipeInterceptor.cpp/.h` to the app target sources. No new Qt modules (uses QtCore +
  Win32 `Kernel32`, already linked on Windows). AUTOMOC: header must be in the sources list and
  carry `Q_OBJECT` (see `reference_qt6_wsl_windows_build`).

## Task 4 — build + smoke (Windows, via WSL interop)
- Incremental build (`rebuild-inc.bat`) — keeps the windeployqt'd Qt runtime; no new QML module
  so no windeployqt re-run needed. Verify `omnipresence.exe` links + launches with zero QML
  errors and reaches Client ready.

## Task 5 — manual end-to-end (Jonathan at keyboard)
- Quit Discord; launch OmniPresence (interceptor grabs ipc-0); launch Discord (→ ipc-1);
  RuneLite Discord plugin ON; log into OSRS + move. Confirm Discord shows
  **"OSRS – …" / region** under OmniPresence's identity (single entry, RuneLite detection
  already off). Confirm OmniPresence's own presence (coding etc.) still publishes (network path).

## Risks / notes
- Thread-safety: only ever mutate `IntegrationContext` on the main thread (queued signal).
- Startup ordering: if Discord already owns ipc-0, `CreateNamedPipeW` fails with
  ERROR_PIPE_BUSY/ACCESS_DENIED — log it and surface guidance; don't crash.
- This replaces the inert sideloaded plugin as the `runelite` source; the plugin stays in-repo
  as the Option-B fallback.
