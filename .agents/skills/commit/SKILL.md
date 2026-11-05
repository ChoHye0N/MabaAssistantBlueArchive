---
name: commit
description: Review pending changes and commit them in this repository's message format. Use whenever the user asks to commit, says "commit", or types /commit, and before creating any commit yourself (including the first scaffold commit).
---

# Commit

## Procedure
1. Run `git status` and `git diff`. One logical change per commit; if changes are mixed, propose splitting them. Read new notes for personal data and one account's values (UID, nickname, friend names, level, resources): `tools/privacy_check.py` finds only the terms it is given.
2. Build and run tests: `cmake --build --preset linux-dev` then `ctest --preset linux-dev`; on Windows use the `windows-msvc` preset. If they fail, do not commit; report the cause. For GUI changes also run `dotnet build gui/Maba.Gui -c Release` and `python tools/check_ui_strings.py`; for tools, `python -m unittest discover tools/tests`. Check that the build itself succeeded: `ctest` also passes on old binaries.
3. Stage only the files you mean to commit with `git add <paths>`. Do not use `git add -A` or `git add .` (exception: the very first scaffold commit, after reviewing `git status`). Never stage `assets-local/`, `.agents/local/` or `.privacy-terms.local` (gitignored anyway).
4. Write the message below and run `git commit`. Never use `--no-verify`: the hooks (privacy scan, noreply email, message rules) are the safety net. If a hook fails, fix the cause and commit again.

## Subject
`type(scope): description`

- type: feat, fix, docs, refactor, test, chore, ci, perf, build, revert
- scope: optional, lowercase. Suggested: core, controller, vision, navigator, tasks, ocr, gui, tools, assets, docs, build
- Description is written in Korean, ends in a noun form (추가, 수정, 삭제, 복원 ...) and has no trailing period.
- Several changes: join them with commas on one line. Mark setting names and identifiers with backticks.
- Aim for about 50 characters (the hook rejects subjects over 100).

Examples:
- `feat(core): 좌표 정규화 모듈 추가, 16:9 비율 검사`
- `fix(navigator): 팝업 처리 중 무한 대기 문제`
- `chore: 프로젝트 초기 뼈대 추가`
- `revert(vision): ROI 지정 기능 제거` (name the reverted feature and end in a noun form)

## Body
- Korean `- ` list: one line per change saying what changed and why. Join items on one line with commas; write identifiers as-is.
- Last line: tests and docs changes, e.g. `- 테스트 4개 추가, README 갱신`.
- For revert: list each removed thing on its own line; the last line says that tests, docs and CHANGELOG were reverted; add notes in parentheses, e.g. `(후속 작업 TODO도 뺌)`.

### Revert example
```
revert(vision): ROI 지정 기능 제거

- `SetRoi`, `ClearRoi` 제거
- [tasks] roi 설정 항목 삭제
- 관련 테스트, vision 문서, 예시 설정 되돌림 (후속 작업 TODO도 뺌)
```

## Never
- Commit messages and PR descriptions contain the change description and, if any, human co-authors. Do not add tool-generated footers, session links or similar marks. The `commit-msg` and `pre-push` hooks reject them.
- The commit author and committer use the contributor's GitHub noreply identity (see `CONTRIBUTING.md`), never a real name or mailbox.
- Push only when the maintainer asks. Never force-push. Never create a remote on your own.
- Build or publish a release, tag or release package only on the maintainer's explicit order.
