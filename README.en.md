<div align="center">

# MabaAssistantBlueArchive (MABA)

**Blue Archive daily-task assistant**

Finish the day's routine in one click, on the Steam PC client and on emulators.

![license](https://img.shields.io/badge/license-AGPL--3.0--only-blue)
![core](https://img.shields.io/badge/core-C%2B%2B17-00599C)
![gui](https://img.shields.io/badge/GUI-C%23%20WPF-512BD4)
![ocr](https://img.shields.io/badge/OCR-RapidOCR%20%2B%20PaddleOCR%20models-2932E1)
![platform](https://img.shields.io/badge/platform-Windows-lightgrey)
![status](https://img.shields.io/badge/status-early%20development-orange)

**English** | [한국어](README.md)

</div>

> **Status:** early development. Core (navigator, popup handler, task engine, Steam and emulator controllers, OCR bridge) and the GUI build and pass their tests, but no task has been verified in the real game yet: the templates still have to be captured. See the [roadmap](docs/ROADMAP.md).

## Features
- **Daily tasks** (planned, in this order): cafe (collect, tap students), schedule (spend tickets), shop (daily purchases), AP spending (event maps, multiplier events), mail (claim all, after AP is spent because AP caps at 999), missions (claim all).
- **Scope:** Windows, global server. The Steam PC client and Android emulators over ADB (MuMu, LDPlayer, BlueStacks; to be verified). The Japan server has no PC client and is not supported.
- **Image recognition and ordinary input only.** It never reads or modifies game memory.
- **Declarative tasks.** Each task is a JSON file with dependencies and steps; the engine resolves the order.
- **GUI with a daily schedule** keyed to the server reset time, not local midnight.
- **Resolution independent.** All logic works in a normalized 1280x720 space.
- **Korean first, more languages later.** Text and templates are per-language data. See [config/languages.json](config/languages.json).
- **Safe by default.** A global popup handler, a return-to-main routine, and caps and timeouts on every loop.

Not planned for now: Total Assault, Limit-Release Showdown, story.

## Quick start
Releases are not available yet. To build the core (Linux/WSL needs `cmake g++ nlohmann-json3-dev`; the `linux-opencv` preset for vision tests also needs `libopencv-dev`):
```
cmake --preset linux-dev
cmake --build --preset linux-dev
ctest --preset linux-dev
```
On Windows use `cmake --preset windows-msvc` with `VCPKG_ROOT` set (needed for the controllers, `MabaCore.dll` and the GUI), then `cmake --build --preset windows-msvc` and `ctest --preset windows-msvc`.

- **GUI:** needs the .NET 10 SDK. Build the core first, then `dotnet build gui/Maba.Gui -c Release`; the build copies `MabaCore.dll` next to the app. See [gui/README.md](gui/README.md).
- **OCR (optional, for AP and ticket counts):** `python -m venv ocr/.venv`, then `ocr/.venv/Scripts/python -m pip install -r ocr/requirements.txt`. Core finds `ocr/.venv` by itself. See [ocr/README.md](ocr/README.md).
- **CLI:** `maba_cli info` checks the game window and both capture methods, `maba_cli grab` saves a 1280x720 frame under `captures/`, `maba_cli smoke --dry-run` checks main-screen detection and the mail button position, `maba_cli devices` lists ADB devices, and `maba_cli run --tasks mail_claim_all [--controller adb --serial 127.0.0.1:16384]` runs tasks.
- **Templates:** capture them with the [capture tool](tools/README.md); `python tools/check_templates.py` lists what each task still needs per language.

## Architecture
| Layer | Language | Folder |
|---|---|---|
| Core: controller, vision, navigator, task engine, C ABI | C++ | [core/](core) |
| GUI | C# (WPF, HandyControls) | [gui/](gui) |
| OCR sidecar | Python (RapidOCR, ONNX Runtime) | [ocr/](ocr) |
| Pages, popups, task definitions, per-language strings | JSON | [pages/](pages), [popups/](popups), [tasks/](tasks), [strings/](strings) |

Details: [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md). Task file format: [docs/PAGE_FORMAT.md](docs/PAGE_FORMAT.md).

## Documentation
- [Architecture](docs/ARCHITECTURE.md)
- [Roadmap](docs/ROADMAP.md)
- [Page and task format](docs/PAGE_FORMAT.md)
- [Decisions](docs/DECISIONS.md)
- [Source policy and porting log](docs/SOURCES.md)
- [Contributing](CONTRIBUTING.md)

## Open-source libraries
| Name | Version | License | Purpose | Status |
|---|---|---|---|---|
| OpenCV | 4.12.0 (vcpkg baseline) | Apache-2.0 | template matching, image I/O | in use |
| nlohmann-json | 3.12.0 (vcpkg baseline) | MIT | reading page/task JSON | in use |
| Pillow | 12.3.0 | MIT-CMU | template capture tool (`tools/`) | in use |
| HandyControls | 3.7.0 | MIT | GUI theme and controls | in use |
| CommunityToolkit.Mvvm | 8.4.2 | MIT | GUI MVVM | in use |
| RapidOCR | 3.10.0 | Apache-2.0 | OCR engine of the sidecar | in use |
| ONNX Runtime | 1.31.1 | MIT | OCR model inference | in use |
| PaddleOCR models (PP-OCRv5/v4/v6) | downloaded by RapidOCR 3.10.0 | Apache-2.0 | text detection and recognition | in use |
| MaaTouch | supplied by the user | Apache-2.0 | optional emulator touch helper (not bundled) | optional |

The OCR sidecar's transitive Python packages (numpy, opencv-python-headless, shapely, ...) are listed with their licenses in the notices.

Licenses are re-verified when each dependency is added. Full notices: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Contributing
Read [CONTRIBUTING.md](CONTRIBUTING.md) first: commit format, noreply commit identity, and the privacy check that runs before every commit and push.

## License and notices
Code is licensed under **AGPL-3.0-only**, see [LICENSE](LICENSE).

Blue Archive and its image resources belong to NEXON Korea Corp. and NEXON GAMES Co., Ltd. and are **not** covered by this license. Where external templates are used, their sources and terms are recorded in [assets/PROVENANCE.md](assets/PROVENANCE.md).

## Disclaimer
Using automation tools may violate the game's terms of service. Any risk to your account, including suspension, is yours alone. This project is not affiliated with or endorsed by NEXON.
