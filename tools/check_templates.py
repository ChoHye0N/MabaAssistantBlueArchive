#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Language matrix and template check.

Collects every template a task needs (its steps, the pages it navigates through, the pages on
the way back home and all popups) and checks, per declared language, that each one resolves
the way Core resolves it: assets-local/<lang>, assets/<lang>, assets-local/common, assets/common.

  python tools/check_templates.py                 # matrix as Markdown
  python tools/check_templates.py --json          # machine-readable
  python tools/check_templates.py --strict        # exit 1 if a task with status "ready" misses
                                                  # a template in a "confirmed" language
  python tools/check_templates.py --committed     # ignore assets-local/ (what CI can see)

Only the standard library is used, so it runs anywhere CI runs.
"""
from __future__ import annotations

import argparse
import json
import sys
from collections import deque
from pathlib import Path
from typing import Dict, Iterable, List, Set

HOME = "main_menu"


def load_dir(path: Path) -> List[dict]:
    if not path.is_dir():
        return []
    return [json.loads(p.read_text(encoding="utf-8")) for p in sorted(path.glob("*.json"))]


def templates_in(obj: object) -> Set[str]:
    """Every "template" value anywhere inside a JSON value."""
    found: Set[str] = set()
    if isinstance(obj, dict):
        for k, v in obj.items():
            if k == "template" and isinstance(v, str):
                found.add(v)
            else:
                found |= templates_in(v)
    elif isinstance(obj, list):
        for v in obj:
            found |= templates_in(v)
    return found


def shortest_path(pages: Dict[str, dict], start: str, goal: str) -> List[str]:
    if start not in pages or goal not in pages:
        return []
    parent = {start: start}
    queue = deque([start])
    while queue:
        cur = queue.popleft()
        if cur == goal:
            path = [cur]
            while path[-1] != start:
                path.append(parent[path[-1]])
            return path[::-1]
        for t in pages[cur].get("transitions", []):
            nxt = t.get("to")
            if nxt in pages and nxt not in parent:
                parent[nxt] = cur
                queue.append(nxt)
    return []


def page_templates(pages: Dict[str, dict], path: List[str]) -> Set[str]:
    """Anchors of every page on the path plus the transitions actually taken."""
    needed: Set[str] = set()
    for i, pid in enumerate(path):
        page = pages[pid]
        needed |= templates_in(page.get("anchors", []))
        if i + 1 < len(path):
            for t in page.get("transitions", []):
                if t.get("to") == path[i + 1]:
                    needed |= templates_in(t.get("click", {}))
    return needed


def all_steps(obj: object):
    """Every step (a dict with "action") anywhere inside a JSON value, nested ones included."""
    if isinstance(obj, dict):
        if "action" in obj:
            yield obj
        for v in obj.values():
            yield from all_steps(v)
    elif isinstance(obj, list):
        for v in obj:
            yield from all_steps(v)


def expand_values(names: Set[str], task: dict) -> Set[str]:
    """"{value}" in a template name stands for each choice of the task's list settings."""
    choices = [c for s in task.get("settings", []) if s.get("type") == "list" for c in s.get("choices", [])]
    out: Set[str] = set()
    for name in names:
        if "{value}" in name:
            out |= {name.replace("{value}", c) for c in choices}
        else:
            out.add(name)
    return out


def task_templates(task: dict, pages: Dict[str, dict], popups: List[dict]) -> Set[str]:
    needed = expand_values(templates_in(task.get("steps", [])), task)
    needed |= templates_in([p.get("anchors", []) for p in popups])
    needed |= templates_in([p.get("dismiss", {}) for p in popups])
    needed |= page_templates(pages, [HOME]) if HOME in pages else set()
    for step in all_steps(task.get("steps", [])):
        target = step.get("page")
        if step.get("action") in ("goto", "wait_page") and target:
            needed |= page_templates(pages, shortest_path(pages, HOME, target))
            needed |= page_templates(pages, shortest_path(pages, target, HOME))
    return needed


def search_dirs(root: Path, lang: str, committed_only: bool) -> List[Path]:
    dirs = [root / "assets-local" / lang, root / "assets" / lang,
            root / "assets-local" / "common", root / "assets" / "common"]
    if committed_only:
        dirs = [d for d in dirs if d.parts[-2] != "assets-local"]
    return dirs


def resolve(root: Path, lang: str, name: str, committed_only: bool) -> bool:
    if "/" in name or "\\" in name or name in ("", ".", ".."):
        return False
    return any((d / name).is_file() for d in search_dirs(root, lang, committed_only))


def build_matrix(root: Path, committed_only: bool = False) -> dict:
    languages = json.loads((root / "config" / "languages.json").read_text(encoding="utf-8"))["languages"]
    pages = {p["id"]: p for p in load_dir(root / "pages")}
    popups = load_dir(root / "popups")
    tasks = load_dir(root / "tasks")
    rows = []
    for task in tasks:
        implemented = bool(task.get("steps"))
        needed = sorted(task_templates(task, pages, popups)) if implemented else []
        cells = {}
        for lang in languages:
            missing = [n for n in needed if not resolve(root, lang["code"], n, committed_only)]
            cells[lang["code"]] = {"missing": missing,
                                   "state": "n/a" if not implemented else ("ok" if not missing else "missing")}
        rows.append({"task": task["id"], "status": task.get("status", "todo"), "implemented": implemented,
                     "templates": needed, "languages": cells})
    return {"languages": languages, "tasks": rows}


def to_markdown(matrix: dict) -> str:
    codes = [l["code"] for l in matrix["languages"]]
    lines = ["| Task | Status | " + " | ".join(codes) + " |",
             "|---|---|" + "---|" * len(codes)]
    marks = {"ok": "ok", "missing": "missing", "n/a": "-"}
    for row in matrix["tasks"]:
        cells = []
        for c in codes:
            cell = row["languages"][c]
            text = marks[cell["state"]]
            if cell["state"] == "missing":
                text += f" ({len(cell['missing'])})"
            cells.append(text)
        lines.append(f"| {row['task']} | {row['status']} | " + " | ".join(cells) + " |")
    missing: Dict[str, Set[str]] = {}
    for row in matrix["tasks"]:
        for c, cell in row["languages"].items():
            for name in cell["missing"]:
                missing.setdefault(name, set()).add(c)
    if missing:
        lines += ["", "Missing templates:"]
        for name in sorted(missing):
            lines.append(f"- `{name}`: " + ", ".join(sorted(missing[name])))
    return "\n".join(lines)


def strict_failures(matrix: dict) -> List[str]:
    confirmed = {l["code"] for l in matrix["languages"] if l.get("status") == "confirmed"}
    out = []
    for row in matrix["tasks"]:
        if row["status"] != "ready":
            continue
        for code, cell in row["languages"].items():
            if code in confirmed and cell["missing"]:
                out.append(f"{row['task']} [{code}]: " + ", ".join(cell["missing"]))
    return out


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--committed", action="store_true")
    args = parser.parse_args(list(argv) if argv is not None else None)
    matrix = build_matrix(args.root, args.committed)
    sys.stdout.reconfigure(encoding="utf-8")
    print(json.dumps(matrix, ensure_ascii=False, indent=2) if args.json else to_markdown(matrix))
    if args.strict:
        failures = strict_failures(matrix)
        for f in failures:
            print("MISSING " + f, file=sys.stderr)
        return 1 if failures else 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
