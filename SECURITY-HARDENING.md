# Corporate-safe hardening baseline

This branch is intentionally conservative. The goal is to preserve Spatial Canvas behavior while removing facilities that are unnecessary for a controlled corporate evaluation.

## Baseline guarantees targeted by this branch

- Runs as the current user (`asInvoker`) and does not request elevation or UIAccess.
- No HTTP/HTTPS update check and no WinINet dependency in the hardened source.
- No autostart persistence and no HKCU `Run` registry read/write path.
- No local named-pipe server.
- No screenshot or captured-frame export/persistence path.
- Crash recovery does not persist window titles; the recovery file keeps only the data needed to identify/restore window geometry while preserving the existing file format.
- A privacy-safe UTF-8 debug log is written beside the executable. It records HWND/process/class metadata and errors, but never window-title text, document names, URLs, screenshots, or frame content.
- No new telemetry is introduced.
- No new services, drivers, scheduled tasks, DLL injection, process injection, or global keyboard hooks are introduced.

## Intentionally retained behavior

These are part of the product's core behavior and are not considered persistence/exfiltration mechanisms:

- Windows Graphics Capture for live window surfaces.
- D3D11/DXGI/D2D/DWrite rendering.
- Win32 window enumeration, positioning, focus and restoration.
- Local layout/settings files used by Spatial Canvas.
- User-initiated app/window management features already present upstream.

## Applying the code hardening

From the repository root on `hardening/corporate-safe`:

```text
python tools/apply_corporate_hardening.py
python tools/verify_corporate_hardening.py
```

The patcher is deliberately fail-closed: if upstream source no longer matches the expected implementation, it stops instead of silently applying an unsafe/partial transformation.

## Validation before corporate use

A hardened build should still be validated at binary/runtime level before use with sensitive data:

1. Verify the binary does not request elevation.
2. Verify no outbound TCP/UDP/DNS activity while starting and using the canvas.
3. Verify no listening named pipe created by Spatial Canvas.
4. Verify no HKCU/HKLM persistence changes.
5. Verify no screenshots/window frames are written to disk.
6. Verify files created under the user profile contain no captured window titles/content.
7. Test crash recovery, lock/unlock, sleep/wake, DPI changes and multi-monitor behavior.
8. Test Office/Excel/Smart View dialogs before using the tool in a production workflow.

## Scope boundary

Technical hardening does not override corporate endpoint/software policy. A clean runtime profile makes the software easier to assess and defend, but does not itself constitute organizational approval.
