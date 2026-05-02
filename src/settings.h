#pragma once

#include <windows.h>

namespace settings {

inline constexpr wchar_t kAppName[]    = L"WinPin";
inline constexpr wchar_t kRunKey[]     = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
inline constexpr wchar_t kAppKey[]     = L"SOFTWARE\\Baltazar Studios, LLC\\WinPin";
inline constexpr wchar_t kHotkeyMod[]  = L"HotkeyMod";
inline constexpr wchar_t kHotkeyVk[]   = L"HotkeyVk";

// Default hotkey: Alt+F2.
inline constexpr UINT kDefaultMod = MOD_ALT;
inline constexpr UINT kDefaultVk  = VK_F2;

// ---------------------------------------------------------------------------
// Autostart (HKCU Run key)
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// App-settings DWORD helpers (HKCU\SOFTWARE\Baltazar Studios, LLC\WinPin)
// ---------------------------------------------------------------------------

inline UINT read_dword(const wchar_t* name, UINT def)
{
    HKEY hk = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kAppKey, 0, KEY_QUERY_VALUE, &hk) != ERROR_SUCCESS)
        return def;
    DWORD v    = 0;
    DWORD sz   = sizeof(v);
    DWORD type = 0;
    LONG const r = RegQueryValueExW(hk, name, nullptr, &type, reinterpret_cast<BYTE*>(&v), &sz);
    RegCloseKey(hk);
    return ((r == ERROR_SUCCESS) && (type == REG_DWORD)) ? UINT(v) : def;
}

inline bool write_dword(const wchar_t* name, UINT v)
{
    HKEY  hk   = NULL;
    DWORD disp = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kAppKey, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &hk, &disp) != ERROR_SUCCESS)
        return false;
    DWORD const dv = DWORD(v);
    LONG const  r  = RegSetValueExW(hk, name, 0, REG_DWORD,
                                    reinterpret_cast<BYTE const*>(&dv), sizeof(dv));
    RegCloseKey(hk);
    return (r == ERROR_SUCCESS);
}

// ---------------------------------------------------------------------------
// Hotkey
// ---------------------------------------------------------------------------

inline UINT hotkey_mod_get() { return read_dword(kHotkeyMod, kDefaultMod); }
inline UINT hotkey_vk_get()  { return read_dword(kHotkeyVk,  kDefaultVk);  }

inline void hotkey_set(UINT mod, UINT vk)
{
    write_dword(kHotkeyMod, mod);
    write_dword(kHotkeyVk,  vk);
}

// (Re-)register the global toggle hotkey for 'owner' under 'id' from the
// persisted settings. Returns true on success.
inline bool hotkey_register(HWND owner, int id)
{
    UnregisterHotKey(owner, id);
    return (RegisterHotKey(owner, id,
                           hotkey_mod_get() | MOD_NOREPEAT,
                           hotkey_vk_get()) != 0);
}

} // namespace settings
