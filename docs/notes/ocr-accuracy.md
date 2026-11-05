# OCR accuracy (Korean client)

**Update 2026-11-04, native 1920x1080 crops of 18 screens with the top bar** (15 of them without a dim layer over it):
- Without a margin credits read `105,521,1,567`: the number was split. With 16 px around the crop: credits and pyroxene 18 of 18, AP 17 of 18.
- `샬레 업무관` is not stable on native crops either: one screen read it correctly, a later one read `살레업무관` five times out of five. So the schedule finds locations with `fuzzy` (one misread character). The margin is for numbers only (`read_number`): with it, names read worse.

Measured 2026-11-04 with `tools/measure_ocr.py`: RapidOCR 3.10.0, PP-OCRv5 mobile Korean model, with detection on as in the task runner. Labels are in `assets-local/ocr-labels/ko.json` (local, they point at screenshots). Every expected value was reviewed by eye against the crop.

## Result
Saved 1280x720 screenshots, unique crops, the regions the tasks use.

| Kind | Items | Exact | Exact % | Mean score |
|---|---|---|---|---|
| AP (`120/240`) | 60 | 60 | 100.0 | 0.999 |
| Credits | 58 | 58 | 100.0 | 0.975 |
| Pyroxene | 58 | 58 | 100.0 | 0.999 |
| Pyroxene, region before the fix | 58 | 29 | 50.0 | 0.981 |
| Tickets (`7/7`) | 4 | 4 | 100.0 | 0.904 |
| Stage numbers | 28 | 28 | 100.0 | 0.997 |
| Student names (invite list) | 15 | 15 | 100.0 | 0.988 |
| Schedule locations | 40 | 34 | 85.0 | 0.952 |

Total without the old pyroxene region: 257 of 263 (97.7%). Digits: 208 of 208. No digit in this table was misread; a failed read comes back empty. Time per crop on this PC: about 1 to 2 s on CPU.

## Limits of the sample
- These are not independent samples. Unique crops repeat few values (14 AP, 14 credits, 10 pyroxene), and the same screens return.
- Tickets, student names and locations are small samples. Locations cover all 12 names, and the 15 student items are 9 different students.
- The table uses saved frames, not native crops; see the update above. A row cut off by the list edge is left out; such a row read `호시노(수영복)` as `호시니(스연보)`.
- English, Thai and Chinese are not measured.

## Findings
**Tight crops make text detection fail.** A failed read is empty, not wrong.
   - The pyroxene region included the edge of the crystal icon: half of the reads were empty. Moving it 10 px right (`account_info`) reads 58 of 58.
   - Simulated native size (crops upscaled by 1493/1280, bicubic): AP 0 of 60, credits 56 of 58, the old pyroxene region 8 of 58. Real runs did read AP, so a blurred upscale is not the same as a native crop, but the detector is clearly sensitive to crop size and margins.
   - A 16 px margin around the crop read AP, credits and pyroxene 176 of 176 at both sizes. Tickets fell to 2 of 4, possibly because the label and the number merge into one line, which `lenient` reading accepts and this score does not. **Verify** on native crops before adding a margin in the sidecar.
