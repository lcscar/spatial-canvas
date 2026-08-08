# Spatial Canvas build information

- Source commit: `a53f527ce4546bf728b0d2fa8f3fb13dc7b12cc1`
- Build date/time (UTC): `2026-08-08T10:06:45Z`
- Configuration: `Release x64`
- Compiler/toolset: `MSVC v143` (`MSBuild 17.14.51`, PE linker `14.44`, Windows SDK NuGet `10.0.26100.1`, runner `windows-2022`)
- Build workflow run: `31251998904`
- Artifact: `SpatialCanvas.exe` (825,344 bytes, PE32+ x86-64 Windows GUI)
- SHA-256: `6b9f3e2b4a93fed1eb38d18d8b77139c8930f89fd448095a5ec533ef60f4a77d`

## Tests and validation

- NuGet restore for `Win32CaptureSample.sln`: passed.
- `WindowDiscoveryTests.exe`: passed (`All WindowDiscovery tests passed`).
  Deterministic coverage includes broad/minimal candidate selection, self/shell
  exclusion, retained HRESULTs, optional configuration not gating `StartCapture`,
  no 12/16 ceiling, every queued HWND eventually being attempted, non-blocking UI
  dispatch, one blocked candidate not stopping later candidates, exactly two
  long-lived workers, and a measured concurrency peak no greater than two normal
  capture initializations.
- `python tools/verify_corporate_hardening.py`: passed locally and on the Windows
  runner. It enforces the bounded executor, callback-direct frame retrieval,
  monitor and unparked-window probes, D3D11 multithread audit, RCA summary,
  absence of optional WGC properties, broad discovery, and executable-only deploy.
- `git diff --check`: passed.
- MSVC v143 `Release|x64` solution build: passed.
- Independent SHA-256 comparison of the downloaded Actions artifact: passed.
- PE inspection: passed. AMD64 GUI, ASLR, high-entropy VA, NX, CFG instrumentation,
  `asInvoker`, and `uiAccess=false` are present.
- Static import/string inspection: passed. No WinINet, WinHTTP, Winsock, external
  update URL, named-pipe server, autostart registry handling, scheduled task,
  screenshot export, injection path, `MinUpdateInterval`, or cursor-session option
  was found. The intentional `SetWindowsHookExW` mouse hook remains.
- Corporate-safe verifier result: **PASSED**.

## Root-cause summary

The real-machine result (18 successful `StartCapture` calls and zero frames) exposed
a common pipeline regression introduced by the earlier asynchronous refactor. The
original implementation initialized WGC sessions sequentially in the WinRT STA that
remained alive. The refactor instead created one detached MTA thread per HWND,
initialized dozens of sessions concurrently, moved their WinRT objects to the UI,
and immediately destroyed every creator apartment. It also registered `FrameArrived`
only as a UI wake-up and then polled `TryGetNextFrame` from the UI thread. This made
creator-apartment lifetime and cross-thread frame-pool consumption common to all
failed sessions. Parking order was not treated as the primary cause because the
original known-good path also parked after `StartCapture` and before later polling.

The repair uses two bounded, long-lived MTA workers. Each worker initializes HWNDs
sequentially and keeps its apartment alive for the application lifetime; one blocked
candidate cannot stop the other worker or the UI. `TryGetNextFrame` now runs directly
inside `FrameArrived`, and the latest frame object is transferred through a mutex for
UI-side D3D copying/rendering. Per-session event counts, callback ticks, callback
HRESULTs, and callback/UI first-frame counts distinguish event delivery from handoff.
Capture/no-frame failures remain visible as `CAPTURE ERROR` or `NO FRAME` placeholders.

`ID3D11Multithread` is queried and logged before/after. Protection remains unchanged
because only the UI thread uses the application's immediate D3D11 context; callbacks
only acquire a WinRT frame and exchange it under a mutex. The device itself supports
concurrent resource use, while enabling immediate-context serialization without
cross-thread context calls would add overhead without addressing the regression.
`GetDeviceRemovedReason` is included in the aggregate summary.

## Diagnostic controls and interpretation

The monitor control uses `CreateForMonitor` on the primary monitor with the same WGC
device/frame-pool/session sequence. It never saves, renders, or persists a pixel. The
control-window probe selects one conventional candidate by structural priority only,
captures it without parking/moving it, and directly consumes its callback frame. All
other candidates begin immediately; the held control HWND is queued afterward (or by
a 12-second probe watchdog that never terminates the probe thread).

The single on-screen RCA summary has these causal interpretations:

- Monitor and windows both have zero callback events/frames: systemic WGC, device,
  session, threading, desktop, or security/environment failure.
- Monitor receives a frame but the unparked control window does not: window-specific
  WGC/lifecycle behavior rather than global D3D/frame delivery.
- `FrameArrived` events are nonzero but callback first frames are zero: events fire,
  but `sender.TryGetNextFrame` returns no valid frame or fails in the callback.
- Unparked control receives a frame but normal sessions do not: normal-session
  lifecycle, ownership, or parking path; perform a parked/unparked comparison.
- Normal callback receives frames but UI-rendered first frames remain zero:
  cross-thread handoff or render/copy failure.
- UI-rendered first frames are nonzero: WGC delivery and rendering are live.
- Pending initializations with an active worker: an OS API did not return; the second
  bounded worker and UI continue without unsafe thread termination.

## Runtime-validation limitation

GitHub-hosted Actions does not provide a representative interactive Windows desktop
with Explorer, browsers, Office, Electron, Java, and corporate endpoint controls.
CI validates deterministic scheduling/failure behavior, security invariants, and the
complete Release build, but cannot claim real desktop frame delivery. On the target
laptop, photograph the single `Capture RCA summary` dialog and verify that placeholders
transition from `WAITING` to `LIVE`, or remain explicitly `NO FRAME`/`CAPTURE ERROR`.
