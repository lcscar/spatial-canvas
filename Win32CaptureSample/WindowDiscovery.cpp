#include "WindowDiscovery.h"

#include <atomic>
#include <condition_variable>
#include <cwchar>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

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
        const AttemptWindow& attempt)
    {
        AttemptSummary summary;
        summary.discovered = windows.size();

        for (HWND hwnd : windows)
        {
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

    struct BoundedAttemptExecutor::State
    {
        std::mutex mutex;
        std::condition_variable changed;
        std::deque<HWND> pending;
        IndependentAttempt attempt;
        DispatchFailure failure;
        WorkerLifecycle workerStart;
        WorkerLifecycle workerStop;
        std::atomic<bool> closing{ false };
        std::atomic_size_t active{ 0 };
        std::atomic_size_t peakActive{ 0 };
    };

    BoundedAttemptExecutor::BoundedAttemptExecutor(size_t workerCount,
        IndependentAttempt attempt,
        DispatchFailure failure,
        WorkerLifecycle workerStart,
        WorkerLifecycle workerStop)
        : state_(std::make_shared<State>())
    {
        state_->attempt = std::move(attempt);
        state_->failure = std::move(failure);
        state_->workerStart = std::move(workerStart);
        state_->workerStop = std::move(workerStop);

        const size_t requested = workerCount == 0 ? 1 : workerCount;
        for (size_t index = 0; index < requested; ++index)
        {
            try
            {
                auto state = state_;
                workers_.emplace_back([state]
                {
                    bool workerReady = true;
                    try { if (state->workerStart) state->workerStart(); }
                    catch (...) { workerReady = false; }

                    for (;;)
                    {
                        HWND hwnd{};
                        {
                            std::unique_lock lock(state->mutex);
                            state->changed.wait(lock, [&]
                            {
                                return state->closing.load() || !state->pending.empty();
                            });
                            if (state->closing.load() && state->pending.empty()) break;
                            hwnd = state->pending.front();
                            state->pending.pop_front();
                        }

                        const size_t active = state->active.fetch_add(1) + 1;
                        size_t peak = state->peakActive.load();
                        while (active > peak &&
                            !state->peakActive.compare_exchange_weak(peak, active)) {}
                        if (!workerReady)
                        {
                            if (state->failure) state->failure(hwnd, E_FAIL);
                        }
                        else
                        {
                            try
                            {
                                if (state->attempt) state->attempt(hwnd);
                            }
                            catch (...)
                            {
                                if (state->failure) state->failure(hwnd, E_FAIL);
                            }
                        }
                        state->active.fetch_sub(1);
                    }

                    try { if (workerReady && state->workerStop) state->workerStop(); }
                    catch (...) {}
                });
            }
            catch (...)
            {
                break;
            }
        }
    }

    BoundedAttemptExecutor::~BoundedAttemptExecutor()
    {
        Stop(false);
    }

    DispatchSummary BoundedAttemptExecutor::Submit(const std::vector<HWND>& windows)
    {
        DispatchSummary summary;
        summary.discovered = windows.size();
        if (!state_ || stopped_ || workers_.empty())
        {
            summary.dispatchFailures = windows.size();
            if (state_ && state_->failure)
                for (HWND hwnd : windows) state_->failure(hwnd, E_OUTOFMEMORY);
            return summary;
        }
        {
            std::lock_guard lock(state_->mutex);
            for (HWND hwnd : windows) state_->pending.push_back(hwnd);
        }
        summary.dispatched = windows.size();
        state_->changed.notify_all();
        return summary;
    }

    void BoundedAttemptExecutor::Stop(bool waitForWorkers)
    {
        if (stopped_) return;
        stopped_ = true;
        if (state_)
        {
            state_->closing = true;
            {
                std::lock_guard lock(state_->mutex);
                state_->pending.clear();
            }
            state_->changed.notify_all();
        }
        for (auto& worker : workers_)
        {
            if (!worker.joinable()) continue;
            if (waitForWorkers) worker.join();
            else worker.detach();
        }
        workers_.clear();
    }

    size_t BoundedAttemptExecutor::WorkerCount() const noexcept
    {
        return workers_.size();
    }

    size_t BoundedAttemptExecutor::ActiveWorkers() const noexcept
    {
        return state_ ? state_->active.load() : 0;
    }

    size_t BoundedAttemptExecutor::PeakActiveWorkers() const noexcept
    {
        return state_ ? state_->peakActive.load() : 0;
    }

    CaptureStartupResult ExecuteCaptureStartup(
        const CaptureStartupStep& optionalConfiguration,
        const CaptureStartupStep& startCapture)
    {
        CaptureStartupResult result;
        if (optionalConfiguration)
        {
            try
            {
                result.optionalConfiguration = optionalConfiguration();
            }
            catch (...)
            {
                result.optionalConfiguration = E_FAIL;
            }
        }

        if (!startCapture) return result;
        try
        {
            result.startCapture = startCapture();
        }
        catch (...)
        {
            result.startCapture = E_FAIL;
        }
        return result;
    }
}
