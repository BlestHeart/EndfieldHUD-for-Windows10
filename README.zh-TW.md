# EndfieldHUD for Windows 10

[English](README.md) · [简体中文](README.zh-CN.md) · [繁體中文](README.zh-TW.md) · [日本語](README.ja.md) · [한국어](README.ko.md)

> **AI 生成聲明：** 本 Windows 10 移植版、文件及發行包由 AI 輔助生成，請在使用前自行審閱程式碼與二進位檔。

[DDDuoDuo/EndfieldHUD](https://github.com/DDDuoDuo/EndfieldHUD) 的 Windows 10 移植版——為桌面帶來一層可分層、可動畫的《明日方舟：終末地》HUD。

本分支面向 **Windows 10 版本 2004（build 19041）或更新版本，64 位元 x64**，同時支援 Windows 11。原版 Windows 連接埠需要 Windows 11 build 22000 的 DWM host backdrop。

## 本分支新增內容

- 支援 Windows 10 2004+，包含全部 HUD 模組與 Windows 系統匣/快速鍵工作流程。
- **背景模糊模式切換**（右鍵系統匣圖示 → **即時背景模糊**）：
  - **取消勾選 — 快照模式（預設，效能優先）**：HUD 初始化時擷取一次桌面，模糊後繪製到下層合成層。
  - **勾選 — 即時模式（效果優先）**：HUD 顯示期間約每 100 ms 更新一次桌面擷取，CPU 使用率較高。
- 安裝程式/發行包最低 Windows 版本改為 build 19041（原本為 22000）。
- 在 Windows 10 為原始 Windows 11 host backdrop 提供退路；Windows 11 仍使用原生 `DWMWA_USE_HOSTBACKDROPBRUSH`。

## 下載

在本倉庫的 [Releases](https://github.com/BlestHeart/EndfieldHUD-for-Windows10/releases) 下載 **Windows 10 x64 安裝程式（EXE）或可攜式 ZIP**。

解壓到任意位置並執行 `EndfieldHUD.exe`。程式常駐通知區域，按 **Ctrl + Backtick** 開啟 HUD。

## 從原始碼建置

需要 Visual Studio Build Tools 2022（MSVC v143 + Windows SDK 10.0.26100）與 CMake 3.25+：

```bat
cmake -S windows -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --target EndfieldHUD --parallel 8
cmake --build build --config Release --target EndfieldHUD --parallel 8
```

## 說明

- Windows 10 的背景是**擷取的模糊桌面**，不是 Windows 11 的即時 DWM host backdrop。即時模式會持續更新擷取，快照模式只擷取一次以降低負擔。
- 原版 README 保留為 [README.upstream.zh-TW.md](README.upstream.zh-TW.md)。
- 這是非官方同人專案，遊戲美術與品牌歸 HYPERGRYPH 及相關權利方所有；原創程式碼使用 MIT 授權。