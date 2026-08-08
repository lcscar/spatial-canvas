# Spatial Canvas build information

- Source commit: `1cf7cd18d2ad8138a5ed60f7b011b91ac76738f4`
- Build date/time (UTC): `2026-08-08T08:29:54Z`
- Configuration: `Release x64`
- Compiler/toolset: `MSVC v143` (`MSBuild 17.14.51`, PE linker `14.44`, Windows runner `windows-2022`)
- Build workflow run: `31248453138`
- Artifact: `SpatialCanvas.exe` (760,832 bytes, PE32+ x86-64 Windows GUI)
- SHA-256: `9816d5a4bfa49619d6d843c1f85b400e4c9be141bd33d5181200386b774da32f`

## Tests and validation

- NuGet restore for `Win32CaptureSample.sln`: passed.
- `WindowDiscoveryTests.exe`: passed (`All WindowDiscovery tests passed`).
  Deterministic coverage includes broad/minimal candidate selection, self/shell
  exclusion, retained HRESULTs, per-HWND failure isolation, optional configuration
  failure not gating the required start step, a blocked candidate not preventing
  later workers, all candidates being dispatched, no 12/16 capture ceiling, and
  dispatch returning before capture completion so UI startup remains independent.
- `python tools/verify_corporate_hardening.py`: passed locally and on the Windows
  build runner. The verifier now also rejects the legacy window ceiling and optional
  WGC session-property calls, requires independent workers and `FrameArrived`, and
  checks that `ShowWindow` precedes asynchronous discovery.
- `git diff --check`: passed.
- MSVC v143 `Release|x64` solution build: passed.
- Independent SHA-256 comparison of the downloaded GitHub Actions artifact: passed.
- PE inspection with `llvm-readobj`: passed. The image is AMD64 GUI with ASLR,
  high-entropy VA, NX compatibility, CFG instrumentation, and embedded
  `asInvoker` / `uiAccess=false`.
- Static import/string inspection: passed. No evidence of WinINet, WinHTTP,
  Winsock, upstream update URLs, named-pipe server, autostart registry handling,
  services, scheduled tasks, screenshot export, process/DLL injection,
  `MinUpdateInterval`, or `IsCursorCaptureEnabled` was found. The intentional
  `SetWindowsHookExW` mouse hook and worker-thread support remain present.

## Root-cause summary

The target log establishes the immediate blocking boundary: capture attempt 8
successfully created the capture item, free-threaded frame pool, and session, then
logged cursor configuration; the next source operation was the optional
`GraphicsCaptureSession::MinUpdateInterval` call, which never returned. A catch block
could not help because the API hung instead of throwing. Two independent design
regressions amplified that single-HWND hang: all capture initialization ran
synchronously before `ShowWindow` and the message pump, and discovery/capture stopped
after the configured 12/16 successful-window ceiling.

The repaired baseline invokes only the required WGC sequence: capture item,
free-threaded frame pool, `FrameArrived` registration, capture session, and
`StartCapture`. Optional session properties are absent. The Canvas is shown before
discovery begins; discovery runs off the UI thread; and every candidate is dispatched
to an independent worker. Successful sessions are transferred through a synchronized
completion queue and finalized on the UI thread. A worker stuck in an OS call remains
pending without unsafe termination while later HWNDs continue. Both initial discovery
and `AdoptNewWindows` have no application window-count ceiling.

## Runtime-validation limitation

GitHub-hosted Actions runners do not provide a representative interactive Windows
desktop populated with Explorer, browsers, Office, Electron, Java, and other real
applications. CI therefore validates deterministic scheduling/failure behavior and
the complete Release build, but does not claim live WGC interoperability. On the
target laptop, verify that the Canvas appears before capture completion, attempt 8
reaches `GraphicsCaptureSession.StartCapture`, later attempts continue, successful
windows render, and any worker still pending after ten seconds receives a privacy-safe
`capture.hung` record in `SpatialCanvas-debug.log`.
