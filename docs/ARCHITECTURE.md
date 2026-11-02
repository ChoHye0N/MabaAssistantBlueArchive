# Architecture

## Overview
```
GUI (C#, WPF)  --P/Invoke, C ABI (functions + JSON)--  Core (C++)
                                                         |- Controller   capture + input, per platform
                                                         |- Vision       template matching, calls IOcr
                                                         |- Navigator    page detection, page graph, popup handler
                                                         |- Task Engine  tasks.json, dependencies, scheduler
                                                         '- OCR Bridge   Python sidecar (PaddleOCR) -> later ONNX in C++
```
The C ABI lives in `core/include/maba/maba_api.h`. Keep it tiny and versioned (`MabaApiVersion`).

## Controllers
| Target | Capture | Input | Notes |
|---|---|---|---|
| Steam PC client (first) | Window capture: BitBlt and PrintWindow, both implemented and selectable | Real mouse movement (SendInput-style), not window messages | DPI awareness and UI scale must be handled inside the controller. |
| Emulators (MuMu etc.) (second) | ADB screencap or emulator-specific API | ADB input or a touch helper | One implementation per emulator type/version. |

Steam PC controller (`core/src/platform/windows/steam_controller.h`):
- Finds the window by class and title (defaults `UnityWndClass` / `Blue Archive`, to verify). Rejects a minimized window or a client area that is not 16:9.
- Capture: `BitBlt` copies the client area from the screen (fast; the window must be visible and unobscured). `PrintWindow` asks the window to render itself (works when obscured, not when minimized; some DirectX windows return black, which is reported as an error). Selectable per call (`maba_cli --capture`).
- Input: real cursor movement with `SendInput` in virtual-desktop coordinates (multiple monitors). Before input the game window is brought to the front and must be the foreground window, and no other window may cover the target point; otherwise the action fails instead of clicking elsewhere. The cursor is restored afterwards. If the game runs as administrator, Windows blocks input from a normal process (UIPI). `Back` sends Esc (to verify).
- DPI: every call runs with per-monitor DPI awareness on its thread, so window rects, client size and cursor coordinates are physical pixels at any display scale.
- `maba_cli info` checks both capture methods; `maba_cli grab` saves a normalized frame (or a native region) under `captures/`.

Contract (`controller.h`): frames are returned scaled to **1280x720** and all coordinates are normalized. `CaptureRegion(Rect)` returns a crop taken at native resolution, for OCR where the downscaled frame may lose text detail. `coords.h` holds the mapping and is unit-tested. Reason: UI scale mismatches (e.g. Steam UI at ~0.8x of template size) otherwise break every layer above.

## Navigator
- Page graph: nodes are pages recognized by anchors, edges are the actions to move between them.
- **Global popup handler**: runs before/after every action; dismisses known popups. Tasks never handle popups themselves.
- **Return-to-main routine**: detect the current page and navigate back to the main screen after a failure.
- Every wait/retry/loop has a cap and timeout; on exceeding: save a screenshot, fail the task.

## Task engine
(Page, ROI and string data format: see `PAGE_FORMAT.md`.)

- Tasks are data in `tasks/*.json` with `after` dependencies. `ResolveOrder` (`task_order.h`) produces the run order and rejects duplicates, unknown dependencies and cycles.
- Required ordering rule: mail claim runs after AP spending (AP cannot exceed 999).
- Scheduling is keyed to the server reset time (a setting), not local midnight.

## Vision and OCR
- Template matching (OpenCV) is the default. Icon-identifiable UI never needs OCR.
- OCR only for AP value, ticket counts, shop items/prices. Prefer reading digits only (tight ROI on the number): digits are language-independent. Identify text items by icon templates instead of OCR where possible. Core depends only on `IOcr`.
- Plan: Python sidecar (PaddleOCR) over local IPC first; later export the models to ONNX and run them natively. Python is used for OCR and model training only.

## Languages
- Korean (`ko`) is the baseline for behavior and design; other languages are added later. UI text that logic compares lives in `strings/<lang>.json` by key, never as literals in code.
- `config/languages.json` lists languages; confirmed for the Steam global client: Korean, English, Traditional Chinese, Thai. Japanese is unverified.
- Template lookup: `assets/<lang>/` then `assets/common/`. Language-neutral anchors (icons, layout) go to common.
- A task x language support matrix is kept up to date; CI should verify required templates exist for every declared language.

## Platforms and testing
- Portable code (`core/src/*.cpp` except platform) builds and tests on Linux/Windows.
- Windows-only code lives in `core/src/platform/windows/` and `gui/`.
- Controllers are faked in tests with screenshot fixtures.

## Out of scope for now
Total Assault, Limit-Release Showdown and story automation (need AI/real gameplay). Model selection and training are decided later.
