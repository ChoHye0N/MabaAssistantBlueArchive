# Assets (templates)

- `common/`: language-neutral templates (icons, layout anchors).
- `<lang>/`: templates containing text, per language code from `config/languages.json` (`ko`, `en`, `zh-Hant`, `th`; `ja` after verification).
- Lookup order: `assets-local/<lang>/`, `assets/<lang>/`, then `assets-local/common/` and `assets/common/`. `assets-local/` mirrors this layout, is gitignored, and holds anything that must not be committed.
- Naming: `<task>_<element>.png`, captured at 1280x720.

**Copyright:** game images belong to NEXON Korea Corp. and NEXON GAMES Co., Ltd. and are NOT covered by this project's AGPL-3.0-only license. Prefer templates from publicly available sources or your own captures, and record every external source in [PROVENANCE.md](PROVENANCE.md). Resources whose source states no license or permission stay in `assets-local/` and are not committed. Before saving a capture, crop or mask anything personal (player nickname, UID, friend list, mail contents).
