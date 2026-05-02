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
    TASKDIALOGCONFIG c = {};
    c.cbSize             = sizeof(c);
    c.hwndParent         = owner;
    c.dwFlags            = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION;
    c.dwCommonButtons    = TDCBF_OK_BUTTON;
    c.pszWindowTitle     = L"About " WIDEN(APP_NAME);
    c.pszMainInstruction = WIDEN(APP_NAME) L"  " WIDEN(APP_VERSION_STR);
    c.pszContent         =
        L"Pin any window on top of all others.\n\n"
        L"Hotkey:  Alt+F2  (toggles the focused window)\n"
        L"Right-click the tray icon for the pin list.\n\n"
        WIDEN(APP_COPYRIGHT) L"\n\n"
        L"<a href=\"" WIDEN(APP_RELEASES_URL) L"\">"
        L"Check for new releases on GitHub</a>";
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
    case WM_INITDIALOG:
        CheckDlgButton(dlg, IDC_CHK_AUTOSTART,
                       (settings::autostart_get()) ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDOK: {
            bool const want = (IsDlgButtonChecked(dlg, IDC_CHK_AUTOSTART) == BST_CHECKED);
            settings::autostart_set(want);
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
    nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_APP_TRAY;
    nid.hIcon            = hIcon;
    lstrcpynW(nid.szTip, L"WinPin  -  Alt+F2 to pin focused window",
              (int)(sizeof nid.szTip / sizeof nid.szTip[0]));

    if (!Shell_NotifyIconW(NIM_ADD, &nid)) return FALSE;

    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid);
    return TRUE;
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
