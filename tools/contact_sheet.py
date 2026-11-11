#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Lay recorded frames out on one image, each with its file name, to read a run at a glance.

  contact_sheet.py FOLDER [--out sheet.png] [--start 0] [--count 42] [--columns 7]
"""
import argparse
import sys
from pathlib import Path

from PIL import Image, ImageDraw

THUMB = (256, 144)
LABEL = 12


def sheet_size(count, columns, thumb=THUMB, label=LABEL):
    rows = (count + columns - 1) // columns
    return thumb[0] * columns, (thumb[1] + label) * rows


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folder")
    ap.add_argument("--out", default="sheet.png")
    ap.add_argument("--start", type=int, default=0)
    ap.add_argument("--count", type=int, default=42)
    ap.add_argument("--columns", type=int, default=7)
    args = ap.parse_args(argv)

    frames = sorted(Path(args.folder).glob("f*.png"))[args.start:args.start + args.count]
    if not frames:
        print("no frames")
        return 1
    sheet = Image.new("RGB", sheet_size(len(frames), args.columns), "white")
    draw = ImageDraw.Draw(sheet)
    for i, frame in enumerate(frames):
        x, y = (i % args.columns) * THUMB[0], (i // args.columns) * (THUMB[1] + LABEL)
        sheet.paste(Image.open(frame).convert("RGB").resize(THUMB), (x, y + LABEL))
        draw.text((x + 2, y), frame.stem, fill="red")
    sheet.save(args.out)
    print(f"{len(frames)} frame(s) -> {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
