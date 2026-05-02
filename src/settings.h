#pragma once

#include <windows.h>

namespace settings {

inline constexpr wchar_t kAppName[] = L"WinPin";
inline constexpr wchar_t kRunKey[]  = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

inline bool autostart_get()
{
    HKEY hk = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &hk) != ERROR_SUCCESS)
        return false;
    DWORD type = 0;
    DWORD sz   = 0;
    LONG const r = RegQueryValueExW(hk, kAppName, nullptr, &type, nullptr, &sz);
    RegCloseKey(hk);
    return ((r == ERROR_SUCCESS) && ((type == REG_SZ) || (type == REG_EXPAND_SZ)));
}

inline bool autostart_set(bool on)
{
    HKEY hk = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &hk) != ERROR_SUCCESS)
        return false;
    LONG r = ERROR_SUCCESS;
    if (on)
    {
        wchar_t buf[MAX_PATH] = {};
        DWORD const n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if ((n == 0) || (n >= MAX_PATH)) { RegCloseKey(hk); return false; }
        wchar_t quoted[MAX_PATH + 4] = {};
        wsprintfW(quoted, L"\"%s\"", buf);
        DWORD const bytes = DWORD((lstrlenW(quoted) + 1) * sizeof(wchar_t));
        r = RegSetValueExW(hk, kAppName, 0, REG_SZ,
                           reinterpret_cast<BYTE const*>(quoted), bytes);
    }
    else
    {
        r = RegDeleteValueW(hk, kAppName);
        if (r == ERROR_FILE_NOT_FOUND) r = ERROR_SUCCESS;
    }
    RegCloseKey(hk);
    return (r == ERROR_SUCCESS);
}

} // namespace settings
