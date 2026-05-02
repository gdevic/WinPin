#include "overlay.h"
#include <dwmapi.h>

/* DWMWA_WINDOW_CORNER_PREFERENCE / DWM_WINDOW_CORNER_PREFERENCE require the
   Windows 11 SDK (10.0.22000) or newer. VS 2022 ships one. */

static const WCHAR kOverlayClass[] = L"WinPinOverlay";
static HBRUSH g_bgBrush;            /* color-key fill */
static HBRUSH g_borderBrush;        /* visible border color */
static BOOL   g_os_rounds_corners;  /* TRUE on Win11+ */

/* Win11 = 10.0 build >= 22000. GetVersionEx lies due to compatibility shims;
   RtlGetVersion reports the real value. */
static BOOL detect_rounded_os(void)
{
    typedef LONG (WINAPI *RtlGetVersionFn)(OSVERSIONINFOW*);
    HMODULE nt = GetModuleHandleW(L"ntdll.dll");
    if (!nt) return FALSE;
    RtlGetVersionFn fn = (RtlGetVersionFn)(void*)GetProcAddress(nt, "RtlGetVersion");
    if (!fn) return FALSE;
    OSVERSIONINFOW v;
    ZeroMemory(&v, sizeof v);
    v.dwOSVersionInfoSize = sizeof v;
    if (fn(&v) != 0) return FALSE;
    return (v.dwMajorVersion > 10) ||
           (v.dwMajorVersion == 10 && v.dwBuildNumber >= 22000);
}

/* Effective corner radius (px) of the target's frame as currently rendered.
   Returns 0 for square corners (Win10, maximized, opted-out). */
static int target_corner_radius(HWND target)
{
    if (!g_os_rounds_corners || !target || !IsWindow(target)) return 0;
    if (IsZoomed(target)) return 0;  /* Win11 squares maximized windows */

    DWM_WINDOW_CORNER_PREFERENCE pref = DWMWCP_DEFAULT;
    DwmGetWindowAttribute(target, DWMWA_WINDOW_CORNER_PREFERENCE,
                          &pref, sizeof pref);
    if (pref == DWMWCP_DONOTROUND) return 0;

    UINT dpi = GetDpiForWindow(target);
    if (!dpi) dpi = 96;
    int base = (pref == DWMWCP_ROUNDSMALL) ? 4 : 8;
    return MulDiv(base, (int)dpi, 96);
}

static LRESULT CALLBACK overlay_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_NCHITTEST:
        /* Click-through (in addition to WS_EX_TRANSPARENT). */
        return HTTRANSPARENT;

    case WM_ERASEBKGND:
        /* We fully repaint in WM_PAINT; suppress flicker. */
        return 1;

    case WM_PAINT: {
        HWND target = (HWND)(LONG_PTR)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        const int targetR = target_corner_radius(target);
        const int t       = BORDER_THICKNESS_PX;
        /* Border bites OVERLAY_BITE_PX into the target, so its inner curve
           sits inside the target's curve by the same amount. */
        const int innerR  = (targetR > OVERLAY_BITE_PX)
                                ? targetR - OVERLAY_BITE_PX : 0;

        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        const int w = rc.right;
        const int h = rc.bottom;

        FillRect(hdc, &rc, g_bgBrush);

        if (innerR > 0) {
            /* Rounded frame:
                 outer rect = full overlay client, radius = innerR + t
                              (a curve parallel to the target's, offset by t)
                 inner rect = target's frame,    radius = innerR
               Paint the outer in border color, then punch the inner with
               the color key, leaving a uniform-thickness rounded frame. */
            const int outerR = innerR + t;
            HRGN outer = CreateRoundRectRgn(0, 0, w + 1, h + 1,
                                            outerR * 2, outerR * 2);
            HRGN inner = CreateRoundRectRgn(t, t, w - t + 1, h - t + 1,
                                            innerR * 2, innerR * 2);
            if (outer && inner) {
                FillRgn(hdc, outer, g_borderBrush);
                FillRgn(hdc, inner, g_bgBrush);
            } else {
                /* Fall back to a square frame if region creation failed. */
                FillRect(hdc, &rc, g_borderBrush);
                RECT in = { t, t, w - t, h - t };
                FillRect(hdc, &in, g_bgBrush);
            }
            if (outer) DeleteObject(outer);
            if (inner) DeleteObject(inner);
        } else {
            /* Square corners: four edge strips. */
            RECT b;
            b.left = 0;     b.top = 0;     b.right = w;   b.bottom = t;
            FillRect(hdc, &b, g_borderBrush);
            b.left = 0;     b.top = h - t; b.right = w;   b.bottom = h;
            FillRect(hdc, &b, g_borderBrush);
            b.left = 0;     b.top = t;     b.right = t;   b.bottom = h - t;
            FillRect(hdc, &b, g_borderBrush);
            b.left = w - t; b.top = t;     b.right = w;   b.bottom = h - t;
            FillRect(hdc, &b, g_borderBrush);
        }

        EndPaint(hwnd, &ps);
        return 0;
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void overlay_register_class(HINSTANCE hInst)
{
    if (!g_bgBrush)     g_bgBrush     = CreateSolidBrush(OVERLAY_COLORKEY);
    if (!g_borderBrush) g_borderBrush = CreateSolidBrush(BORDER_COLOR);
    g_os_rounds_corners = detect_rounded_os();

    WNDCLASSEXW wc = {0};
    wc.cbSize        = sizeof wc;
    wc.style         = 0;
    wc.lpfnWndProc   = overlay_proc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = kOverlayClass;
    RegisterClassExW(&wc);
}

HWND overlay_create(HINSTANCE hInst, HWND target)
{
    DWORD ex = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW
             | WS_EX_NOACTIVATE | WS_EX_TOPMOST;
    HWND hwnd = CreateWindowExW(
        ex, kOverlayClass, L"",
        WS_POPUP,
        0, 0, 0, 0,
        NULL, NULL, hInst, NULL);
    if (!hwnd) return NULL;

    /* Stash target HWND so WM_PAINT can query its current state for radius. */
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)target);

    /* Color-key transparency: pixels matching OVERLAY_COLORKEY are see-through. */
    SetLayeredWindowAttributes(hwnd, OVERLAY_COLORKEY, 0, LWA_COLORKEY);
    return hwnd;
}

void overlay_destroy(HWND overlay)
{
    if (overlay && IsWindow(overlay)) DestroyWindow(overlay);
}

void overlay_reposition(HWND overlay, HWND target)
{
    if (!overlay || !target || !IsWindow(target)) return;

    RECT r = {0};
    if (FAILED(DwmGetWindowAttribute(target, DWMWA_EXTENDED_FRAME_BOUNDS,
                                     &r, sizeof r))) {
        if (!GetWindowRect(target, &r)) return;
    }
    /* Place overlay so the border sits (t - bite) px outside the target's
       frame and (bite) px inside it. */
    const int outset = BORDER_THICKNESS_PX - OVERLAY_BITE_PX;
    InflateRect(&r, outset, outset);

    SetWindowPos(overlay, HWND_TOPMOST,
                 r.left, r.top,
                 r.right - r.left, r.bottom - r.top,
                 SWP_NOACTIVATE | SWP_NOREDRAW);
    /* Force a repaint of the new client area. */
    InvalidateRect(overlay, NULL, FALSE);
}

void overlay_set_visible(HWND overlay, BOOL visible)
{
    if (!overlay) return;
    ShowWindow(overlay, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
}
