#pragma once
#include "winpin.h"

void  overlay_register_class(HINSTANCE hInst);
HWND  overlay_create(HINSTANCE hInst, HWND target);
void  overlay_destroy(HWND overlay);
void  overlay_reposition(HWND overlay, HWND target);
void  overlay_set_visible(HWND overlay, BOOL visible);
