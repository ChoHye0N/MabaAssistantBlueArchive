# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Tests for the live-check tools (live_tour, record_frames, contact_sheet, check_all).

Run: python -m unittest discover tools/tests
"""
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import check_all  # noqa: E402
import contact_sheet  # noqa: E402
import live_tour  # noqa: E402

try:
    import numpy as np
    import record_frames
except ImportError:  # numpy is optional for the tool tests
    np = None


class LintSteps(unittest.TestCase):
    def test_fixed_point_needs_a_guard(self):
        problems = live_tour.lint_steps([{"action": "click", "x": 10, "y": 20}])
        self.assertEqual(len(problems), 1)
        self.assertIn("steps[0]", problems[0])

    def test_guarded_and_template_clicks_pass(self):
        steps = [
            {"action": "goto", "page": "shop"},
            {"action": "click", "x": 10, "y": 20, "on_page": "shop"},
            {"action": "click", "x": 10, "y": 20, "require": {"template": "a.png"}},
            {"action": "click", "template": "a.png"},
        ]
        self.assertEqual(live_tour.lint_steps(steps), [])

    def test_nested_steps_are_checked(self):
        steps = [{"action": "if", "when": {"present": {"template": "a.png"}},
                  "then": [{"action": "loop", "max": 2, "steps": [{"action": "click", "x": 1, "y": 2}]}],
                  "else": [{"action": "click", "points": [{"x": 1, "y": 2}]}]}]
        problems = live_tour.lint_steps(steps)
        self.assertEqual(len(problems), 2)
        self.assertIn("then[0].steps[0]", problems[0])

    def test_not_a_list(self):
        self.assertEqual(len(live_tour.lint_steps({"action": "back"})), 1)


class BuildTask(unittest.TestCase):
    def test_ends_at_home(self):
        task = live_tour.build_task([{"action": "goto", "page": "shop"}])
        self.assertEqual(task["steps"][-1], {"action": "return_home"})
        self.assertEqual(len(live_tour.build_task(task["steps"])["steps"]), 2)

    def test_temporary_root_holds_only_the_tour(self):
        with tempfile.TemporaryDirectory() as src, tempfile.TemporaryDirectory() as dst:
            src, dst = Path(src), Path(dst) / "root"
            for d in ("pages", "popups", "tasks", "config", "assets/ko"):
                (src / d).mkdir(parents=True)
            (src / "tasks" / "cafe.json").write_text("{}", encoding="utf-8")
            (src / "pages" / "shop.json").write_text("{}", encoding="utf-8")
            extra = src / "new_page.json"
            extra.write_text("{}", encoding="utf-8")
            dst.mkdir()
            live_tour.make_root(dst, [{"action": "back"}], pages=[extra], root=src)
            self.assertEqual([p.name for p in (dst / "tasks").iterdir()], [f"{live_tour.TASK_ID}.json"])
            self.assertTrue((dst / "pages" / "shop.json").exists())
            self.assertTrue((dst / "pages" / "new_page.json").exists())
            task = json.loads((dst / "tasks" / f"{live_tour.TASK_ID}.json").read_text(encoding="utf-8"))
            self.assertEqual(task["steps"][-1]["action"], "return_home")


class Sheet(unittest.TestCase):
    def test_size(self):
        self.assertEqual(contact_sheet.sheet_size(1, 7), (256 * 7, 156))
        self.assertEqual(contact_sheet.sheet_size(8, 7), (256 * 7, 312))


class CheckAll(unittest.TestCase):
    def test_order_and_options(self):
        with tempfile.TemporaryDirectory() as root:
            names = [n for n, _ in check_all.checks("linux-dev", root=Path(root))]
            self.assertEqual(names[0], "core tests")
            self.assertEqual(names[-1], "privacy")
            self.assertNotIn("screenshot fixtures", names)
            full = [n for n, _ in check_all.checks("linux-dev", build=True, gui=True, root=Path(root))]
            self.assertEqual(full[0], "build core")
            self.assertIn("build gui", full)


@unittest.skipIf(np is None, "numpy not installed")
class Changed(unittest.TestCase):
    def test_small_and_large_changes(self):
        a = np.zeros((72, 128, 3), dtype=np.int16)
        b = a.copy()
        self.assertTrue(record_frames.changed(None, a))
        self.assertFalse(record_frames.changed(a, b))
        b[:1, :10] = 255      # under 2 % of the pixels
        self.assertFalse(record_frames.changed(a, b))
        b[:36] = 255          # half the frame
        self.assertTrue(record_frames.changed(a, b))


if __name__ == "__main__":
    unittest.main()
