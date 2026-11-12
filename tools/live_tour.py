#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Run a short list of steps in the real game from a temporary copy of the data, and record the screen.

  live_tour.py STEPS.json [--out captures/tour] [--seconds 120] [--ocr] [--page FILE ...] [--template FILE ...]

STEPS.json is a list of task steps (docs/PAGE_FORMAT.md). The repository data is copied to a temporary
root, so pages, popups and templates under test can be added with --page / --template without touching
the tracked files. Frames are saved whenever the screen changes and a contact sheet is written at the end.

Safety, checked before anything runs:
  - a click at a fixed point needs `on_page` or `require`: a point is only safe on its own screen;
  - the run always ends with `return_home`, so the game is never left on a screen the next run does not know.
A run starts by going to the main screen: put the route to the screen under test into the steps (`goto`).
"""
import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA_DIRS = ("pages", "popups", "tasks", "strings", "config", "assets")
CLI_CANDIDATES = ("build/windows-msvc/core/Release/maba_cli.exe", "build/linux-opencv/core/maba_cli", "build/linux-dev/core/maba_cli")
TASK_ID = "live_tour"


def find_cli(root=ROOT):
    for rel in CLI_CANDIDATES:
        if (root / rel).exists():
            return root / rel
    return None


def lint_steps(steps, path="steps"):
    """Problems that make a tour unsafe; an empty list means it may run."""
    problems = []
    if not isinstance(steps, list):
        return [f"{path}: expected a list of steps"]
    for i, step in enumerate(steps):
        where = f"{path}[{i}]"
        if not isinstance(step, dict) or "action" not in step:
            problems.append(f"{where}: not a step")
            continue
        fixed_point = "x" in step or "points" in step
        if step["action"] == "click" and fixed_point and "on_page" not in step and "require" not in step:
            problems.append(f"{where}: a click at a fixed point needs 'on_page' or 'require'")
        for key in ("then", "else", "steps"):
            if key in step:
                problems += lint_steps(step[key], f"{where}.{key}")
        for case, sub in (step.get("cases") or {}).items():
            problems += lint_steps(sub, f"{where}.cases.{case}")
    return problems


def build_task(steps):
    steps = list(steps)
    if not steps or steps[-1].get("action") != "return_home":
        steps.append({"action": "return_home"})
    return {"id": TASK_ID, "name": "live tour", "order": 1, "after": [], "status": "wip", "needs_ocr": False, "notes": [], "steps": steps}


def link_ocr(tmp, root=ROOT):
    """Core looks for the OCR environment beside the data: link the repository's into the temporary root."""
    src = root / "ocr"
    if not src.is_dir():
        return False
    try:
        os.symlink(src, tmp / "ocr", target_is_directory=True)
    except OSError:
        import _winapi  # Windows without the symlink privilege: a junction needs none
        _winapi.CreateJunction(str(src), str(tmp / "ocr"))
    return True


def make_root(tmp, steps, pages=(), templates=(), root=ROOT):
    for d in DATA_DIRS:
        if d != "tasks" and (root / d).is_dir():
            shutil.copytree(root / d, tmp / d)
    (tmp / "tasks").mkdir()
    (tmp / "tasks" / f"{TASK_ID}.json").write_text(json.dumps(build_task(steps)), encoding="utf-8")
    for page in pages:
        shutil.copy(page, tmp / "pages" / Path(page).name)
    for template in templates:
        src = Path(template)
        lang = src.parent.name if src.parent.name in [p.name for p in (tmp / "assets").iterdir()] else "common"
        shutil.copy(src, tmp / "assets" / lang / src.name)
    return tmp


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("steps")
    ap.add_argument("--out", default="captures/tour", help="folder for the frames and sheet.png (default captures/tour)")
    ap.add_argument("--seconds", type=float, default=120, help="upper bound for the run and the recording")
    ap.add_argument("--ocr", action="store_true", help="start the OCR sidecar (off by default)")
    ap.add_argument("--page", action="append", default=[], help="extra page or popup JSON for the temporary root")
    ap.add_argument("--template", action="append", default=[], help="extra template; its parent folder name picks the language")
    ap.add_argument("--cli", help="maba_cli path (default: the newest build)")
    args = ap.parse_args(argv)

    steps = json.loads(Path(args.steps).read_text(encoding="utf-8"))
    problems = lint_steps(steps)
    if problems:
        print("\n".join(problems))
        return 2
    cli = Path(args.cli) if args.cli else find_cli()
    if not cli:
        print("maba_cli not found: build Core first")
        return 2
    out = (ROOT / args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    for old in list(out.glob("f*.png")) + [out / "stop.flag"]:
        old.unlink(missing_ok=True)

    here = Path(__file__).resolve().parent
    recorder = subprocess.Popen([sys.executable, str(here / "record_frames.py"), "--out", str(out), "--seconds", str(args.seconds), "--cli", str(cli)])
    time.sleep(1)
    with tempfile.TemporaryDirectory(prefix="maba_tour_") as tmp:
        make_root(Path(tmp), steps, args.page, args.template)
        if args.ocr:
            link_ocr(Path(tmp))
        cmd = [str(cli), "run", "--tasks", TASK_ID, "--root", tmp] + ([] if args.ocr else ["--no-ocr"])
        try:
            code = subprocess.run(cmd, cwd=ROOT, stdin=subprocess.DEVNULL, timeout=args.seconds).returncode
        except subprocess.TimeoutExpired:
            print(f"the run did not finish within {args.seconds:.0f} s and was stopped")
            code = 1
        if (Path(tmp) / "ocr").exists():
            try:  # the link only: never let the clean-up walk into the real folder
                os.unlink(Path(tmp) / "ocr")
            except OSError:
                os.rmdir(Path(tmp) / "ocr")  # a junction is removed like an empty folder
    (out / "stop.flag").touch()
    recorder.wait(timeout=30)
    subprocess.run([sys.executable, str(here / "contact_sheet.py"), str(out), "--out", str(out / "sheet.png")])
    return code


if __name__ == "__main__":
    sys.exit(main())
