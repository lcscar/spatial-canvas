#include "WindowDiscovery.h"

#include <iostream>
#include <stdexcept>

namespace
{
    int failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            ++failures;
            std::cerr << "FAILED: " << message << '\n';
        }
    }

    spatial::discovery::WindowFacts NormalWindow()
    {
        spatial::discovery::WindowFacts facts;
        facts.isWindow = true;
        facts.visible = true;
        facts.topLevel = true;
        facts.processId = 200;
        facts.className = L"ConventionalApplicationWindow";
        return facts;
    }
}

int main()
{
    using namespace spatial::discovery;

    {
        auto facts = NormalWindow();
        Expect(EvaluateWindow(facts, 100) == WindowDecision::Candidate,
            "a conventional visible top-level application window is a candidate");
    }
    {
        auto facts = NormalWindow();
        facts.className = L"ApplicationFrameWindow";
        Expect(EvaluateWindow(facts, 100) == WindowDecision::Candidate,
            "UWP/WinUI framing is not pre-rejected");
    }
    {
        auto facts = NormalWindow();
        facts.className = L"Chrome_WidgetWin_1";
        Expect(EvaluateWindow(facts, 100) == WindowDecision::Candidate,
            "Chromium/Electron classes are not pre-rejected");
    }
    {
        auto facts = NormalWindow();
        facts.processId = 100;
        Expect(EvaluateWindow(facts, 100) == WindowDecision::SelfProcess,
            "Spatial Canvas's process is excluded to prevent recursive capture");
    }
    {
        auto facts = NormalWindow();
        facts.className = L"Progman";
        Expect(EvaluateWindow(facts, 100) == WindowDecision::DesktopShellSurface,
            "the desktop Progman surface is excluded");
        facts.className = L"WorkerW";
        Expect(EvaluateWindow(facts, 100) == WindowDecision::DesktopShellSurface,
            "the desktop WorkerW surface is excluded");
    }
    {
        auto facts = NormalWindow();
        facts.visible = false;
        Expect(EvaluateWindow(facts, 100) == WindowDecision::Invisible,
            "an invisible object is structurally skipped");
        facts = NormalWindow();
        facts.topLevel = false;
        Expect(EvaluateWindow(facts, 100) == WindowDecision::NotTopLevel,
            "a child/non-top-level HWND is structurally skipped");
    }
    {
        std::vector<HWND> windows{
            reinterpret_cast<HWND>(1), reinterpret_cast<HWND>(2), reinterpret_cast<HWND>(3) };
        auto summary = ProcessWindowAttempts(windows, 10, [](HWND hwnd) -> AttemptResult {
            if (hwnd == reinterpret_cast<HWND>(1))
                return { AttemptStatus::Failed, E_ACCESSDENIED };
            if (hwnd == reinterpret_cast<HWND>(2))
                return { AttemptStatus::Captured, S_OK };
            throw std::runtime_error("synthetic per-window failure");
        });
        Expect(summary.discovered == 3 && summary.attempted == 3,
            "enumeration results are retained and every HWND is attempted");
        Expect(summary.captured == 1 && summary.failed == 2,
            "failure of one HWND does not stop later HWNDs");
        Expect(summary.firstFailure == E_ACCESSDENIED,
            "the first HRESULT is retained exactly");
    }
    {
        std::vector<HWND> none;
        auto empty = ProcessWindowAttempts(none, 10, [](HWND) {
            return AttemptResult{ AttemptStatus::Captured, S_OK };
        });
        std::vector<HWND> failing{ reinterpret_cast<HWND>(1) };
        auto failed = ProcessWindowAttempts(failing, 10, [](HWND) {
            return AttemptResult{ AttemptStatus::Failed, HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED) };
        });
        Expect(empty.discovered == 0 && empty.attempted == 0,
            "zero enumeration is represented distinctly");
        Expect(failed.discovered == 1 && failed.attempted == 1 && failed.failed == 1,
            "zero successful captures after an attempt is represented distinctly");
        Expect(failed.firstFailure == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED),
            "Win32-derived HRESULTs remain intact");
    }
    {
        std::vector<HWND> windows{
            reinterpret_cast<HWND>(1), reinterpret_cast<HWND>(2), reinterpret_cast<HWND>(3) };
        auto summary = ProcessWindowAttempts(windows, 1, [](HWND hwnd) {
            return hwnd == reinterpret_cast<HWND>(1)
                ? AttemptResult{ AttemptStatus::Failed, E_FAIL }
                : AttemptResult{ AttemptStatus::Captured, S_OK };
        });
        Expect(summary.attempted == 2 && summary.captured == 1,
            "a failed HWND does not consume the successful-capture limit");
    }

    if (failures != 0)
    {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All WindowDiscovery tests passed\n";
    return 0;
}
