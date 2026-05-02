#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

/* Maximum number of simultaneously pinned windows. */
#define MAX_PINS 32

/* Global toggle-pin hotkey ID. The actual key is user-configurable —
   defaults to Alt+F2; see settings::kDefault* and settings::hotkey_*. */
#define HOTKEY_ID_TOGGLE 1

/* Custom messages on the hidden owner window. */
#define WM_APP_TRAY      (WM_APP + 1)   /* tray icon callback */
#define WM_APP_REVEAL    (WM_APP + 2)   /* second-instance ping -> show menu */

/* Border overlay appearance. */
#define BORDER_THICKNESS_PX 3
/* The border overlaps the target's outermost OVERLAY_BITE_PX pixels on each
   side. This guarantees no see-through gap between border and window even if
   the DWM extended-frame bounds are off by a pixel. */
#define OVERLAY_BITE_PX     1
#define BORDER_COLOR        RGB(255, 96, 0)   /* orange */
#define OVERLAY_COLORKEY    RGB(0, 0, 0)      /* transparent fill color */

/* Periodic timer used to re-evaluate virtual-desktop visibility. */
#define TIMER_VDESK_POLL    1
#define TIMER_VDESK_MS      500

/* Single-instance mutex name. */
#define WINPIN_MUTEX_NAME   L"Local\\WinPin.SingleInstance"
#define WINPIN_BROADCAST    L"WinPin.Reveal"
