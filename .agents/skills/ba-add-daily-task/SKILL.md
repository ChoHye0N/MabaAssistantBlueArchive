---
name: ba-add-daily-task
description: Procedure for adding or changing a daily task (cafe, schedule, shop, AP spending, mail, missions, etc.) in the Blue Archive automation project. Use for any work involving task definition files, page anchors, template capture, dependency order, popup coverage, screenshot-fixture tests, or per-language support status.
---

# Adding a daily task

Read `ba-architecture` first. This skill is the step-by-step for one task. The fixed procedure exists because per-task ad-hoc implementations drift apart in popup handling and language support, which raises maintenance cost.

## Steps

1. **Define it** in `tasks/<id>.json`, declaratively. Do not hard-code flow in C++. Step and setting format: `docs/PAGE_FORMAT.md`. Shape:
   ```json
   { "id": "mail_claim_all", "name": "...", "order": 90, "after": ["spend_mission"], "status": "wip", "needs_ocr": false,
     "notes": [], "settings": [], "steps": [] }
   ```
   - `order` sets the sequence and never blocks. `after` is only for a failure that must block. Express order as data so it is easy to change.
   - Keep per-task flow in the step language (`if`, `loop`, `each`, conditions on settings, templates, colours and OCR numbers). Change the engine only when a step cannot be expressed.
   - Options the user should choose are `settings` (bool, int, choice, list). The GUI shows them from `MabaGetTasks`; `label_key` and `ui: "checklist"` cover named check lists, with the text in `strings/<lang>.json`.
   - Stay `wip` until the task has run on a fresh day in the real client; only then mark it `ready` (CI then requires its templates).
2. **Add pages and anchors** in `pages/<id>.json` (the Navigator page graph). Prefer language-neutral anchors (icons, layout positions); use per-language anchors only when text is unavoidable.
3. **Get templates**: capture them with the project capture tool from a 1280x720 screenshot (mask nickname/UID), or reuse a public source recorded in `assets/PROVENANCE.md` (see `ba-third-party-license`; unlicensed ones stay in gitignored `assets-local/`).
   - Language-neutral -> `common/`; contains text -> `<lang>/`, under `assets-local/` or `assets/`. Lookup is `assets-local/` then `assets/`, per language then common.
   - Template matching ignores brightness, so a dimmed or greyed state still matches. Check such states with colour conditions (`color`, `no_color`, `click_color`) or a higher threshold.
   - Take the 1280x720 frame from `maba_cli grab` with the game at 1920x1080, on a screen with nothing dimming it. Cut whole words without neighbouring elements or key hints, and compare the result with the screen by eye.
   - Check look-alikes: two templates that differ by a letter or digit need a higher threshold, verified on saved frames.
   - A dialog's own text must match before a purchase or sweep is confirmed. An `expect` can be satisfied by a title behind a dialog; wait for the dialog to go away instead.
   - Name files `<task>_<element>.png` so ownership is obvious.
4. **Check popups.** Make sure every popup that can appear on entry, during, or on exit is in the global handler list. Add missing ones to `popups/*.json`, not to the task.
5. **Bound everything.** Every wait, retry and loop gets a cap and a timeout. On exceeding it: save a screenshot and fail the task.
   - Wait for the screen, not the clock: `wait_stable` or `wait_template` instead of `sleep`. A fixed-point click needs `expect`, `on_page` or `require`. Toggles get `no_verify`, small changes `change_roi` (`docs/PAGE_FORMAT.md`).
6. **Test.** Engine behaviour goes in `core/tests/flow_tests.cpp` with the fakes (`fakes.h`, `nav_rig.h`). `RepoTaskFilesLoad` in `task_tests.cpp` checks the run order. Screenshot fixtures (`maba_fixture_tests`) stay local because they show UIDs. Items that need the real game go into `docs/ROADMAP.md` and the PR description as a manual checklist.
7. **Check the language matrix** with `python tools/check_templates.py` (it covers nested steps, the pages on the route, popups and `{value}` names); languages without templates show as missing. Korean (`ko`) first: implement and verify it before other languages, with UI text in `strings/ko.json` by key.
8. **Update docs**: README feature list (both `README.md` and `README.en.md`), `docs/ROADMAP.md`, and `docs/PAGE_FORMAT.md` for new steps or conditions.

## Daily run order and notes
Order (`RepoTaskFilesLoad` checks it): cafe, schedule, circle, free package, shop, bounty, commissions, scrimmage, event, campaign missions, tactical challenge, mail, daily missions, account record. The server reset is 04:00 KST; schedule by it or by a clock time, not local midnight.

| Task | Notes |
|---|---|
| `cafe` | Income, invitations from a settings list (second cafe included), student taps. The view is zoomed out with the wheel first, then dragged to the corners. Income includes AP. |
| `schedule` | 12 locations by name, chosen with check boxes. Preferring rooms by student is still open. |
| `circle` | Attendance gives 10 AP through the mailbox. Skip when no circle is joined. |
| `free_package`, `shop` | Shop stock rotates, so templates need upkeep. Purchases are confirmed by the dialog's text. |
| `commission` | No tickets and AP is the only limit, so it sweeps a set count (`runs`) instead of MAX. Runs before the AP tasks. |
| `scrimmage` | Uses tickets and AP. Runs before the AP tasks. |
| `spend_event`, `spend_mission` | Event: Story (scenes skipped), then the highest 3-star Quest. Missions: one stage of the shown area. Sweeping needs a 3-star clear. Reads AP with OCR. |
| `tactical_challenge` | Claim rewards, then use every ticket. |
| `mail_claim_all` | AP above the 999 cap waits in the mailbox, so it runs after both AP tasks. |
| `mission_claim_all` | Rewards include pyroxene and AP (claimed AP can pass the regular cap), so it runs last. |
| `account_info` | Saves credits, pyroxene and AP per UID in the data directory, never in the repository. |

Items marked "verify" are unconfirmed game rules. Check them in the real client and update this table. Open items live in `docs/ROADMAP.md`; facts about the game worth keeping in `docs/notes/game-structure.md`, without personal data or one account's values.

## Deferred
Total Assault and Limit-Release Showdown need AI or real gameplay. Do not add them with this procedure. Story is only skipped (scene -> back -> confirm skip), never read or chosen.
