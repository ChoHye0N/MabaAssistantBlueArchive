# Page and task data format (draft)

Pages are data, not code. The schema below is a draft; values shown are placeholders, not real coordinates. All coordinates are in the normalized 1280x720 space.

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
- **Location**: one file per page in `pages/<id>.json`. Implemented so far (`page.h`): `id`, `anchors`, `transitions`; other fields (`rois`, `roi_by_lang`, `strings`, `notes`) are accepted and ignored until used.
- **Anchors**: a page is detected when every anchor matches. `threshold` defaults to 0.9 and must be in (0, 1]. A `roi` with zero width or height means "whole frame".
- **Transitions**: `click` is either `{ "template": ..., "roi"?, "threshold"? }` (click the center of the match) or a fixed point `{ "x": ..., "y": ... }`.
- **Templates with masks**: transparent pixels (alpha 0, e.g. masked nicknames) are ignored by matching.
- **Templates** resolve in this order: `assets-local/<lang>/`, `assets/<lang>/`, `assets-local/common/`, `assets/common/`. Language-neutral anchors (icons, layout) belong in `common/`.
- **ROIs** for OCR should be tight around digits (AP value, ticket counts). Digits are language-independent. Identify text items by icon templates instead of OCR where possible.
- **`roi_by_lang`** is an optional per-language override, needed only when text length changes the layout. Without an entry the base `rois` apply.
- **Strings**: UI text that logic needs to compare lives in `strings/<lang>.json` under a key, never as a literal in code. A key missing for a language marks the task unsupported in that language; there is no silent fallback to another language.
- **Korean (`ko`) is the baseline**: implement and verify it first. Other languages are added by filling `strings/<lang>.json`, templates and optional `roi_by_lang`, without changing logic.
- OCR may need native-resolution detail: use `IController::CaptureRegion` for OCR crops instead of cropping the downscaled 1280x720 frame.
- Ground truth: keep labeled screenshots (image + expected values) for every OCR ROI so accuracy can be measured per language.
