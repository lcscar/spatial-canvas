#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
SOURCE_DIR = ROOT / "Win32CaptureSample"
MANIFEST = SOURCE_DIR / "app.manifest"
DISCOVERY = SOURCE_DIR / "WindowDiscovery.cpp"
DIAGNOSTICS = SOURCE_DIR / "Diagnostics.cpp"
CANVAS = SOURCE_DIR / "Canvas.cpp"
BUILD_WORKFLOW = ROOT / ".github" / "workflows" / "corporate-safe-build.yml"

runtime_files = sorted(
    list(SOURCE_DIR.glob("*.cpp"))
    + list(SOURCE_DIR.glob("*.h"))
    + list(SOURCE_DIR.glob("*.vcxproj"))
)
runtime_text = "\n".join(path.read_text(encoding="utf-8-sig") for path in runtime_files)
runtime_lower = runtime_text.lower()
manifest = MANIFEST.read_text(encoding="utf-8-sig")
discovery = DISCOVERY.read_text(encoding="utf-8-sig")
diagnostics = DIAGNOSTICS.read_text(encoding="utf-8-sig")
canvas = CANVAS.read_text(encoding="utf-8-sig")
build_workflow = BUILD_WORKFLOW.read_text(encoding="utf-8-sig")

forbidden_case_sensitive = {
    "WinINet header": "#include <wininet.h>",
    "WinINet library": 'pragma comment(lib, "wininet.lib")',
    "WinHTTP header": "#include <winhttp.h>",
    "WinHTTP library": 'pragma comment(lib, "winhttp.lib")',
    "WinINet API": "InternetOpenW(",
    "WinINet URL API": "InternetOpenUrlW(",
    "WinHTTP API": "WinHttpOpen(",
    "Winsock startup": "WSAStartup(",
    "Winsock socket": "WSASocketW(",
    "registry read": "RegOpenKeyExW(",
    "registry value query": "RegQueryValueExW(",
    "registry App Paths lookup": "RegGetValueW(",
    "registry write": "RegSetValueExW(",
    "registry delete": "RegDeleteValueW(",
    "named pipe server": "CreateNamedPipeW(",
    "service creation": "CreateServiceW(",
    "service manager": "OpenSCManagerW(",
    "remote process memory": "WriteProcessMemory(",
    "remote thread injection": "CreateRemoteThread(",
    "cross-process allocation": "VirtualAllocEx(",
    "screenshot export": "SaveCanvasPng",
    "PNG frame encoder": "GUID_ContainerFormatPng",
    "persisted window-title capture": "GetWindowTextW(t.source, title, 256);",
    "optional WGC update interval in startup": ".MinUpdateInterval(",
    "optional WGC cursor property in startup": ".IsCursorCaptureEnabled(",
    "legacy capture-count ceiling": "maxTiles",
}

forbidden_case_insensitive = {
    "Winsock header": "winsock2.h",
    "Winsock library": "ws2_32.lib",
    "upstream update host": "raw.githubusercontent.com/13auth/spatial-canvas",
    "upstream release URL": "github.com/13auth/spatial-canvas/releases",
    "upstream host reference in executable": "github.com/13auth/spatial-canvas",
    "scheduled-task launcher": "schtasks.exe",
}

errors = []
for name, needle in forbidden_case_sensitive.items():
    if needle in runtime_text:
        errors.append(f"FORBIDDEN: {name}: {needle}")
for name, needle in forbidden_case_insensitive.items():
    if needle.lower() in runtime_lower:
        errors.append(f"FORBIDDEN: {name}: {needle}")

required_runtime = {
    "explicit non-elevated execution": (
        manifest, 'requestedExecutionLevel level="asInvoker" uiAccess="false"'),
    "network default disabled": (runtime_text, "bool updateCheck = false;"),
    "IPC server neutralized": (
        runtime_text, "Corporate-safe baseline: no local named-pipe server."),
    "window-title crash persistence redacted": (
        runtime_text, "Corporate-safe: do not persist window titles"),
    "executable-adjacent debug log": (diagnostics, "SpatialCanvas-debug.log"),
    "debug log is UTF-8": (diagnostics, "WideCharToMultiByte(CP_UTF8"),
    "debug log flushes": (diagnostics, "FlushFileBuffers(g_log)"),
    "privacy-safe title metadata": (runtime_text, 'L" title_length="'),
    "broad discovery evaluator": (discovery, "WindowDecision EvaluateWindow"),
    "per-HWND failure isolation": (discovery, "ProcessWindowAttempts"),
    "independent capture workers": (
        discovery, "DispatchWindowAttemptsIndependently"),
    "frame-arrival registration": (canvas, ".FrameArrived("),
    "unlimited capture dispatch": (canvas, "capture_limit=none"),
    "EnumDesktopWindows fallback": (runtime_text, "EnumDesktopWindows("),
    "intentional global mouse hook preserved": (
        runtime_text, "SetWindowsHookExW(WH_MOUSE_LL"),
    "capture attempt continues": (runtime_text, "action=skip_and_continue"),
}
for name, (haystack, needle) in required_runtime.items():
    if needle not in haystack:
        errors.append(f"MISSING: {name}: {needle}")

# The pure decision function must not turn presentation metadata or application
# technology into compatibility filters. Those facts may still be logged by Canvas.
for needle in (
    "WS_EX_TOOLWINDOW",
    "GW_OWNER",
    "GetWindowText",
    "ApplicationFrameWindow",
    "Chrome_WidgetWin",
    "Electron",
):
    if needle in discovery:
        errors.append(f"OVER-FILTERING: discovery logic contains {needle}")

if "GetWindowText" in diagnostics:
    errors.append("PRIVACY: Diagnostics.cpp must never obtain window-title text")

show_canvas = canvas.find("ShowWindow(g_hwnd, SW_SHOW)")
start_discovery = canvas.find("StartDiscoveryAsync(true)")
if show_canvas < 0 or start_discovery < 0 or show_canvas > start_discovery:
    errors.append("STARTUP: Canvas must be shown before asynchronous discovery begins")

for needle in (
    "contents: write",
    "Publish executable-only distribution branch",
    "refs/heads/release/exe-only",
    "git update-index --add --cacheinfo",
    "git write-tree",
):
    if needle not in build_workflow:
        errors.append(f"DELIVERY: automated executable-only publish is missing: {needle}")

if errors:
    print("Corporate-safe verification FAILED")
    for error in errors:
        print(" -", error)
    sys.exit(1)

print("Corporate-safe verification PASSED")
print(" - no WinINet, WinHTTP, or Winsock path")
print(" - no HKCU Run/registry persistence path")
print(" - no named-pipe server, services, tasks, or injection APIs")
print(" - no screenshot/frame export path")
print(" - debug log is executable-adjacent UTF-8 and title-content safe")
print(" - broad discovery, independent per-HWND workers, and no capture ceiling are enforced")
print(" - optional WGC session properties are absent from the startup path")
print(" - successful Release builds publish only SpatialCanvas.exe to release/exe-only")
print(" - manifest explicitly uses asInvoker / uiAccess=false")
