#pragma once
#include "winpin.h"

/* Caller must have CoInitializeEx'd the calling thread before vdesk_init. */
void  vdesk_init(void);
void  vdesk_shutdown(void);

/* TRUE if hwnd is on the user's current virtual desktop, OR the API is
   unavailable (in which case treat as visible). */
BOOL  vdesk_is_window_on_current(HWND hwnd);
