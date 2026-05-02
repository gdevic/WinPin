#include "winpin.h"
#include "pins.h"
#include "overlay.h"
#include "winevents.h"
#include "tray.h"
#include "vdesk.h"
#include "resource.h"
#include <objbase.h>
#include <shellapi.h>

static const WCHAR kOwnerClass[] = L"WinPinOwnerWnd_v1";

static LRESULT CALLBACK owner_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_HOTKEY:
        if (wp == HOTKEY_ID_TOGGLE) {
            HWND fg = GetForegroundWindow();
            if (fg) pins_toggle(fg);
        }
        return 0;

    case WM_APP_TRAY: {
        /* NOTIFYICON_VERSION_4 callback layout:
             LOWORD(lp) = notification event id
             HIWORD(lp) = icon uID
             wp         = packed cursor coords */
        WORD event = LOWORD(lp);
        switch (event) {
        case WM_CONTEXTMENU:
        case WM_RBUTTONUP:
        case NIN_SELECT:
        case WM_LBUTTONDBLCLK:
            tray_show_menu(hwnd);
            break;
        }
        return 0;
    }

    case WM_COMMAND:
        tray_handle_command(hwnd, LOWORD(wp));
        return 0;

    case WM_TIMER:
        if (wp == TIMER_VDESK_POLL) pins_refresh_visibility();
        return 0;

    case WM_APP_REVEAL:
        tray_show_menu(hwnd);
        return 0;

    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static HWND register_and_create_owner(HINSTANCE hInst)
{
    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof wc;
    wc.lpfnWndProc   = owner_proc;
    wc.hInstance     = hInst;
    wc.lpszClassName = kOwnerClass;
    if (!RegisterClassExW(&wc)) return NULL;

    /* Message-only window: HWND_MESSAGE parent, no UI surface. */
    return CreateWindowExW(
        0, kOwnerClass, L"WinPin",
        0,
        0, 0, 0, 0,
        HWND_MESSAGE, NULL, hInst, NULL);
}

int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE hPrev, PWSTR cmdLine, int nShow)
{
    (void)hPrev; (void)cmdLine; (void)nShow;

    /* ---------- single-instance ---------- */
    HANDLE mutex = CreateMutexW(NULL, TRUE, WINPIN_MUTEX_NAME);
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(kOwnerClass, NULL);
        if (existing) PostMessageW(existing, WM_APP_REVEAL, 0, 0);
        CloseHandle(mutex);
        return 0;
    }

    /* ---------- COM (tray + IVirtualDesktopManager) ---------- */
    HRESULT coHr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    /* ---------- subsystems ---------- */
    HWND owner = register_and_create_owner(hInst);
    if (!owner) {
        if (SUCCEEDED(coHr)) CoUninitialize();
        if (mutex) { ReleaseMutex(mutex); CloseHandle(mutex); }
        return 1;
    }

    pins_init(hInst);
    overlay_register_class(hInst);
    vdesk_init();
    winevents_install();

    if (!tray_init(owner, hInst)) {
        MessageBoxW(NULL, L"Failed to create tray icon.", L"WinPin",
                    MB_OK | MB_ICONERROR);
    }

    if (!RegisterHotKey(owner, HOTKEY_ID_TOGGLE, HOTKEY_MOD, HOTKEY_VK)) {
        MessageBoxW(owner,
            L"Could not register the global hotkey Alt+F2 "
            L"(another app may already own it).\r\n\r\n"
            L"WinPin will keep running; pin via the tray icon menu.",
            L"WinPin", MB_OK | MB_ICONWARNING);
    }

    SetTimer(owner, TIMER_VDESK_POLL, TIMER_VDESK_MS, NULL);

    /* ---------- message loop ---------- */
    MSG m;
    while (GetMessageW(&m, NULL, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    /* ---------- shutdown ---------- */
    KillTimer(owner, TIMER_VDESK_POLL);
    UnregisterHotKey(owner, HOTKEY_ID_TOGGLE);
    winevents_uninstall();
    pins_shutdown();
    tray_shutdown();
    vdesk_shutdown();

    if (SUCCEEDED(coHr)) CoUninitialize();
    if (mutex) { ReleaseMutex(mutex); CloseHandle(mutex); }
    return (int)m.wParam;
}
