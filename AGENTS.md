# AGENTS.md

Blue Archive daily-task automation for the Steam PC client (global) and emulators. C++ core + C# GUI + Python OCR. License: AGPL-3.0-only. Project name: MabaAssistantBlueArchive (MABA). C++ namespace `maba`, C ABI prefix `Maba`, env prefix `MABA_`. Scope: Windows only, Blue Archive global server only (see `docs/DECISIONS.md`).

## Read first
- `docs/ARCHITECTURE.md`, `docs/SOURCES.md`, `docs/ROADMAP.md`, `docs/notes/game-structure.md`
- Skills in `.agents/skills/`: `ba-add-daily-task`, `ba-live-check` (looking at and verifying in the real game), `ba-third-party-license`, `commit`, `ba-release`. Load the matching one before related work.
- Tool setup: a tool that expects skills in another directory needs a link or copy of `.agents/skills`. Such tool directories are gitignored; never commit them.

## Build and test (Core logic builds on Linux too)
```
cmake --preset linux-dev
cmake --build --preset linux-dev
ctest --preset linux-dev --output-on-failure
```
Linux packages: `cmake g++ nlohmann-json3-dev`; add `libopencv-dev` for the `linux-opencv` preset (Vision tests, same commands with `linux-opencv`).
Windows-only code (window capture, input injection, WPF GUI) does not build on Linux. Keep it under `core/src/platform/windows/` and `gui/`, guarded by `_WIN32`, and keep everything else portable so it stays unit-testable.
Windows build: `cmake --preset windows-msvc` (needs `VCPKG_ROOT`).

## Code source policy (strict)
- See `docs/SOURCES.md`. Default: other projects are reference only. Write behavior notes in your own words in `docs/notes/` and implement from the notes. Moving code needs the maintainer's OK, the original header, and a porting-log row.
- Public files (README, docs, headers, commit messages) must not contain other projects' names, links, commit hashes or owner handles, except a porting-log row for code actually ported (license attribution).
- Never commit snapshots or clones of other repositories; clone them outside this repo.
- Game images/templates are Nexon's. Templates are the maintainer's own captures in `assets/`; read each one for personal data before committing it (nickname, UID, friend names). External resources: record each in `assets/PROVENANCE.md`; sources with no stated license/permission stay in gitignored `assets-local/`. Avoid repos that redistribute files extracted from game packages.
- README: `README.md` is Korean (default), `README.en.md` is English. Always change both together and keep them in sync.
- New dependency: follow `ba-third-party-license` (README table + THIRD_PARTY_NOTICES.md).

## Rules
- Internal coordinates are 1280x720; Controllers normalize (see `core/include/maba/coords.h`).
- Popups are handled by one global handler, never per task. All waits/retries/loops have caps and timeouts.
- No game memory access, injection or packet manipulation. Screen capture + ordinary input only.
- Resources (`config/`, `pages/`, `popups/`, `tasks/`, `strings/`, `assets/`) are released apart from the program and install themselves: when data starts to use something the released program does not know, raise `min_core` in `config/resource.json` (`docs/notes/updates.md`).
- Tasks are data (`tasks/*.json`): `order` sets the sequence, `after` only where a failure must block; do not hard-code order. Mail must run after AP spending.
- Korean (`ko`) is the baseline; other languages come later. Languages are data (`config/languages.json`); UI text that logic compares lives in `strings/<lang>.json` by key, never as literals; templates resolve `assets-local/` then `assets/`, per language then common. See `docs/PAGE_FORMAT.md`.
- OCR reads digits only where possible (language-independent); identify text items by icon templates. Use `CaptureRegion` for OCR crops.
- Every source file starts with `SPDX-License-Identifier: AGPL-3.0-only` and `SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors` (collective holder; never a personal name).
- Identifiers and docs in English.
- Do not add Total Assault or Showdown automation yet (deferred). Story is only skipped (scene -> back -> confirm skip), never read or chosen.

## Git and privacy (strict)
- Never commit personal data: real name, email, phone, home paths, tokens, keys, account IDs, or screenshots showing them (game UID, nickname, friend list, mail). Crop or mask captures before saving.
- Commit identity: `git config user.name <GitHub handle>` and `git config user.email <ID>+<handle>@users.noreply.github.com`. Never a real mailbox or real name.
- Commit messages: follow `.agents/skills/commit/SKILL.md` (`type(scope): 한국어 설명`, Korean noun-form subject, Korean `- ` body list). No tool-generated footers or session links. Stage named files, never `--no-verify`.
- Once per clone: `git config core.hooksPath .githooks`. Run `python tools/privacy_check.py` before every push. Extra personal terms go in `.privacy-terms.local` (gitignored).
- Do not push or create a remote unless the maintainer asks. Never force-push.
- Personal and tool-specific instructions stay local and gitignored (`AGENTS.local.md`, `.agents/local/`, your tool's own files). Do not reference them from tracked files.

## Working mode
- One main agent by default. Use subagents only for independent review after a milestone or independent research (license audit, OCR language coverage).
- Verify with `python tools/check_all.py` (Core tests, fixtures, templates, GUI strings, tool tests, privacy) before declaring work done; in the real game follow `ba-live-check`. Mark unverified game rules as "verify" in docs instead of asserting them.
- Facts about the game worth keeping go into `docs/notes/game-structure.md` (options into `docs/notes/game-options.md`). Decide case by case: skip one-off observations and what the code already shows.
- Documents hold values that apply to every player. Never write personal data or one account's values (UID, nickname, friend names, level, resources).
