#pragma once

#include <windows.h>
#include <functional>
#include <string>
#include <vector>

namespace spatial::discovery
{
    enum class WindowDecision
    {
        Candidate,
        InvalidWindow,
        Invisible,
        NotTopLevel,
        SelfProcess,
        DesktopShellSurface,
    };

    struct WindowFacts
    {
        bool isWindow = false;
        bool visible = false;
        bool topLevel = false;
        DWORD processId = 0;
        std::wstring className;
    };

    WindowDecision EvaluateWindow(const WindowFacts& facts, DWORD currentProcessId);
    const wchar_t* WindowDecisionName(WindowDecision decision);

    enum class AttemptStatus
    {
        Captured,
        Skipped,
        Failed,
    };

    struct AttemptResult
    {
        AttemptStatus status = AttemptStatus::Failed;
        HRESULT error = E_FAIL;
    };

    struct AttemptSummary
    {
        size_t discovered = 0;
        size_t attempted = 0;
        size_t captured = 0;
        size_t skipped = 0;
        size_t failed = 0;
        HRESULT firstFailure = S_OK;
    };

    using AttemptWindow = std::function<AttemptResult(HWND)>;

    // Processes each HWND independently. A failure or exception from one HWND is
    // recorded and never prevents a later HWND from being attempted.
    AttemptSummary ProcessWindowAttempts(
        const std::vector<HWND>& windows,
        const AttemptWindow& attempt);

    struct DispatchSummary
    {
        size_t discovered = 0;
        size_t dispatched = 0;
        size_t dispatchFailures = 0;
    };

    using IndependentAttempt = std::function<void(HWND)>;
    using DispatchFailure = std::function<void(HWND, HRESULT)>;
    using CaptureStartupStep = std::function<HRESULT()>;

    struct CaptureStartupResult
    {
        HRESULT optionalConfiguration = S_OK;
        HRESULT startCapture = E_FAIL;
    };

    // Optional configuration is best-effort and can never gate the required
    // StartCapture step. The baseline caller deliberately supplies no optional
    // WGC properties; this seam protects future feature-detected additions.
    CaptureStartupResult ExecuteCaptureStartup(
        const CaptureStartupStep& optionalConfiguration,
        const CaptureStartupStep& startCapture);

    // Starts every HWND on an independent worker and returns without waiting for
    // capture setup. A worker that blocks inside an OS API therefore cannot hold
    // the UI thread or prevent later HWNDs from being dispatched.
    DispatchSummary DispatchWindowAttemptsIndependently(
        const std::vector<HWND>& windows,
        const IndependentAttempt& attempt,
        const DispatchFailure& failure);
}
