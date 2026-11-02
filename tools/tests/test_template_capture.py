# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Tests for the non-GUI parts of tools/template_capture.py. Run: python -m unittest discover tools/tests"""
import datetime as dt
import json
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import template_capture as tc  # noqa: E402

LANGS = ["ko", "en", "zh-Hant", "th", "ja"]


def gradient(w, h):
    img = Image.new("RGB", (w, h))
    img.putdata([((x * 7) % 256, (y * 5) % 256, (x + y) % 256) for y in range(h) for x in range(w)])
    return img


class NormalizeTests(unittest.TestCase):
    def test_keeps_normalized(self):
        img = gradient(1280, 720)
        self.assertEqual(tc.normalize(img).tobytes(), img.tobytes())

    def test_scales_16_9(self):
        self.assertEqual(tc.normalize(gradient(1920, 1080)).size, (1280, 720))
        self.assertEqual(tc.normalize(gradient(960, 540)).size, (1280, 720))

    def test_rejects_other_aspect(self):
        with self.assertRaises(ValueError):
            tc.normalize(gradient(1280, 800))

    def test_converts_rgba(self):
        self.assertEqual(tc.normalize(Image.new("RGBA", (1280, 720))).mode, "RGB")


class NameAndPathTests(unittest.TestCase):
    def test_name(self):
        self.assertEqual(tc.template_name("main_menu", "mail_icon"), "main_menu_mail_icon.png")
        for task, element in (("Main", "x"), ("main menu", "x"), ("a", "../b"), ("a", ""), ("a_", "b")):
            with self.assertRaises(ValueError):
                tc.template_name(task, element)

    def test_output_path(self):
        root = Path("r")
        self.assertEqual(tc.output_path(root, "common", "main_menu", "mail_icon", False, LANGS),
                         root / "assets-local" / "common" / "main_menu_mail_icon.png")
        self.assertEqual(tc.output_path(root, "ko", "mail", "title", True, LANGS),
                         root / "assets" / "ko" / "mail_title.png")
        with self.assertRaises(ValueError):
            tc.output_path(root, "xx", "a", "b", False, LANGS)

    def test_languages_file(self):
        self.assertIn("ko", tc.load_languages())


class CutTests(unittest.TestCase):
    def setUp(self):
        self.frame = gradient(1280, 720)

    def test_crop_without_masks(self):
        img, applied = tc.cut_template(self.frame, tc.Rect(100, 50, 40, 20), [])
        self.assertEqual(img.size, (40, 20))
        self.assertEqual(img.mode, "RGBA")
        self.assertEqual(applied, [])
        self.assertEqual(img.getpixel((0, 0))[:3], self.frame.getpixel((100, 50)))
        self.assertEqual(img.getchannel("A").getextrema(), (255, 255))

    def test_masks_are_transparent_and_clipped(self):
        crop = tc.Rect(100, 50, 40, 20)
        masks = [tc.Rect(90, 60, 20, 5), tc.Rect(500, 500, 10, 10)]  # second is outside the crop
        img, applied = tc.cut_template(self.frame, crop, masks)
        self.assertEqual(applied, [tc.Rect(100, 60, 10, 5)])
        alpha = img.getchannel("A")
        self.assertEqual(alpha.getpixel((0, 10)), 0)
        self.assertEqual(alpha.getpixel((9, 14)), 0)
        self.assertEqual(alpha.getpixel((10, 10)), 255)
        self.assertEqual(alpha.getpixel((0, 15)), 255)

    def test_rejects_bad_input(self):
        with self.assertRaises(ValueError):
            tc.cut_template(self.frame, tc.Rect(1270, 0, 20, 20), [])
        with self.assertRaises(ValueError):
            tc.cut_template(gradient(1920, 1080), tc.Rect(0, 0, 10, 10), [])
        with self.assertRaises(ValueError):
            tc.cut_template(self.frame, tc.Rect(0, 0, 10, 10), [tc.Rect(0, 0, 10, 10)])


class SaveTests(unittest.TestCase):
    def test_metadata_and_files(self):
        crop = tc.Rect(1, 2, 30, 40)
        meta = tc.build_metadata("main_menu", "mail_icon", "common", crop, [tc.Rect(1, 2, 3, 4)],
                                 (1920, 1080), "1.2.3", today=dt.date(2026, 11, 2))
        self.assertEqual(meta["roi"], {"x": 1, "y": 2, "w": 30, "h": 40})
        self.assertEqual(meta["capture_date"], "2026-11-02")
        self.assertEqual(meta["source_size"], {"w": 1920, "h": 1080})
        self.assertNotIn("source_path", meta)

        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / "assets-local" / "common" / "main_menu_mail_icon.png"
            img, _ = tc.cut_template(gradient(1280, 720), crop, [])
            tc.save_template(path, img, meta)
            self.assertTrue(path.exists())
            self.assertEqual(json.loads(path.with_suffix(".json").read_text(encoding="utf-8"))["task"], "main_menu")
            with self.assertRaises(FileExistsError):
                tc.save_template(path, img, meta)
            tc.save_template(path, img, meta, force=True)

    def test_rect_parse(self):
        self.assertEqual(tc.Rect.parse("1,2,3,4"), tc.Rect(1, 2, 3, 4))
        self.assertEqual(tc.Rect.from_corners(10, 10, 4, 2), tc.Rect(4, 2, 6, 8))
        with self.assertRaises(ValueError):
            tc.Rect.parse("1,2,3")


if __name__ == "__main__":
    unittest.main()
