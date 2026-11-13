# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Tests for tools/build_update.py. Run: python -m unittest discover tools/tests"""
import hashlib
import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import build_update as bu  # noqa: E402


def make_package(tmp, rapidocr="3.10.0"):
    pkg = Path(tmp) / "MABA-test"
    files = {
        "MabaAssistantBlueArchive.exe": b"exe",
        "LICENSE": b"license",
        "bin/MabaCore.dll": b"core",
        "bin/maba_updater.exe": b"updater",
        "docs/images/logo.png": b"logo",
        "data/tools/check_templates.py": b"tool",
        "data/tasks/cafe.json": b"{}",
        "data/assets/ko/x.png": b"png",
        "data/assets-local/README.txt": b"mine",
        "ocr/server.py": b"server",
        "ocr/requirements.txt": b"rapidocr",
        "ocr/python/python312.zip": b"stdlib",
        f"ocr/python/Lib/site-packages/rapidocr-{rapidocr}.dist-info/METADATA": b"meta",
        "ocr/python/Lib/site-packages/rapidocr/__pycache__/x.pyc": b"cache",
    }
    for rel, data in files.items():
        p = pkg / rel
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(data)
    return pkg


class Build(unittest.TestCase):
    def test_parts_and_manifest(self):
        with tempfile.TemporaryDirectory() as tmp:
            pkg = make_package(tmp)
            out = Path(tmp) / "out"
            manifest = bu.build(pkg, "0.1.5", out)
            self.assertEqual(manifest["version"], "0.1.5")
            parts = {p["name"]: p for p in manifest["parts"]}
            self.assertEqual(parts["app"]["version"], "0.1.5")
            with zipfile.ZipFile(out / parts["app"]["file"]) as z:
                app = z.namelist()
            # program files and the tools, never the resources or the player's templates
            self.assertEqual(app, ["LICENSE", "MabaAssistantBlueArchive.exe", "bin/MabaCore.dll", "bin/maba_updater.exe",
                                   "data/tools/check_templates.py", "docs/images/logo.png"])
            with zipfile.ZipFile(out / parts["ocr"]["file"]) as z:
                ocr = z.namelist()
            self.assertIn("ocr/part.json", ocr)
            self.assertFalse(any("__pycache__" in n for n in ocr))
            self.assertEqual(json.loads((pkg / "ocr" / "part.json").read_text(encoding="utf-8"))["version"], parts["ocr"]["version"])
            data = (out / parts["app"]["file"]).read_bytes()
            self.assertEqual(parts["app"]["sha256"], hashlib.sha256(data).hexdigest())
            self.assertEqual(parts["app"]["size"], len(data))
            self.assertEqual(json.loads((out / "update.json").read_text(encoding="utf-8")), manifest)

    def test_ocr_version_follows_what_is_installed(self):
        with tempfile.TemporaryDirectory() as a, tempfile.TemporaryDirectory() as b, tempfile.TemporaryDirectory() as c:
            same1 = bu.build(make_package(a), "0.1.5", Path(a) / "out")
            same2 = bu.build(make_package(b), "0.1.6", Path(b) / "out")
            other = bu.build(make_package(c, rapidocr="3.11.0"), "0.1.6", Path(c) / "out")
            v = lambda m: next(p["version"] for p in m["parts"] if p["name"] == "ocr")  # noqa: E731
            self.assertEqual(v(same1), v(same2))     # a new program version alone does not change it
            self.assertNotEqual(v(same1), v(other))  # a new package version does

    def test_bad_input(self):
        with tempfile.TemporaryDirectory() as tmp:
            pkg = make_package(tmp)
            with self.assertRaises(ValueError):
                bu.build(pkg, "v0.1.5", Path(tmp) / "out")
            with self.assertRaises(ValueError):
                bu.build(Path(tmp) / "nothing", "0.1.5", Path(tmp) / "out")


if __name__ == "__main__":
    unittest.main()
