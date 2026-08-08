#pragma once

#include <windows.h>
#include <string>
#include <string_view>

namespace spatial::diagnostics
{
    // Creates SpatialCanvas-debug.log beside the running executable.
    bool Initialize();
    void Shutdown();
    void Log(std::wstring_view stage, std::wstring_view message);
    void LogFailure(std::wstring_view stage, HRESULT hr, DWORD win32Error = ERROR_SUCCESS);
    void LogStartupEnvironment(bool graphicsCaptureSupported);
    std::wstring LogPath();
    bool DesktopMismatchDetected();
    std::wstring HexHandle(HWND hwnd);
    std::wstring HexHRESULT(HRESULT hr);
}
