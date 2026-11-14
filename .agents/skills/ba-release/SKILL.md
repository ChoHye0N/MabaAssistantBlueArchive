---
name: ba-release
description: Procedure for versions and releases of the Blue Archive automation project. Use when the maintainer orders a release, a version change, a release build or release notes, and never start any of it without that order.
---

# Releases

A release, tag or package is made only on the maintainer's explicit order (`docs/DECISIONS.md`, 24 and 28). "Commit and push" is not an order to release.

## Steps
1. **Finish the work first.** Everything that belongs in the release is committed and pushed, `python tools/check_all.py --build --gui` passes, and CI is green on that commit. A fix found after the draft was built means a new build, not notes that promise it.
2. **Version and build**: `python tools/release.py program X.Y.Z` (try `--dry-run` first). It sets the version in `CMakeLists.txt`, `vcpkg.json` and `gui/Maba.Gui/Maba.Gui.csproj`, sets the resource version and `min_core` in `config/resource.json`, commits (`chore: 버전 X.Y.Z로 변경`), pushes, and starts the `release` workflow and, for players who update inside the program, the `resources` workflow. A version with `-` (for example `0.2.0-beta.1`) becomes a prerelease.
3. **Wait** for both runs and for CI on the same commit. The workflow builds `MABA-X.Y.Z-win-x64.zip` and, for the program's own updater, `MABA-X.Y.Z-app.zip`, `MABA-X.Y.Z-ocr.zip` and `update.json`, and creates a draft with all four; it never publishes. A release without `update.json` only shows a notice to players.
4. **Notes** in Korean, for players: one opening sentence, then the `## ` sections `새 기능` and `수정`. The program shows exactly these two sections in a window after it updated itself. Nothing else: no list of things the player has to do (make the program do it, or say it inside the feature's line), no section on how to update, no notice text (the README carries the terms-of-service and rights notices), no known limits. No internal names, no personal data, nothing that was not verified.
5. **Publish** only when both runs succeeded: `gh release edit vX.Y.Z --notes-file FILE --draft=false --latest`, then check that the tag points at the intended commit.

## Resource releases
Task data and templates reach players without a program release (`docs/notes/updates.md`). Also only on the maintainer's order.
1. Everything is committed, pushed and green; the change was checked in the game (`ba-live-check`), because a published pack installs itself on every player's PC.
2. `python tools/release.py resources` (try `--dry-run` first): it raises `version` in `config/resource.json`, decides `min_core` (the released program unless Core changed since; `--min-core` overrides), commits, pushes and starts the `resources` workflow. A pack that needs a program not released yet only reaches players after that release.
3. Wait for it, read the draft `resources-<version>` (pack and `resources.json`), then publish it as a pre-release: `gh release edit resources-<version> --draft=false --prerelease`. Never mark it latest.
4. A bad pack is not deleted: publish a higher version with the fix (players never go back to an older pack).

## Rules
- A draft that misses a fix is deleted and rebuilt (a draft has no tag yet). A published release is not deleted or moved: the next version carries the fix.
- The GUI built in the repository shows the version it was built with: rebuild it after a version change (`dotnet build gui/Maba.Gui -c Release`) before checking it by hand.
- Never force-push, never skip the hooks, no tool-generated footers in commits or notes.
