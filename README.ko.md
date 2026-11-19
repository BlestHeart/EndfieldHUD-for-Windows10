# EndfieldHUD for Windows 10

[English](README.md) · [简体中文](README.zh-CN.md) · [繁體中文](README.zh-TW.md) · [日本語](README.ja.md) · [한국어](README.ko.md)

> **AI 생성 고지:** 이 Windows 10 포팅 버전, 문서 및 배포 패키지는 AI 지원으로 생성되었습니다. 사용 전에 코드와 바이너리를 검토하세요.

[DDDuoDuo/EndfieldHUD](https://github.com/DDDuoDuo/EndfieldHUD)의 Windows 10 포팅 버전입니다. 레이어와 애니메이션이 적용된 《명일방주: 엔드필드》 HUD를 데스크톱에 표시합니다.

이 포크는 **Windows 10 버전 2004(build 19041) 이상, 64비트 x64**를 대상으로 하며 Windows 11에서도 실행됩니다. 원래 Windows 버전은 DWM host backdrop 때문에 Windows 11 build 22000이 필요했습니다.

## 이 포크에서 추가된 것

- Windows 10 2004+ 지원. 모든 HUD 모듈과 Windows 트레이/단축키 흐름을 포함합니다.
- **배경 흐림 모드 전환**(트레이 아이콘 우클릭 → **실시간 배경 흐림**):
  - **체크 해제 — 스냅숏 모드(기본, 성능 우선)**: HUD 초기화 시 데스크톱을 한 번 캡처해 흐리게 만든 뒤 하위 합성 레이어에 그립니다.
  - **체크 — 실시간 모드(효과 우선)**: HUD가 표시되는 동안 약 100 ms마다 데스크톱 캡처를 갱신하며 CPU 사용량이 더 높습니다.
- 설치 프로그램/패키지의 최소 Windows 버전을 build 19041로 변경(기존 22000).
- Windows 10에서는 기존 Windows 11 host backdrop의 대체 경로를 사용하고, Windows 11에서는 네이티브 `DWMWA_USE_HOSTBACKDROPBRUSH`를 계속 사용합니다.

## 다운로드

이 저장소의 [Releases](https://github.com/BlestHeart/EndfieldHUD-for-Windows10/releases)에서 **Windows 10 x64 설치 EXE 또는 포터블 ZIP**을 내려받으세요.

원하는 위치에 압축을 풀고 `EndfieldHUD.exe`를 실행하세요. 앱은 알림 영역에 상주하며 **Ctrl + Backtick**으로 HUD를 엽니다.

## 소스에서 빌드

Visual Studio Build Tools 2022(MSVC v143 + Windows SDK 10.0.26100)와 CMake 3.25+가 필요합니다:

```bat
cmake -S windows -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=ON
cmake --build build --config Release --target EndfieldHUD --parallel 8
cmake --build build --config Release --target EndfieldHUD --parallel 8
```

## 참고

- Windows 10의 배경은 **캡처한 데스크톱의 흐림**이며 Windows 11의 실시간 DWM host backdrop이 아닙니다. 실시간 모드는 캡처를 계속 갱신하고, 스냅숏 모드는 한 번만 캡처해 부담을 줄입니다.
- 원본 README는 [README.upstream.ko.md](README.upstream.ko.md)로 보존했습니다.
- 비공식 팬 프로젝트입니다. 게임 아트워크와 브랜드는 HYPERGRYPH 및 각 권리자에게 있으며, 원본 코드는 MIT 라이선스입니다.