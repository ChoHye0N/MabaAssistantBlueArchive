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
| 9 | OCR: PaddleOCR (Python sidecar first). |
| 10 | Logo and icons: later, with clearly owned rights, no game characters. |
| 11 | Personal and sensitive data are never committed (see `CONTRIBUTING.md`, `tools/privacy_check.py`). |

## Checklist before going public
- [ ] `python tools/privacy_check.py` passes, and the whole history is scanned (`git log -p` for personal data, paths, keys).
- [ ] No snapshots or clones of other repositories; `docs/SOURCES.md` porting log is complete and headers carry original notices.
- [ ] Every external resource is recorded in `assets/PROVENANCE.md`; unlicensed ones are not in the repo.
- [ ] README (both languages), `THIRD_PARTY_NOTICES.md` and the library table are current.
- [ ] Logo and screenshots contain no game characters, nicknames, UIDs or other personal data.
