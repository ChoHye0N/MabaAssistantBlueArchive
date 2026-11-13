#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Start a release with one command: versions, commit, push and the build workflows.

  release.py program 0.2.0 [--dry-run]     a new program version (with the resources of this commit)
  release.py resources [--dry-run]         only task data and templates, for players' running programs
      [--min-core X.Y.Z]                   overrides the program version the resources ask for

It ends with drafts on GitHub. Read them, then publish: the script prints the commands and the order.
Nothing is published from here (docs/notes/updates.md).

What it decides by itself:
- `min_core` in config/resource.json, the oldest program that understands the data. Data can only
  need a newer program when Core changed (`core/src`, `core/include`) since the last program
  release: then it is the version being released (`program`) or the version in the source that is
  not released yet (`resources`); otherwise it stays, at most the last released version.
- the resource version (date and counter), raised when the data changed since it was last set
  (`program`) or when a release with the current one exists already (`resources`).

Needs a clean `main` that equals `origin/main`, and the `gh` command signed in. Standard library only.
"""
import argparse
import datetime
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA_DIRS = ("config", "pages", "popups", "tasks", "strings", "assets")
CORE_DIRS = ("core/src", "core/include")
VERSION_RE = re.compile(r"^(\d+)\.(\d+)\.(\d+)(-[0-9A-Za-z.]+)?$")


class ReleaseError(Exception):
    pass


# --- decisions (no git, tested in tools/tests/test_release.py) -------------------------------

def parse_version(text):
    """(major, minor, patch) of '0.2.0' or '0.2.0-beta.1'; None when it is not a version."""
    m = VERSION_RE.match(text or "")
    return tuple(int(m.group(i)) for i in (1, 2, 3)) if m else None


def base_version(text):
    return text.split("-", 1)[0]


def next_resource_version(current, today):
    """'20261113.1' -> the next counter of the same day, or '<today>.1' on a later day."""
    stamp = today.strftime("%Y%m%d")
    date, _, counter = (current or "").partition(".")
    if date == stamp and counter.isdigit():
        return f"{stamp}.{int(counter) + 1}"
    if date.isdigit() and date > stamp:
        raise ReleaseError(f"resource version {current} is ahead of today ({stamp})")
    return f"{stamp}.1"


def decide_min_core(current, released, core_changed, upcoming):
    """The program version the data needs.

    current: the value in config/resource.json; released: the last released program version;
    core_changed: Core differs from that release; upcoming: the version this run releases
    (`program`), or the version in the source (`resources`).
    """
    if core_changed:
        if parse_version(upcoming) <= parse_version(released):
            raise ReleaseError(
                f"Core changed since {released}, so the data may need a program that is not released yet. "
                f"Release the program first (release.py program X.Y.Z), or pass --min-core when the "
                f"data does not use anything new.")
        return base_version(upcoming)
    # Core is the released one: the data cannot need more than that.
    if parse_version(current) is None or parse_version(current) > parse_version(released):
        return released
    return current


def set_program_version(texts, version):
    """texts: {'cmake': ..., 'vcpkg': ..., 'csproj': ...} -> the same with the new version."""
    out = {}
    out["cmake"], n1 = re.subn(r"(project\(MabaAssistantBlueArchive VERSION )\d+\.\d+\.\d+",
                               r"\g<1>" + base_version(version), texts["cmake"])
    out["vcpkg"], n2 = re.subn(r'("version-string"\s*:\s*")[^"]*(")', r"\g<1>" + version + r"\g<2>", texts["vcpkg"])
    out["csproj"], n3 = re.subn(r"(<Version>)[^<]*(</Version>)", r"\g<1>" + version + r"\g<2>", texts["csproj"])
    if (n1, n2, n3) != (1, 1, 1):
        raise ReleaseError("could not find the version in CMakeLists.txt, vcpkg.json or Maba.Gui.csproj")
    return out


def source_version(cmake_text):
    m = re.search(r"project\(MabaAssistantBlueArchive VERSION (\d+\.\d+\.\d+)", cmake_text)
    if not m:
        raise ReleaseError("CMakeLists.txt has no project version")
    return m.group(1)


def latest_program_version(tags):
    """The highest 'vX.Y.Z' tag without a suffix (pre-releases never count as released)."""
    versions = [t[1:] for t in tags if t.startswith("v") and "-" not in t and parse_version(t[1:])]
    if not versions:
        raise ReleaseError("no program release tag (vX.Y.Z) found")
    return max(versions, key=parse_version)


# --- git and GitHub ----------------------------------------------------------------------------

VERSION_FILES = {"cmake": "CMakeLists.txt", "vcpkg": "vcpkg.json", "csproj": "gui/Maba.Gui/Maba.Gui.csproj"}
RESOURCE_FILE = "config/resource.json"


def run(*cmd, check=True):
    p = subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, encoding="utf-8")
    if check and p.returncode != 0:
        raise ReleaseError(f"{' '.join(cmd)}: {(p.stderr or p.stdout).strip()}")
    return p


def git(*args, check=True):
    return run("git", *args, check=check)


def changed_since(ref, paths):
    return git("diff", "--quiet", ref, "HEAD", "--", *paths, check=False).returncode != 0


def read(rel):
    return (ROOT / rel).read_bytes().decode("utf-8")


def write(rel, text):
    (ROOT / rel).write_bytes(text.encode("utf-8"))


def check_tree():
    if git("status", "--porcelain").stdout.strip():
        raise ReleaseError("the working tree has changes: commit or stash them first")
    if git("rev-parse", "--abbrev-ref", "HEAD").stdout.strip() != "main":
        raise ReleaseError("releases are made from main")
    git("fetch", "--tags", "origin", "main")
    if git("rev-parse", "HEAD").stdout != git("rev-parse", "origin/main").stdout:
        raise ReleaseError("main differs from origin/main: push or pull first")
    run("gh", "auth", "status")


def release_exists(tag):
    return run("gh", "release", "view", tag, check=False).returncode == 0


def to_particle(version):
    """The Korean particle after a version read as digits: '0.2.0으로', '0.1.5로'."""
    last = [c for c in version if c.isdigit()][-1]
    return "으로" if last in "036" else "로"


def write_resource(version, min_core):
    write(RESOURCE_FILE, json.dumps({"version": version, "min_core": min_core}, indent=2) + "\n")


def commit_and_push(files, subject, body, dry):
    if dry:
        print(f"  would commit: {subject}")
        git("checkout", "--", *files)
        return
    git("add", *files)
    git("commit", "-m", subject, "-m", body)
    git("push", "origin", "main")


def start(workflow, *fields, dry):
    if dry:
        print(f"  would start workflow {workflow} {' '.join(fields)}")
        return
    args = [a for f in fields for a in ("-f", f)]
    run("gh", "workflow", "run", workflow, *args)


def program(version, dry):
    if parse_version(version) is None:
        raise ReleaseError(f"version '{version}' must look like 0.2.0 or 0.2.0-beta.1")
    check_tree()
    released = latest_program_version(git("tag").stdout.split())
    if parse_version(version) <= parse_version(released) and "-" not in version:
        raise ReleaseError(f"{version} is not above the last release {released}")
    if release_exists("v" + version) or ("v" + version) in git("tag").stdout.split():
        raise ReleaseError(f"v{version} exists already")

    resource = json.loads(read(RESOURCE_FILE))
    core_changed = changed_since("v" + released, CORE_DIRS)
    min_core = decide_min_core(resource.get("min_core", ""), released, core_changed, version)
    last_set = git("log", "-1", "--format=%H", "--", RESOURCE_FILE).stdout.strip()
    data_changed = changed_since(last_set, DATA_DIRS)
    res_version = next_resource_version(resource["version"], datetime.date.today()) if data_changed else resource["version"]

    for key, text in set_program_version({k: read(f) for k, f in VERSION_FILES.items()}, version).items():
        write(VERSION_FILES[key], text)
    write_resource(res_version, min_core)
    print(f"program {released} -> {version}; resources {resource['version']} -> {res_version}, min_core {min_core}"
          f" (Core {'changed' if core_changed else 'unchanged'} since {released})")
    body = f"- 프로그램 버전 {version}\n- 리소스 버전 {res_version}, `min_core` {min_core}"
    commit_and_push(list(VERSION_FILES.values()) + [RESOURCE_FILE], f"chore: 버전 {version}{to_particle(version)} 변경", body, dry)
    start("release.yml", f"version={version}", dry=dry)
    # Players who update inside the program keep their data folder: they get the data as a pack.
    with_pack = not release_exists("resources-" + res_version)
    if with_pack:
        start("resources.yml", dry=dry)
    print("\nNext, when the workflows and CI are green (gh run list):")
    print(f"  1. write the notes, then: gh release edit v{version} --notes-file NOTES.md --draft=false --latest")
    if with_pack:
        print(f"  2. after that: gh release edit resources-{res_version} --draft=false --prerelease")


def resources(min_core_arg, dry):
    check_tree()
    released = latest_program_version(git("tag").stdout.split())
    resource = json.loads(read(RESOURCE_FILE))
    if min_core_arg:
        if parse_version(min_core_arg) is None or "-" in min_core_arg:
            raise ReleaseError(f"--min-core '{min_core_arg}' must look like 0.2.0")
        min_core = min_core_arg
    else:
        core_changed = changed_since("v" + released, CORE_DIRS)
        min_core = decide_min_core(resource.get("min_core", ""), released, core_changed, source_version(read("CMakeLists.txt")))
    version = resource["version"]
    if release_exists("resources-" + version):
        last_set = git("log", "-1", "--format=%H", "--", RESOURCE_FILE).stdout.strip()
        if not changed_since(last_set, DATA_DIRS):
            raise ReleaseError(f"resources-{version} is released and the data did not change since")
        version = next_resource_version(version, datetime.date.today())
    print(f"resources {resource['version']} -> {version}, min_core {min_core} (last program release {released})")
    if parse_version(min_core) > parse_version(released):
        print(f"note: players get this pack only after program {min_core} is released")
    if (version, min_core) != (resource["version"], resource.get("min_core")):
        write_resource(version, min_core)
        commit_and_push([RESOURCE_FILE], f"chore: 리소스 버전 {version}{to_particle(version)} 변경", f"- 리소스 버전 {version}, `min_core` {min_core}", dry)
    start("resources.yml", dry=dry)
    print("\nNext, when the workflow and CI are green (gh run list):")
    print(f"  gh release edit resources-{version} --draft=false --prerelease")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="what", required=True)
    p = sub.add_parser("program")
    p.add_argument("version")
    p.add_argument("--dry-run", action="store_true")
    r = sub.add_parser("resources")
    r.add_argument("--min-core", default="")
    r.add_argument("--dry-run", action="store_true")
    args = ap.parse_args(argv)
    try:
        if args.what == "program":
            program(args.version, args.dry_run)
        else:
            resources(args.min_core, args.dry_run)
    except ReleaseError as e:
        print(f"error: {e}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
