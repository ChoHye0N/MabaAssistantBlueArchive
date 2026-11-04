#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Measures OCR accuracy on labeled game crops.

Runs the OCR engine of the sidecar over a label file and prints, per kind of sample, how many
were read exactly, the character error rate, the mean confidence and the time per crop.

Label file (keep it in assets-local/: labels point at screenshots, which stay local):
    {"language": "ko", "ocr_lang": "korean", "samples": [
        {"image": "captures/a.png", "roi": [500, 8, 105, 30], "kind": "ap", "expected": "820/240"},
        {"image": "captures/b.png", "roi": [690, 130, 90, 560], "kind": "stage", "expected": ["13", "14"]}
    ]}
`expected` is a text (the whole crop must read as it) or a list of texts (each item is scored on
its own, order ignored: a list crop such as stage rows). `roi` is optional. Spaces are ignored.

Needs the OCR environment: ocr/.venv/Scripts/python tools/measure_ocr.py LABELS
"""
import argparse
import json
import sys
import time
from pathlib import Path
from typing import Any, Callable, Dict, List, Sequence

ROOT = Path(__file__).resolve().parent.parent

# recognize(sample) -> [{"text", "box": [x, y, w, h], "score"}]
Recognize = Callable[[Dict[str, Any]], List[Dict[str, Any]]]


def squeeze(text: str) -> str:
    return "".join(text.split())


def edit_distance(a: str, b: str) -> int:
    row = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        previous, row[0] = row[0], i
        for j, cb in enumerate(b, 1):
            previous, row[j] = row[j], min(row[j] + 1, row[j - 1] + 1, previous + (ca != cb))
    return row[-1]


def reading_order(results: Sequence[Dict[str, Any]]) -> List[Dict[str, Any]]:
    """Lines top to bottom; boxes whose centres are within half a box height share a line."""
    ordered: List[Dict[str, Any]] = []
    rows: List[List[Dict[str, Any]]] = []
    for r in sorted(results, key=lambda r: r["box"][1] + r["box"][3] / 2):
        centre = r["box"][1] + r["box"][3] / 2
        if rows:
            first = rows[-1][0]["box"]
            if abs(first[1] + first[3] / 2 - centre) <= first[3] / 2:
                rows[-1].append(r)
                continue
        rows.append([r])
    for row in rows:
        ordered.extend(sorted(row, key=lambda r: r["box"][0]))
    return ordered


def score_sample(sample: Dict[str, Any], results: Sequence[Dict[str, Any]]) -> Dict[str, Any]:
    """Returns items, exact items, edit errors, reference characters, and what went wrong."""
    ordered = reading_order(results)
    expected = sample["expected"]
    if isinstance(expected, list):
        found = [squeeze(r["text"]) for r in ordered]
        spare = list(found)
        exact = errors = chars = 0
        missed = []
        for item in (squeeze(e) for e in expected):
            chars += len(item)
            if item in spare:
                spare.remove(item)
                exact += 1
                continue
            missed.append(item)
            # The closest unused line shows what the item was misread as.
            best = min(spare, key=lambda t: edit_distance(item, t), default="")
            if best:
                spare.remove(best)
            errors += edit_distance(item, best)
        return {"items": len(expected), "exact": exact, "errors": errors, "chars": chars,
                "misses": [f"{m!r} not found in {found!r}" for m in missed]}
    want = squeeze(expected)
    got = "".join(squeeze(r["text"]) for r in ordered)
    ok = want == got
    return {"items": 1, "exact": int(ok), "errors": edit_distance(want, got), "chars": len(want),
            "misses": [] if ok else [f"{want!r} read as {got!r}"]}


def evaluate(samples: Sequence[Dict[str, Any]], recognize: Recognize) -> Dict[str, Dict[str, Any]]:
    report: Dict[str, Dict[str, Any]] = {}
    for sample in samples:
        started = time.perf_counter()
        results = recognize(sample)
        elapsed = time.perf_counter() - started
        scored = score_sample(sample, results)
        row = report.setdefault(sample["kind"], {"crops": 0, "items": 0, "exact": 0, "errors": 0,
                                                 "chars": 0, "score_sum": 0.0, "score_count": 0,
                                                 "seconds": 0.0, "misses": []})
        row["crops"] += 1
        row["seconds"] += elapsed
        row["score_sum"] += sum(r["score"] for r in results)
        row["score_count"] += len(results)
        for key in ("items", "exact", "errors", "chars"):
            row[key] += scored[key]
        where = f"{sample.get('image', '?')} {sample.get('roi', '')}".strip()
        row["misses"].extend(f"{where}: {m}" for m in scored["misses"])
    return report


def summary(report: Dict[str, Dict[str, Any]]) -> str:
    lines = ["| Kind | Crops | Items | Exact | Exact % | Char errors | Mean score | ms/crop |",
             "|---|---|---|---|---|---|---|---|"]
    totals = {"crops": 0, "items": 0, "exact": 0, "errors": 0, "chars": 0}
    for kind in sorted(report):
        r = report[kind]
        for key in totals:
            totals[key] += r[key]
        score = r["score_sum"] / r["score_count"] if r["score_count"] else 0.0
        lines.append(f"| {kind} | {r['crops']} | {r['items']} | {r['exact']} "
                     f"| {100 * r['exact'] / r['items']:.1f} | {r['errors']}/{r['chars']} "
                     f"| {score:.3f} | {1000 * r['seconds'] / r['crops']:.0f} |")
    if totals["items"]:
        lines.append(f"| total | {totals['crops']} | {totals['items']} | {totals['exact']} "
                     f"| {100 * totals['exact'] / totals['items']:.1f} "
                     f"| {totals['errors']}/{totals['chars']} | | |")
    return "\n".join(lines)


def engine_recognizer(ocr_lang: str, base: Path) -> Recognize:
    import numpy as np
    from PIL import Image

    sys.path.insert(0, str(ROOT / "ocr"))
    from server import RapidOcrEngine

    engine = RapidOcrEngine()

    def recognize(sample: Dict[str, Any]) -> List[Dict[str, Any]]:
        with Image.open(base / sample["image"]) as image:
            crop = image.convert("RGB")
        if sample.get("roi"):
            x, y, w, h = sample["roi"]
            crop = crop.crop((x, y, x + w, y + h))
        bgr = np.asarray(crop)[:, :, ::-1]  # the sidecar receives BGR
        return engine.recognize(bgr, crop.width, crop.height, ocr_lang, True)

    return recognize


def main(argv: Sequence[str] = ()) -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("labels", help="label file (see above)")
    parser.add_argument("--base", default=str(ROOT), help="folder image paths are relative to")
    parser.add_argument("--json", action="store_true", help="print the report as JSON")
    args = parser.parse_args(argv or None)
    labels = json.loads(Path(args.labels).read_text(encoding="utf-8"))
    report = evaluate(labels["samples"], engine_recognizer(labels["ocr_lang"], Path(args.base)))
    if args.json:
        print(json.dumps(report, ensure_ascii=False, indent=1))
        return 0
    print(summary(report))
    misses = [m for kind in sorted(report) for m in report[kind]["misses"]]
    if misses:
        print(f"\nMisses ({len(misses)}):")
        for miss in misses:
            print(f"- {miss}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
