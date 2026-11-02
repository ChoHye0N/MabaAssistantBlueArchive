**한국어** | [English](README.en.md)

<div align="center">

# MABA

**MabaAssistantBlueArchive**

블루 아카이브 하루 숙제를 대신 돌려 주는 도우미

![license](https://img.shields.io/badge/license-AGPL--3.0--only-blue)
![platform](https://img.shields.io/badge/platform-Windows-lightgrey)
![status](https://img.shields.io/badge/status-beta-orange)

</div>

<div align="center">
  <img src="docs/images/screenshot-ko.png" alt="MABA 화면" width="720">
</div>

## 개요

카페부터 미션 수령까지, 매일 반복하는 일을 정해 둔 순서대로 처리합니다. 화면을 보고 마우스를 움직이는 방식이라 게임 파일이나 메모리는 건드리지 않습니다.

**지원 환경** · Windows 10/11 · 글로벌 서버(스팀 PC 클라이언트, 한국어 기준)
안드로이드 에뮬레이터(ADB)도 연결할 수 있지만 아직 검증 전입니다.

[릴리스 페이지에서 내려받기](../../releases)

## 하는 일

실행 순서대로입니다. 숙제마다 켜고 끌 수 있고, 세부 동작은 설정에서 고릅니다.

**자원 모으기**: 카페 수익 수령 · 학생 초대 · 학생 터치 · 스케줄 · 서클 출석 · 일일 무료 패키지

**소비하기**: 상점 일괄 구매 · 현상수배 · 활동력 소비(이벤트 스토리와 퀘스트, 임무, 학원교류회) · 전술 대회

**마무리**: 우편함 수령 · 남은 활동력 소비 · 미션 수령 · 계정별 재화 기록

그 밖에 서버 초기화 시각 기준 예약 실행, 로그인 공지와 보상 창 자동 처리, 전투 중 AUTO와 3배속 유지를 지원합니다.

## 설치

릴리스에서 압축 파일을 받아 원하는 폴더에 풀고 `MabaAssistantBlueArchive.exe`를 실행하면 됩니다. .NET, Python, OCR 모델, 화면 인식용 템플릿이 모두 들어 있어서 따로 설치할 것은 없습니다.

템플릿은 한국어 클라이언트에서 캡처한 것입니다. 화면이 달라 인식이 안 되는 부분은 [템플릿 캡처 도구](tools/README.md)로 다시 캡처해 `assets-local/`에 넣으면 그쪽이 우선합니다.

설정은 `%appdata%\MabaAssistantBlueArchive` 폴더에 저장됩니다.

## 사용 팁

> [!TIP]
> 게임은 **창 모드(16:9)** 로 켜 두고, 실행 중에는 마우스를 움직이지 말아 주세요. 다른 창이 게임을 가리면 클릭하지 않고 멈춥니다.

> [!WARNING]
> 게임을 관리자 권한으로 실행하면 Windows가 입력을 막습니다. 게임과 MABA를 같은 권한으로 실행해 주세요.

- 퀘스트를 직접 깨야 할 때 쓸 부대(1~4)는 게임 안에서 미리 편성해 두면 됩니다.
- 초대할 학생은 게임에 표시되는 이름 그대로 적습니다. 예: `아루, 히나(수영복)`

## 개발

**코어**는 C++20(OpenCV), **GUI**는 C# WPF(.NET 10), **OCR**은 Python 사이드카(RapidOCR)입니다. 숙제는 `tasks/*.json`에 데이터로 적혀 있어서, 순서나 동작을 바꿀 때 코드를 고치지 않아도 됩니다.

```
cmake --preset windows-msvc
cmake --build --preset windows-msvc
ctest --preset windows-msvc
dotnet build gui/Maba.Gui -c Release
```

Windows 빌드에는 Visual Studio 2022와 `VCPKG_ROOT`가 필요합니다. 리눅스에서는 `linux-dev` 프리셋으로 코어 로직만 빌드하고 테스트할 수 있습니다.

더 읽을거리: [구조](docs/ARCHITECTURE.md) · [숙제 형식](docs/PAGE_FORMAT.md) · [로드맵](docs/ROADMAP.md) · [결정 사항](docs/DECISIONS.md)

<details>
<summary><b>사용한 오픈소스</b></summary>

| 이름 | 버전 | 라이선스 | 용도 |
|---|---|---|---|
| OpenCV | 4.12.0 | Apache-2.0 | 템플릿 매칭, 이미지 입출력 |
| nlohmann-json | 3.12.0 | MIT | 데이터 파일 읽기 |
| HandyControls | 3.7.0 | MIT | GUI 테마와 컨트롤 |
| CommunityToolkit.Mvvm | 8.4.2 | MIT | GUI MVVM |
| RapidOCR | 3.10.0 | Apache-2.0 | OCR 엔진 |
| ONNX Runtime | 1.31.1 | MIT | OCR 모델 추론 |
| PaddleOCR 모델 | RapidOCR가 내려받음 | Apache-2.0 | 글자 검출과 인식 |
| Pillow | 12.3.0 | MIT-CMU | 템플릿 캡처 도구 |
| MaaTouch | 사용자가 준비 | Apache-2.0 | 에뮬레이터 터치(선택, 포함하지 않음) |
| .NET 런타임 | 10.0 | MIT | GUI 실행(릴리스에 포함) |
| Python | 3.12.10 | PSF-2.0 | OCR 실행(릴리스에 포함) |

전체 고지는 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)에 있습니다.

</details>

## 기여하기

[기여 가이드](CONTRIBUTING.md)를 먼저 읽어 주세요. 커밋 형식과 개인정보 검사가 정리되어 있습니다.

## 라이선스

코드는 [AGPL-3.0-only](LICENSE)입니다.

블루 아카이브와 게임 이미지의 저작권은 NEXON Korea Corp.와 NEXON GAMES Co., Ltd.에 있으며 이 라이선스에 포함되지 않습니다. 릴리스에 들어 있는 템플릿(게임 화면 조각)도 마찬가지입니다. 이 프로젝트는 NEXON과 관련이 없고 승인받지 않았습니다. 자동화 도구 사용은 이용약관에 어긋날 수 있으며, 계정 제재를 포함한 결과는 사용자 책임입니다.
