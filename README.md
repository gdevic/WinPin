# WinPin

A tiny Win32 utility that pins any window on top of all others. Press a global
hotkey while a window is focused — it becomes always-on-top and gets a colored
border. Press the hotkey again to unpin. Pure Win32 + C++17, no MFC, no Qt,
no .NET. Single static-CRT binary, no installer, no admin required.

## Behavior

* **Hotkey** `Alt + F2` — toggles pin on the currently focused top-level window.
* **Border overlay** — a 3-px orange frame is drawn around each pinned window,
  rounded on Windows 11 to match the OS frame.
* **Auto-hide** — the border hides while the target is minimized or on a
  different virtual desktop, and reappears when it comes back.
* **Per-monitor DPI** aware; works across mixed-DPI multi-monitor setups.
* **In-memory only** — pins clear when WinPin exits.

## Run

```
WinPin.exe
```

A tray icon appears. Right-click for the menu:

* The list of currently pinned windows (click any entry to unpin it)
* **Unpin all**
* **About** / **Exit**

Launching `WinPin.exe` while it is already running re-opens the tray menu of
the existing instance.

## Build

Requires Visual Studio 2022 with the **Desktop development with C++** workload
(MSVC v143 toolset, Windows 10/11 SDK).

```cmd
build.bat
```

The script locates VS2022 via `vswhere`, calls `vcvarsall x64`, runs `rc` and
`cl` directly, and emits `WinPin.exe` next to `build.bat`. Intermediate files
go to `build\`.

## Limitations

* **Elevated apps** — WinPin runs `asInvoker`, so it cannot manipulate windows
  of processes running with higher integrity (UAC-elevated). Run WinPin
  elevated to pin elevated windows.
* **Exclusive fullscreen apps** (some games) ignore topmost windows; the border
  may not be drawn over them.

## Customizing

The handful of knobs are in `src\winpin.h`:

* `HOTKEY_MOD` / `HOTKEY_VK` — change the global hotkey.
* `BORDER_COLOR` / `BORDER_THICKNESS_PX` — change the highlight border.
* `MAX_PINS` — raise the cap on simultaneously pinned windows.

## Layout

```
src\main.cpp          WinMain, message loop, hotkey, single-instance, glue
src\pins.cpp/.h       Pin table (pin / unpin / toggle)
src\overlay.cpp/.h    Layered click-through border-overlay window
src\winevents.cpp/.h  SetWinEventHook dispatch (move, destroy, minimize, fg)
src\tray.cpp/.h       Tray icon + dynamic context menu
src\vdesk.cpp/.h      IVirtualDesktopManager wrapper
src\winpin.h          Shared constants / common include
src\resource.h        Resource IDs
res\app.rc            Icon + version info
res\app.manifest      PerMonitorV2 DPI, asInvoker, comctl6, Win10/11
res\app.ico           Application icon
res\make_icon.ps1     Regenerates app.ico
build.bat             Direct-cl build via vswhere + vcvarsall
.github\workflows\    GitHub Actions CI (Windows runner)
```
