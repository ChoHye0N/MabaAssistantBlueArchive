# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Tests for tools/check_templates.py. Run: python -m unittest discover tools/tests"""
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import check_templates as ct  # noqa: E402

REPO = Path(__file__).resolve().parent.parent.parent


def write(root: Path, rel: str, obj) -> None:
    p = root / rel
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(obj), encoding="utf-8")


class MatrixTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        r = self.root = Path(self.tmp.name)
        write(r, "config/languages.json", {"primary": "ko", "languages": [
            {"code": "ko", "status": "confirmed", "ocr_lang": "korean"},
            {"code": "ja", "status": "unverified", "ocr_lang": "japan"}]})
        write(r, "pages/main_menu.json", {"id": "main_menu", "anchors": [{"template": "home.png"}],
                                          "transitions": [{"to": "mail", "click": {"template": "mail_btn.png"}}]})
        write(r, "pages/mail.json", {"id": "mail", "anchors": [{"template": "mail_title.png"}],
                                     "transitions": [{"to": "main_menu", "click": {"x": 1, "y": 1}}]})
        write(r, "popups/notice.json", {"id": "notice", "anchors": [{"template": "notice.png"}], "dismiss": "back"})
        write(r, "tasks/mail.json", {"id": "mail", "status": "ready", "steps": [
            {"action": "goto", "page": "mail"}, {"action": "click", "template": "claim.png"}]})
        write(r, "tasks/cafe.json", {"id": "cafe", "status": "todo"})
        for rel in ["assets/common/home.png", "assets/common/mail_btn.png", "assets/common/notice.png",
                    "assets/ko/mail_title.png", "assets-local/ko/claim.png"]:
            (r / rel).parent.mkdir(parents=True, exist_ok=True)
            (r / rel).write_bytes(b"png")

    def tearDown(self):
        self.tmp.cleanup()

    def row(self, matrix, task):
        return next(t for t in matrix["tasks"] if t["task"] == task)

    def test_collects_templates_along_the_route(self):
        m = ct.build_matrix(self.root)
        self.assertEqual(self.row(m, "mail")["templates"],
                         ["claim.png", "home.png", "mail_btn.png", "mail_title.png", "notice.png"])
        self.assertEqual(self.row(m, "mail")["languages"]["ko"]["state"], "ok")
        self.assertEqual(self.row(m, "mail")["languages"]["ja"]["missing"], ["claim.png", "mail_title.png"])
        self.assertEqual(self.row(m, "cafe")["languages"]["ko"]["state"], "n/a")

    def test_committed_only_ignores_assets_local(self):
        m = ct.build_matrix(self.root, committed_only=True)
        self.assertEqual(self.row(m, "mail")["languages"]["ko"]["missing"], ["claim.png"])

    def test_strict_checks_ready_tasks_in_confirmed_languages(self):
        self.assertEqual(ct.strict_failures(ct.build_matrix(self.root)), [])
        failures = ct.strict_failures(ct.build_matrix(self.root, committed_only=True))
        self.assertEqual(failures, ["mail [ko]: claim.png"])

    def test_markdown_lists_missing(self):
        text = ct.to_markdown(ct.build_matrix(self.root))
        self.assertIn("| mail | ready | ok | missing (2) |", text)
        self.assertIn("- `claim.png`: ja", text)

    def test_repository_data_loads(self):
        m = ct.build_matrix(REPO)
        self.assertTrue(any(t["task"] == "mail_claim_all" for t in m["tasks"]))


if __name__ == "__main__":
    unittest.main()
