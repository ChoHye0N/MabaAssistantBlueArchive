# Asset provenance

Every template or data file that did not come from the project owner's own captures gets a row here. Public availability is not a license: game images remain NEXON's, and a repository's code license does not cover them.

Rules:
- Source states a license or explicit permission that covers the files -> may be committed.
- Source states nothing, or only claims fair use -> treat as unlicensed: keep it in `assets-local/` (gitignored), do not commit.
- Avoid repositories that redistribute files extracted from game packages.
- Own captures need no row, but name the tool/version in the capture metadata.

| Asset set (folder or pattern) | Source URL | License / terms as stated there | Retrieved | Version / commit | Committed? | Notes |
|---|---|---|---|---|---|---|
| `assets-local/students/names.json` (student names in Korean and English) | Community game database, public data files (URL in the maintainer's local notes) | None stated | 2026-11-04 | 281 students | no | Local only. Name lists only, no images. |
