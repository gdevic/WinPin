#include "pins.h"
#include "overlay.h"
#include "vdesk.h"

static Pin       g_pins[MAX_PINS];
static int       g_count;
static HINSTANCE g_hInst;

static BOOL is_pinnable(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd)) return FALSE;

    /* Reject the shell / desktop. */
    WCHAR cls[64];
    if (GetClassNameW(hwnd, cls, (int)(sizeof cls / sizeof cls[0])) == 0)
        return FALSE;

    static const WCHAR *const kBlocked[] = {
        L"Progman",
        L"WorkerW",
        L"Shell_TrayWnd",
        L"Shell_SecondaryTrayWnd",
        L"NotifyIconOverflowWindow",
        L"WinPinOverlay",   /* our own overlays */
    };
    for (size_t i = 0; i < sizeof kBlocked / sizeof kBlocked[0]; ++i) {
        if (lstrcmpW(cls, kBlocked[i]) == 0) return FALSE;
    }
    return TRUE;
}

void pins_init(HINSTANCE hInst)
{
    g_hInst = hInst;
    g_count = 0;
    ZeroMemory(g_pins, sizeof g_pins);
}

void pins_shutdown(void)
{
    pins_unpin_all();
}

int pins_count(void)
{
    return g_count;
}

Pin* pins_at(int index)
{
    if (index < 0 || index >= g_count) return NULL;
    return &g_pins[index];
}

Pin* pins_find(HWND target)
{
    for (int i = 0; i < g_count; ++i) {
        if (g_pins[i].target == target) return &g_pins[i];
    }
    return NULL;
}

static BOOL pin_window(HWND target)
{
    if (g_count >= MAX_PINS) return FALSE;
    if (!is_pinnable(target)) return FALSE;
    if (pins_find(target)) return FALSE;

    /* Make it topmost. */
    if (!SetWindowPos(target, HWND_TOPMOST, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE)) {
        /* UIPI may reject elevated targets; bail without polluting list. */
        return FALSE;
    }

    HWND overlay = overlay_create(g_hInst, target);
    /* If overlay creation fails the pin still works, just without indicator. */

    Pin *p = &g_pins[g_count++];
    p->target  = target;
    p->overlay = overlay;
    p->visible = TRUE;

    if (overlay) {
        overlay_reposition(overlay, target);
        BOOL on_current = vdesk_is_window_on_current(target);
        BOOL minimized  = IsIconic(target) ? TRUE : FALSE;
        p->visible = on_current && !minimized;
        overlay_set_visible(overlay, p->visible);
    }
    return TRUE;
}

static void remove_slot(int idx)
{
    if (idx < 0 || idx >= g_count) return;
    if (g_pins[idx].overlay) overlay_destroy(g_pins[idx].overlay);

    /* Compact: move last into the freed slot. */
    int last = g_count - 1;
    if (idx != last) g_pins[idx] = g_pins[last];
    ZeroMemory(&g_pins[last], sizeof g_pins[last]);
    g_count = last;
}

void pins_unpin(HWND target)
{
    for (int i = 0; i < g_count; ++i) {
        if (g_pins[i].target == target) {
            if (IsWindow(target)) {
                SetWindowPos(target, HWND_NOTOPMOST, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            }
            remove_slot(i);
            return;
        }
    }
}

void pins_unpin_all(void)
{
    /* Iterate from the end since remove_slot compacts. */
    while (g_count > 0) {
        HWND t = g_pins[g_count - 1].target;
        if (IsWindow(t)) {
            SetWindowPos(t, HWND_NOTOPMOST, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        remove_slot(g_count - 1);
    }
}

BOOL pins_toggle(HWND target)
{
    /* Always operate on the top-level owner. */
    HWND root = GetAncestor(target, GA_ROOT);
    if (root) target = root;

    if (pins_find(target)) {
        pins_unpin(target);
        return FALSE;
    }
    return pin_window(target);
}

void pins_refresh_visibility(void)
{
    for (int i = 0; i < g_count; ++i) {
        Pin *p = &g_pins[i];
        if (!p->overlay) continue;

        BOOL on_current = vdesk_is_window_on_current(p->target);
        BOOL minimized  = IsIconic(p->target) ? TRUE : FALSE;
        BOOL alive      = IsWindow(p->target) ? TRUE : FALSE;
        BOOL want       = alive && on_current && !minimized;

        if (want != p->visible) {
            overlay_set_visible(p->overlay, want);
            p->visible = want;
        }
        if (want) overlay_reposition(p->overlay, p->target);
    }
}
