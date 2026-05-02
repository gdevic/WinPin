# WinPin

A tiny Win32 utility that pins any window on top of all others. Press a global
hotkey while a window is focused — it becomes always-on-top and gets a colored
border. Press the hotkey again to unpin. Pure Win32 + C++17, no MFC, no Qt,
no .NET. Single static-CRT binary, no installer, no admin required.

## Behavior

* **Hotkey** — toggles pin on the currently focused top-level window. Default
  is `Alt + F2`; configurable in **Settings** (any modifier + key combo).
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
* **Settings...** — pick a hotkey, toggle Start with Windows
* **About** / **Exit**

Double-click the tray icon to open Settings. Launching `WinPin.exe` while it
is already running re-opens the tray menu of the existing instance.

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

* **Hotkey** — pick any modifier + key in the Settings dialog. Persisted in
  `HKCU\SOFTWARE\Baltazar Studios, LLC\WinPin` as `HotkeyMod` / `HotkeyVk`
  DWORDs.
* **Border / pin cap** — compile-time knobs in `src\winpin.h`
  (`BORDER_COLOR`, `BORDER_THICKNESS_PX`, `MAX_PINS`).

## Layout

```
src\main.cpp          WinMain, message loop, hotkey, single-instance, glue
src\pins.cpp/.h       Pin table (pin / unpin / toggle)
src\overlay.cpp/.h    Layered click-through border-overlay window
src\winevents.cpp/.h  SetWinEventHook dispatch (move, destroy, minimize, fg)
src\tray.cpp/.h       Tray icon, context menu, Settings dialog
src\settings.h        Autostart + hotkey registry helpers
src\vdesk.cpp/.h      IVirtualDesktopManager wrapper
src\winpin.h          Shared constants / common include
src\version.h         Version + release URL (single source of truth)
src\resource.h        Resource IDs
res\app.rc            Icon + version info
res\app.manifest      PerMonitorV2 DPI, asInvoker, comctl6, Win10/11
res\app.ico           Application icon
res\make_icon.ps1     Regenerates app.ico
build.bat             Direct-cl build via vswhere + vcvarsall
.github\workflows\    GitHub Actions CI (Windows runner)
```

## Releases

Latest builds: <https://github.com/gdevic/WinPin/releases>.
Tray menu → **About WinPin** shows the running version and a link.

## License

Copyright (c) 2026 Baltazar Studios, LLC.

Licensed under [Creative Commons Attribution-NonCommercial-ShareAlike 4.0
International](LICENSE) (CC BY-NC-SA 4.0).
