# Spatial Canvas build information

- Source commit: `03b89ac6fcb7190fe987640b3b87dcdfcbcdd87b`
- Build date/time (UTC): `2026-08-08T07:24:01Z`
- Configuration: `Release x64`
- Compiler/toolset: `MSVC v143` (`MSBuild 17.14.51`, PE linker `14.44`, Windows runner `windows-2022`)
- Build workflow run: `31246008584`
- Artifact: `SpatialCanvas.exe` (734,208 bytes, PE32+ x86-64 Windows GUI)
- SHA-256: `3807bd679f55b005f95ee875aeb351213c0196e764695d027f7e9e682a546464`

## Tests and validation

- NuGet restore for `Win32CaptureSample.sln`: passed.
- `WindowDiscoveryTests.exe`: passed (`All WindowDiscovery tests passed`).
  The deterministic tests cover broad candidate selection, self/shell exclusion,
  per-HWND capture-failure isolation, enumeration-vs-capture outcome separation,
  capture-limit behavior, and HRESULT/Win32 error retention.
- `python tools/verify_corporate_hardening.py`: passed locally and on the Windows build runner.
- `git diff --check`: passed.
- MSVC v143 `Release|x64` solution build: passed.
- Independent SHA-256 comparison of downloaded GitHub Actions artifact: passed.
- PE inspection with `llvm-readobj`: passed. The image is AMD64 GUI with ASLR,
  high-entropy VA, NX compatibility, and embedded `asInvoker` / `uiAccess=false`.
- Static import/string inspection: passed. No evidence of WinINet, WinHTTP,
  Winsock, upstream update URLs, named-pipe server, autostart registry handling,
  services, scheduled tasks, screenshot export, or process/DLL injection was found.
  The intentional `SetWindowsHookExW` mouse hook remains present.

## Root-cause summary

The startup path treated a heavily filtered list as if it were raw enumeration.
It pre-rejected visible top-level windows for `WS_EX_TOOLWINDOW`, owner relationships,
missing titles, small dimensions, cloaking, and `ApplicationFrameWindow`, then exposed
only the final zero count. It did not retain `EnumWindows` return/error/callback data
or try a desktop enumeration fallback, so the observed `Eligible top-level windows: 0`
could not distinguish an API/desktop-context failure from filtering everything. The
one-off per-filter diagnostic workflow intended to clarify this failed before build
because its regex/text anchor no longer matched the source.

The repaired implementation applies only structural exclusions (invalid, invisible,
non-top-level, Spatial Canvas itself, and known desktop shell surfaces), attempts WGC
capture independently for each remaining HWND, logs exact per-stage failures, and
continues after failures. It also differentiates enumeration, capture startup, frame
delivery, and rendering failures in the local executable-adjacent debug log/UI.

## Runtime-validation limitation

GitHub-hosted Actions runners do not provide a representative interactive Windows
desktop populated with real application windows. Therefore, CI validates deterministic
discovery/failure-isolation logic and the complete Release build, but does not claim to
validate live WGC capture of Explorer, browsers, Office, Electron, Java, or other real
desktop applications. Use `SpatialCanvas-debug.log` from one run on the target laptop
for that final environment-specific validation; the log intentionally excludes window
title text, document names, URLs, screenshots, and frame content.
