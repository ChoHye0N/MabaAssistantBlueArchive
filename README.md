<div align="center">

# MabaAssistantBlueArchive (MABA)

**블루 아카이브 일일 숙제 도우미**

스팀 PC 클라이언트와 에뮬레이터에서 하루 숙제를 한 번에 끝냅니다.

![license](https://img.shields.io/badge/license-AGPL--3.0--only-blue)
![core](https://img.shields.io/badge/core-C%2B%2B17-00599C)
![gui](https://img.shields.io/badge/GUI-C%23%20WPF-512BD4)
![ocr](https://img.shields.io/badge/OCR-PaddleOCR-2932E1)
![platform](https://img.shields.io/badge/platform-Windows-lightgrey)
![status](https://img.shields.io/badge/status-early%20development-orange)

[English](README.en.md) | **한국어**

</div>

> **상태:** 초기 개발 단계입니다. 코어 뼈대는 빌드와 테스트를 통과하지만, 아직 끝까지 동작하는 숙제는 없습니다. [로드맵](docs/ROADMAP.md)을 참고하세요.

## 주요 기능
- **일일 숙제** (예정, 실행 순서대로): 카페(수령, 학생 터치), 스케줄(티켓 소비), 상점(일일 구매), 활동력 소비(이벤트 맵, 배수 이벤트), 우편함(일괄 수령, 활동력 상한 999 때문에 소비 후 수령), 미션(일괄 완료).
- **지원 범위:** 윈도우, 글로벌 서버. 스팀 PC 클라이언트를 먼저 지원하고, 그다음 뮤뮤 같은 에뮬레이터를 지원합니다. 일본 서버는 PC 클라이언트가 없어 지원하지 않습니다.
- **이미지 인식과 일반 입력만 사용합니다.** 게임 메모리를 읽거나 수정하지 않습니다.
- **선언형 숙제 정의.** 숙제마다 JSON 파일로 의존 관계를 적고, 엔진이 실행 순서를 정합니다.
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
윈도우에서는 `VCPKG_ROOT`를 설정하고 `cmake --preset windows-msvc`를 사용합니다(윈도우 컨트롤러와 GUI에 필요).

윈도우 빌드에는 `maba_cli`가 포함됩니다. `maba_cli info`는 게임 창과 두 캡처 방식을 점검하고, `maba_cli grab`은 1280x720 화면을 `captures/`에 저장하며, `maba_cli smoke --dry-run`은 메인 화면 감지와 우편 버튼 위치를 확인합니다. 템플릿 캡처 방법은 [tools/README.md](tools/README.md)를 참고하세요.

## 구조
| 계층 | 언어 | 폴더 |
|---|---|---|
| 코어: 컨트롤러, 비전, 네비게이터, 태스크 엔진, C ABI | C++ | [core/](core) |
| GUI | C# (WPF) | [gui/](gui) |
| OCR 사이드카 | Python (PaddleOCR) | [ocr/](ocr) |
| 숙제 정의, 언어별 문구 | JSON | [tasks/](tasks), [strings/](strings) |

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
| PaddleOCR | 미정 | Apache-2.0 | OCR | 예정 |
| ONNX Runtime | 미정 | MIT | 네이티브 OCR 추론 | 예정 |
| ModernWpfUI | 미정 | MIT | GUI 테마 | 예정 |

라이선스는 의존성을 실제로 추가할 때 다시 확인합니다. 전체 고지: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## 기여
먼저 [CONTRIBUTING.md](CONTRIBUTING.md)를 읽어 주세요. 커밋 형식, noreply 커밋 신원, 커밋과 푸시 전에 실행되는 개인정보 검사를 설명합니다.

## 라이선스 및 고지
코드는 **AGPL-3.0-only** 라이선스입니다. [LICENSE](LICENSE)를 참고하세요.

블루 아카이브와 게임 이미지 리소스의 저작권은 NEXON Korea Corp.와 NEXON GAMES Co., Ltd.에 있으며, 이 라이선스의 적용 대상이 **아닙니다**. 외부 템플릿을 사용하는 경우 출처와 이용 조건을 [assets/PROVENANCE.md](assets/PROVENANCE.md)에 기록합니다.

## 면책
자동화 도구 사용은 게임 이용약관에 위반될 수 있습니다. 계정 정지를 포함한 모든 위험은 사용자 본인에게 있습니다. 이 프로젝트는 NEXON과 관련이 없으며 승인을 받지 않았습니다.
