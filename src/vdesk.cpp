#include "vdesk.h"
#include <objbase.h>
#include <shobjidl.h>

static IVirtualDesktopManager* g_vdm;

void vdesk_init(void)
{
    HRESULT hr = CoCreateInstance(
        CLSID_VirtualDesktopManager,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&g_vdm));
    if (FAILED(hr)) g_vdm = nullptr;
}

void vdesk_shutdown(void)
{
    if (g_vdm) {
        g_vdm->Release();
        g_vdm = nullptr;
    }
}

BOOL vdesk_is_window_on_current(HWND hwnd)
{
    if (!g_vdm || !hwnd) return TRUE;
    BOOL on = TRUE;
    HRESULT hr = g_vdm->IsWindowOnCurrentVirtualDesktop(hwnd, &on);
    if (FAILED(hr)) return TRUE;   /* fail-open: keep overlay visible */
    return on;
}
