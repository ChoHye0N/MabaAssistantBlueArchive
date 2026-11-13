# Page, popup and task data format

Pages, popups and tasks are data, not code. Values shown are placeholders, not real coordinates. All coordinates are in the normalized 1280x720 space. `notes` arrays are allowed everywhere and ignored by Core.

## Pages (`pages/<id>.json`)
```json
{
  "id": "main_menu",
  "anchors": [
    { "template": "main_menu_mail_icon.png", "roi": { "x": 0, "y": 0, "w": 0, "h": 0 }, "threshold": 0.9 }
  ],
  "rois": {
    "ap_value": { "x": 0, "y": 0, "w": 0, "h": 0 }
  },
  "roi_by_lang": {
    "th": { "ap_value": { "x": 0, "y": 0, "w": 0, "h": 0 } }
  },
  "strings": ["main_menu.title"],
  "enter": { "tap": { "x": 0, "y": 0 }, "max_taps": 30, "interval_ms": 2000 },
  "transitions": [
    { "to": "mail", "click": { "template": "main_menu_mail_icon.png" } }
  ]
}
```

Rules:
- **Location**: one file per page in `pages/<id>.json`. Read by Core (`page.h`): `id`, `anchors`, `transitions`, `rois`, `enter`; `roi_by_lang` and `strings` are accepted and ignored until used.
- **Home page**: `main_menu`. The Navigator returns there before each task and after a failure; every page should have a route back to it.
- **Waited-out pages (`wait_ms`)**: a screen that ends by itself, like a battle. Navigation waits up to this long, also through a short unknown stretch right after it (skill cut-ins), and never presses back, because back opens the pause menu.
- **Start-up (`enter`, home page only)**: screens before the main screen (title, event splash, loading) have no stable anchors. At the start of a run, while no known page is visible, `tap` is clicked every `interval_ms` (100..10000, default 2000), at most `max_taps` times (1..100, default 30). `screens` (optional, in order) is a list of `{"when": template, "tap": point}`: when a start-up screen shows that template, its own point is tapped instead. Login popups are dismissed by `popups/` before each look. Pick a point that is harmless on the main screen.
- **Anchors**: a page is detected when every anchor matches. `threshold` defaults to 0.9 and must be in (0, 1]. A `roi` with zero width or height means "whole frame".
- **Transitions**: `click` is either `{ "template": ..., "roi"?, "threshold"? }` (click the center of the match) or a fixed point `{ "x": ..., "y": ... }`. Every `to` must name a defined page (`ValidatePageGraph` warns otherwise).
- **Templates with masks**: transparent pixels (alpha 0) are ignored by matching, which stays `TM_CCOEFF_NORMED`. Use them for background showing through a template (outside a round button) and for masked personal data, never for text that tells two templates apart.
- **Look-alike titles**: `업무` and `임무` differ by one letter and score 0.906 against each other. Raise `threshold` for such pairs (0.95) instead of leaving neighbouring pixels in the crop.
- **Templates** resolve in this order: `assets-local/<lang>/`, `assets/<lang>/`, `assets-local/common/`, `assets/common/`. Language-neutral anchors (icons, layout) belong in `common/`.
- **ROIs** for OCR should be tight around digits (AP value, ticket counts). Digits are language-independent. Identify text items by icon templates instead of OCR where possible.
- **`roi_by_lang`** is an optional per-language override, needed only when text length changes the layout. Without an entry the base `rois` apply.

## Popups (`popups/<id>.json`)
```json
{
  "id": "reward_result",
  "priority": 10,
  "anchors": [ { "template": "popup_reward_result.png" } ],
  "dismiss": { "x": 640, "y": 650 }
}
```
- Detected when every anchor matches; checked in `priority` order (higher first, default 0), then by id.
- `colors`, `no_colors` (optional): colour specs (see Tasks) that must all be visible, or must all be absent, besides the anchors. Used where a state is a colour (battle HUD: AUTO is yellow when on, white when off); requiring a bright colour also keeps darkened frames from triggering.
- `dismiss`: `{ "template": ... }` (click its center), `{ "x", "y" }` (fixed point) or `"back"` (back key).
- Handled only by the global popup handler, never inside a task. A popup still visible after `max_dismissals` attempts fails the current step with a saved frame.

## Tasks (`tasks/<id>.json`)
```json
{
  "id": "mail_claim_all",
  "name": "Mail: claim all",
  "order": 90,
  "after": ["spend_mission"],
  "status": "wip",
  "needs_ocr": false,
  "steps": [
    { "action": "goto", "page": "mail" },
    { "action": "click_if_present", "template": "mail_claim_all.png", "timeout_ms": 3000, "sleep_ms": 1500 },
    { "action": "return_home" }
  ]
}
```
- `order` (0..100000, default 1000): run order among tasks that do not depend on each other; tasks are sorted by it when loaded. It never blocks: a failed task does not stop the ones after it.
- `after`: ids that run first when selected together, and a failed one blocks this task. Use it only where a failure must stop the task (mail after AP spending); use `order` for plain sequencing.
- `keep_popups`: popup ids the task works in itself; they are not dismissed while it runs (buying AP opens the AP purchase window that every other task closes).
- `settings`: user options of the task. Each entry has `key`, `type` (`bool`, `int`, `choice`, `list`), `default`, `min`/`max` (int), `choices` (choice; allowed items of a list), and `description`. Optional `label_key` shows choices by their text in `strings/<lang>.json` (`<label_key><choice>`), and `ui: "checklist"` shows a list as check boxes in the GUI. A run overrides them with `{"<task id>": {"<key>": value}}` (C ABI `MabaStart` `"settings"`, CLI `--settings FILE`, GUI task settings); unknown keys and wrong types are rejected.
- `status`: `todo` (no steps), `wip` (steps written, templates or checks missing), `ready` (verified in the game; CI then requires its templates in every confirmed language).
- No `steps` = not implemented: the runner skips it and it does not block dependents.

| Action | Fields | Behavior |
|---|---|---|
| `goto` | `page` | Navigate (popups settled, bounded steps) |
| `return_home` | | Navigate to `main_menu` |
| `wait_page` | `page` | Wait until it is the current page |
| `click` | `template` (+`roi`, `threshold`), `x`/`y`, or `points` | Wait until the template is visible and click it; fail if it never appears. `points` picks a point by the innermost loop iteration (cycled), or with `"point_by": "value"` by the value of the innermost `each` (`"1"` = first point), for list settings that name positions |
| `click_if_present` | `template` (+`roi`, `threshold`) | Same, but a missing template is not an error |
| `wait_template` | `template` (+`roi`, `threshold`) | Wait until visible |
| `back` | | Back key |
| `sleep` | `sleep_ms` (1..10000) | Fixed pause. Only where time itself matters (polling a battle, a cooldown); otherwise `wait_stable` |
| `wait_stable` | `timeout_ms` | Wait until the screen stops moving. A nearly black loading screen does not count as resting. A screen that never rests is not an error |
| `read_number` | `roi`, `store`, `lenient` | OCR on a native-resolution crop after popups are settled; `120/240` stores `store` = 120 and `store_max` = 240. `lenient: true` reads the number at the end of the text ("tickets 5/5") |
| `read_text` | `roi`, `store` | OCR on a native-resolution crop; stores the text for `save` (`values`). A record keeps it without a history. For names: identify items by templates where possible |
| `click_color` | `color`, `offset`, `retry_offset`, `max` (1..50) | Click colour blobs one at a time, looking again after each click (moving targets), until none is left or `max` is reached. A blob still there after its click is clicked once more at `retry_offset` (when given) and then left alone. None found is not an error |
| `click_text` | `roi`, `texts`, `text_setting` (list, or a choice for one text such as a stage number) or `text_key` (a key of `strings/<lang>.json`, `{value}` filled in inside `each`), `click_x`, `offset`, `max_scrolls` (0..80), `from`, `to` | OCR the region (native resolution) and click the first wanted text found (spaces ignored, whole text must match); `click_x` clicks the row's button instead of the text. `fuzzy: true` also accepts one misread character in a text of 4 or more characters when nothing matches exactly; use it for fixed lists of distinct names, not for numbers or student names. Not found: swipe `from` -> `to` and read again, up to `max_scrolls` times, then fail (use `optional`) |
| `swipe` | `from`, `to`, `duration_ms` (50..5000) | Drag |
| `wheel` | `x`, `y`, `notches` (-20..20, not 0) | Mouse wheel at a point; negative turns it towards the user (zooms the cafe out). Emulators over ADB have no wheel: mark the step `optional` |
| `if` | `when`, `then`, `else` | Run `then` when the condition holds, else `else` |
| `loop` | `max` (1..50) or `max_setting` (int setting) or `max_value` (stored number), `max_adjust`, `until`, `steps` | Run `steps` up to that many times; `max_adjust` is added to the count (-1 for a quantity stepper that starts at 1); `until` is checked before each iteration |
| `each` | `setting` (list), `cases`, `steps`, `rotate_setting` | For every value of the list, in order: its `cases` entry, then `steps` (`{value}` in a template name is replaced by the value). While the bool setting `rotate_setting` is on, only one value runs, in turn per game day (from the 04:00 KST reset) |
| `save` | `file`, `values`, `history` | Write the stored numbers into a JSON record `file` under the data directory (`{name}` = a stored number, e.g. `accounts/{uid}.json`); `history` also appends them with a UTC timestamp (newest 1000 kept). The data directory is outside the repository (records hold personal data) |
| `break` | | Leave the innermost `loop`, or the current value of `each` |
| `end` | `status` (`succeeded`/`skipped`), `message` | Finish the task now |
| `fail` | `message` | Fail the task |

Click options (`click`, `click_if_present`): `pick` (`best`, `top`, `bottom`, `left`, `right`) chooses among several matches; `offset` `{x, y}` moves the click from the match (may be negative); `expect` (a template) is waited for after the click, and the click is repeated up to `retries` (0..5, default 2) times when it does not appear, because the game ignores input while a screen is still sliding in.

Reaction check (`click`, `click_if_present`, `back`): the screen is compared before and after, and a step whose action changed nothing is repeated `retries` times (default 2), then fails. `expect` replaces the comparison, unless the template was already visible before the click: then the screen must change as well. After every reaction the runner waits for the screen to rest.
- `expect_gone`: the click worked when this template left the screen, otherwise it is repeated. For buttons on a screen that moves by itself: a formation screen shows Sortie before its team has loaded and ignores the click, and the team appearing looks like a reaction.
- `change_roi`: compare only this region, for changes too small to show on the whole frame (a number, a check box).
- `no_verify: true`: no check, for clicks where no change is normal or a repeat would undo them (toggles, a tab that may already be open, MAX when one sweep is all the AP allows).
- A fixed-point click must name the screen it belongs to: `expect`, `on_page` (a page id) or `require` (a template). With `on_page` or `require` nothing is clicked on any other screen. `ValidateTasks` rejects a fixed-point click without one of them.

Conditions (`when`, `until`):
- `{"setting": key, "equals" | "not_equals": value}`, `{"setting": key, "contains": item}` (list).
- `{"present": template}`, `{"absent": template}`: one look, or up to `timeout_ms` for `present`.
- `{"color": colour}`: a colour blob is visible; `{"no_color": colour}`: none is. `roi_by_value` (a list of areas) takes the area by the value of the innermost `each` (`"1"` = first); `roi_by_loop` by the round of the innermost `loop` (cycled), to look at the item a `points` click of that round picks. With `color`, `min_count` (0..64, default 1) asks for that many blobs, and `min_count_by_outer_loop` (a list) takes the count by the round of the outermost loop (cycled): several passes over the same items, each accepting fewer (`[3, 2, 1, 0]`; 0 always holds).
- `{"value": name, "equals" | "not_equals" | "lt" | "le" | "gt" | "ge": n}`: a number stored by `read_number` (a missing value is false). `n` can be `{"setting": key}`, an int setting (a price limit).
- `{"first_iteration": true | false}`: whether the innermost `loop` is in its first round (outside a loop: first).

Colours (`click_color`, `{"color": ...}`): `hsv_min`, `hsv_max` (`[h, s, v]` on OpenCV's scale: h 0..180, s and v 0..255), optional `width`, `height`, `area` as `[min, max]` (pixels of the 1280x720 frame) and `roi`. Use them for markers whose shape animates (cafe students) or for states a template cannot see (a dimmed card: template matching ignores brightness).

Common fields: `when` (the step runs only when it holds), `timeout_ms` (100..60000, default 5000) bounds every wait in the step; `repeat` (1..50) runs the action again; `sleep_ms` on a non-sleep action pauses after each repetition; `optional: true` continues with the next step after a failure (also of a nested step) and reports it in the task's result message; add `quiet: true` where failing is normal, such as the Quest tab of an event that has none. Steps nest at most 8 levels deep.

Safety rules for data: confirm a purchase or a sweep only after the confirmation dialog's own text matched (`gem_shop_confirm_price_free.png`, `stage_sweep_confirm_question.png`); close any other dialog with `back`. Never click a fixed point that is a purchase button on another screen.

## Strings and languages
- **Strings**: UI text that logic needs to compare lives in `strings/<lang>.json` under a key, never as a literal in code (for example `schedule.location.*`, the schedule location names found with OCR). A key missing for a language marks the task unsupported in that language; there is no silent fallback to another language.
- **Korean (`ko`) is the baseline**: implement and verify it first. Other languages are added by filling `strings/<lang>.json`, templates and optional `roi_by_lang`, without changing logic.
- OCR may need native-resolution detail: use `IController::CaptureRegion` for OCR crops instead of cropping the downscaled 1280x720 frame.
- Ground truth: keep labeled screenshots (image + expected values) for every OCR ROI so accuracy can be measured per language.
- Matrix: `python tools/check_templates.py` lists, per task and language, the templates still missing.
