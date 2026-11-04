# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import measure_ocr  # noqa: E402


def line(text, x=0, y=0, score=1.0):
    return {"text": text, "box": [x, y, 40, 20], "score": score}


class ScoreTest(unittest.TestCase):
    def test_edit_distance(self):
        self.assertEqual(measure_ocr.edit_distance("820/240", "820/240"), 0)
        self.assertEqual(measure_ocr.edit_distance("820/240", "82O/240"), 1)
        self.assertEqual(measure_ocr.edit_distance("", "abc"), 3)

    def test_reading_order_joins_boxes_on_a_line(self):
        ordered = measure_ocr.reading_order([line("c", 0, 40), line("b", 50, 2), line("a", 0, 0)])
        self.assertEqual([r["text"] for r in ordered], ["a", "b", "c"])

    def test_text_sample_ignores_spaces(self):
        scored = measure_ocr.score_sample({"expected": "820/240"}, [line("820 /"), line("240", 50)])
        self.assertEqual((scored["exact"], scored["errors"], scored["misses"]), (1, 0, []))

    def test_text_sample_counts_errors(self):
        scored = measure_ocr.score_sample({"expected": "1,234"}, [line("1,284")])
        self.assertEqual((scored["exact"], scored["errors"], scored["chars"]), (0, 1, 5))
        self.assertIn("read as", scored["misses"][0])

    def test_list_sample_scores_each_item(self):
        results = [line("13", 0, 0), line("I4", 0, 40), line("99", 0, 80)]
        scored = measure_ocr.score_sample({"expected": ["13", "14"]}, results)
        self.assertEqual((scored["items"], scored["exact"], scored["errors"]), (2, 1, 1))

    def test_list_sample_with_nothing_read(self):
        scored = measure_ocr.score_sample({"expected": ["ab"]}, [])
        self.assertEqual((scored["exact"], scored["errors"], scored["chars"]), (0, 2, 2))


class ReportTest(unittest.TestCase):
    def test_evaluate_groups_by_kind_and_keeps_misses(self):
        samples = [{"kind": "ap", "expected": "1/2", "image": "a.png"},
                   {"kind": "ap", "expected": "3/4", "image": "b.png"},
                   {"kind": "credits", "expected": "5", "image": "c.png"}]
        reads = {"a.png": "1/2", "b.png": "3/5", "c.png": "5"}
        report = measure_ocr.evaluate(samples, lambda s: [line(reads[s["image"]], score=0.5)])
        self.assertEqual((report["ap"]["crops"], report["ap"]["exact"]), (2, 1))
        self.assertEqual(len(report["ap"]["misses"]), 1)
        self.assertIn("b.png", report["ap"]["misses"][0])
        text = measure_ocr.summary(report)
        self.assertIn("| ap | 2 | 2 | 1 | 50.0 | 1/6 | 0.500 |", text)
        self.assertIn("| total | 3 | 3 | 2 | 66.7 | 1/7 |", text)

    def test_empty_report(self):
        self.assertNotIn("total", measure_ocr.summary({}))


if __name__ == "__main__":
    unittest.main()
