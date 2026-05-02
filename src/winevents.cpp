#include "winevents.h"
#include "pins.h"
#include "overlay.h"
#include "vdesk.h"

static HWINEVENTHOOK g_hook_fg;
static HWINEVENTHOOK g_hook_min;
static HWINEVENTHOOK g_hook_destroy;
static HWINEVENTHOOK g_hook_loc;

static void CALLBACK hook_proc(HWINEVENTHOOK hook, DWORD event, HWND hwnd,
                               LONG idObject, LONG idChild,
                               DWORD eventThread, DWORD eventTime)
{
    (void)hook; (void)idChild; (void)eventThread; (void)eventTime;

    /* Only care about top-level windows for OBJECT_* events. */
    if ((event == EVENT_OBJECT_LOCATIONCHANGE || event == EVENT_OBJECT_DESTROY)
        && idObject != OBJID_WINDOW) {
        return;
    }

    switch (event) {
    case EVENT_OBJECT_LOCATIONCHANGE: {
        if (!hwnd) return;
        Pin *p = pins_find(hwnd);
        if (p && p->overlay && p->visible) {
            overlay_reposition(p->overlay, hwnd);
        }
        break;
    }
    case EVENT_OBJECT_DESTROY: {
        if (!hwnd) return;
        if (pins_find(hwnd)) pins_unpin(hwnd);
        break;
    }
    case EVENT_SYSTEM_MINIMIZESTART: {
        if (!hwnd) return;
        Pin *p = pins_find(hwnd);
        if (p && p->overlay) {
            overlay_set_visible(p->overlay, FALSE);
            p->visible = FALSE;
        }
        break;
    }
    case EVENT_SYSTEM_MINIMIZEEND: {
        if (!hwnd) return;
        Pin *p = pins_find(hwnd);
        if (p && p->overlay && vdesk_is_window_on_current(hwnd)) {
            overlay_set_visible(p->overlay, TRUE);
            overlay_reposition(p->overlay, hwnd);
            p->visible = TRUE;
        }
        break;
    }
    case EVENT_SYSTEM_FOREGROUND:
        /* Foreground changes also fire on virtual-desktop switches.
           Cheapest correct response: re-evaluate every pinned overlay. */
        pins_refresh_visibility();
        break;
    }
}

void winevents_install(void)
{
    const DWORD flags = WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS;

    g_hook_fg = SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        NULL, hook_proc, 0, 0, flags);

    g_hook_min = SetWinEventHook(
        EVENT_SYSTEM_MINIMIZESTART, EVENT_SYSTEM_MINIMIZEEND,
        NULL, hook_proc, 0, 0, flags);

    g_hook_destroy = SetWinEventHook(
        EVENT_OBJECT_DESTROY, EVENT_OBJECT_DESTROY,
        NULL, hook_proc, 0, 0, flags);

    g_hook_loc = SetWinEventHook(
        EVENT_OBJECT_LOCATIONCHANGE, EVENT_OBJECT_LOCATIONCHANGE,
        NULL, hook_proc, 0, 0, flags);
}

void winevents_uninstall(void)
{
    if (g_hook_fg)      { UnhookWinEvent(g_hook_fg);      g_hook_fg = NULL; }
    if (g_hook_min)     { UnhookWinEvent(g_hook_min);     g_hook_min = NULL; }
    if (g_hook_destroy) { UnhookWinEvent(g_hook_destroy); g_hook_destroy = NULL; }
    if (g_hook_loc)     { UnhookWinEvent(g_hook_loc);     g_hook_loc = NULL; }
}
