# Decisions

Fixed choices. Change one only with a note here.

| # | Decision |
|---|---|
| 1 | Name: MabaAssistantBlueArchive (MABA). Namespace `maba`, C ABI prefix `Maba`, env prefix `MABA_`. |
| 2 | Commit identity: the contributor's GitHub noreply address. Real names and mailboxes are never committed. |
| 3 | Copyright holder in headers: "MabaAssistantBlueArchive contributors". License AGPL-3.0-only. |
| 4 | The repository is public (kept public by the owner, 2026-11-06). It may move to an organization later, with the templates in a repository of their own. |
| 5 | Steam PC capture: BitBlt and PrintWindow, both implemented. |
| 6 | Steam PC input: real mouse movement. Emulator input: ADB / touch helper, designed per the source policy in `docs/SOURCES.md`. |
| 7 | Platform order: Steam PC client first, emulators second. |
| 8 | Windows only for now. Global server only; the Japan server has no PC client and is out of scope. |
| 9 | OCR: PaddleOCR models run by RapidOCR on ONNX Runtime, in a Python sidecar first (changed 2026-11-02 from the PaddleOCR package: same models without the PaddlePaddle framework, much smaller install, and the same ONNX files can later run natively in C++). |
| 10 | Logo and icons: later, with clearly owned rights, no game characters. |
| 11 | Personal and sensitive data are never committed (see `CONTRIBUTING.md`, `tools/privacy_check.py`). |
| 12 | GUI: WPF on .NET 10 (LTS; .NET 8 support ends 2026-11-10) with HandyControls for theme and controls and CommunityToolkit.Mvvm for MVVM (maintained, source-generated, no IL weaving). Talks to Core only through the C ABI. |
| 13 | Emulator control: own ADB controller written from behavior notes (`docs/notes/controllers.md`), `adb shell input` by default and MaaTouch as an optional faster helper the user supplies. A full automation framework with its own controllers was considered and not adopted: it would bring a second OpenCV/ONNX stack and its own task model. |
| 14 | Child processes (adb, OCR sidecar) get only their own std handles (explicit inheritance list), and a timed-out command is killed with its process tree. |
| 15 | C++20 for Core (changed 2026-11-02 from C++17): supported by MSVC 2022 and GCC 13 (Ubuntu 24.04, CI). UTF-8 paths go through `maba/utf8.h` because `path::u8string()` returns `std::u8string` since C++20. |
| 16 | Story stages (event Story tabs) are automated by skipping every scene (back opens the skip confirmation, which is confirmed), the way the owner's earlier project skips relationship stories; battles in them use the preset team with AUTO. Decided 2026-11-02 by the owner. |
| 17 | Tasks stay data. Flow control (`if`, `loop`, `each`, conditions on settings, templates, colours and OCR numbers) is part of the step language instead of per-task C++. Purchases and sweeps are confirmed only after the confirmation dialog's own text matched. |
| 18 | Starting and closing the game is optional and stays out of the controllers: `steam://rungameid/<app id>` for Steam, `adb` plus the configured emulator path for emulators (`SteamSession`, `EmulatorSession`). Every wait is bounded and stoppable. Only an emulator this run started is closed, and a run stopped by the user leaves everything running. Steam itself is never closed. |
| 19 | Daily schedule: "server reset + delay" stays the default; a local clock time is the alternative, with a warning when it falls less than an hour before the reset. A missed slot runs once when the GUI is open, and a scheduled day counts as done even after failures (no retry loop). |
| 20 | GUI display language (Korean, English) is separate from the game client language (templates, OCR, `strings/`). Default is the Windows language; a missing key shows the Korean text. English text follows the global client's terms. |
| 21 | Commissions sweep a set count (`runs`) instead of MAX and run before AP spending: they have no tickets, so MAX would spend all AP. Decided 2026-11-04. |
| 22 | Student names per language are kept only in gitignored `assets-local/students/`, because their source states no license. Nothing in the program depends on them yet. Decided 2026-11-04. |
| 23 | OCR accuracy is measured with `tools/measure_ocr.py` on local labeled crops. Labels stay in `assets-local/` with the screenshots they point at; the results go to `docs/notes/ocr-accuracy.md`. |
| 24 | Releases are built and published only on the maintainer's explicit order. Beta releases are for testing how the program feels in use and need not follow `main`. |
| 25 | Reliability comes before speed (owner, 2026-11-04). A click or back press has to change the screen, a fixed point is clicked only on its own screen, and waits follow the screen instead of the clock. Time-based waits remain only where time is what matters. |
| 26 | Templates stay 1280x720, cut from frames that Core itself normalized from 1920x1080: the same path as run time. Matching at the window's own size was not adopted: game text drawn at that size differs from a scaled image, while scaling both sides reduces the difference. Measured 2026-11-05: these templates reach a median of 0.993 and at least 0.958 on frames of a 1493x840 window. |
| 27 | Background in a template is made transparent from evidence: pixels that change between frames showing the same UI, after normalizing brightness. Masked matching stays `TM_CCOEFF_NORMED`. PNG alpha is used, not black paint, so black text is not lost. |
| 28 | The `release` workflow starts by hand and creates a draft. Publishing stays the maintainer's decision. |
| 29 | Templates (the owner's captures, no personal data) are committed in `assets/`, so a checkout and a release have everything. Game images stay NEXON's and outside the project license. |

## Checklist for a public repository
- [ ] `python tools/privacy_check.py` passes, and the whole history is scanned (`git log -p` for personal data, paths, keys).
- [ ] No snapshots or clones of other repositories; `docs/SOURCES.md` porting log is complete and headers carry original notices.
- [ ] Every external resource is recorded in `assets/PROVENANCE.md`; unlicensed ones are not in the repo.
- [ ] README (both languages), `THIRD_PARTY_NOTICES.md` and the library table are current.
- [ ] Logo and screenshots contain no game characters, nicknames, UIDs or other personal data.
