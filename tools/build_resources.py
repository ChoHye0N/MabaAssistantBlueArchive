#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Build the resource pack: the data the program updates by itself, apart from the program.

  build_resources.py [--out dist]

Writes `MABA-resources-<version>.zip` (config/, pages/, popups/, tasks/, strings/, assets/) and
`resources.json` (version, the program version it needs, file name, size, SHA-256) into the output
folder. Version and `min_core` come from `config/resource.json`: raise the version for every pack,
and `min_core` whenever the data starts to use something an older program does not know.

The archive is deterministic: the same files give the same bytes and hash. Standard library only.
"""
import argparse
import hashlib
import json
import re
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RESOURCE_DIRS = ("config", "pages", "popups", "tasks", "strings", "assets")
VERSION_RE = re.compile(r"^\d{8}\.\d{1,3}$")       # date and a counter: 20261113.1
CORE_RE = re.compile(r"^\d+\.\d+\.\d+$")
FIXED_TIME = (2026, 1, 1, 0, 0, 0)                  # one timestamp for every entry


def read_version(root=ROOT):
    info = json.loads((root / "config" / "resource.json").read_text(encoding="utf-8"))
    version, min_core = info.get("version", ""), info.get("min_core", "")
    if not VERSION_RE.match(version):
        raise ValueError(f"config/resource.json: version '{version}' must look like 20261113.1")
    if not CORE_RE.match(min_core):
        raise ValueError(f"config/resource.json: min_core '{min_core}' must look like 0.1.5")
    return version, min_core


def resource_files(root=ROOT):
    """Paths relative to the root, sorted, forward slashes."""
    out = []
    for name in RESOURCE_DIRS:
        base = root / name
        if base.is_dir():
            out += [p.relative_to(root).as_posix() for p in base.rglob("*") if p.is_file()]
    return sorted(out)


def build(root=ROOT, out=None):
    out = Path(out) if out else root / "dist"
    out.mkdir(parents=True, exist_ok=True)
    version, min_core = read_version(root)
    files = resource_files(root)
    if not files:
        raise ValueError("no resource files found")
    archive = out / f"MABA-resources-{version}.zip"
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for rel in files:
            info = zipfile.ZipInfo(rel, FIXED_TIME)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, (root / rel).read_bytes())
    data = archive.read_bytes()
    manifest = {"version": version, "min_core": min_core, "file": archive.name, "size": len(data),
                "sha256": hashlib.sha256(data).hexdigest(), "files": len(files)}
    (out / "resources.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return archive, manifest


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--out", help="output folder (default dist/)")
    args = ap.parse_args(argv)
    try:
        archive, manifest = build(out=args.out)
    except (ValueError, OSError, json.JSONDecodeError) as e:
        print(f"error: {e}")
        return 1
    print(f"{archive} ({manifest['size'] / 1e6:.1f} MB, {manifest['files']} files)")
    print(f"version {manifest['version']}, needs program {manifest['min_core']}, sha256 {manifest['sha256']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
