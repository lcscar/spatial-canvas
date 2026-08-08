from pathlib import Path
import re

canvas_path = Path("Win32CaptureSample/Canvas.cpp")
verify_path = Path("tools/verify_corporate_hardening.py")

source = canvas_path.read_text(encoding="utf-8")

# 1) Remove the upstream VERSION URL while preserving the now-inert setting field.
old_url = '    std::wstring updateUrl = L"https://raw.githubusercontent.com/13auth/spatial-canvas/main/VERSION";\n'
new_url = '    std::wstring updateUrl; // Corporate-safe: external update URL removed\n'
if old_url in source:
    source = source.replace(old_url, new_url, 1)
elif new_url not in source:
    raise SystemExit("FAIL: expected updateUrl initializer not found")

# 2) Remove the UI path that can open the upstream releases page in a browser.
release_block = re.compile(
    r'\s*if \(g_updateAvail && g_updateRect\.right > g_updateRect\.left &&\n'
    r'\s*cp\.x >= g_updateRect\.left && cp\.x <= g_updateRect\.right &&\n'
    r'\s*cp\.y >= g_updateRect\.top && cp\.y <= g_updateRect\.bottom\)\n'
    r'\s*\{\n'
    r'\s*ShellExecuteW\(nullptr, L"open",\n'
    r'\s*L"https://github\.com/13auth/spatial-canvas/releases/latest",\n'
    r'\s*nullptr, nullptr, SW_SHOWNORMAL\);\n'
    r'\s*return 0;\n'
    r'\s*\}\n'
)
if "https://github.com/13auth/spatial-canvas/releases/latest" in source:
    source, count = release_block.subn(
        "\n        // Corporate-safe: external update/release navigation removed.\n",
        source,
        count=1,
    )
    if count != 1:
        raise SystemExit(f"FAIL: release URL block removed {count} times")

# 3) Remove Registry App Paths lookup. Explicit paths and normal ShellExecute
# resolution remain available; no HKLM/HKCU reads are needed for the baseline.
registry_block = re.compile(
    r'\s*else // App Paths: HKLM sonra HKCU\n'
    r'\s*\{\n'
    r'\s*std::wstring key = L"SOFTWARE\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\App Paths\\\\" \+ path;\n'
    r'\s*wchar_t val\[MAX_PATH\]; DWORD sz = sizeof\(val\);\n'
    r'\s*if \(RegGetValueW\(HKEY_LOCAL_MACHINE, key\.c_str\(\), nullptr,\n'
    r'\s*RRF_RT_REG_SZ, nullptr, val, &sz\) == ERROR_SUCCESS\)\n'
    r'\s*path = val;\n'
    r'\s*else \{ sz = sizeof\(val\);\n'
    r'\s*if \(RegGetValueW\(HKEY_CURRENT_USER, key\.c_str\(\), nullptr,\n'
    r'\s*RRF_RT_REG_SZ, nullptr, val, &sz\) == ERROR_SUCCESS\)\n'
    r'\s*path = val; \}\n'
    r'\s*\}\n'
)
if "RegGetValueW(" in source:
    source, count = registry_block.subn(
        "\n        else\n        {\n            // Corporate-safe: do not query HKLM/HKCU App Paths.\n            // Explicit paths and normal executable resolution remain available.\n        }\n",
        source,
        count=1,
    )
    if count != 1:
        raise SystemExit(f"FAIL: Registry App Paths block removed {count} times")

canvas_path.write_text(source, encoding="utf-8")

# Strengthen the verifier. We intentionally KEEP the WH_MOUSE_LL hook because it
# provides global wheel/back-button ergonomics; that behavior is runtime-tested
# rather than forbidden by the source baseline.
verify = verify_path.read_text(encoding="utf-8")
anchor = '    "registry value query": "RegQueryValueExW(",\n'
addition = (
    '    "registry value query": "RegQueryValueExW(",\n'
    '    "registry App Paths lookup": "RegGetValueW(",\n'
    '    "upstream VERSION URL": "raw.githubusercontent.com/13auth/spatial-canvas",\n'
    '    "upstream release URL": "github.com/13auth/spatial-canvas/releases",\n'
)
if '"registry App Paths lookup"' not in verify:
    if anchor not in verify:
        raise SystemExit("FAIL: verifier insertion point not found")
    verify = verify.replace(anchor, addition, 1)

verify_path.write_text(verify, encoding="utf-8")

# Fail closed on the final source state.
for forbidden in (
    "RegGetValueW(",
    "RegOpenKeyExW(",
    "RegQueryValueExW(",
    "RegSetValueExW(",
    "RegDeleteValueW(",
    "InternetOpenW(",
    "InternetOpenUrlW(",
    "InternetReadFile(",
    "CreateNamedPipeW(",
    "raw.githubusercontent.com/13auth/spatial-canvas",
    "github.com/13auth/spatial-canvas/releases",
):
    if forbidden in source:
        raise SystemExit(f"FAIL: forbidden residue remains: {forbidden}")

# Preserve the global mouse hook deliberately.
for required in ("SetWindowsHookExW(WH_MOUSE_LL", "case WM_MOUSEWHEEL:"):
    if required not in source:
        raise SystemExit(f"FAIL: expected canvas input behavior missing: {required}")

print("PASS: Registry fallback and external URLs removed")
print("PASS: global mouse-wheel hook intentionally preserved")
