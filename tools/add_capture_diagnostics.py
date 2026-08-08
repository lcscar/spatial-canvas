from pathlib import Path

p = Path("Win32CaptureSample/Canvas.cpp")
s = p.read_text(encoding="utf-8")

# Fail closed / idempotent.
if "g_diagEligibleWindows" in s:
    print("Capture diagnostics already present.")
    raise SystemExit(0)

anchor = "    Settings g_set;\n    bool g_panelOpen = false;"
replacement = """    Settings g_set;
    // Corporate-safe diagnostic counters: counts/HRESULT only, never window titles/content.
    size_t g_diagEligibleWindows = 0;
    int g_diagCaptureFailures = 0;
    HRESULT g_diagFirstCaptureHr = S_OK;
    bool g_panelOpen = false;"""
if anchor not in s:
    raise SystemExit("ERROR: globals anchor not found")
s = s.replace(anchor, replacement, 1)

anchor = """static void CreateTiles()
{
    std::vector<HWND> wins;
    EnumWindows(EnumCb, reinterpret_cast<LPARAM>(&wins));
    for (HWND w : wins)
    {"""
replacement = """static void CreateTiles()
{
    g_diagEligibleWindows = 0;
    g_diagCaptureFailures = 0;
    g_diagFirstCaptureHr = S_OK;
    std::vector<HWND> wins;
    EnumWindows(EnumCb, reinterpret_cast<LPARAM>(&wins));
    g_diagEligibleWindows = wins.size();
    for (HWND w : wins)
    {"""
if anchor not in s:
    raise SystemExit("ERROR: CreateTiles anchor not found")
s = s.replace(anchor, replacement, 1)

anchor = """        t.session.StartCapture();
    }
    catch (...) { return false; }
    t.ww = (float)t.lastSize.Width;"""
replacement = """        t.session.StartCapture();
    }
    catch (winrt::hresult_error const& e)
    {
        ++g_diagCaptureFailures;
        if (g_diagFirstCaptureHr == S_OK) g_diagFirstCaptureHr = e.code();
        return false;
    }
    catch (...)
    {
        ++g_diagCaptureFailures;
        if (g_diagFirstCaptureHr == S_OK) g_diagFirstCaptureHr = E_FAIL;
        return false;
    }
    t.ww = (float)t.lastSize.Width;"""
if anchor not in s:
    raise SystemExit("ERROR: AddTile catch anchor not found")
s = s.replace(anchor, replacement, 1)

anchor = """    if (g_tiles.empty())
    {
        MessageBoxW(nullptr, TL(L\"No window found to capture.\", L\"Yakalanacak pencere bulunamadı.\"),
            L\"Spatial Canvas\", MB_OK | MB_ICONWARNING);
        return 1;
    }"""
replacement = """    if (g_tiles.empty())
    {
        bool wgcSupported = false;
        try { wgcSupported = winrt::GraphicsCaptureSession::IsSupported(); } catch (...) {}
        wchar_t msg[768]{};
        swprintf_s(msg,
            L\"No window found to capture.\\n\\n\"
            L\"Corporate-safe diagnostics (no window titles/content logged):\\n\"
            L\"Eligible top-level windows: %zu\\n\"
            L\"Windows.Graphics.Capture supported: %s\\n\"
            L\"Capture failures: %d\\n\"
            L\"First capture HRESULT: 0x%08lX\",
            g_diagEligibleWindows,
            wgcSupported ? L\"YES\" : L\"NO\",
            g_diagCaptureFailures,
            static_cast<unsigned long>(g_diagFirstCaptureHr));
        MessageBoxW(nullptr, msg, L\"Spatial Canvas - Capture diagnostics\", MB_OK | MB_ICONWARNING);
        return 1;
    }"""
if anchor not in s:
    raise SystemExit("ERROR: startup error block anchor not found")
s = s.replace(anchor, replacement, 1)

p.write_text(s, encoding="utf-8")
print("OK: capture diagnostics added (counts + HRESULT only).")
