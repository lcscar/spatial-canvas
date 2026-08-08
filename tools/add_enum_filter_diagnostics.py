from pathlib import Path

p = Path("Win32CaptureSample/Canvas.cpp")
s = p.read_text(encoding="utf-8")

# Add counters next to existing corporate-safe diagnostics.
anchor = """    size_t g_diagEligibleWindows = 0;
    int g_diagCaptureFailures = 0;
    HRESULT g_diagFirstCaptureHr = S_OK;
"""
replacement = """    size_t g_diagEligibleWindows = 0;
    int g_diagCaptureFailures = 0;
    HRESULT g_diagFirstCaptureHr = S_OK;
    size_t g_diagEnumTotal = 0;
    size_t g_diagRejectInvisible = 0;
    size_t g_diagRejectNonRoot = 0;
    size_t g_diagRejectToolWindow = 0;
    size_t g_diagRejectOwned = 0;
    size_t g_diagRejectNoTitle = 0;
    size_t g_diagRejectCloaked = 0;
    size_t g_diagRejectTooSmall = 0;
    size_t g_diagRejectSelf = 0;
    size_t g_diagRejectClass = 0;
"""
if "g_diagEnumTotal" not in s:
    if anchor not in s:
        raise SystemExit("ERROR: diagnostic globals anchor not found")
    s = s.replace(anchor, replacement, 1)

old_enum = """static BOOL CALLBACK EnumCb(HWND hwnd, LPARAM lp)
{
    auto* out = reinterpret_cast<std::vector<HWND>*>(lp);
    if (!IsWindowVisible(hwnd)) return TRUE;
    if (GetAncestor(hwnd, GA_ROOT) != hwnd) return TRUE;
    LONG ex = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (ex & WS_EX_TOOLWINDOW) return TRUE;
    if (GetWindow(hwnd, GW_OWNER)) return TRUE; // sahipli (dialog/popup) dışla
    wchar_t title[256];
    if (GetWindowTextW(hwnd, title, 256) == 0) return TRUE;
    DWORD cloaked = 0;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (cloaked) return TRUE;
    RECT r{}; GetWindowRect(hwnd, &r);
    if (r.right - r.left < 300 || r.bottom - r.top < 200) return TRUE;
    DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId()) return TRUE;
    wchar_t cls[128]; GetClassNameW(hwnd, cls, 128);
    if (!wcscmp(cls, L\"SpatialCanvasWnd\")) return TRUE; // M16: kendi tuvalimiz (ikinci örnek dahil)
    if (!wcscmp(cls, L\"Progman\") || !wcscmp(cls, L\"WorkerW\")) return TRUE;
    if (!wcscmp(cls, L\"ApplicationFrameWindow\")) return TRUE; // UWP: park/restore guvenilmez
    out->push_back(hwnd);
    return TRUE;
}
"""
new_enum = """static BOOL CALLBACK EnumCb(HWND hwnd, LPARAM lp)
{
    ++g_diagEnumTotal;
    auto* out = reinterpret_cast<std::vector<HWND>*>(lp);
    if (!IsWindowVisible(hwnd)) { ++g_diagRejectInvisible; return TRUE; }
    if (GetAncestor(hwnd, GA_ROOT) != hwnd) { ++g_diagRejectNonRoot; return TRUE; }
    LONG ex = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (ex & WS_EX_TOOLWINDOW) { ++g_diagRejectToolWindow; return TRUE; }
    if (GetWindow(hwnd, GW_OWNER)) { ++g_diagRejectOwned; return TRUE; } // sahipli (dialog/popup) dışla
    wchar_t title[256];
    if (GetWindowTextW(hwnd, title, 256) == 0) { ++g_diagRejectNoTitle; return TRUE; }
    DWORD cloaked = 0;
    DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (cloaked) { ++g_diagRejectCloaked; return TRUE; }
    RECT r{}; GetWindowRect(hwnd, &r);
    if (r.right - r.left < 300 || r.bottom - r.top < 200) { ++g_diagRejectTooSmall; return TRUE; }
    DWORD pid = 0; GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId()) { ++g_diagRejectSelf; return TRUE; }
    wchar_t cls[128]; GetClassNameW(hwnd, cls, 128);
    if (!wcscmp(cls, L\"SpatialCanvasWnd\") ||
        !wcscmp(cls, L\"Progman\") || !wcscmp(cls, L\"WorkerW\") ||
        !wcscmp(cls, L\"ApplicationFrameWindow\"))
    {
        ++g_diagRejectClass;
        return TRUE;
    }
    out->push_back(hwnd);
    return TRUE;
}
"""
if old_enum not in s:
    if new_enum not in s:
        raise SystemExit("ERROR: EnumCb anchor not found")
else:
    s = s.replace(old_enum, new_enum, 1)

# Reset counters before enumeration.
reset_anchor = """    g_diagEligibleWindows = 0;
    g_diagCaptureFailures = 0;
    g_diagFirstCaptureHr = S_OK;
    std::vector<HWND> wins;
"""
reset_replacement = """    g_diagEligibleWindows = 0;
    g_diagCaptureFailures = 0;
    g_diagFirstCaptureHr = S_OK;
    g_diagEnumTotal = 0;
    g_diagRejectInvisible = 0;
    g_diagRejectNonRoot = 0;
    g_diagRejectToolWindow = 0;
    g_diagRejectOwned = 0;
    g_diagRejectNoTitle = 0;
    g_diagRejectCloaked = 0;
    g_diagRejectTooSmall = 0;
    g_diagRejectSelf = 0;
    g_diagRejectClass = 0;
    std::vector<HWND> wins;
"""
if "g_diagRejectInvisible = 0;\n    g_diagRejectNonRoot = 0;" not in s:
    if reset_anchor not in s:
        raise SystemExit("ERROR: CreateTiles diagnostic reset anchor not found")
    s = s.replace(reset_anchor, reset_replacement, 1)

# Expand the existing diagnostic message.
msg_anchor = """        diag += L\"\\nEligible top-level windows: \" + std::to_wstring(g_diagEligibleWindows);
        diag += L\"\\nWindows.Graphics.Capture supported: \";
"""
msg_replacement = """        diag += L\"\\nEnumWindows callbacks: \" + std::to_wstring(g_diagEnumTotal);
        diag += L\"\\nEligible top-level windows: \" + std::to_wstring(g_diagEligibleWindows);
        diag += L\"\\nRejected - invisible: \" + std::to_wstring(g_diagRejectInvisible);
        diag += L\"\\nRejected - non-root: \" + std::to_wstring(g_diagRejectNonRoot);
        diag += L\"\\nRejected - tool window: \" + std::to_wstring(g_diagRejectToolWindow);
        diag += L\"\\nRejected - owned: \" + std::to_wstring(g_diagRejectOwned);
        diag += L\"\\nRejected - no title: \" + std::to_wstring(g_diagRejectNoTitle);
        diag += L\"\\nRejected - cloaked: \" + std::to_wstring(g_diagRejectCloaked);
        diag += L\"\\nRejected - too small: \" + std::to_wstring(g_diagRejectTooSmall);
        diag += L\"\\nRejected - self: \" + std::to_wstring(g_diagRejectSelf);
        diag += L\"\\nRejected - excluded class: \" + std::to_wstring(g_diagRejectClass);
        diag += L\"\\nWindows.Graphics.Capture supported: \";
"""
if "Rejected - invisible:" not in s:
    if msg_anchor not in s:
        raise SystemExit("ERROR: diagnostic message anchor not found")
    s = s.replace(msg_anchor, msg_replacement, 1)

p.write_text(s, encoding="utf-8")
print("OK: per-filter enumeration diagnostics added")
