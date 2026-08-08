#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
CANVAS = ROOT / "Win32CaptureSample" / "Canvas.cpp"
MANIFEST = ROOT / "Win32CaptureSample" / "app.manifest"

source = CANVAS.read_text(encoding="utf-8-sig")
manifest = MANIFEST.read_text(encoding="utf-8-sig")

forbidden = {
    "WinINet header": "#include <wininet.h>",
    "WinINet library": 'pragma comment(lib, "wininet.lib")',
    "network open": "InternetOpenW(",
    "network URL open": "InternetOpenUrlW(",
    "registry read": "RegOpenKeyExW(",
    "registry value query": "RegQueryValueExW(",
    "registry write": "RegSetValueExW(",
    "registry delete": "RegDeleteValueW(",
    "named pipe server": "CreateNamedPipeW(",
    "persisted window-title capture": "GetWindowTextW(t.source, title, 256);",
}

errors = []
for name, needle in forbidden.items():
    if needle in source:
        errors.append(f"FORBIDDEN: {name}: {needle}")

# Core behavior that must survive the hardening patch.
core_required = {
    "mouse-key helper preserved": "static bool IsMouseVk(int vk)",
    "InitD2D declaration preserved": "static void InitD2D();",
    "pull-hotkey declaration preserved": "static void ReRegisterPullHotkey();",
    "search declaration preserved": "static void UpdateMatches();",
    "raise-canvas declaration preserved": "static void RaiseCanvasTopmost();",
    "lower-canvas declaration preserved": "static void LowerCanvas();",
}

for name, required_text in core_required.items():
    if required_text not in source:
        errors.append(f"MISSING CORE: {name}: {required_text}")

required = {
    "explicit non-elevated execution": 'requestedExecutionLevel level="asInvoker" uiAccess="false"',
    "network default disabled": "bool updateCheck = false;",
    "IPC neutralized": "Corporate-safe baseline: no local named-pipe server.",
    "window titles redacted": "Corporate-safe: do not persist window titles",
}

for name, needle in required.items():
    haystack = manifest if "requestedExecutionLevel" in needle else source
    if needle not in haystack:
        errors.append(f"MISSING: {name}: {needle}")

if errors:
    print("Corporate-safe verification FAILED")
    for error in errors:
        print(" -", error)
    sys.exit(1)

print("Corporate-safe verification PASSED")
print(" - no WinINet update path")
print(" - no HKCU Run registry read/write path")
print(" - no named-pipe server")
print(" - no persisted window-title capture in crash-recovery file")
print(" - manifest explicitly uses asInvoker / uiAccess=false")
