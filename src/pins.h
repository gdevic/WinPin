#pragma once
#include "winpin.h"

typedef struct Pin {
    HWND target;     /* the user's window we made topmost */
    HWND overlay;    /* our colored-border overlay window */
    BOOL visible;    /* overlay should currently be shown */
} Pin;

void  pins_init(HINSTANCE hInst);
void  pins_shutdown(void);

int   pins_count(void);
Pin*  pins_at(int index);          /* by occupied-slot index (0..count-1), else NULL */
Pin*  pins_find(HWND target);

/* Returns TRUE if the window is now pinned, FALSE if now unpinned (or rejected). */
BOOL  pins_toggle(HWND target);
void  pins_unpin(HWND target);
void  pins_unpin_all(void);

/* Re-evaluate which overlays should be visible (e.g. after virtual-desktop switch). */
void  pins_refresh_visibility(void);
