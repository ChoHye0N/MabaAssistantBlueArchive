# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Tests for tools/build_resources.py. Run: python -m unittest discover tools/tests"""
import hashlib
import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import build_resources as br  # noqa: E402

REPO = Path(__file__).resolve().parent.parent.parent


def make_root(tmp, version="20261113.1", min_core="0.1.5"):
    root = Path(tmp) / "root"
    for d in ("config", "pages", "tasks", "assets/ko", "assets-local/ko", "tools"):
        (root / d).mkdir(parents=True)
    (root / "config" / "resource.json").write_text(json.dumps({"version": version, "min_core": min_core}), encoding="utf-8")
    (root / "pages" / "a.json").write_text("{}", encoding="utf-8")
    (root / "tasks" / "t.json").write_text("{}", encoding="utf-8")
    (root / "assets" / "ko" / "x.png").write_bytes(b"png")
    (root / "assets-local" / "ko" / "mine.png").write_bytes(b"local")
    (root / "tools" / "tool.py").write_text("", encoding="utf-8")
    return root


class Build(unittest.TestCase):
    def test_pack_and_manifest(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = make_root(tmp)
            archive, manifest = br.build(root, Path(tmp) / "out")
            self.assertEqual(archive.name, "MABA-resources-20261113.1.zip")
            data = archive.read_bytes()
            self.assertEqual(manifest["sha256"], hashlib.sha256(data).hexdigest())
            self.assertEqual(manifest["size"], len(data))
            self.assertEqual(manifest["min_core"], "0.1.5")
            with zipfile.ZipFile(archive) as z:
                names = z.namelist()
            self.assertEqual(names, ["assets/ko/x.png", "config/resource.json", "pages/a.json", "tasks/t.json"])
            saved = json.loads((Path(tmp) / "out" / "resources.json").read_text(encoding="utf-8"))
            self.assertEqual(saved, manifest)

    def test_same_files_same_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = make_root(tmp)
            a, _ = br.build(root, Path(tmp) / "a")
            b, _ = br.build(root, Path(tmp) / "b")
            self.assertEqual(a.read_bytes(), b.read_bytes())

    def test_bad_versions_are_refused(self):
        with tempfile.TemporaryDirectory() as tmp:
            for version, min_core in (("1.0", "0.1.5"), ("20261113.1", "0.1"), ("", "0.1.5")):
                root = make_root(Path(tmp) / version.replace(".", "_") / min_core.replace(".", "_"), version, min_core)
                with self.assertRaises(ValueError):
                    br.build(root, Path(tmp) / "out")

    def test_repository_version_file(self):
        version, min_core = br.read_version(REPO)
        self.assertRegex(version, br.VERSION_RE)
        self.assertRegex(min_core, br.CORE_RE)


if __name__ == "__main__":
    unittest.main()
