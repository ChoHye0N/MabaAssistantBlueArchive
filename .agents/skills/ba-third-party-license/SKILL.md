---
name: ba-third-party-license
description: Use when adding or updating any external dependency (library, model file, DLL) in this AGPL-3.0 Blue Archive automation project, or when consulting or porting code from other repositories. Covers the reuse policy, the README library table, THIRD_PARTY_NOTICES.md, source-header attribution and AGPL compatibility checks.
---

# Third-party code and license management

This project is AGPL-3.0-only. External code brings attribution duties, and incompatible licenses make distribution impossible. This is not legal advice; ask a professional before release if unsure.

## Reuse policy
The public policy is in `docs/SOURCES.md`. Per-project decisions (which projects are reference-only, which may be ported) are kept by the maintainer; if you do not have them, treat every other project as **reference only**: read it, write behavior notes in your own words in `docs/notes/`, implement from the notes, and ask the maintainer before moving any code.

Verify the exact license text of any upstream before porting; licenses can change.

## When adding or updating a dependency

1. **Confirm the license**: read the project's LICENSE file for the exact name and version; do not trust summaries.
2. **Judge AGPL-3.0 compatibility**
   - Generally fine: MIT, BSD, Apache-2.0, GPL-3.0, AGPL-3.0, LGPL.
   - Careful: GPL-2.0-only cannot be combined with GPL-3.0/AGPL-3.0 ("or later" can). Binaries with their own redistribution terms (e.g. DirectML-style DLLs) need their terms read and redistribution confirmed.
   - **Model weights and data files** can carry different licenses than the code; check separately.
3. **Update the README table in both `README.md` (Korean) and `README.en.md`** ("Open Source Libraries"): linked name, version, purpose, license. Say so when a release includes or leaves out the item.
4. **Update `THIRD_PARTY_NOTICES.md`** with copyright notices and full license text. MIT and Apache-2.0 require the notice to be kept. Release archives also bundle the .NET runtime, an embeddable Python, the OCR packages and models and the OpenCV DLLs, so anything new in the archive needs a row even when it is not in this repository.
5. **Pin versions** (vcpkg, NuGet, pip) and record which version's license you checked. Re-check on upgrade.

## When porting code from another project

- Distinguish learning ideas from moving code. Moving or translating code makes a derivative work: honor the original license.
- Behaviour adapted from notes, with no code copied, still gets a porting-log row. If the upstream license requires its text, add it to `THIRD_PARTY_NOTICES.md`.
- In the file header put: original copyright, source (repo, file, commit), original license.
- **Game resources are always separate.** Template images and game assets are Nexon's copyright regardless of any code license. Prefer resources from publicly available repositories or sites, but public is not licensed. Record each one in `assets/PROVENANCE.md` (URL, license/terms as stated, retrieval date, version). A stated license or explicit permission covering the files -> may be committed. Nothing stated, or only a fair-use claim -> keep it in gitignored `assets-local/`, never commit. Avoid repositories that redistribute files extracted from game packages.
- Release archives carry the owner's template captures in `assets/`; they are not in this repository. Going public makes them public too, so decide on that before publishing.

## Fixed README text
- Library section: keep the table format above.
- Resource notice: "Copyright of game-related image resources belongs to NEXON Korea Corp. and NEXON GAMES Co., Ltd.; they are not covered by this project's license."
- Disclaimer: this tool may violate the game's terms; account-ban risk is the user's responsibility. It uses image recognition and never modifies game memory.

## Checklist
- [ ] License name and version confirmed
- [ ] AGPL-3.0 compatibility checked
- [ ] README table updated
- [ ] THIRD_PARTY_NOTICES.md updated
- [ ] Ported code has source attribution in its header and an entry in docs/SOURCES.md
- [ ] Every external game resource has a row in assets/PROVENANCE.md; unlicensed ones stay in assets-local/ (not committed)
- [ ] `python tools/privacy_check.py` passes
