# Tools

## Template capture (`template_capture.py`)
Needs Python 3.10+ with tkinter and `pip install -r tools/requirements.txt`.

1. Take a normalized 1280x720 screenshot from the game with `maba_cli grab` (saved under `captures/`, gitignored). Any other 16:9 screenshot also works; it is scaled to 1280x720.
2. `python tools/template_capture.py captures/<file>.png`: left-drag selects the template, right-drag adds a mask over personal data (nickname, UID, friend list, mail contents), Ctrl+Z removes the last mask. Masked pixels are saved as transparent and are ignored by template matching.
3. Fill in task, element (file name `<task>_<element>.png`, lowercase and underscores), language (`common` for icons and layout, a language code for text) and optionally the game version, then Save. A preview shows exactly what will be written.

Templates go to `assets-local/<lang|common>/` (gitignored) by default; "save to assets/" puts them in the committed tree, so check `assets/PROVENANCE.md` first. Each template gets a `.json` beside it with the crop (`roi`, usable as an anchor ROI), masks, source client, game version, capture date and tool version. The source file path is not recorded.

Batch mode for scripted captures: `python tools/template_capture.py SHOT.png --crop x,y,w,h --mask x,y,w,h --task main_menu --element mail_icon`.

Tests: `python -m unittest discover tools/tests`.

## Template check and language matrix (`check_templates.py`)
Lists, per task and language, the templates the task needs (its steps including nested ones, the pages on its route from and back to `main_menu`, and all popups; `{value}` in a template name is expanded to every choice of the task's list settings) and whether each resolves like Core resolves it (`assets-local/<lang>`, `assets/<lang>`, `assets-local/common`, `assets/common`). Standard library only.

- `python tools/check_templates.py`: Markdown matrix plus the missing templates.
- `--committed`: ignore `assets-local/` (what CI sees). `--json`: machine-readable.
- `--strict`: exit 1 if a task with `"status": "ready"` misses a template in a confirmed language (CI runs this).

## GUI string tables (`check_ui_strings.py`)
`python tools/check_ui_strings.py` compares every `gui/Maba.Gui/Resources/Strings.<lang>.xaml` with the Korean baseline: missing or extra keys, duplicates, empty texts and `{0}` placeholders that differ. Exit 1 on any problem (CI runs it). Standard library only.

## OCR accuracy (`measure_ocr.py`)
`ocr/.venv/Scripts/python tools/measure_ocr.py LABELS` runs the sidecar's engine over a label file and prints, per kind of sample, the exact reads, character errors, mean confidence and time per crop, then every miss. The label format is in the file's header. Labels point at screenshots, so keep them in `assets-local/`. Results: `docs/notes/ocr-accuracy.md`.

## Release archive (`build_package.ps1`)
`tools\build_package.ps1 -Version 0.1.0-beta.2` builds `dist\MABA-<version>-win-x64.zip` from a finished Core build and `ocr/.venv` with its models: self-contained GUI, embeddable Python, data files and templates. The archive holds one executable and the folders `bin/`, `data/`, `ocr/` and `docs/`; these tools sit in `data/tools/`, and `bin/maba_cli.exe` needs `--root ..\data`. `-Repository owner/name` turns on the new-version notice of the GUI. The `release` workflow runs it when started by hand and creates a draft.

## Privacy check (available now)
`python tools/privacy_check.py` scans files that would be committed for emails, local user paths, tokens, keys and any personal terms listed in `.privacy-terms.local` (gitignored). `--staged` checks staged content only. Enable the pre-commit hook once per clone with `git config core.hooksPath .githooks`; it also requires the commit author email to be a GitHub noreply address.

## Commit and push rules (available now)
`tools/git_rules.py` is called by `.githooks/commit-msg` (subject format `type(scope): description`, no trailing period, max 100 chars, no AI attribution lines) and `.githooks/pre-push` (privacy scan, plus every new commit: message rules, noreply author/committer email, no force push). See `CONTRIBUTING.md`. `MABA_ALLOW_ANY_EMAIL=1` and `MABA_ALLOW_FORCE=1` are explicit, owner-only escapes.
