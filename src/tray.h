#pragma once
#include "winpin.h"

BOOL  tray_init(HWND owner, HINSTANCE hInst);
void  tray_shutdown(void);
void  tray_show_menu(HWND owner);
void  tray_handle_command(HWND owner, WORD cmdId);
