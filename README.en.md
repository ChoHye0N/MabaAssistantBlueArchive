<div align="center">

# MabaAssistantBlueArchive (MABA)

**Blue Archive daily-task assistant**

Finish the day's routine in one click, on the Steam PC client and on emulators.

![license](https://img.shields.io/badge/license-AGPL--3.0--only-blue)
![core](https://img.shields.io/badge/core-C%2B%2B17-00599C)
![gui](https://img.shields.io/badge/GUI-C%23%20WPF-512BD4)
![ocr](https://img.shields.io/badge/OCR-PaddleOCR-2932E1)
![platform](https://img.shields.io/badge/platform-Windows-lightgrey)
![status](https://img.shields.io/badge/status-early%20development-orange)

**English** | [한국어](README.md)

</div>

> **Status:** early development. The core skeleton builds and passes its tests; no task runs end to end yet. See the [roadmap](docs/ROADMAP.md).

## Features
- **Daily tasks** (planned, in this order): cafe (collect, tap students), schedule (spend tickets), shop (daily purchases), AP spending (event maps, multiplier events), mail (claim all, after AP is spent because AP caps at 999), missions (claim all).
- **Scope:** Windows, global server. The Steam PC client comes first, emulators such as MuMu after that. The Japan server has no PC client and is not supported.
- **Image recognition and ordinary input only.** It never reads or modifies game memory.
- **Declarative tasks.** Each task is a JSON file with dependencies; the engine resolves the order.
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
On Windows use `cmake --preset windows-msvc` with `VCPKG_ROOT` set (needed for the Windows controller and the GUI).

## Architecture
| Layer | Language | Folder |
|---|---|---|
| Core: controller, vision, navigator, task engine, C ABI | C++ | [core/](core) |
| GUI | C# (WPF) | [gui/](gui) |
| OCR sidecar | Python (PaddleOCR) | [ocr/](ocr) |
| Task definitions, per-language strings | JSON | [tasks/](tasks), [strings/](strings) |

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
| PaddleOCR | TBD | Apache-2.0 | OCR | planned |
| ONNX Runtime | TBD | MIT | native OCR inference | planned |
| ModernWpfUI | TBD | MIT | GUI theme | planned |

Licenses are re-verified when each dependency is added. Full notices: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Contributing
Read [CONTRIBUTING.md](CONTRIBUTING.md) first: commit format, noreply commit identity, and the privacy check that runs before every commit and push.

## License and notices
Code is licensed under **AGPL-3.0-only**, see [LICENSE](LICENSE).

Blue Archive and its image resources belong to NEXON Korea Corp. and NEXON GAMES Co., Ltd. and are **not** covered by this license. Where external templates are used, their sources and terms are recorded in [assets/PROVENANCE.md](assets/PROVENANCE.md).

## Disclaimer
Using automation tools may violate the game's terms of service. Any risk to your account, including suspension, is yours alone. This project is not affiliated with or endorsed by NEXON.
