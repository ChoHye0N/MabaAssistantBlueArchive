#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Save a frame of the game whenever the screen changed, while something else plays it. Read only.

  record_frames.py --out captures/run1 [--seconds 120] [--interval 0.5] [--cli PATH]

Frames come from `maba_cli grab`, so they are normalized to 1280x720 like the frames Core matches.
Creating `stop.flag` in the output folder ends the recording early. Frames show the account's
nickname and resources: keep them under captures/ (gitignored).
"""
import argparse
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
PIXEL_DIFF = 24      # a pixel changed when its mean channel difference is above this
CHANGED_SHARE = 0.02  # a frame is new when more than this share of its pixels changed


def changed(a, b, pixel_diff=PIXEL_DIFF, share=CHANGED_SHARE):
    if a is None or a.shape != b.shape:
        return True
    return float((np.abs(a - b).mean(axis=2) > pixel_diff).mean()) > share


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", required=True)
    ap.add_argument("--seconds", type=float, default=120)
    ap.add_argument("--interval", type=float, default=0.5)
    ap.add_argument("--cli", required=True, help="maba_cli path")
    args = ap.parse_args(argv)

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    stop, tmp = out / "stop.flag", out / "tmp.png"
    stop.unlink(missing_ok=True)
    last, saved = None, 0
    end = time.time() + args.seconds
    while time.time() < end and not stop.exists():
        r = subprocess.run([args.cli, "grab", "--out", str(tmp)], cwd=ROOT, capture_output=True)
        if r.returncode == 0 and tmp.exists():
            frame = np.asarray(Image.open(tmp).convert("RGB")).astype(np.int16)
            if changed(last, frame):
                saved += 1
                tmp.replace(out / f"f{saved:05d}.png")
                last = frame
        time.sleep(args.interval)
    tmp.unlink(missing_ok=True)
    print(f"saved {saved} frame(s) to {out}", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
