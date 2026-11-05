# Asset provenance

Every template or data file that did not come from the project owner's own captures gets a row here. Public availability is not a license: game images remain NEXON's, and a repository's code license does not cover them.

The rules are in `docs/SOURCES.md`. Own captures need no row, but name the tool and version in the capture metadata.

| Asset set (folder or pattern) | Source URL | License / terms as stated there | Retrieved | Version / commit | Committed? | Notes |
|---|---|---|---|---|---|---|
| `assets-local/students/names.json` (student names in Korean and English) | Community game database, public data files (URL in the maintainer's local notes) | None stated | 2026-11-04 | 281 students | no | Local only. Name lists only, no images. |

## Missing
- Templates for languages other than Korean (`python tools/check_templates.py` lists them).
- Student names for languages other than Korean and English. The file above has `id`, `dev_name`, `ko`, `en`, family names and `global` (released on the global server). Nothing reads it yet; its uses are correcting OCR of student names and recording owned students.
- Student portraits, to recognise students in schedule rooms (schedule preference, roadmap).
- Event-specific screens: the tasks avoid depending on them.

## Candidate source
A community database of the game (checked 2026-11-02) publishes names per language, stage data and images. Its repository states no license and is archived, though its public site still gets new data. It counts as unlicensed. No openly licensed source of game screens was found: templates stay owner captures.
