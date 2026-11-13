---
name: ba-release
description: Procedure for versions and releases of the Blue Archive automation project. Use when the maintainer orders a release, a version change, a release build or release notes, and never start any of it without that order.
---

# Releases

A release, tag or package is made only on the maintainer's explicit order (`docs/DECISIONS.md`, 24 and 28). "Commit and push" is not an order to release.

## Steps
1. **Finish the work first.** Everything that belongs in the release is committed and pushed, `python tools/check_all.py --build --gui` passes, and CI is green on that commit. A fix found after the draft was built means a new build, not notes that promise it.
2. **Version** in three files, in one commit `chore: 버전 X.Y.Z로 변경`: `CMakeLists.txt` (`project(... VERSION`), `vcpkg.json` (`version-string`), `gui/Maba.Gui/Maba.Gui.csproj` (`<Version>`). A version with `-` (for example `0.2.0-beta.1`) becomes a prerelease.
3. **Build the draft**: `gh workflow run release.yml -f version=X.Y.Z`, then wait for the run and for CI on the same commit. The workflow builds `MABA-X.Y.Z-win-x64.zip` and, for the program's own updater, `MABA-X.Y.Z-app.zip`, `MABA-X.Y.Z-ocr.zip` and `update.json`, and creates a draft with all four; it never publishes. A release without `update.json` only shows a notice to players.
4. **Notes** in Korean, for players: what changed (new, fixed), how to update (from 0.1.5 on the program installs it from the new-version link; older ones unpack into a new folder; settings and account records stay in the user folder), known limits that are still true, and the fixed notice (terms-of-service risk, screen recognition and ordinary input only, game images belong to NEXON Korea Corp. and NEXON GAMES Co., Ltd.). No internal names, no personal data, nothing that was not verified.
5. **Publish** only when both runs succeeded: `gh release edit vX.Y.Z --notes-file FILE --draft=false --latest`, then check that the tag points at the intended commit.

## Resource releases
Task data and templates reach players without a program release (`docs/notes/updates.md`). Also only on the maintainer's order.
1. Everything is committed, pushed and green; the change was checked in the game (`ba-live-check`), because a published pack installs itself on every player's PC.
2. Raise `version` in `config/resource.json` (date and counter, `20261113.1`). Raise `min_core` to the next program version when the data uses a step, condition or field the released program does not know; such a pack only reaches players after that program is released.
3. `gh workflow run resources.yml`, wait for it, read the draft `resources-<version>` (pack and `resources.json`), then publish it as a pre-release: `gh release edit resources-<version> --draft=false --prerelease`. Never mark it latest.
4. A bad pack is not deleted: publish a higher version with the fix (players never go back to an older pack).

## Rules
- A draft that misses a fix is deleted and rebuilt (a draft has no tag yet). A published release is not deleted or moved: the next version carries the fix.
- The GUI built in the repository shows the version it was built with: rebuild it after a version change (`dotnet build gui/Maba.Gui -c Release`) before checking it by hand.
- Never force-push, never skip the hooks, no tool-generated footers in commits or notes.
