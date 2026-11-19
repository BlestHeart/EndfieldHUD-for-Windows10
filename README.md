# EndfieldHUD for Windows 10

[English](README.md) · [简体中文](README.zh-CN.md) · [繁體中文](README.zh-TW.md) · [日本語](README.ja.md) · [한국어](README.ko.md)

> **AI-generated notice:** This Windows 10 port, its documentation and release packages were produced with AI assistance. Please review the code and binaries before relying on them.

Windows 10 port of [DDDuoDuo/EndfieldHUD](https://github.com/DDDuoDuo/EndfieldHUD) — a layered, animated *Arknights: Endfield* HUD for your desktop.

This fork targets **Windows 10 version 2004 (build 19041) or later, x64**, and also runs on Windows 11. The original Windows port required Windows 11 build 22000 for its DWM host backdrop.

## What this fork adds

- Windows 10 2004+ support, including all HUD modules and the Windows tray/hotkey workflow.
- **Background blur mode switch** (right-click the tray icon → **Real-time background blur**):
  - **Unchecked — Snapshot mode (default, performance priority):** the desktop is captured once when the HUD initializes and blurred into the lower composition layer.
  - **Checked — Real-time mode (effect priority):** the desktop capture is refreshed about every 100 ms while the HUD is visible, with correspondingly higher CPU usage.
- Windows 10-compatible Setup/package minimum version (build 19041 instead of 22000).
- Windows 10 fallback for the original Windows 11 host backdrop, while Windows 11 continues to use the native `DWMWA_USE_HOSTBACKDROPBRUSH` path.

## Download

See this repository's [Releases](https://github.com/BlestHeart/EndfieldHUD-for-Windows10/releases) for the **Windows 10 x64 Setup EXE or portable ZIP**.

Unzip it anywhere and run `EndfieldHUD.exe`. The app uses the notification area; press **Ctrl + Backtick** to open the HUD.

## Build from source

Requires Visual Studio Build Tools 2022 (MSVC v143 + Windows SDK 10.0.26100) and CMake 3.25+:

```bat
cmake -S windows -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --target EndfieldHUD --parallel 8
cmake --build build --config Release --target EndfieldHUD --parallel 8
```

## Notes

- On Windows 10 the background is a **captured blurred desktop**, not the live DWM host backdrop available on Windows 11. Real-time mode refreshes that capture continuously; snapshot mode keeps a single capture for lower overhead.
- The original upstream README is preserved as [README.upstream.md](README.upstream.md).
- This is an unofficial fan project. Game artwork and branding belong to HYPERGRYPH and their respective owners; original code is MIT licensed.