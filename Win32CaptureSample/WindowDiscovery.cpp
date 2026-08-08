#include "WindowDiscovery.h"

#include <cwchar>

namespace spatial::discovery
{
    namespace
    {
        bool IsDesktopShellSurface(const std::wstring& className)
        {
            return _wcsicmp(className.c_str(), L"Progman") == 0 ||
                _wcsicmp(className.c_str(), L"WorkerW") == 0 ||
                _wcsicmp(className.c_str(), L"Shell_TrayWnd") == 0 ||
                _wcsicmp(className.c_str(), L"Shell_SecondaryTrayWnd") == 0;
        }
    }

    WindowDecision EvaluateWindow(const WindowFacts& facts, DWORD currentProcessId)
    {
        if (!facts.isWindow) return WindowDecision::InvalidWindow;
        if (!facts.visible) return WindowDecision::Invisible;
        if (!facts.topLevel) return WindowDecision::NotTopLevel;
        if (facts.processId != 0 && facts.processId == currentProcessId)
            return WindowDecision::SelfProcess;
        if (IsDesktopShellSurface(facts.className))
            return WindowDecision::DesktopShellSurface;
        return WindowDecision::Candidate;
    }

    const wchar_t* WindowDecisionName(WindowDecision decision)
    {
        switch (decision)
        {
        case WindowDecision::Candidate: return L"candidate";
        case WindowDecision::InvalidWindow: return L"invalid_or_destroyed";
        case WindowDecision::Invisible: return L"not_visible";
        case WindowDecision::NotTopLevel: return L"not_top_level";
        case WindowDecision::SelfProcess: return L"self_process";
        case WindowDecision::DesktopShellSurface: return L"desktop_shell_surface";
        }
        return L"unknown";
    }

    AttemptSummary ProcessWindowAttempts(
        const std::vector<HWND>& windows,
        size_t maximumSuccessfulCaptures,
        const AttemptWindow& attempt)
    {
        AttemptSummary summary;
        summary.discovered = windows.size();

        for (HWND hwnd : windows)
        {
            if (summary.captured >= maximumSuccessfulCaptures) break;

            AttemptResult result;
            ++summary.attempted;
            try
            {
                result = attempt(hwnd);
            }
            catch (...)
            {
                result = { AttemptStatus::Failed, E_FAIL };
            }

            switch (result.status)
            {
            case AttemptStatus::Captured:
                ++summary.captured;
                break;
            case AttemptStatus::Skipped:
                ++summary.skipped;
                break;
            case AttemptStatus::Failed:
                ++summary.failed;
                if (summary.firstFailure == S_OK)
                    summary.firstFailure = FAILED(result.error) ? result.error : E_FAIL;
                break;
            }
        }

        return summary;
    }
}
