#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Scan files that would be committed for personal data and secrets.

Usage:
  python tools/privacy_check.py            # tracked + untracked-but-not-ignored files
  python tools/privacy_check.py --staged   # staged content only (used by the pre-commit hook)
  python tools/privacy_check.py PATH...    # explicit files

Extra personal terms (real name, nickname, ...) go into `.privacy-terms.local`,
one per line, case-insensitive. That file is gitignored and never committed.
Exit code 1 if anything is found.
"""
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAX_BYTES = 2_000_000
SKIP_DIRS = {".git", "build", "out", "bin", "obj", ".venv", "__pycache__", "vcpkg_installed"}

ALLOWED_EMAIL_SUFFIXES = ("@users.noreply.github.com", "@example.com", "@example.org", "@example.net")
FILE_LIKE_TLDS = (".png", ".jpg", ".jpeg", ".gif", ".svg", ".webp", ".ico")

PATTERNS = [
    ("email address", re.compile(r"[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)*\.[A-Za-z]{2,}")),
    ("windows user path", re.compile(r"[A-Za-z]:\\Users\\[^\\\s\"']+", re.I)),
    ("macOS user path", re.compile(r"/Users/[A-Za-z0-9_.-]+")),
    ("linux home path", re.compile(r"/home/[A-Za-z0-9_.-]+")),
    ("private key", re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----")),
    ("GitHub token", re.compile(r"\b(?:ghp|gho|ghu|ghs|ghr)_[A-Za-z0-9]{30,}\b|github_pat_[A-Za-z0-9_]{20,}")),
    ("AWS access key", re.compile(r"\bAKIA[0-9A-Z]{16}\b")),
    ("Slack token", re.compile(r"\bxox[baprs]-[A-Za-z0-9-]{10,}")),
    ("API key (sk-...)", re.compile(r"\bsk-[A-Za-z0-9_-]{20,}")),
    ("hard-coded credential", re.compile(
        r"(?i)\b(?:password|passwd|secret|api[_-]?key|access[_-]?token)\b\s*[:=]\s*['\"][^'\"\s]{8,}['\"]")),
]


def git(*args, binary=False):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, check=True,
                          text=not binary).stdout


def in_git_repo():
    try:
        git("rev-parse", "--is-inside-work-tree")
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def list_files(mode):
    if mode == "staged":
        out = git("diff", "--cached", "--name-only", "--diff-filter=ACM", "-z")
    elif in_git_repo():
        out = git("ls-files", "-co", "--exclude-standard", "-z")
    else:  # not a repository yet: walk the tree
        names = []
        for dirpath, dirnames, filenames in os.walk(ROOT):
            dirnames[:] = [d for d in dirnames if d not in SKIP_DIRS]
            for f in filenames:
                names.append(str(Path(dirpath, f).relative_to(ROOT)))
        return names
    return [n for n in out.split("\0") if n]


def read_bytes(mode, name):
    if mode == "staged":
        return git("show", f":{name}", binary=True)
    return (ROOT / name).read_bytes()


def load_terms():
    p = ROOT / ".privacy-terms.local"
    if not p.exists():
        return []
    terms = []
    for line in p.read_text(encoding="utf-8", errors="ignore").splitlines():
        line = line.strip()
        if line and not line.startswith("#"):
            terms.append(line.lower())
    return terms


def mask(s):
    return s[:4] + "***" if len(s) > 6 else "***"


def email_ok(addr):
    low = addr.lower()
    return low.endswith(ALLOWED_EMAIL_SUFFIXES) or low.endswith(FILE_LIKE_TLDS)


def scan(name, data, terms):
    if len(data) > MAX_BYTES or b"\0" in data[:8192]:
        return []
    text = data.decode("utf-8", errors="ignore")
    findings = []
    for lineno, line in enumerate(text.splitlines(), 1):
        for kind, rx in PATTERNS:
            for m in rx.finditer(line):
                if kind == "email address" and email_ok(m.group(0)):
                    continue
                findings.append((name, lineno, kind, mask(m.group(0))))
        low = line.lower()
        for t in terms:
            if t in low:
                findings.append((name, lineno, "personal term from .privacy-terms.local", mask(t)))
    return findings


def main(argv):
    mode = "all"
    paths = []
    for a in argv:
        if a == "--staged":
            mode = "staged"
        else:
            paths.append(a)
    terms = load_terms()
    findings = []
    if paths:
        names = paths
        for n in names:
            findings += scan(n, Path(n).read_bytes(), terms)
    else:
        names = list_files(mode)
        for n in names:
            try:
                data = read_bytes(mode, n)
            except (OSError, subprocess.CalledProcessError):
                continue
            findings += scan(n, data, terms)
    for name, lineno, kind, snippet in findings:
        print(f"{name}:{lineno}: {kind}: {snippet}")
    if findings:
        print(f"\nprivacy check FAILED: {len(findings)} finding(s). Remove them before committing.")
        return 1
    print(f"privacy check passed ({len(names)} files)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
