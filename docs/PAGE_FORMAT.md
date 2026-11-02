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
  "transitions": [
    { "to": "mail", "click": { "template": "main_menu_mail_icon.png" } }
  ]
}
```

Rules:
- **Location**: one file per page in `pages/<id>.json`. Read by Core (`page.h`): `id`, `anchors`, `transitions`, `rois`; `roi_by_lang` and `strings` are accepted and ignored until used.
- **Home page**: `main_menu`. The Navigator returns there before each task and after a failure; every page should have a route back to it.
- **Anchors**: a page is detected when every anchor matches. `threshold` defaults to 0.9 and must be in (0, 1]. A `roi` with zero width or height means "whole frame".
- **Transitions**: `click` is either `{ "template": ..., "roi"?, "threshold"? }` (click the center of the match) or a fixed point `{ "x": ..., "y": ... }`. Every `to` must name a defined page (`ValidatePageGraph` warns otherwise).
- **Templates with masks**: transparent pixels (alpha 0, e.g. masked nicknames) are ignored by matching.
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
- `dismiss`: `{ "template": ... }` (click its center), `{ "x", "y" }` (fixed point) or `"back"` (back key).
- Handled only by the global popup handler, never inside a task. A popup still visible after `max_dismissals` attempts fails the current step with a saved frame.

## Tasks (`tasks/<id>.json`)
```json
{
  "id": "mail_claim_all",
  "name": "Mail: claim all",
  "after": ["spend_ap"],
  "status": "wip",
  "needs_ocr": false,
  "steps": [
    { "action": "goto", "page": "mail" },
    { "action": "click_if_present", "template": "mail_claim_all.png", "timeout_ms": 3000, "sleep_ms": 1500 },
    { "action": "return_home" }
  ]
}
```
- `after`: ids that run first when selected together. `status`: `todo` (no steps), `wip` (steps written, templates or checks missing), `ready` (verified in the game; CI then requires its templates in every confirmed language).
- No `steps` = not implemented: the runner skips it and it does not block dependents.

| Action | Fields | Behavior |
|---|---|---|
| `goto` | `page` | Navigate (popups settled, bounded steps) |
| `return_home` | | Navigate to `main_menu` |
| `wait_page` | `page` | Wait until it is the current page |
| `click` | `template` (+`roi`, `threshold`) or `x`, `y` | Wait until the template is visible, click its center; fail if it never appears |
| `click_if_present` | `template` (+`roi`, `threshold`) | Same, but a missing template is not an error |
| `wait_template` | `template` (+`roi`, `threshold`) | Wait until visible |
| `back` | | Back key |
| `sleep` | `sleep_ms` (1..10000) | Fixed pause |
| `read_number` | `roi`, `store` | OCR on a native-resolution crop; `120/240` stores `store` = 120 and `store_max` = 240 |

Common fields: `timeout_ms` (100..60000, default 5000) bounds every wait in the step; `repeat` (1..50) runs the action again; `sleep_ms` on a non-sleep action pauses after each repetition; `optional: true` logs a failure and continues with the next step.

## Strings and languages
- **Strings**: UI text that logic needs to compare lives in `strings/<lang>.json` under a key, never as a literal in code. A key missing for a language marks the task unsupported in that language; there is no silent fallback to another language.
- **Korean (`ko`) is the baseline**: implement and verify it first. Other languages are added by filling `strings/<lang>.json`, templates and optional `roi_by_lang`, without changing logic.
- OCR may need native-resolution detail: use `IController::CaptureRegion` for OCR crops instead of cropping the downscaled 1280x720 frame.
- Ground truth: keep labeled screenshots (image + expected values) for every OCR ROI so accuracy can be measured per language.
- Matrix: `python tools/check_templates.py` lists, per task and language, the templates still missing.
