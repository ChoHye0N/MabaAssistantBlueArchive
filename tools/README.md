# Tools - planned

## Template capture tool (needed at M0)
Take a screenshot from a controller, normalize to 1280x720, crop a region, and save to `assets/common/` or `assets/<lang>/` with metadata (task, element, language, source client, game version, capture date). Templates are captured by the project owner, or taken from public sources recorded in `assets/PROVENANCE.md`. The tool should offer cropping/masking of personal areas (nickname, UID) before saving.

## Privacy check (available now)
`python tools/privacy_check.py` scans files that would be committed for emails, local user paths, tokens, keys and any personal terms listed in `.privacy-terms.local` (gitignored). `--staged` checks staged content only. Enable the pre-commit hook once per clone with `git config core.hooksPath .githooks`; it also requires the commit author email to be a GitHub noreply address.

## Commit and push rules (available now)
`tools/git_rules.py` is called by `.githooks/commit-msg` (subject format `type(scope): description`, no trailing period, max 100 chars, no AI attribution lines) and `.githooks/pre-push` (privacy scan, plus every new commit: message rules, noreply author/committer email, no force push). See `CONTRIBUTING.md`. `MABA_ALLOW_ANY_EMAIL=1` and `MABA_ALLOW_FORCE=1` are explicit, owner-only escapes.
