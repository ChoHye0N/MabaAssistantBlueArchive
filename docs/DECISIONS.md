# Decisions

Fixed choices. Change one only with a note here.

| # | Decision |
|---|---|
| 1 | Name: MabaAssistantBlueArchive (MABA). Namespace `maba`, C ABI prefix `Maba`, env prefix `MABA_`. |
| 2 | Commit identity: the contributor's GitHub noreply address. Real names and mailboxes are never committed. |
| 3 | Copyright holder in headers: "MabaAssistantBlueArchive contributors". License AGPL-3.0-only. |
| 4 | Developed in a private repository first; made public later. Before going public run the checklist below. |
| 5 | Steam PC capture: BitBlt and PrintWindow, both implemented. |
| 6 | Steam PC input: real mouse movement. Emulator input: ADB / touch helper, designed per the source policy in `docs/SOURCES.md`. |
| 7 | Platform order: Steam PC client first, emulators second. |
| 8 | Windows only for now. Global server only; the Japan server has no PC client and is out of scope. |
| 9 | OCR: PaddleOCR models run by RapidOCR on ONNX Runtime, in a Python sidecar first (changed 2026-11-02 from the PaddleOCR package: same models without the PaddlePaddle framework, much smaller install, and the same ONNX files can later run natively in C++). |
| 10 | Logo and icons: later, with clearly owned rights, no game characters. |
| 11 | Personal and sensitive data are never committed (see `CONTRIBUTING.md`, `tools/privacy_check.py`). |
| 12 | GUI: WPF on .NET 10 (LTS; .NET 8 support ends 2026-11-10) with HandyControls for theme and controls and CommunityToolkit.Mvvm for MVVM (maintained, source-generated, no IL weaving). Talks to Core only through the C ABI. |
| 13 | Emulator control: own ADB controller written from behavior notes (`docs/notes/emulator-adb.md`), `adb shell input` by default and MaaTouch as an optional faster helper the user supplies. A full automation framework with its own controllers was considered and not adopted: it would bring a second OpenCV/ONNX stack and its own task model. |
| 14 | Child processes (adb, OCR sidecar) get only their own std handles (explicit inheritance list), and a timed-out command is killed with its process tree. |
| 15 | C++20 for Core (changed 2026-11-02 from C++17): supported by MSVC 2022 and GCC 13 (Ubuntu 24.04, CI). UTF-8 paths go through `maba/utf8.h` because `path::u8string()` returns `std::u8string` since C++20. |
| 16 | Story stages (event Story tabs) are automated by skipping every scene (back opens the skip confirmation, which is confirmed), the way the owner's earlier project skips relationship stories; battles in them use the preset team with AUTO. Decided 2026-11-02 by the owner. |
| 17 | Tasks stay data. Flow control (`if`, `loop`, `each`, conditions on settings, templates, colours and OCR numbers) is part of the step language instead of per-task C++. Purchases and sweeps are confirmed only after the confirmation dialog's own text matched. |

## Checklist before going public
- [ ] `python tools/privacy_check.py` passes, and the whole history is scanned (`git log -p` for personal data, paths, keys).
- [ ] No snapshots or clones of other repositories; `docs/SOURCES.md` porting log is complete and headers carry original notices.
- [ ] Every external resource is recorded in `assets/PROVENANCE.md`; unlicensed ones are not in the repo.
- [ ] README (both languages), `THIRD_PARTY_NOTICES.md` and the library table are current.
- [ ] Logo and screenshots contain no game characters, nicknames, UIDs or other personal data.
