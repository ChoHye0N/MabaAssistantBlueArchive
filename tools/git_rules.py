#!/usr/bin/env python3
# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
"""Commit message and push rules, used by the hooks in .githooks/.

  git_rules.py commit-msg FILE   check a commit message file
  git_rules.py pre-push          read the pre-push hook lines from stdin, check every new commit

Rules:
  - Subject: `type(scope): description`, type from TYPES, no trailing period, at most 100 characters.
  - No AI attribution: AI-tool Co-authored-by lines, Claude-Session, "Generated with", robot emoji, session links.
  - On push: author and committer emails must be GitHub noreply addresses (MABA_ALLOW_ANY_EMAIL=1 skips this),
    and non-fast-forward (force) pushes are refused (MABA_ALLOW_FORCE=1 skips this).
"""
import os
import re
import subprocess
import sys

TYPES = ("feat", "fix", "docs", "refactor", "test", "chore", "ci", "perf", "build", "revert")
SUBJECT_RE = re.compile(r"^(?:%s)(?:\([a-z0-9][a-z0-9-]*\))?: \S.*$" % "|".join(TYPES))
MAX_SUBJECT = 100
AUTO_PREFIXES = ("Merge ", "fixup! ", "squash! ", "amend! ")
NOREPLY = "@users.noreply.github.com"
ZERO = "0" * 40

AI_PATTERNS = [
    ("Claude-Session trailer", re.compile(r"(?im)^\s*claude-session\s*:")),
    ("'Generated with/by' line", re.compile(r"(?im)^\W*generated (?:with|by)\b")),
    ("robot emoji", re.compile("\U0001F916")),
    ("AI session link", re.compile(r"(?i)claude\.(?:ai|com)/code")),
    ("AI vendor noreply address", re.compile(r"(?i)noreply@anthropic\.com")),
    ("AI Co-authored-by trailer", re.compile(
        r"(?im)^\s*co-authored-by\s*:.*(?:claude|anthropic|openai|chatgpt|\bgpt|copilot|gemini|cursor|codex|\bai\b|\bbot\b)")),
]


def check_message(text):
    lines = [l for l in text.splitlines() if not l.startswith("#")]
    body = "\n".join(lines)
    problems = []
    subject = next((l for l in lines if l.strip()), "")
    if not subject.startswith(AUTO_PREFIXES):
        if not SUBJECT_RE.match(subject):
            problems.append("subject must look like `type(scope): description` with type in: " + ", ".join(TYPES))
        if subject.rstrip().endswith("."):
            problems.append("subject must not end with a period")
        if len(subject) > MAX_SUBJECT:
            problems.append(f"subject is {len(subject)} characters (limit {MAX_SUBJECT})")
    for name, rx in AI_PATTERNS:
        if rx.search(body):
            problems.append(f"AI attribution is not allowed: {name}")
    return problems


def git(*args):
    # Git output is UTF-8 (commit messages are Korean); never decode with the locale code page.
    return subprocess.run(["git", *args], capture_output=True, check=True,
                          encoding="utf-8", errors="replace").stdout


def cmd_commit_msg(path):
    with open(path, encoding="utf-8", errors="replace") as f:
        problems = check_message(f.read())
    for p in problems:
        print(f"commit-msg: {p}", file=sys.stderr)
    return 1 if problems else 0


def new_commits(local, remote):
    if remote != ZERO:
        try:
            return git("rev-list", f"{remote}..{local}").split()
        except subprocess.CalledProcessError:
            pass  # remote commit unknown locally: fall back
    return git("rev-list", local, "--not", "--remotes").split()


def is_force_push(local, remote):
    if remote == ZERO:
        return False
    try:
        subprocess.run(["git", "merge-base", "--is-ancestor", remote, local], check=True,
                       capture_output=True)
        return False
    except subprocess.CalledProcessError:
        return True


def cmd_pre_push():
    failed = False
    allow_email = bool(os.environ.get("MABA_ALLOW_ANY_EMAIL"))
    allow_force = bool(os.environ.get("MABA_ALLOW_FORCE"))
    for line in sys.stdin.read().splitlines():
        parts = line.split()
        if len(parts) != 4:
            continue
        _lref, local, _rref, remote = parts
        if local == ZERO:  # branch deletion
            continue
        if is_force_push(local, remote) and not allow_force:
            print("pre-push: non-fast-forward (force) push refused. Set MABA_ALLOW_FORCE=1 only if the owner asked for it.",
                  file=sys.stderr)
            failed = True
        for sha in new_commits(local, remote):
            short = sha[:8]
            msg = git("log", "-1", "--format=%B", sha)
            for p in check_message(msg):
                print(f"pre-push: {short}: {p}", file=sys.stderr)
                failed = True
            if not allow_email:
                ae, ce = git("log", "-1", "--format=%ae%n%ce", sha).split("\n")[:2]
                for who, email in (("author", ae), ("committer", ce)):
                    if not email.lower().endswith(NOREPLY):
                        print(f"pre-push: {short}: {who} email is not a GitHub noreply address", file=sys.stderr)
                        failed = True
    if failed:
        print("pre-push: push blocked. Fix the commits (git commit --amend / rebase) and try again.", file=sys.stderr)
    return 1 if failed else 0


def main(argv):
    if len(argv) >= 2 and argv[0] == "commit-msg":
        return cmd_commit_msg(argv[1])
    if argv and argv[0] == "pre-push":
        return cmd_pre_push()
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
