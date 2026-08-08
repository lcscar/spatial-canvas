#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CANVAS = ROOT / "Win32CaptureSample" / "Canvas.cpp"


def replace_exact(text: str, old: str, new: str, label: str) -> str:
    if old not in text:
        if new in text:
            print(f"[ok] {label}: already hardened")
            return text
        raise RuntimeError(f"Expected source not found for: {label}")
    print(f"[patch] {label}")
    return text.replace(old, new, 1)


def replace_function_body(text: str, signature: str, body: str, label: str) -> str:
    pos = text.find(signature)
    if pos < 0:
        raise RuntimeError(f"Function signature not found for: {label}")
    brace = text.find("{", pos)
    if brace < 0:
        raise RuntimeError(f"Function opening brace not found for: {label}")

    depth = 0
    in_string = False
    in_char = False
    escape = False
    i = brace
    while i < len(text):
        c = text[i]
        if escape:
            escape = False
        elif c == "\\" and (in_string or in_char):
            escape = True
        elif c == '"' and not in_char:
            in_string = not in_string
        elif c == "'" and not in_string:
            in_char = not in_char
        elif not in_string and not in_char:
            if c == "{":
                depth += 1
            elif c == "}":
                depth -= 1
                if depth == 0:
                    replacement = signature + "\n{\n" + body.rstrip() + "\n}"
                    print(f"[patch] {label}")
                    return text[:pos] + replacement + text[i + 1:]
        i += 1
    raise RuntimeError(f"Function closing brace not found for: {label}")


def main() -> None:
    text = CANVAS.read_text(encoding="utf-8-sig")

    # 1) Remove the only explicit network dependency from the binary.
    text = replace_exact(
        text,
        '#include <wininet.h>             // M48: yeni-sürüm bildirimi (HTTP GET)\n#pragma comment(lib, "wininet.lib")\n',
        '',
        'remove WinINet dependency',
    )

    # 2) Force safe defaults and make the settings UI unable to re-enable them.
    text = replace_exact(
        text,
        '    bool updateCheck = true; // M48: açılışta yeni-sürüm kontrolü (sadece bildirim)',
        '    bool updateCheck = false; // Corporate-safe: network update checks are disabled',
        'disable update check default',
    )
    text = replace_exact(
        text,
        '    case 7: g_set.autostart = !g_set.autostart; ApplyAutostart(); break;',
        '    case 7: g_set.autostart = false; break; // Corporate-safe: autostart disabled',
        'disable autostart UI action',
    )
    text = replace_exact(
        text,
        '    case 12: g_set.updateCheck = !g_set.updateCheck; break; // M48',
        '    case 12: g_set.updateCheck = false; break; // Corporate-safe: network disabled',
        'disable update-check UI action',
    )
    text = replace_exact(
        text,
        '        else if (k == L"updchk") g_set.updateCheck = _wtoi(v.c_str()) != 0; // M48',
        '        else if (k == L"updchk") g_set.updateCheck = false; // Corporate-safe: ignore persisted enablement',
        'ignore persisted update enablement',
    )

    # 3) Remove runtime network, registry persistence, and local IPC entry points.
    text = replace_function_body(
        text,
        'static void UpdateCheckThread(std::wstring url)',
        '    (void)url;\n    // Corporate-safe build: outbound network activity intentionally disabled.',
        'neutralize update thread',
    )
    text = replace_function_body(
        text,
        'static bool QueryAutostart()',
        '    // Corporate-safe build: do not inspect login persistence settings.\n    return false;',
        'neutralize autostart registry read',
    )
    text = replace_function_body(
        text,
        'static void ApplyAutostart()',
        '    // Corporate-safe build: never create or remove Run-key persistence.',
        'neutralize autostart registry write',
    )
    text = replace_function_body(
        text,
        'static void IpcServerThread()',
        '    // Corporate-safe baseline: no local named-pipe server.',
        'neutralize named-pipe IPC server',
    )

    # 4) Do not even create background threads for disabled facilities.
    text = replace_exact(
        text,
        '    // M30: IPC named pipe sunucusu (g_hwnd hazır - PostMessage güvenli)\n    std::thread(IpcServerThread).detach();\n    // M48: yeni-sürüm bildirimi (opt-in; ağ yoksa sessiz, sadece bildirim)\n    if (g_set.updateCheck)\n        std::thread(UpdateCheckThread, g_set.updateUrl).detach();',
        '    // Corporate-safe baseline: IPC server and network update thread are intentionally disabled.',
        'remove IPC/update thread startup',
    )

    # 5) Crash recovery still persists only what is needed to restore geometry.
    # Keep the trailing title field for file-format compatibility, but leave it empty.
    text = replace_exact(
        text,
        '        GetWindowTextW(t.source, title, 256);',
        '        // Corporate-safe: do not persist window titles (may contain confidential data).',
        'redact persisted window titles',
    )

    CANVAS.write_text(text, encoding="utf-8", newline="\n")
    print(f"[done] hardened {CANVAS}")


if __name__ == "__main__":
    main()
