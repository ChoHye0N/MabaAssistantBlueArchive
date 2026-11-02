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
- [x] Navigator: page graph, global popup handler, return-to-main, bounded GoTo (`navigator.h`, `popup.h`), unit-tested with scene fakes
- [x] Task engine: declarative steps, dependency-aware runner with recovery and stop (`task.h`, `task_runner.h`)
- [x] Mail claim, mission claim: steps written (`status: wip`)
- [ ] Capture their templates (`python tools/check_templates.py` lists them) and the real popups; run `maba_cli run --tasks mail_claim_all,mission_claim_all`; then mark them `ready`

## M2
- [ ] Cafe, shop

## M3: OCR-dependent tasks
- [x] `IOcr` Python sidecar: RapidOCR on ONNX Runtime with PaddleOCR models, line-JSON protocol, restart and timeouts (`ocr_sidecar.h`, `ocr/server.py`)
- [x] Number parsing for OCR text (`ocr_text.h`), `read_number` step
- [ ] Measure digit accuracy on labeled game crops per language (Korean first)
- [ ] Spend AP, schedule

## M4
- [x] Emulator controller over ADB: raw/PNG screencap with probe, `input` or MaaTouch, CRLF repair, reconnect (`adb_controller.h`), unit-tested with a fake adb
- [ ] Verify against MuMu 12, LDPlayer 9 and BlueStacks 5 (serials, back key, MaaTouch); faster capture channels later
- [x] C# GUI (WPF, HandyControls, CommunityToolkit.Mvvm) over the C ABI, scheduler keyed to the server reset (`gui/`, `schedule.h`)
- [x] Language matrix + CI template check (`tools/check_templates.py`, `.github/workflows/ci.yml`)

## Open questions to verify in the real game
- Steam client window: class `UnityWndClass`, title `Blue Archive`? Is the client area always 16:9 in windowed mode?
- Which capture method works (BitBlt needs an unobscured window; PrintWindow may return black frames for some DirectX windows)?
- Does the game accept `SendInput` mouse input, and does Esc act as "back" on every page (on the main screen it may open the quit dialog)?
- Emulator: keycode 111 (ESCAPE) or 4 (BACK) for "back"?
- Does the mail button keep its position on the main screen? Which login popups (notices, attendance) appear before it?
- Is the claim-all button greyed out when there is nothing to claim, and does the grey state still match the template?
- Does cafe income include AP? (decides ordering vs AP spending)
- Do daily-mission rewards include AP?
- Server reset time: default 19:00 UTC (04:00 KST) is not confirmed by a primary source.
- Is Japanese really supported by the Steam global client? (store sources disagree). The Japan server has no PC client and is out of scope.
