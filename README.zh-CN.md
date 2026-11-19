# EndfieldHUD for Windows 10

[English](README.md) · [简体中文](README.zh-CN.md) · [繁體中文](README.zh-TW.md) · [日本語](README.ja.md) · [한국어](README.ko.md)

> **AI 生成声明：** 本 Windows 10 移植版、文档及发布包由 AI 辅助生成，请在使用前自行审阅代码和二进制文件。

[DDDuoDuo/EndfieldHUD](https://github.com/DDDuoDuo/EndfieldHUD) 的 Windows 10 移植版——为桌面带来一层可分层、可动画的《明日方舟：终末地》HUD。

本分支面向 **Windows 10 版本 2004（build 19041）或更高版本，64 位 x64**，同时支持 Windows 11。原版 Windows 端口需要 Windows 11 build 22000 的 DWM host backdrop。

## 本分支新增内容

- 支持 Windows 10 2004+，包括全部 HUD 模块和 Windows 托盘/热键工作流。
- **背景模糊模式切换**（右键托盘图标 → **实时背景模糊**）：
  - **取消勾选 — 快照模式（默认，性能优先）**：HUD 初始化时截取一次桌面，模糊后绘制到下层合成层。
  - **勾选 — 实时模式（效果优先）**：HUD 显示期间约每 100 ms 刷新一次桌面截取，CPU 占用更高。
- 安装包/发布包最低 Windows 版本改为 build 19041（原为 22000）。
- Windows 10 上为原 Windows 11 host backdrop 提供回退；Windows 11 仍使用原生 `DWMWA_USE_HOSTBACKDROPBRUSH`。

## 下载

在本仓库的 [Releases](https://github.com/BlestHeart/EndfieldHUD-for-Windows10/releases) 下载 **Windows 10 x64 安装程序（EXE）或便携 ZIP**。

解压到任意位置并运行 `EndfieldHUD.exe`。程序常驻通知区域，按 **Ctrl + Backtick** 打开 HUD。

## 从源码构建

需要 Visual Studio Build Tools 2022（MSVC v143 + Windows SDK 10.0.26100）和 CMake 3.25+：

```bat
cmake -S windows -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --target EndfieldHUD --parallel 8
cmake --build build --config Release --target EndfieldHUD --parallel 8
```

## 说明

- Windows 10 下背景是**截取的模糊桌面**，不是 Windows 11 的实时 DWM host backdrop。实时模式会持续刷新截取，快照模式只截取一次以降低开销。
- 原版 README 保留为 [README.upstream.zh-CN.md](README.upstream.zh-CN.md)。
- 这是非官方同人项目，游戏美术与品牌归 HYPERGRYPH 及相关权利方所有；原创代码使用 MIT 许可。