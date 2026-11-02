# Roadmap

## M-1: scaffold (done in this repo)
- [x] AGPL-3.0-only license, README, docs
- [x] Portable core: `types.h`, `coords.h` (1280x720 normalization), `task_order.h` (dependency order), `IController`, `IOcr`, C ABI stub
- [x] Unit tests (`ctest`), presets for Linux dev and Windows MSVC
- [x] Privacy check (`tools/privacy_check.py`) and a pre-commit hook (`.githooks/`)
- [x] Task definitions for the six daily tasks, `languages.json`, asset folders

## M0: first end-to-end on one platform
- [x] Windows Steam controller behind `IController`: BitBlt and PrintWindow capture (selectable), real mouse input, normalize to 1280x720 (`core/src/platform/windows/`)
- [x] Template matching in Vision (OpenCV), ROI support (`vision.h`)
- [x] Template capture tool (`tools/template_capture.py`): screenshot -> crop -> mask -> save with metadata
- [x] Main-screen detection and "open mail" click as a smoke test (`maba_cli smoke`, unit-tested with a fake controller)
- [ ] Run against the real Steam client: capture the templates, `maba_cli info`, `maba_cli smoke --dry-run`, then `maba_cli smoke`, with both capture methods, at 100/125/150% display scale

## M1: navigation + simplest tasks
- [ ] Navigator: page graph, global popup handler, return-to-main
- [ ] Mail claim, mission claim (no OCR)

## M2
- [ ] Cafe, shop

## M3: OCR-dependent tasks
- [ ] `IOcr` Python sidecar (PaddleOCR)
- [ ] Spend AP, schedule

## M4
- [ ] Emulator controller
- [ ] C# GUI + scheduler (server reset time)
- [ ] Language matrix + CI template check

## Open questions to verify in the real game
- Steam client window: class `UnityWndClass`, title `Blue Archive`? Is the client area always 16:9 in windowed mode?
- Which capture method works (BitBlt needs an unobscured window; PrintWindow may return black frames for some DirectX windows)?
- Does the game accept `SendInput` mouse input, and does Esc act as "back" on every page (on the main screen it may open the quit dialog)?
- Does the mail button keep its position on the main screen? Which login popups (notices, attendance) appear before it?
- Does cafe income include AP? (decides ordering vs AP spending)
- Do daily-mission rewards include AP?
- Server reset time per server.
- Is Japanese really supported by the Steam global client? (store sources disagree). The Japan server has no PC client and is out of scope.
