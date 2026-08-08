#include "pch.h"
#include "Canvas.h"
#include "Diagnostics.h"

int __stdcall WinMain(HINSTANCE, HINSTANCE, PSTR, int)
{
    const bool logReady = spatial::diagnostics::Initialize();
    struct LogCloser
    {
        ~LogCloser() { spatial::diagnostics::Shutdown(); }
    } logCloser;
    if (!logReady)
    {
        MessageBoxW(nullptr,
            L"Spatial Canvas could not create SpatialCanvas-debug.log beside the executable.\n\n"
            L"Check that this directory is writable. The application will continue.",
            L"Spatial Canvas - Diagnostic log unavailable", MB_OK | MB_ICONWARNING);
    }

    try
    {
        spatial::diagnostics::Log(L"startup.com", L"calling winrt::init_apartment(STA)");
        winrt::init_apartment(winrt::apartment_type::single_threaded);
        spatial::diagnostics::Log(L"startup.com", L"COM/WinRT initialization succeeded");

        spatial::diagnostics::Log(L"startup.wgc", L"calling GraphicsCaptureSession::IsSupported");
        bool isCaptureSupported =
            winrt::Windows::Graphics::Capture::GraphicsCaptureSession::IsSupported();
        spatial::diagnostics::LogStartupEnvironment(isCaptureSupported);
        if (!isCaptureSupported)
        {
            std::wstring message =
                L"Windows.Graphics.Capture is unavailable on this system.\n\n"
                L"Spatial Canvas requires Windows 10 1903+ or Windows 11.\n"
                L"See SpatialCanvas-debug.log beside the executable for details.";
            MessageBoxW(nullptr, message.c_str(), L"Spatial Canvas - Capture unavailable",
                MB_OK | MB_ICONERROR);
            spatial::diagnostics::Log(L"startup.wgc", L"aborting: WGC unsupported");
            return 1;
        }

        spatial::diagnostics::Log(L"startup.canvas", L"entering RunCanvasApp");
        int result = RunCanvasApp();
        spatial::diagnostics::Log(L"shutdown", L"RunCanvasApp returned exit_code=" +
            std::to_wstring(result));
        return result;
    }
    catch (winrt::hresult_error const& error)
    {
        spatial::diagnostics::LogFailure(L"fatal.winrt", error.code());
        std::wstring message = L"Spatial Canvas failed during startup.\n\nHRESULT: " +
            spatial::diagnostics::HexHRESULT(error.code()) +
            L"\nSee SpatialCanvas-debug.log beside the executable for details.";
        MessageBoxW(nullptr, message.c_str(), L"Spatial Canvas - Startup failure",
            MB_OK | MB_ICONERROR);
        return 1;
    }
    catch (std::exception const& error)
    {
        int length = MultiByteToWideChar(CP_UTF8, 0, error.what(), -1, nullptr, 0);
        std::wstring detail(length > 0 ? static_cast<size_t>(length) : 0, L'\0');
        if (length > 1)
            MultiByteToWideChar(CP_UTF8, 0, error.what(), -1, detail.data(), length);
        if (!detail.empty() && detail.back() == L'\0') detail.pop_back();
        spatial::diagnostics::Log(L"fatal.exception", L"message=" + detail);
        MessageBoxW(nullptr,
            L"Spatial Canvas failed during startup.\n\n"
            L"See SpatialCanvas-debug.log beside the executable for details.",
            L"Spatial Canvas - Startup failure", MB_OK | MB_ICONERROR);
        return 1;
    }
    catch (...)
    {
        spatial::diagnostics::Log(L"fatal.unknown", L"unhandled non-standard exception");
        MessageBoxW(nullptr,
            L"Spatial Canvas failed during startup.\n\n"
            L"See SpatialCanvas-debug.log beside the executable for details.",
            L"Spatial Canvas - Startup failure", MB_OK | MB_ICONERROR);
        return 1;
    }
}
