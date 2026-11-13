#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Split a built release folder into the packages the program updates itself from.

  build_update.py PACKAGE_DIR --version 0.1.5 [--out dist]

PACKAGE_DIR is the unpacked release (the folder build_package.ps1 fills). Written to the output folder:

  MABA-<version>-app.zip   the executable, bin/, docs/, the licence files and data/tools/
  MABA-<version>-ocr.zip   ocr/ (Python and the OCR packages and models): large, changes seldom
  update.json              version and, per part, its version, file, size and SHA-256

The resource folders under data/ are in neither: they have their own releases (build_resources.py).
`ocr/part.json` is written into the package first; it names the OCR part's version, which is a hash
of what was installed (Python, package versions, server.py), so the program downloads that part
only when it really changed. Archives are deterministic. Standard library only.
"""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_resources import FIXED_TIME  # noqa: E402

import zipfile  # noqa: E402

VERSION_RE = re.compile(r"^\d+\.\d+\.\d+(-[0-9A-Za-z.]+)?$")
RESOURCE_DIRS = ("config", "pages", "popups", "tasks", "strings", "assets", "assets-local")


def files_under(root, rel):
    base = root / rel
    if base.is_file():
        return [rel]
    return sorted(p.relative_to(root).as_posix() for p in base.rglob("*") if p.is_file())


def part_files(package):
    """{part: [relative paths]}; resources and anything unexpected under data/ belong to no part."""
    app, ocr = [], []
    for item in sorted(package.iterdir()):
        if item.name == "ocr":
            ocr = [f for f in files_under(package, "ocr") if "__pycache__" not in f]
        elif item.name == "data":
            if (item / "tools").is_dir():
                app += files_under(package, "data/tools")
        elif item.name != "update":
            app += files_under(package, item.name)
    return {"app": sorted(app), "ocr": sorted(ocr)}  # by text: the same order on every platform


def ocr_version(package):
    """A hash of what the OCR part is made of: the Python runtime, every installed package version and the server."""
    ocr = package / "ocr"
    h = hashlib.sha256()
    for name in sorted(p.name for p in (ocr / "python").glob("python3*.zip")):
        h.update(name.encode())
    site = ocr / "python" / "Lib" / "site-packages"
    if site.is_dir():
        for name in sorted(p.name for p in site.iterdir() if p.name.endswith(".dist-info")):
            h.update(name.encode())
    for rel in ("server.py", "requirements.txt"):
        if (ocr / rel).is_file():
            h.update((ocr / rel).read_bytes())
    return h.hexdigest()[:12]


def write_zip(archive, package, files):
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for rel in files:
            info = zipfile.ZipInfo(rel, FIXED_TIME)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o644 << 16
            z.writestr(info, (package / rel).read_bytes())
    data = archive.read_bytes()
    return len(data), hashlib.sha256(data).hexdigest()


def build(package, version, out):
    package, out = Path(package), Path(out)
    if not VERSION_RE.match(version):
        raise ValueError(f"version '{version}' must look like 0.1.5 or 0.2.0-beta.1")
    if not package.is_dir():
        raise ValueError(f"{package} is not a folder")
    out.mkdir(parents=True, exist_ok=True)
    versions = {"app": version, "ocr": ocr_version(package)}
    if (package / "ocr").is_dir():
        (package / "ocr" / "part.json").write_text(json.dumps({"version": versions["ocr"]}) + "\n", encoding="utf-8")
    parts = []
    for name, files in part_files(package).items():
        if not files:
            continue
        archive = out / f"MABA-{version}-{name}.zip"
        size, sha = write_zip(archive, package, files)
        parts.append({"name": name, "version": versions[name], "file": archive.name, "size": size, "sha256": sha, "files": len(files)})
    if not any(p["name"] == "app" for p in parts):
        raise ValueError("the package has no program files")
    manifest = {"version": version, "parts": parts}
    (out / "update.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    return manifest


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("package")
    ap.add_argument("--version", required=True)
    ap.add_argument("--out", default="dist")
    args = ap.parse_args(argv)
    try:
        manifest = build(args.package, args.version, args.out)
    except (ValueError, OSError) as e:
        print(f"error: {e}")
        return 1
    for p in manifest["parts"]:
        print(f"{p['file']}: {p['size'] / 1e6:.1f} MB, {p['files']} files, part version {p['version']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
