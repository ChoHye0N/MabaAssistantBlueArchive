# Updates

How the program updates itself. Two things are updated apart from each other, because they change at different speeds: the game's screens change often and need new task data and templates quickly, the program changes rarely.

| Part | Holds | Changes | How it is installed |
|---|---|---|---|
| Resources | `config/`, `pages/`, `popups/`, `tasks/`, `strings/`, `assets/` under the data folder | often | by the running program, no restart |
| Program | the executable, `bin/`, `docs/`, `data/tools/`, licence texts | sometimes | a small updater replaces the files and starts the program again |
| OCR | `ocr/` (Python, packages and models) | seldom | with the program, downloaded only when it changed |

## Releasing
`tools/release.py` starts either kind of release with one command (`program X.Y.Z` or `resources`): it sets the versions, commits, pushes and starts the workflows, which end in drafts the maintainer publishes. A program release also builds a resource pack of the same commit, because a program that updates itself keeps its data folder and gets new data only as a pack; publish the program first, then the pack.

`min_core` is decided there: data can only need a newer program when Core (`core/src`, `core/include`) changed since the last program release. Then it becomes the version being released; otherwise it stays, at most the last released version. A resource release while Core changed and no newer version is in the source is refused, unless `--min-core` says the data uses nothing new.

## Resources (implemented)
- **Version**: `config/resource.json` holds `version` (date and a counter, `20261113.1`) and `min_core`, the oldest program that understands the data. Raise `version` for every pack and `min_core` whenever the data starts to use a step, condition or field an older program does not know.
- **Pack**: `tools/build_resources.py` writes `MABA-resources-<version>.zip` and `resources.json` (version, `min_core`, file name, size, SHA-256). The archive is deterministic.
- **Release**: the `resources` workflow (started by hand) attaches both to a draft pre-release tagged `resources-<version>`; the maintainer publishes it. A pre-release never becomes the "latest" release, which stays the program's.
- **Check**: a release build looks at the repository's releases at start (the setting that also controls the new-version notice) and takes the highest `resources-*` tag that is not a draft. It reads that release's `resources.json`.
- **Install**, only when all of these hold:
  1. the offered version is higher than the installed one (never back to an older pack);
  2. this program is at least `min_core` (otherwise the log asks for a program update);
  3. size and SHA-256 of the download match the manifest;
  4. every entry of the archive lies inside one of the six folders (no absolute path, no `..`, nothing in `assets-local/`);
  5. Core loads the new data without an error.
  Then the six folders are swapped and the previous ones kept under `update/resources-old/`; a failed swap is put back. The task list is loaded again.
- **Never** while tasks run: the pack then waits behind a link in the status bar. `assets-local/` (the player's own templates) is not touched.
- **Setting**: "update resources automatically" (on by default). Off: a link shows the waiting version and installs it on a click. The settings tab also has a button to check now.
- **Development builds** read the repository and are never changed: updating needs the data folder beside the executable.
- `MABA_RESOURCE_FEED` (a folder with `resources.json` and the pack) replaces GitHub, for tests and offline installs.

There is no signature check: the pack is trusted as far as the repository's releases and HTTPS are. A checksum catches a damaged download, not a replaced release; protect the account that can publish (two-factor sign-in). A signature can be added on top later.

## Program (implemented)
A running program cannot replace its own files, so a small separate updater does it.

- **Packages**: `tools/build_update.py` (run by `build_package.ps1`) splits the release folder into `MABA-<version>-app.zip` (the executable, `bin/`, `docs/`, `data/tools/`, licence texts) and `MABA-<version>-ocr.zip` (`ocr/`), and writes `update.json`: the version and, per part, its version, file, size and SHA-256. The OCR part's version is a hash of what it is made of (Python runtime, installed package versions, `server.py`, `requirements.txt`), kept in `ocr/part.json`; it stays the same from release to release until one of those changes. The release workflow attaches the three files beside the full archive. Resources are in neither package.
- **Check**: at start (the same setting as the new-version notice) the program reads `update.json` of the latest full release. Pre-releases and resource releases are never "latest", so they are never offered. Only a higher version counts (never back). A part whose version equals the installed one is not downloaded; the program part always is.
- **Download**: each part is fetched into `update/`, its size and SHA-256 compared with the manifest, and unpacked into `update/program-new/`. Entries may only go where their part belongs (the program part: an executable or a licence text at the top, `bin/`, `docs/`, `data/tools/`; the OCR part: `ocr/`); an absolute path, `..` or anything else stops the update.
- **Install**: the program copies the updater to `update/maba_updater.exe` (the new release's updater when it brings one, so the updater is renewed before it runs, and `bin/` is free to be replaced), writes `update/plan.json` (root, unpacked folder, backup folder, entries to swap, the executable to start, its own process id) and exits. The updater waits for that process, moves each old entry to `update/program-old/` and the new one into place, trying again for a while when a file is still in use. When one entry fails, the entries already moved are put back, so the installation is the old one or the new one, never a mix. It writes `update/status.json` and starts the program, which logs the result at that start.
- **Kept**: settings and account records (`%APPDATA%`), the resource folders, `data/assets-local/`, and `ocr/` when that part did not change. The replaced files stay in `update/program-old/` until the next update.
- **Never** while tasks run: the link then only says that the update waits for the run to end.
- **Setting** "program update": notice only (default; the status-bar link asks and installs), download automatically (the link then installs without waiting), install automatically at start.
- An installation without `bin/maba_updater.exe` (a release older than this feature, a development build) keeps the plain notice: the link opens the release page.
- `MABA_UPDATE_FEED` (a folder with `update.json` and the packages) replaces GitHub, for tests and offline installs.
- The portable part (reading the plan, swapping, undoing) is `core/src/update_apply.cpp` with unit tests; `core/apps/maba_updater.cpp` is the Windows program around it. It links the static C runtime and no other library.

As with resources there is no signature check: the release and HTTPS are trusted, the checksum catches a damaged download.

Checked 2026-11-13 with two locally built releases and a local feed: 0.1.5 installed 0.1.6 and started again in each mode, the OCR part was not downloaded, a wrong checksum was refused. Verify: the same against a published release.
