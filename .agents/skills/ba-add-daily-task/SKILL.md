---
name: ba-add-daily-task
description: Procedure for adding or changing a daily task (cafe, schedule, shop, AP spending, mail, missions, etc.) in the Blue Archive automation project. Use for any work involving task definition files, page anchors, template capture, dependency order, popup coverage, screenshot-fixture tests, or per-language support status.
---

# Adding a daily task

Read `ba-architecture` first. This skill is the step-by-step for one task. The fixed procedure exists because per-task ad-hoc implementations drift apart in popup handling and language support, which raises maintenance cost.

## Steps

1. **Define it** in `tasks/<id>.json`, declaratively. Do not hard-code flow in C++. Proposed shape:
   ```json
   { "id": "mail_claim_all", "after": ["spend_ap"], "status": "todo", "needs_ocr": false, "notes": [] }
   ```
   - `after` is the ordering dependency. Express order as data so it is easy to change.
   - Schedule by server reset time, not local midnight. Keep the reset time as a setting and verify the value in-game.
2. **Add pages and anchors** to the Navigator page graph. Prefer language-neutral anchors (icons, layout positions); use per-language anchors only when text is unavoidable.
3. **Get templates**: capture them with the project capture tool from a 1280x720 screenshot (mask nickname/UID), or reuse a public source recorded in `assets/PROVENANCE.md` (see `ba-third-party-license`; unlicensed ones stay in gitignored `assets-local/`).
   - Language-neutral -> `assets/common/`; contains text -> `assets/<lang>/`.
   - Name files `<task>_<element>.png` so ownership is obvious.
4. **Check popups.** Make sure every popup that can appear on entry, during, or on exit is in the global handler list. Add missing ones to the handler, not to the task.
5. **Bound everything.** Every wait, retry and loop gets a cap and a timeout. On exceeding it: save a screenshot and fail the task.
6. **Test.** Feed screenshot fixtures to a fake Controller and assert page detection and click coordinates at each step. Items that need the real game go into a manual checklist in the PR description.
7. **Update the language matrix** with the languages the task works in; mark languages without templates as unsupported. Korean (`ko`) first: implement and verify it before other languages, with UI text in `strings/ko.json` by key.
8. **Update docs**: README feature list (both `README.md` and `README.en.md`) and any settings docs.

## First six tasks: notes

| Task | Notes |
|---|---|
| Cafe: enter, collect, tap students | Daily head-pat count is capped; keep it a setting. Verify whether cafe earnings include AP; if so, order it after AP spending. |
| Schedule: enter, spend tickets | First version: fixed location or priority list. Choosing by a specific student is hard to recognize; defer. |
| Shop: daily purchases | Purchase list comes from a config file. Rotating stock means templates need ongoing upkeep. |
| Spend AP (event maps, multiplier events) | Stage priority is a setting. Sweeping requires a 3-star clear: skip and log when not cleared. Remaining AP needs OCR. |
| Mail: claim all | AP cannot be held above 999, so run after AP spending: `after: ["spend_ap"]`. |
| Missions: claim all | Daily mission rewards give pyroxene. Check whether rewards include AP; adjust order if so. |

Items marked "verify" are unconfirmed game rules. Check them in the real client before implementing and update this table.

## Deferred
Total Assault, Limit-Release Showdown and story need AI or real gameplay. Do not add them with this procedure.
