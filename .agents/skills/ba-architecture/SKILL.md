---
name: ba-architecture
description: Architecture and rules for the Blue Archive automation project (C++ core + C# GUI + Python OCR). Use before writing or changing Core, Controller, Vision, Navigator, Task Engine, OCR bridge or GUI code; when adding a platform (Steam PC or emulator) or a UI language; when reviewing design; and when deciding whether to use subagents.
---

# Architecture and rules

## Goal
Automate Blue Archive daily tasks on the Steam PC client (global) and on emulators (MuMu etc.). License: AGPL-3.0-only. Reference policy for other projects: `docs/SOURCES.md`.

## Layers

```
GUI (C#, WPF + HandyControls) --P/Invoke (C ABI, JSON strings)-- Core (C++)
                                             |- Controller   capture + input (Steam PC, ADB emulators)
                                             |- Vision       template matching (OpenCV), calls IOcr
                                             |- Navigator    page detection, page graph, popup handling
                                             |- Task Engine  task steps, dependencies, schedule
                                             |- OCR Bridge   Python sidecar (RapidOCR + PaddleOCR models)
```
Entry points: `assistant.h` (composition), `navigator.h`, `popup.h`, `task.h`, `task_runner.h`, `adb_controller.h`, `ocr_sidecar.h`, `maba_api.h`.

- Core and GUI talk only through the C ABI (few functions + JSON), so the GUI can change without touching Core.
- Python is for OCR and model training/tools only. Do not move game flow control into Python.

## Controller rules
- Keep the interface small: `Screenshot()`, `Click`, `Swipe`, `Back`. One implementation per platform (Steam PC, each emulator type).
- **The internal coordinate space is 1280x720.** Controllers return frames already scaled to 1280x720 and accept normalized coordinates; all scaling lives inside the Controller (`coords.h`). Otherwise any UI scale mismatch (e.g. Steam UI at 0.8x of the template) breaks every layer above.
- Use screen capture and ordinary input only. No game memory reads/writes, no code injection, no packet manipulation.
- `CaptureRegion(Rect)` returns a native-resolution crop for OCR; the downscaled 1280x720 frame can lose text detail.
- Emulators differ by type and version: separate implementations, and document the supported list.

## Navigator rules
- Never handle popups inside an individual task. A **global popup handler** (known popups -> dismiss) and a **"detect current page, return to main" routine** live in Core and run around every action. Unexpected screens and popups were the most common failure cause in the reference projects' bug reports.
- Every wait/retry/loop has a cap and a timeout. On exceeding it: save a screenshot and fail the task. No infinite loops.

## OCR rules
- Core depends only on `IOcr`. The Python sidecar (`ocr/server.py`, line-JSON over stdin/stdout) runs RapidOCR on ONNX Runtime; the same ONNX models can later run natively in C++.
- Use OCR only where needed (AP value, ticket counts, shop prices/items), and read digits only where possible: digits are language-independent. Icon-identifiable UI uses template matching, which is faster and more stable.

## Languages
- Korean (`ko`) is the baseline for behavior and design; other languages come later. Keep UI text out of logic: strings live in `strings/<lang>.json` by key (`docs/PAGE_FORMAT.md`).
- Supported languages are data (`config/languages.json`), not code. Confirmed: Korean, English, Traditional Chinese, Thai. Japanese is unverified: confirm on the Steam page before enabling.
- Template lookup order: `assets/<lang>/` then `assets/common/`. Icon/layout anchors go to common; only UI containing text goes to per-language folders.
- Track support per task per language in a matrix; CI should check that every declared language has the required templates.

## License and resources
- Every source file starts with an SPDX header: `SPDX-License-Identifier: AGPL-3.0-only` and `SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors` (collective holder, never a personal name).
- Game images (including templates) are Nexon's copyright and are NOT covered by the AGPL; say so in the README.
- Resources: prefer public repositories/sites and record each in `assets/PROVENANCE.md`; unlicensed ones stay in gitignored `assets-local/`. For code and resource reuse follow `ba-third-party-license`.

## Git and privacy
See `AGENTS.md`. Never commit personal data or local-only agent files; run `python tools/privacy_check.py` before pushing.

## Scope
- In scope: daily tasks (cafe, schedule, shop, AP spending, mail, missions).
- Deferred: Total Assault, Limit-Release Showdown, story. They need AI or real gameplay. Model choice and training come later.

## Working mode (agents)
- Default to one main agent. Interfaces (IController, IOcr, task definitions) and Core are tightly coupled; parallel agents editing them cause conflicts and rework, and each subagent starts cold at real cost.
- Use subagents only for: (1) independent review after a milestone (interface compliance, license headers, timeout/popup coverage); (2) independent research (dependency license audit, PaddleOCR language-model coverage).
- Windows-only code (window capture, WPF) cannot be built in a Linux cloud workspace. Build it on the user's machine; in the cloud, stop at unit tests with screenshot fixtures.
