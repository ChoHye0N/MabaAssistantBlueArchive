# Resources: what is missing and where it could come from

Game images and data belong to NEXON. Every template in this project is captured by the owner with the capture tool and kept in `assets-local/` (gitignored); see `assets/PROVENANCE.md` for the rules on external resources.

## Missing
- Templates for languages other than Korean (`python tools/check_templates.py` lists them).
- A list of student names per language, to correct OCR of student names (owned students, cafe invitations, schedule preferences). Needed before recording owned students again.
- Student portraits, to recognise students in schedule rooms by image (schedule preference, roadmap).
- Event-specific screens (each event has its own banner and stage names); the tasks avoid depending on them.

## Candidate source (checked 2026-11-02)
A community database of the game publishes student data (names per language, including Korean), stage data and images (portraits, icons, UI). Its repository states no license and is archived. Under the source policy it is therefore unlicensed: anything taken from it stays in gitignored local folders, is never committed, and gets a row in `assets/PROVENANCE.md` with "Committed? no" when it is first used. Nothing from it is used yet.

## Not found
- An openly licensed source of game screens or templates: none found so far; templates stay owner captures.
