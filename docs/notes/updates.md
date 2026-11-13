# Updates

How the program updates itself. Two things are updated apart from each other, because they change at different speeds: the game's screens change often and need new task data and templates quickly, the program changes rarely.

| Part | Holds | Changes | How it is installed |
|---|---|---|---|
| Resources | `config/`, `pages/`, `popups/`, `tasks/`, `strings/`, `assets/` under the data folder | often | by the running program, no restart |
| Program | the executable and `bin/` | sometimes | planned: a small updater replaces the files and starts the program again |
| OCR | `ocr/` (Python and models) | seldom | planned: with the program, only when it changed |

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

## Program (planned)
Observed in other tools of this kind, written down here before anything is implemented:
- A small separate updater does the replacing, because a running program cannot replace its own files. The main program downloads and checks the package, unpacks it beside itself, writes what to move and remove, starts the updater and exits; the updater waits for it, moves the files, keeps the old ones for a rollback, starts the program again and leaves a success or failure note the program shows at its next start.
- The updater itself is updated by the main program before it is started: a new package's updater is copied over the old one first. An updater that tries to replace itself cannot.
- Only files whose content changed need to be replaced (compare hashes), so large folders that rarely change cost nothing.
- Folders where players keep their own files are listed as "never remove".
- A package that only touches resources needs no updater and no restart.

Planned shape here: one updater built with Core (`bin/`), packages per part (program, OCR) with a manifest in the release, so an update downloads only the parts whose version changed.
