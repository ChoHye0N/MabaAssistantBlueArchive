# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Tests for tools/check_ui_strings.py. Run: python -m unittest discover tools/tests"""
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import check_ui_strings as cu  # noqa: E402

REPO = Path(__file__).resolve().parent.parent.parent

HEAD = ('<ResourceDictionary xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" '
        'xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" '
        'xmlns:sys="clr-namespace:System;assembly=System.Runtime">')


def table(entries):
    body = "".join(f'<sys:String x:Key="{k}">{v}</sys:String>' for k, v in entries)
    return HEAD + body + "</ResourceDictionary>"


class UiStringTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.dir = Path(self.tmp.name)
        (self.dir / "Strings.ko.xaml").write_text(
            table([("A", "가"), ("Next", "다음: {0}"), ("Braces", "{{문자}} {0}")]), encoding="utf-8")

    def tearDown(self):
        self.tmp.cleanup()

    def write(self, entries):
        (self.dir / "Strings.en.xaml").write_text(table(entries), encoding="utf-8")

    def test_matching_tables_pass(self):
        self.write([("A", "a"), ("Next", "Next: {0}"), ("Braces", "{{text}} {0}")])
        self.assertEqual(cu.check(self.dir), [])

    def test_reports_missing_extra_and_placeholders(self):
        self.write([("A", "a"), ("Next", "Next"), ("Braces", "{0}"), ("Extra", "x"), ("A", "b")])
        problems = "\n".join(cu.check(self.dir))
        self.assertIn("duplicate key A", problems)
        self.assertIn("key Extra is not in", problems)
        self.assertIn("Next uses placeholders []", problems)
        self.assertNotIn("Braces", problems)

    def test_reports_missing_and_empty(self):
        self.write([("A", " "), ("Next", "Next: {0}")])
        problems = "\n".join(cu.check(self.dir))
        self.assertIn("missing key Braces", problems)
        self.assertIn("empty text for A", problems)

    def test_repository_tables_match(self):
        self.assertEqual(cu.check(REPO / "gui" / "Maba.Gui" / "Resources"), [])


if __name__ == "__main__":
    unittest.main()
