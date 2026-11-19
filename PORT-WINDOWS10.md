# EndfieldHUD Windows 10 移植说明

> **AI 生成声明：** 本移植版、文档及发布包由 AI 辅助生成，请在使用前自行审阅。

## 目标
让上游 EndfieldHUD Windows 端（原本只支持 Windows 11 build 22000+）在 Windows 10 2004（build 19041）及以上运行，同时保留 Windows 11 原有的 host backdrop 体验与全部 HUD 功能。

## 核心改动
1. **桌面背景（最关键）**：`windows/native/desktop_backdrop.cpp` / `.hpp`
   - 用 `RtlGetVersion` 判断真实 Windows 版本，仅 Win11 build 22000+ 使用 `DWMWA_USE_HOSTBACKDROPBRUSH` + `CreateHostBackdropBrush`。
   - Windows 10 主回退：在 HUD 尚未显示时截取窗口所在显示器（1/8 降采样），做两遍盒式模糊，再通过 `CompositionGraphicsDevice` + `CompositionDrawingSurface`（D2D）绘制为 lower composition target 的背景，保留原有 tint、vignette、blurAmount、sourceOpacity 动画。
   - 若快照失败，再依次回退到 `CreateBackdropBrush`、纯色 tint brush，保证 HUD 仍能启动且可读。
   - 相关统计新增 `capturedBackdropEnabled` 字段，测试同步覆盖。
   - **模式切换**：通知区域托盘菜单新增“实时背景模糊”（`backgroundBlurLive`，默认关闭，菜单项带勾选状态）。关闭时为一次性的快照模糊（性能优先）；打开时每约 100 ms 从桌面窗口重新截取并模糊（效果优先，CPU 占用更高）。
2. **启动硬门槛**：`windows/app/application.cpp` 删除 `DwmSetWindowAttribute` 失败即抛 `SystemBackdropUnavailable` 的逻辑；`application_lifetime.cpp` 的恢复检查只在实际尝试过 host 属性时要求成功。
3. **安装器/包元数据**：`package_format.hpp` 最低 Windows build 从 22000 改为 19041（Windows 10 2004）；Setup 文案、manifest 注释、release 检查、size report 的 runtimeDependencies 同步改为 Windows 10 2004+。
4. **文档/测试**：5 个 README 的平台要求更新；`desktop_backdrop_tests` 及 projection 相关测试兼容 Win10（不要求 host 属性存在/恢复）。

## 构建
需要 Visual Studio Build Tools 2022（MSVC v143 + Windows SDK 10.0.26100）与 CMake 3.25+。本机验证命令：
```bat
cmake -S windows -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --target EndfieldHUD --parallel 8
cmake --build build --config Release --target EndfieldHUD --parallel 8
```

## 验证结果（Windows 10 专业版 22H2 build 19045）
- 官方未修改 exe 启动时弹窗：`EndfieldHUD 需要 Windows 11（内部版本 22000 或更高版本）`，确认了原始卡点。
- 移植版 exe 复制到官方便携包资源目录后，`--verify-package` 全部资源通过，`shell: ready, modules disabled: 0`。
- 启动移植版并按 Ctrl+` 打开 HUD，Map、Work Mode 等模块正常渲染；快照模式与实时刷新模式均已实测可打开、背景模糊正常。
- `desktop_backdrop_tests.exe`：PASS 12167 checks。

## 可运行包
`build\release\EndfieldHUD.exe`（资源/字体来自官方 v1.3.1 build21 便携包）。

## 已知限制
- Win10 背景是打开 HUD 时截取的静态模糊快照，不是 Win11 的实时 DWM host backdrop；HUD 打开期间如果桌面内容大幅变化，背景不会实时更新。Win11 上仍走原生 host backdrop。
- 没有构建安装包（本机测试使用官方便携包资源替换 exe）；安装器门槛已改为 Windows 10 2004，若需要可分发包可继续执行 `endfieldhud_release` 目标。
