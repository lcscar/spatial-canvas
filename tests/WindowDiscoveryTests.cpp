#include "WindowDiscovery.h"

#include <iostream>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
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
        auto summary = ProcessWindowAttempts(windows, [](HWND hwnd) -> AttemptResult {
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
        auto empty = ProcessWindowAttempts(none, [](HWND) {
            return AttemptResult{ AttemptStatus::Captured, S_OK };
        });
        std::vector<HWND> failing{ reinterpret_cast<HWND>(1) };
        auto failed = ProcessWindowAttempts(failing, [](HWND) {
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
        std::vector<HWND> windows;
        for (uintptr_t value = 1; value <= 24; ++value)
            windows.push_back(reinterpret_cast<HWND>(value));
        auto summary = ProcessWindowAttempts(windows, [](HWND) {
            return AttemptResult{ AttemptStatus::Captured, S_OK };
        });
        Expect(summary.attempted == 24 && summary.captured == 24,
            "capture processing is not capped at 12 or 16 successful windows");
    }
    {
        bool startCalled = false;
        auto result = ExecuteCaptureStartup(
            [] { return E_INVALIDARG; },
            [&] { startCalled = true; return S_OK; });
        Expect(result.optionalConfiguration == E_INVALIDARG,
            "an optional capture configuration error is retained");
        Expect(startCalled && result.startCapture == S_OK,
            "an optional capture configuration failure does not prevent StartCapture");
    }
    {
        std::vector<HWND> windows;
        for (uintptr_t value = 1; value <= 24; ++value)
            windows.push_back(reinterpret_cast<HWND>(value));

        struct AsyncState
        {
            std::mutex mutex;
            std::condition_variable changed;
            bool releaseFirst = false;
            std::atomic<int> started = 0;
            std::atomic<int> finished = 0;
        };
        auto state = std::make_shared<AsyncState>();
        auto before = std::chrono::steady_clock::now();
        auto dispatch = DispatchWindowAttemptsIndependently(windows,
            [state](HWND hwnd) {
                ++state->started;
                state->changed.notify_all();
                if (hwnd == reinterpret_cast<HWND>(1))
                {
                    std::unique_lock lock(state->mutex);
                    state->changed.wait(lock, [&] { return state->releaseFirst; });
                }
                ++state->finished;
                state->changed.notify_all();
            },
            [state](HWND, HRESULT) {
                ++state->finished;
                state->changed.notify_all();
            });
        auto dispatchElapsed = std::chrono::steady_clock::now() - before;

        {
            std::unique_lock lock(state->mutex);
            state->changed.wait_for(lock, std::chrono::seconds(5), [&] {
                return state->started.load() == static_cast<int>(windows.size());
            });
        }
        Expect(dispatch.discovered == 24 && dispatch.dispatched == 24 &&
            dispatch.dispatchFailures == 0,
            "every discovered candidate is dispatched without a 12/16 ceiling");
        Expect(state->started.load() == 24,
            "a blocked candidate cannot prevent later candidates from starting");
        Expect(dispatchElapsed < std::chrono::seconds(2),
            "dispatch returns before capture attempts complete so UI startup is independent");

        {
            std::lock_guard lock(state->mutex);
            state->releaseFirst = true;
        }
        state->changed.notify_all();
        {
            std::unique_lock lock(state->mutex);
            state->changed.wait_for(lock, std::chrono::seconds(5), [&] {
                return state->finished.load() == static_cast<int>(windows.size());
            });
        }
        Expect(state->finished.load() == 24,
            "all independently dispatched candidates eventually complete when unblocked");
    }

    if (failures != 0)
    {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All WindowDiscovery tests passed\n";
    return 0;
}
