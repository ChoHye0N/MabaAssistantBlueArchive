# Assets (templates)

- `common/`: language-neutral templates (icons, layout anchors).
- `<lang>/`: templates containing text, per language code from `config/languages.json` (`ko`, `en`, `zh-Hant`, `th`; `ja` after verification).
- Lookup order: `assets-local/<lang>/`, `assets/<lang>/`, then `assets-local/common/` and `assets/common/`. `assets-local/` mirrors this layout, is gitignored, and holds anything that must not be committed.
- Naming: `<task>_<element>.png`, captured at 1280x720 with `tools/template_capture.py`. The `.json` it writes beside a template (crop, masks, capture details) is a record only: nothing reads it, and a template without one is valid.
- A template that no page, popup or task names is removed (`tools/check_templates.py` lists what each task needs).
- Test screenshots of real game screens go to `assets-local/fixtures/<lang>/<page_id>__<name>.png` (never committed); `maba_fixture_tests` checks that each is detected as that page only.

**Copyright:** game images belong to NEXON Korea Corp. and NEXON GAMES Co., Ltd. and are NOT covered by this project's AGPL-3.0-only license. The templates here are the maintainer's own captures and show no personal data; external resources are recorded in `PROVENANCE.md`.
