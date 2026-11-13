# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Tests for the decisions of tools/release.py. Run: python -m unittest discover tools/tests"""
import datetime
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import release as rl  # noqa: E402


class Decisions(unittest.TestCase):
    def test_min_core_follows_core_changes(self):
        # Core changed since the last release: the data may need the version being released.
        self.assertEqual(rl.decide_min_core("0.1.4", "0.1.4", True, "0.2.0"), "0.2.0")
        self.assertEqual(rl.decide_min_core("0.1.4", "0.1.4", True, "0.2.0-beta.1"), "0.2.0")
        # Core unchanged: the value stays, and never asks for more than what is released.
        self.assertEqual(rl.decide_min_core("0.1.2", "0.1.4", False, "0.1.5"), "0.1.2")
        self.assertEqual(rl.decide_min_core("0.1.5", "0.1.4", False, "0.1.5"), "0.1.4")
        self.assertEqual(rl.decide_min_core("", "0.1.4", False, "0.1.4"), "0.1.4")
        # Resources alone while Core changed but no newer version is in the source: refuse.
        with self.assertRaises(rl.ReleaseError):
            rl.decide_min_core("0.1.4", "0.1.4", True, "0.1.4")

    def test_resource_version(self):
        day = datetime.date(2026, 11, 14)
        self.assertEqual(rl.next_resource_version("20261113.1", day), "20261114.1")
        self.assertEqual(rl.next_resource_version("20261114.1", day), "20261114.2")
        self.assertEqual(rl.next_resource_version("", day), "20261114.1")
        with self.assertRaises(rl.ReleaseError):
            rl.next_resource_version("20261201.1", day)

    def test_program_version_in_the_three_files(self):
        texts = {
            "cmake": "cmake_minimum_required(VERSION 3.21)\nproject(MabaAssistantBlueArchive VERSION 0.1.4 LANGUAGES CXX)\n",
            "vcpkg": '{\n  "name": "maba",\n  "version-string": "0.1.4"\n}\n',
            "csproj": "<Project>\n    <Version>0.1.4</Version>\n</Project>\n",
        }
        out = rl.set_program_version(texts, "0.2.0-beta.1")
        self.assertIn("VERSION 0.2.0 LANGUAGES", out["cmake"])  # CMake takes no suffix
        self.assertIn("cmake_minimum_required(VERSION 3.21)", out["cmake"])
        self.assertIn('"version-string": "0.2.0-beta.1"', out["vcpkg"])
        self.assertIn("<Version>0.2.0-beta.1</Version>", out["csproj"])
        self.assertEqual(rl.source_version(out["cmake"]), "0.2.0")
        with self.assertRaises(rl.ReleaseError):
            rl.set_program_version({**texts, "csproj": "<Project/>"}, "0.2.0")

    def test_latest_release_ignores_pre_releases_and_other_tags(self):
        tags = ["v0.1.0-beta.1", "v0.1.0", "v0.1.10", "v0.1.9", "v0.2.0-beta.1", "resources-20261113.1", "vnext"]
        self.assertEqual(rl.latest_program_version(tags), "0.1.10")
        with self.assertRaises(rl.ReleaseError):
            rl.latest_program_version(["resources-20261113.1"])
        self.assertIsNone(rl.parse_version("v0.1.0"))
        self.assertEqual(rl.parse_version("0.2.0-beta.1"), (0, 2, 0))


if __name__ == "__main__":
    unittest.main()
