# Contributing

Project: MabaAssistantBlueArchive (MABA).

## Setup
```
git config core.hooksPath .githooks
git config user.name  <your GitHub handle>
git config user.email <ID>+<handle>@users.noreply.github.com
```
Use your GitHub noreply address, never a personal mailbox. The hooks check this, scan for personal data and secrets (`tools/privacy_check.py`), and validate commit messages.

## Commits
Format: `type(scope): description`

- type: feat, fix, docs, refactor, test, chore, ci, perf, build, revert
- scope: optional, lowercase (core, controller, vision, navigator, tasks, ocr, gui, tools, assets, docs, build)
- The description is written in Korean, ends in a noun form (추가, 수정, 삭제 ...) and has no trailing period. About 50 characters, at most 100.
- Body: a `- ` list, one line per change (what and why), last line for tests/docs changes.
- One logical change per commit. Stage named files, not everything.

Examples: `feat(core): 좌표 정규화 모듈 추가`, `fix(navigator): 팝업 처리 중 무한 대기 문제`.

## Before you push
```
cmake --preset linux-dev && cmake --build --preset linux-dev && ctest --preset linux-dev
python tools/privacy_check.py
```
Do not force-push shared branches.

## Agent instructions
`AGENTS.md` and `.agents/skills/` hold project instructions for any coding agent and are shared. Keep personal or tool-specific files local (`.agents/local/`, `AGENTS.local.md`, tool directories); they are gitignored. Skills hold project facts only: no personal data, no names or links of other projects. Update them in the same commit as the change they describe.

## License and sources
Contributions are licensed under AGPL-3.0-only. Do not add game images you have no right to publish (see `assets/README.md` and `assets/PROVENANCE.md`). Do not copy code from other projects without checking its license first (see `docs/SOURCES.md`).
