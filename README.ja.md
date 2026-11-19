# EndfieldHUD for Windows 10

[English](README.md) · [简体中文](README.zh-CN.md) · [繁體中文](README.zh-TW.md) · [日本語](README.ja.md) · [한국어](README.ko.md)

> **AI 生成に関する注意：** この Windows 10 移植版、ドキュメント、配布パッケージは AI 支援で生成されています。使用前にコードとバイナリを確認してください。

[DDDuoDuo/EndfieldHUD](https://github.com/DDDuoDuo/EndfieldHUD) の Windows 10 移植版です。レイヤー化されたアニメーション付きの『アークナイツ：エンドフィールド』HUD をデスクトップに表示します。

このフォークは **Windows 10 バージョン 2004（build 19041）以降、64 ビット x64** を対象とし、Windows 11 でも動作します。元の Windows 版は DWM host backdrop のため Windows 11 build 22000 が必要でした。

## このフォークの追加点

- Windows 10 2004+ に対応。すべての HUD モジュールと Windows のトレイ/ホットキー操作を含みます。
- **背景ぼかしモードの切り替え**（トレイアイコンを右クリック → **リアルタイム背景ぼかし**）：
  - **チェックなし — スナップショットモード（既定、性能優先）**：HUD 初期化時にデスクトップを一度キャプチャし、ぼかして下層の合成レイヤーに描画します。
  - **チェックあり — リアルタイムモード（効果優先）**：HUD 表示中に約 100 ms ごとにデスクトップのキャプチャを更新します。CPU 使用率は高くなります。
- インストーラー/パッケージの最低 Windows バージョンを build 19041 に変更（元は 22000）。
- Windows 10 では元の Windows 11 host backdrop の代替を使用し、Windows 11 では引き続きネイティブの `DWMWA_USE_HOSTBACKDROPBRUSH` を使用します。

## ダウンロード

このリポジトリの [Releases](https://github.com/BlestHeart/EndfieldHUD-for-Windows10/releases) から **Windows 10 x64 セットアップ EXE またはポータブル ZIP** をダウンロードしてください。

任意の場所に展開して `EndfieldHUD.exe` を実行します。アプリは通知領域に常駐し、**Ctrl + Backtick** で HUD を開けます。

## ソースからビルド

Visual Studio Build Tools 2022（MSVC v143 + Windows SDK 10.0.26100）と CMake 3.25+ が必要です：

```bat
cmake -S windows -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --target EndfieldHUD --parallel 8
cmake --build build --config Release --target EndfieldHUD --parallel 8
```

## 注意

- Windows 10 の背景は**キャプチャしたデスクトップのぼかし**であり、Windows 11 のリアルタイム DWM host backdrop ではありません。リアルタイムモードはキャプチャを継続更新し、スナップショットモードは一度だけキャプチャして負荷を抑えます。
- 元の README は [README.upstream.ja.md](README.upstream.ja.md) として保持しています。
- 非公式のファンプロジェクトです。ゲームのアートワークとブランドは HYPERGRYPH と各権利者に帰属し、オリジナルコードは MIT ライセンスです。