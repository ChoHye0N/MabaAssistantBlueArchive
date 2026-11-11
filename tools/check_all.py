#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Run every check a change should pass before it is committed, and print one line per check.

  check_all.py [--preset windows-msvc|linux-dev|linux-opencv] [--build] [--gui]

  --build   build Core first (ctest passes on old binaries too)
  --gui     also build the GUI (Windows, needs the .NET SDK)

Checks: Core tests (ctest), screenshot fixtures when the local fixtures and their test exist, the
template matrix as CI sees it, GUI string tables, the tool and OCR unit tests, and the privacy scan.
Standard library only. Exit 1 when a check fails.
"""
import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def default_preset():
    return "windows-msvc" if os.name == "nt" else "linux-dev"


def fixture_test(preset, root=ROOT):
    for rel in (f"build/{preset}/core/tests/Release/maba_fixture_tests.exe", f"build/{preset}/core/tests/maba_fixture_tests"):
        if (root / rel).exists():
            return root / rel
    return None


def ocr_python(root=ROOT):
    """The OCR environment has numpy, which the OCR tests need; the current interpreter otherwise."""
    for rel in ("ocr/.venv/Scripts/python.exe", "ocr/.venv/bin/python"):
        if (root / rel).exists():
            return str(root / rel)
    return sys.executable


def checks(preset, build=False, gui=False, root=ROOT):
    """(name, command) pairs in the order they run."""
    py = sys.executable
    out = []
    if build:
        out.append(("build core", ["cmake", "--build", "--preset", preset]))
    out.append(("core tests", ["ctest", "--preset", preset, "--output-on-failure"]))
    fixtures = fixture_test(preset, root)
    if fixtures and (root / "assets-local" / "fixtures").is_dir():
        out.append(("screenshot fixtures", [str(fixtures)]))
    out.append(("templates (as CI)", [py, "tools/check_templates.py", "--committed", "--strict"]))
    out.append(("gui strings", [py, "tools/check_ui_strings.py"]))
    out.append(("tool tests", [py, "-m", "unittest", "discover", "tools/tests"]))
    out.append(("ocr tests", [ocr_python(root), "-m", "unittest", "discover", "ocr/tests"]))
    if gui:
        out.append(("build gui", ["dotnet", "build", "gui/Maba.Gui", "-c", "Release", "-nologo", "-v", "q"]))
    out.append(("privacy", [py, "tools/privacy_check.py"]))
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--preset", default=default_preset())
    ap.add_argument("--build", action="store_true")
    ap.add_argument("--gui", action="store_true")
    args = ap.parse_args(argv)

    failed = []
    for name, cmd in checks(args.preset, args.build, args.gui):
        if shutil.which(cmd[0]) is None and not Path(cmd[0]).exists():
            print(f"SKIP  {name}: {cmd[0]} not found")
            continue
        r = subprocess.run(cmd, cwd=ROOT, stdin=subprocess.DEVNULL, capture_output=True, text=True, encoding="utf-8", errors="replace")
        print(f"{'ok  ' if r.returncode == 0 else 'FAIL'}  {name}")
        if r.returncode != 0:
            failed.append(name)
            tail = (r.stdout + r.stderr).strip().splitlines()[-15:]
            print("\n".join("      " + line[:200] for line in tail))
    print("all checks passed" if not failed else "failed: " + ", ".join(failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
