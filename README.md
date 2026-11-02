<div align="center">

# MabaAssistantBlueArchive (MABA)

**블루 아카이브 일일 숙제 도우미**

스팀 PC 클라이언트와 에뮬레이터에서 하루 숙제를 한 번에 끝냅니다.

![license](https://img.shields.io/badge/license-AGPL--3.0--only-blue)
![core](https://img.shields.io/badge/core-C%2B%2B20-00599C)
![gui](https://img.shields.io/badge/GUI-C%23%20WPF-512BD4)
![ocr](https://img.shields.io/badge/OCR-RapidOCR%20%2B%20PaddleOCR%20models-2932E1)
![platform](https://img.shields.io/badge/platform-Windows-lightgrey)
![status](https://img.shields.io/badge/status-early%20development-orange)

[English](README.en.md) | **한국어**

</div>

> **상태:** 초기 개발 단계입니다. 코어(네비게이터, 팝업 처리, 태스크 엔진, 스팀·에뮬레이터 컨트롤러, OCR 연동)와 GUI는 빌드와 테스트를 통과하지만, 실제 게임에서 검증된 숙제는 아직 없습니다. 템플릿을 먼저 캡처해야 합니다. [로드맵](docs/ROADMAP.md)을 참고하세요.

## 주요 기능
- **일일 숙제** (예정, 실행 순서대로): 카페(수령, 학생 터치), 스케줄(티켓 소비), 상점(일일 구매), 활동력 소비(이벤트 맵, 배수 이벤트), 우편함(일괄 수령, 활동력 상한 999 때문에 소비 후 수령), 미션(일괄 완료).
- **지원 범위:** 윈도우, 글로벌 서버. 스팀 PC 클라이언트와 ADB로 연결하는 안드로이드 에뮬레이터(뮤뮤, LD플레이어, 블루스택; 검증 필요)를 지원합니다. 일본 서버는 PC 클라이언트가 없어 지원하지 않습니다.
- **이미지 인식과 일반 입력만 사용합니다.** 게임 메모리를 읽거나 수정하지 않습니다.
- **선언형 숙제 정의.** 숙제마다 JSON 파일로 의존 관계와 단계를 적고, 엔진이 실행 순서를 정합니다.
- **GUI와 매일 예약 실행.** 현지 자정이 아니라 서버 초기화 시각을 기준으로 실행합니다.
- **해상도 독립.** 모든 로직은 정규화된 1280x720 좌표계에서 동작합니다.
- **한국어 우선, 다국어는 이후 지원.** 문구와 템플릿은 언어별 데이터입니다. [config/languages.json](config/languages.json)을 참고하세요.
- **안전 장치.** 전역 팝업 처리, 메인 화면 복귀 루틴, 모든 반복문의 횟수 상한과 시간 제한.

당분간 계획에 없는 것: 총력전, 제약해제결전, 스토리.

## 빠른 시작
아직 릴리스는 없습니다. 코어를 빌드하려면(Linux/WSL: `cmake g++ nlohmann-json3-dev` 필요, 비전 테스트용 `linux-opencv` 프리셋은 `libopencv-dev`도 필요):
```
cmake --preset linux-dev
cmake --build --preset linux-dev
ctest --preset linux-dev
```
윈도우에서는 `VCPKG_ROOT`를 설정하고 `cmake --preset windows-msvc`를 사용합니다(컨트롤러, `MabaCore.dll`, GUI에 필요). 이어서 `cmake --build --preset windows-msvc`, `ctest --preset windows-msvc`를 실행합니다.

- **GUI:** .NET 10 SDK가 필요합니다. 코어를 먼저 빌드한 뒤 `dotnet build gui/Maba.Gui -c Release`를 실행하면 `MabaCore.dll`이 앱 옆에 복사됩니다. [gui/README.md](gui/README.md)를 참고하세요.
- **OCR (선택, 활동력·티켓 수 읽기):** `python -m venv ocr/.venv` 후 `ocr/.venv/Scripts/python -m pip install -r ocr/requirements.txt`를 실행합니다. 코어가 `ocr/.venv`를 자동으로 찾습니다. [ocr/README.md](ocr/README.md)를 참고하세요.
- **CLI:** `maba_cli info`는 게임 창과 두 캡처 방식을 점검하고, `maba_cli grab`은 1280x720 화면을 `captures/`에 저장하며, `maba_cli smoke --dry-run`은 메인 화면 감지와 우편 버튼 위치를 확인합니다. `maba_cli devices`는 ADB 기기 목록을 보여 주고, `maba_cli run --tasks mail_claim_all [--controller adb --serial 127.0.0.1:16384]`은 숙제를 실행합니다.
- **템플릿:** [캡처 도구](tools/README.md)로 만듭니다. `python tools/check_templates.py`는 숙제와 언어별로 아직 없는 템플릿을 보여 줍니다.

## 구조
| 계층 | 언어 | 폴더 |
|---|---|---|
| 코어: 컨트롤러, 비전, 네비게이터, 태스크 엔진, C ABI | C++ | [core/](core) |
| GUI | C# (WPF, HandyControls) | [gui/](gui) |
| OCR 사이드카 | Python (RapidOCR, ONNX Runtime) | [ocr/](ocr) |
| 페이지, 팝업, 숙제 정의, 언어별 문구 | JSON | [pages/](pages), [popups/](popups), [tasks/](tasks), [strings/](strings) |

자세한 내용: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md). 숙제 파일 형식: [docs/PAGE_FORMAT.md](docs/PAGE_FORMAT.md).

## 문서
- [아키텍처](docs/ARCHITECTURE.md)
- [로드맵](docs/ROADMAP.md)
- [페이지 및 숙제 형식](docs/PAGE_FORMAT.md)
- [결정 사항](docs/DECISIONS.md)
- [출처 정책 및 포팅 기록](docs/SOURCES.md)
- [기여 가이드](CONTRIBUTING.md)

## 오픈소스 라이브러리
| 이름 | 버전 | 라이선스 | 용도 | 상태 |
|---|---|---|---|---|
| OpenCV | 4.12.0 (vcpkg 베이스라인) | Apache-2.0 | 템플릿 매칭, 이미지 입출력 | 사용 중 |
| nlohmann-json | 3.12.0 (vcpkg 베이스라인) | MIT | 페이지·작업 JSON 읽기 | 사용 중 |
| Pillow | 12.3.0 | MIT-CMU | 템플릿 캡처 도구 (`tools/`) | 사용 중 |
| HandyControls | 3.7.0 | MIT | GUI 테마와 컨트롤 | 사용 중 |
| CommunityToolkit.Mvvm | 8.4.2 | MIT | GUI MVVM | 사용 중 |
| RapidOCR | 3.10.0 | Apache-2.0 | OCR 사이드카 엔진 | 사용 중 |
| ONNX Runtime | 1.31.1 | MIT | OCR 모델 추론 | 사용 중 |
| PaddleOCR 모델 (PP-OCRv5/v4/v6) | RapidOCR 3.10.0이 내려받음 | Apache-2.0 | 글자 검출과 인식 | 사용 중 |
| MaaTouch | 사용자가 준비 | Apache-2.0 | 에뮬레이터 터치 도우미 (포함하지 않음) | 선택 |

OCR 사이드카가 함께 설치하는 Python 패키지(numpy, opencv-python-headless, shapely 등)와 라이선스는 고지 파일에 있습니다.

라이선스는 의존성을 실제로 추가할 때 다시 확인합니다. 전체 고지: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## 기여
먼저 [CONTRIBUTING.md](CONTRIBUTING.md)를 읽어 주세요. 커밋 형식, noreply 커밋 신원, 커밋과 푸시 전에 실행되는 개인정보 검사를 설명합니다.

## 라이선스 및 고지
코드는 **AGPL-3.0-only** 라이선스입니다. [LICENSE](LICENSE)를 참고하세요.

블루 아카이브와 게임 이미지 리소스의 저작권은 NEXON Korea Corp.와 NEXON GAMES Co., Ltd.에 있으며, 이 라이선스의 적용 대상이 **아닙니다**. 외부 템플릿을 사용하는 경우 출처와 이용 조건을 [assets/PROVENANCE.md](assets/PROVENANCE.md)에 기록합니다.

## 면책
자동화 도구 사용은 게임 이용약관에 위반될 수 있습니다. 계정 정지를 포함한 모든 위험은 사용자 본인에게 있습니다. 이 프로젝트는 NEXON과 관련이 없으며 승인을 받지 않았습니다.
