#include "Diagnostics.h"

#include <mutex>
#include <sstream>
#include <vector>

#pragma comment(lib, "advapi32.lib")

namespace spatial::diagnostics
{
    namespace
    {
        HANDLE g_log = INVALID_HANDLE_VALUE;
        std::wstring g_logPath;
        std::mutex g_logMutex;
        bool g_desktopMismatch = false;

        std::wstring ModulePath()
        {
            std::vector<wchar_t> buffer(512);
            for (;;)
            {
                DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                    static_cast<DWORD>(buffer.size()));
                if (length == 0) return L"";
                if (length < buffer.size() - 1)
                    return std::wstring(buffer.data(), length);
                buffer.resize(buffer.size() * 2);
            }
        }

        std::string Utf8(std::wstring_view input)
        {
            if (input.empty()) return {};
            int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                input.data(), static_cast<int>(input.size()), nullptr, 0, nullptr, nullptr);
            if (bytes <= 0) return "<utf8-conversion-failed>";
            std::string output(static_cast<size_t>(bytes), '\0');
            WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                input.data(), static_cast<int>(input.size()), output.data(), bytes, nullptr, nullptr);
            return output;
        }

        std::wstring TimestampUtc()
        {
            SYSTEMTIME st{};
            GetSystemTime(&st);
            wchar_t value[40]{};
            swprintf_s(value, L"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute,
                st.wSecond, st.wMilliseconds);
            return value;
        }

        std::wstring UserObjectName(HANDLE object)
        {
            if (!object) return L"<null>";
            DWORD bytes = 0;
            GetUserObjectInformationW(object, UOI_NAME, nullptr, 0, &bytes);
            if (bytes == 0) return L"<unavailable>";
            std::vector<wchar_t> value(bytes / sizeof(wchar_t) + 1);
            if (!GetUserObjectInformationW(object, UOI_NAME, value.data(), bytes, &bytes))
                return L"<unavailable>";
            return value.data();
        }

        std::wstring IntegrityLevel()
        {
            HANDLE token = nullptr;
            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
                return L"unavailable";
            DWORD bytes = 0;
            GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &bytes);
            std::vector<BYTE> data(bytes);
            if (bytes == 0 || !GetTokenInformation(token, TokenIntegrityLevel,
                data.data(), bytes, &bytes))
            {
                CloseHandle(token);
                return L"unavailable";
            }
            auto label = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(data.data());
            DWORD rid = *GetSidSubAuthority(label->Label.Sid,
                *GetSidSubAuthorityCount(label->Label.Sid) - 1);
            CloseHandle(token);
            if (rid < SECURITY_MANDATORY_LOW_RID) return L"untrusted";
            if (rid < SECURITY_MANDATORY_MEDIUM_RID) return L"low";
            if (rid < SECURITY_MANDATORY_HIGH_RID) return L"medium";
            if (rid < SECURITY_MANDATORY_SYSTEM_RID) return L"high";
            if (rid < SECURITY_MANDATORY_PROTECTED_PROCESS_RID) return L"system";
            return L"protected";
        }

        std::wstring AppContainerState()
        {
            HANDLE token = nullptr;
            if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
                return L"unavailable";
            DWORD value = 0;
            DWORD bytes = sizeof(value);
            BOOL ok = GetTokenInformation(token, TokenIsAppContainer, &value, bytes, &bytes);
            DWORD error = ok ? ERROR_SUCCESS : GetLastError();
            CloseHandle(token);
            if (!ok) return L"unavailable(error=" + std::to_wstring(error) + L")";
            return value ? L"true" : L"false";
        }

        std::wstring OsVersion()
        {
            using RtlGetVersionFn = LONG(WINAPI*)(OSVERSIONINFOW*);
            HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
            auto rtlGetVersion = reinterpret_cast<RtlGetVersionFn>(
                ntdll ? GetProcAddress(ntdll, "RtlGetVersion") : nullptr);
            if (!rtlGetVersion) return L"unavailable";
            OSVERSIONINFOW info{};
            info.dwOSVersionInfoSize = sizeof(info);
            if (rtlGetVersion(&info) != 0) return L"unavailable";
            return std::to_wstring(info.dwMajorVersion) + L"." +
                std::to_wstring(info.dwMinorVersion) + L" build " +
                std::to_wstring(info.dwBuildNumber);
        }

        std::wstring SystemMessage(HRESULT hr)
        {
            wchar_t* allocated = nullptr;
            DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, static_cast<DWORD>(hr), 0,
                reinterpret_cast<wchar_t*>(&allocated), 0, nullptr);
            if (!length || !allocated) return L"unavailable";
            std::wstring message(allocated, length);
            LocalFree(allocated);
            while (!message.empty() &&
                (message.back() == L'\r' || message.back() == L'\n' || message.back() == L' '))
                message.pop_back();
            return message;
        }
    }

    bool Initialize()
    {
        std::wstring executable = ModulePath();
        size_t slash = executable.find_last_of(L"\\/");
        std::wstring directory = slash == std::wstring::npos ? L"." : executable.substr(0, slash);
        g_logPath = directory + L"\\SpatialCanvas-debug.log";
        g_log = CreateFileW(g_logPath.c_str(), GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
        if (g_log == INVALID_HANDLE_VALUE) return false;
        Log(L"startup", L"Spatial Canvas 0.60.0 corporate-safe diagnostic build");
        Log(L"startup", L"executable_path=" + executable);
        Log(L"startup", L"log_path=" + g_logPath);
        return true;
    }

    void Shutdown()
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        if (g_log != INVALID_HANDLE_VALUE)
        {
            FlushFileBuffers(g_log);
            CloseHandle(g_log);
            g_log = INVALID_HANDLE_VALUE;
        }
    }

    void Log(std::wstring_view stage, std::wstring_view message)
    {
        std::lock_guard<std::mutex> lock(g_logMutex);
        if (g_log == INVALID_HANDLE_VALUE) return;
        std::wstring line = L"[" + TimestampUtc() + L"] [" + std::wstring(stage) +
            L"] " + std::wstring(message) + L"\r\n";
        std::string utf8 = Utf8(line);
        DWORD written = 0;
        WriteFile(g_log, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
        FlushFileBuffers(g_log);
    }

    void LogFailure(std::wstring_view stage, HRESULT hr, DWORD win32Error)
    {
        std::wstring message = L"HRESULT=" + HexHRESULT(hr) +
            L" interpretation=" + SystemMessage(hr);
        if (win32Error != ERROR_SUCCESS)
            message += L" Win32Error=" + std::to_wstring(win32Error) +
                L" interpretation=" + SystemMessage(HRESULT_FROM_WIN32(win32Error));
        Log(stage, message);
    }

    void LogStartupEnvironment(bool graphicsCaptureSupported)
    {
        DWORD sessionId = 0;
        BOOL sessionOk = ProcessIdToSessionId(GetCurrentProcessId(), &sessionId);
        Log(L"environment", L"pid=" + std::to_wstring(GetCurrentProcessId()) +
            L" session_id=" + (sessionOk ? std::to_wstring(sessionId) : L"unavailable") +
            L" session_error=" + std::to_wstring(sessionOk ? ERROR_SUCCESS : GetLastError()));
        Log(L"environment", L"os_version=" + OsVersion());
        Log(L"environment", L"integrity_level=" + IntegrityLevel() +
            L" app_container=" + AppContainerState());

        std::wstring station = UserObjectName(GetProcessWindowStation());
        HDESK threadDesktop = GetThreadDesktop(GetCurrentThreadId());
        std::wstring desktop = UserObjectName(threadDesktop);
        DWORD inputError = ERROR_SUCCESS;
        HDESK inputDesktop = OpenInputDesktop(0, FALSE,
            DESKTOP_ENUMERATE | DESKTOP_READOBJECTS);
        if (!inputDesktop) inputError = GetLastError();
        std::wstring inputName = inputDesktop ? UserObjectName(inputDesktop) : L"<unavailable>";
        if (inputDesktop) CloseDesktop(inputDesktop);
        g_desktopMismatch = desktop != L"<unavailable>" && inputName != L"<unavailable>" &&
            _wcsicmp(desktop.c_str(), inputName.c_str()) != 0;

        Log(L"environment", L"window_station=" + station +
            L" thread_desktop=" + desktop + L" input_desktop=" + inputName +
            L" input_desktop_error=" + std::to_wstring(inputError) +
            L" desktop_mismatch=" + (g_desktopMismatch ? L"true" : L"false"));
        Log(L"environment", L"Windows.Graphics.Capture.IsSupported=" +
            std::wstring(graphicsCaptureSupported ? L"true" : L"false"));
    }

    std::wstring LogPath()
    {
        return g_logPath;
    }

    bool DesktopMismatchDetected()
    {
        return g_desktopMismatch;
    }

    std::wstring HexHandle(HWND hwnd)
    {
        wchar_t value[32]{};
        swprintf_s(value, L"0x%p", hwnd);
        return value;
    }

    std::wstring HexHRESULT(HRESULT hr)
    {
        wchar_t value[16]{};
        swprintf_s(value, L"0x%08lX", static_cast<unsigned long>(hr));
        return value;
    }
}
