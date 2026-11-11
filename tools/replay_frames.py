#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Check saved 1280x720 frames against the page, popup and template data, without the game.

  replay_frames.py "captures/run1/f*.png"                       which page and popups each frame is
  replay_frames.py "captures/**/*.png" --template NAME.png --roi x,y,w,h [--threshold 0.9]
                                                                where a template matches, and how close
                                                                the best frame below the threshold comes

The second form answers "is this template unique?" before it becomes an anchor: frames at or above the
threshold should all be the intended screen, and the best score of the others should stay well below it.
Scores follow Core (TM_CCOEFF_NORMED, alpha as mask) but are an estimate: Core is the reference.
A popup printed with "?" also has colour rules, which are not checked here.
Needs numpy, Pillow and OpenCV (the OCR environment has them: ocr/.venv).
"""
import argparse
import glob
import json
import sys
from pathlib import Path

import cv2
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
FRAME = (1280, 720)


def load_frame(path):
    image = Image.open(path).convert("RGB")
    return cv2.cvtColor(np.asarray(image), cv2.COLOR_RGB2BGR) if image.size == FRAME else None


class Templates:
    """Resolves and caches templates like Core: assets-local before assets, the language before common."""

    def __init__(self, root=ROOT, lang="ko"):
        self.dirs = [root / "assets-local" / lang, root / "assets" / lang, root / "assets-local" / "common", root / "assets" / "common"]
        self.cache = {}

    def get(self, name):
        if name not in self.cache:
            path = next((d / name for d in self.dirs if (d / name).exists()), None)
            if path is None:
                raise FileNotFoundError(name)
            rgba = np.asarray(Image.open(path).convert("RGBA"))
            mask = rgba[:, :, 3]
            self.cache[name] = (cv2.cvtColor(rgba[:, :, :3], cv2.COLOR_RGB2BGR), None if mask.min() == 255 else mask)
        return self.cache[name]


def score(frame, template, mask, roi=None):
    x, y, w, h = roi if roi else (0, 0, FRAME[0], FRAME[1])
    w, h = max(w, template.shape[1]), max(h, template.shape[0])
    region = frame[y:min(y + h, FRAME[1]), x:min(x + w, FRAME[0])]
    if region.shape[0] < template.shape[0] or region.shape[1] < template.shape[1]:
        return 0.0
    result = cv2.matchTemplate(region, template, cv2.TM_CCOEFF_NORMED, mask=mask)
    result = result[np.isfinite(result)]
    return float(result.max()) if result.size else 0.0


def anchors_match(frame, anchors, templates):
    for a in anchors:
        r = a.get("roi")
        template, mask = templates.get(a["template"])
        if score(frame, template, mask, (r["x"], r["y"], r["w"], r["h"]) if r else None) < a.get("threshold", 0.9):
            return False
    return True


def load_items(folder):
    items = []
    for path in sorted(Path(folder).glob("*.json")):
        item = json.loads(path.read_text(encoding="utf-8"))
        if "id" in item and item.get("anchors"):
            items.append(item)
    return items


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("frames", help="glob of frames (quote it)")
    ap.add_argument("--lang", default="ko")
    ap.add_argument("--template")
    ap.add_argument("--roi", help="x,y,w,h")
    ap.add_argument("--threshold", type=float, default=0.9)
    args = ap.parse_args(argv)
    sys.stdout.reconfigure(encoding="utf-8")

    paths = sorted(glob.glob(args.frames, recursive=True))
    templates = Templates(lang=args.lang)
    if args.template:
        template, mask = templates.get(args.template)
        roi = tuple(int(v) for v in args.roi.split(",")) if args.roi else None
        hits, best_other, best_path, seen = [], 0.0, None, 0
        for path in paths:
            frame = load_frame(path)
            if frame is None:
                continue
            seen += 1
            s = score(frame, template, mask, roi)
            if s >= args.threshold:
                hits.append(path)
            elif s > best_other:
                best_other, best_path = s, path
        print(f"{seen} frame(s), {len(hits)} at or above {args.threshold}")
        folders = sorted({str(Path(p).parent) for p in hits})
        print("matching folders:", ", ".join(folders) if folders else "none")
        print(f"best below the threshold: {best_other:.3f}" + (f" ({best_path})" if best_path else ""))
        return 0

    pages, popups = load_items(ROOT / "pages"), load_items(ROOT / "popups")
    previous = None
    for path in paths:
        frame = load_frame(path)
        if frame is None:
            continue
        now = ([p["id"] for p in pages if anchors_match(frame, p["anchors"], templates)],
               [p["id"] + ("?" if "colors" in p or "no_colors" in p else "")
                for p in popups if anchors_match(frame, p["anchors"], templates)])
        if now != previous:
            print(Path(path).stem, "page", now[0], "popup", now[1], flush=True)
            previous = now
    return 0


if __name__ == "__main__":
    sys.exit(main())
