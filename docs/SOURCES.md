# Source policy and porting log

This project is AGPL-3.0-only.

## Code
- Write new code here. Do not copy, translate or line-by-line port code from projects whose license is incompatible with AGPL-3.0-only.
- Code taken from a compatible project (AGPL-3.0 or compatible) keeps its original copyright header and gets a row in the porting log below, in the same commit.
- Code the project owner wrote elsewhere may be moved in freely; log it too.
- Behavior notes written in your own words go to `docs/notes/<topic>.md`. Implement from the notes, never with another project's source open beside you.
- Do not commit snapshots or clones of other repositories.

## Game resources (images, data)
Game images belong to NEXON; a repository's code license does not cover them.
- Record every external resource in `assets/PROVENANCE.md`: URL, the license or terms as stated there, retrieval date, version/commit, what was used.
- Source states a license or explicit permission covering the files: may be committed.
- Source states nothing, or only claims fair use: treat as unlicensed. Keep it in `assets-local/` (gitignored); do not commit it.
- Avoid repositories that redistribute files extracted from game packages.
- Until suitable public resources are found, templates are captured by the owner with the capture tool.

## Porting log
Add one row per ported or heavily adapted file.

| Date | Source (repo / file / commit) | Destination | Original license | Notes |
|---|---|---|---|---|
| 2026-11-02 | Auto-Affection-Skip (project owner's earlier project) / `Core/imageProcessing.cpp` / `14cafd5` | `core/src/platform/windows/gdi_capture.{h,cpp}` | AGPL-3.0 (same author) | Window capture (BitBlt, PrintWindow), reused memory DC. Changed: client origin from `ClientToScreen` instead of symmetric borders, BitBlt copies the client area only, no global state. |
| 2026-11-02 | Auto-Affection-Skip (project owner's earlier project) / `Core/macro.cpp` / `14cafd5` (`MouseClick`, `KeyPressScan`) | `core/src/platform/windows/send_input.{h,cpp}` | AGPL-3.0 (same author) | SendInput mouse and scan-code keys. Changed: virtual-desktop coordinates for multiple monitors, separate move/down/up. |
| 2026-11-02 | Auto-Affection-Skip (project owner's earlier project) / `Core/dllmain.cpp` / `14cafd5` | `core/src/platform/windows/dpi_scope.h` | AGPL-3.0 (same author) | Per-monitor DPI awareness. Changed: per-thread scope instead of process-wide. |
| 2026-11-02 | Auto-Affection-Skip (project owner's earlier project) / `Core/macro.cpp` / `14cafd5` (`LoadTemplate`, `FindImage`, `FindMultiImage`) | `core/src/vision.cpp` | AGPL-3.0 (same author) | Alpha-mask templates, single/multiple matching with ROI. Changed: normalized 1280x720 space, per-instance cache, Unicode-safe file I/O, NaN-safe scores. |
