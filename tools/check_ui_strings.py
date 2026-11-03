#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""GUI string tables check.

Compares every gui/Maba.Gui/Resources/Strings.<lang>.xaml with the Korean baseline
(Strings.ko.xaml): missing keys, keys the baseline does not have, duplicate keys, empty texts
and format placeholders ({0}, {1}, ...) that differ from the baseline.

  python tools/check_ui_strings.py          # report; exit 1 on any problem

Only the standard library is used, so it runs anywhere CI runs.
"""
from __future__ import annotations

import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Dict, List, Tuple

BASELINE = "ko"
XAML_NS = "{http://schemas.microsoft.com/winfx/2006/xaml}"
PLACEHOLDER = re.compile(r"(?<!\{)\{(\d+)(?:[,:][^}]*)?\}(?!\})")


def load(path: Path) -> Tuple[Dict[str, str], List[str]]:
    """Keys and texts of one string table, plus duplicate keys."""
    root = ET.parse(path).getroot()
    texts: Dict[str, str] = {}
    duplicates: List[str] = []
    for el in root:
        key = el.get(XAML_NS + "Key")
        if key is None:
            continue
        if key in texts:
            duplicates.append(key)
        texts[key] = el.text or ""
    return texts, duplicates


def placeholders(text: str) -> List[str]:
    return sorted(set(PLACEHOLDER.findall(text)))


def check(resources: Path) -> List[str]:
    """Problems found in the string tables under `resources` (empty when all are fine)."""
    problems: List[str] = []
    base_path = resources / f"Strings.{BASELINE}.xaml"
    base, dup = load(base_path)
    problems += [f"{base_path.name}: duplicate key {k}" for k in dup]
    problems += [f"{base_path.name}: empty text for {k}" for k, v in base.items() if not v.strip()]
    for path in sorted(resources.glob("Strings.*.xaml")):
        if path == base_path:
            continue
        texts, dup = load(path)
        name = path.name
        problems += [f"{name}: duplicate key {k}" for k in dup]
        problems += [f"{name}: missing key {k}" for k in base if k not in texts]
        problems += [f"{name}: key {k} is not in {base_path.name}" for k in texts if k not in base]
        for k, v in texts.items():
            if not v.strip():
                problems.append(f"{name}: empty text for {k}")
            elif k in base and placeholders(v) != placeholders(base[k]):
                problems.append(f"{name}: {k} uses placeholders {placeholders(v)}, "
                                f"{base_path.name} uses {placeholders(base[k])}")
    return problems


def main() -> int:
    resources = Path(__file__).resolve().parent.parent / "gui" / "Maba.Gui" / "Resources"
    problems = check(resources)
    for p in problems:
        print(p)
    tables = sorted(p.name for p in resources.glob("Strings.*.xaml"))
    print(f"{len(tables)} string tables ({', '.join(tables)}): "
          f"{'OK' if not problems else str(len(problems)) + ' problem(s)'}")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
