#include "tray.h"
#include "pins.h"
#include "resource.h"
#include "settings.h"
#include "version.h"
#include <shellapi.h>
#include <commctrl.h>

#define TRAY_UID 1

static HWND      g_owner;
static HINSTANCE g_inst;

// ---------------------------------------------------------------------------
// Hotkey <-> hotkey-control encoding
// ---------------------------------------------------------------------------
//
// The msctls_hotkey32 control packs (vk, modifier-flags) as a WORD:
//   LOBYTE = virtual-key, HIBYTE = HOTKEYF_* bitmask.
// MOD_* (RegisterHotKey) and HOTKEYF_* (the control) are different bit
// values, so we convert.

static UINT hkf_to_mod(UINT hkf)
{
    UINT m = 0;
    if (hkf & HOTKEYF_ALT)     m |= MOD_ALT;
    if (hkf & HOTKEYF_CONTROL) m |= MOD_CONTROL;
    if (hkf & HOTKEYF_SHIFT)   m |= MOD_SHIFT;
    return m;
}

static UINT mod_to_hkf(UINT mod)
{
    UINT h = 0;
    if (mod & MOD_ALT)     h |= HOTKEYF_ALT;
    if (mod & MOD_CONTROL) h |= HOTKEYF_CONTROL;
    if (mod & MOD_SHIFT)   h |= HOTKEYF_SHIFT;
    return h;
}

static void format_hotkey_text(WCHAR* dst, size_t cch, UINT mod, UINT vk)
{
    dst[0] = 0;
    if (mod & MOD_CONTROL) lstrcatW(dst, L"Ctrl+");
    if (mod & MOD_ALT)     lstrcatW(dst, L"Alt+");
    if (mod & MOD_SHIFT)   lstrcatW(dst, L"Shift+");
    if (mod & MOD_WIN)     lstrcatW(dst, L"Win+");
    WCHAR name[32] = {};
    LONG  lparam = (MapVirtualKeyW(vk, MAPVK_VK_TO_VSC) << 16);
    if (GetKeyNameTextW(lparam, name, ARRAYSIZE(name)) <= 0)
        wsprintfW(name, L"VK_%02X", vk);
    lstrcpynW(dst + lstrlenW(dst), name, int(cch - lstrlenW(dst)));
}

// ---------------------------------------------------------------------------
// About box
// ---------------------------------------------------------------------------

static HRESULT CALLBACK about_callback(HWND, UINT msg, WPARAM, LPARAM lp, LONG_PTR)
{
    if (msg == TDN_HYPERLINK_CLICKED)
        ShellExecuteW(NULL, L"open", reinterpret_cast<LPCWSTR>(lp),
                      NULL, NULL, SW_SHOWNORMAL);
    return S_OK;
}

static void show_about(HWND owner)
{
    WCHAR hk[96] = {};
    format_hotkey_text(hk, ARRAYSIZE(hk),
                       settings::hotkey_mod_get(),
                       settings::hotkey_vk_get());
    WCHAR content[512] = {};
    wsprintfW(content,
        L"Pin any window on top of all others.\n\n"
        L"Hotkey:  %s  (toggles the focused window)\n"
        L"Right-click the tray icon for the pin list.\n\n"
        WIDEN(APP_COPYRIGHT) L"\n"
        WIDEN(APP_LICENSE)   L"\n\n"
        L"<a href=\"" WIDEN(APP_RELEASES_URL) L"\">"
        L"Check for new releases on GitHub</a>",
        hk);

    TASKDIALOGCONFIG c = {};
    c.cbSize             = sizeof(c);
    c.hwndParent         = owner;
    c.dwFlags            = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION;
    c.dwCommonButtons    = TDCBF_OK_BUTTON;
    c.pszWindowTitle     = L"About " WIDEN(APP_NAME);
    c.pszMainInstruction = WIDEN(APP_NAME) L"  " WIDEN(APP_VERSION_STR);
    c.pszContent         = content;
    c.pszMainIcon        = TD_INFORMATION_ICON;
    c.pfCallback         = about_callback;
    TaskDialogIndirect(&c, NULL, NULL, NULL);
}

// ---------------------------------------------------------------------------
// Settings dialog
// ---------------------------------------------------------------------------

static INT_PTR CALLBACK settings_proc(HWND dlg, UINT msg, WPARAM wp, LPARAM)
{
    switch (msg) {
    case WM_INITDIALOG: {
        CheckDlgButton(dlg, IDC_CHK_AUTOSTART,
                       (settings::autostart_get()) ? BST_CHECKED : BST_UNCHECKED);
        UINT const mod = settings::hotkey_mod_get();
        UINT const vk  = settings::hotkey_vk_get();
        WORD const packed = WORD((mod_to_hkf(mod) << 8) | (vk & 0xFF));
        SendDlgItemMessageW(dlg, IDC_HOTKEY, HKM_SETHOTKEY, packed, 0);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDOK: {
            // Hotkey
            WORD const packed = WORD(SendDlgItemMessageW(dlg, IDC_HOTKEY, HKM_GETHOTKEY, 0, 0));
            UINT const vk     = LOBYTE(packed);
            UINT const hkf    = HIBYTE(packed);
            UINT const mod    = hkf_to_mod(hkf);
            if (vk == 0) {
                MessageBoxW(dlg, L"Please choose a hotkey first.",
                            L"WinPin Settings", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            // Try the new hotkey before persisting it. If it conflicts (or
            // is otherwise invalid), restore the previous registration.
            UnregisterHotKey(g_owner, HOTKEY_ID_TOGGLE);
            if (!RegisterHotKey(g_owner, HOTKEY_ID_TOGGLE, mod | MOD_NOREPEAT, vk)) {
                settings::hotkey_register(g_owner, HOTKEY_ID_TOGGLE);
                MessageBoxW(dlg,
                    L"That hotkey is already in use by another application "
                    L"or is invalid. Please pick a different one.",
                    L"WinPin Settings", MB_OK | MB_ICONWARNING);
                return TRUE;
            }
            settings::hotkey_set(mod, vk);

            // Autostart
            settings::autostart_set(IsDlgButtonChecked(dlg, IDC_CHK_AUTOSTART) == BST_CHECKED);

            tray_update_tip();
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        }
        break;

    case WM_CLOSE:
        EndDialog(dlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

void tray_show_settings(HWND owner)
{
    DialogBoxParamW(g_inst, MAKEINTRESOURCEW(IDD_SETTINGS), owner, settings_proc, 0);
}

// ---------------------------------------------------------------------------
// Tray icon
// ---------------------------------------------------------------------------

static void build_tip(WCHAR* dst, size_t cch)
{
    WCHAR hk[96] = {};
    format_hotkey_text(hk, ARRAYSIZE(hk),
                       settings::hotkey_mod_get(),
                       settings::hotkey_vk_get());
    wsprintfW(dst, L"WinPin  -  %s to pin focused window", hk);
    dst[cch - 1] = 0;
}

BOOL tray_init(HWND owner, HINSTANCE hInst)
{
    g_owner = owner;
    g_inst  = hInst;

    /* Pick the small-icon variant from our multi-size .ico for crisp tray
       rendering at the user's current DPI. */
    HICON hIcon = (HICON)LoadImageW(
        hInst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
        GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR);
    if (!hIcon) hIcon = LoadIconW(NULL, IDI_APPLICATION);

    NOTIFYICONDATAW nid = {0};
    nid.cbSize           = sizeof nid;
    nid.hWnd             = owner;
    nid.uID              = TRAY_UID;
    /* NIF_SHOWTIP is required alongside NIF_TIP once NOTIFYICON_VERSION_4 is
       set below; otherwise the shell suppresses the standard hover tooltip. */
    nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = WM_APP_TRAY;
    nid.hIcon            = hIcon;
    build_tip(nid.szTip, sizeof nid.szTip / sizeof nid.szTip[0]);

    if (!Shell_NotifyIconW(NIM_ADD, &nid)) return FALSE;

    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid);
    return TRUE;
}

void tray_update_tip(void)
{
    if (!g_owner) return;
    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof nid;
    nid.hWnd   = g_owner;
    nid.uID    = TRAY_UID;
    nid.uFlags = NIF_TIP | NIF_SHOWTIP;
    build_tip(nid.szTip, sizeof nid.szTip / sizeof nid.szTip[0]);
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void tray_shutdown(void)
{
    if (!g_owner) return;
    NOTIFYICONDATAW nid = {0};
    nid.cbSize = sizeof nid;
    nid.hWnd   = g_owner;
    nid.uID    = TRAY_UID;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    g_owner = NULL;
}

void tray_show_menu(HWND owner)
{
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    int n = pins_count();
    if (n == 0) {
        AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"(no pinned windows)");
    } else {
        for (int i = 0; i < n && i < (IDM_PIN_LAST - IDM_PIN_FIRST + 1); ++i) {
            Pin *p = pins_at(i);
            WCHAR title[120] = L"";
            GetWindowTextW(p->target, title,
                           (int)(sizeof title / sizeof title[0]));
            if (!title[0]) lstrcpyW(title, L"(no title)");
            WCHAR label[160];
            wsprintfW(label, L"Unpin: %.120s", title);
            AppendMenuW(menu, MF_STRING, (UINT_PTR)(IDM_PIN_FIRST + i), label);
        }
        AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(menu, MF_STRING, IDM_UNPIN_ALL, L"Unpin all");
    }
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, IDM_SETTINGS, L"&Settings...");
    AppendMenuW(menu, MF_STRING, IDM_ABOUT,    L"&About...");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, IDM_EXIT,     L"E&xit");

    SetMenuDefaultItem(menu, IDM_SETTINGS, FALSE);

    POINT pt;
    GetCursorPos(&pt);
    /* Required so the popup dismisses correctly on outside-clicks. */
    SetForegroundWindow(owner);
    TrackPopupMenu(menu,
                   TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_RIGHTALIGN,
                   pt.x, pt.y, 0, owner, NULL);
    /* Per MSDN, post a benign message after TrackPopupMenu. */
    PostMessageW(owner, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

void tray_handle_command(HWND owner, WORD id)
{
    if (id >= IDM_PIN_FIRST && id <= IDM_PIN_LAST) {
        Pin *p = pins_at(id - IDM_PIN_FIRST);
        if (p) pins_unpin(p->target);
        return;
    }
    switch (id) {
    case IDM_UNPIN_ALL:
        pins_unpin_all();
        break;
    case IDM_SETTINGS:
        tray_show_settings(owner);
        break;
    case IDM_ABOUT:
        show_about(owner);
        break;
    case IDM_EXIT:
        PostMessageW(owner, WM_CLOSE, 0, 0);
        break;
    }
}
