#!/usr/bin/env python3
"""Validate repository commit messages use English Conventional Commit subjects."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys


SUBJECT_RE = re.compile(
    r"^(build|chore|ci|docs|feat|fix|perf|refactor|revert|style|test)"
    r"(\([a-z0-9][a-z0-9._-]*\))?!?: [A-Za-z0-9].+"
)


def run_git_log(revisions: list[str]) -> str:
    result = subprocess.run(
        ["git", "log", "--format=%H%x00%B%x1e", *revisions],
        check=False,
        text=True,
        capture_output=True,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
        raise SystemExit(result.returncode)
    return result.stdout


def iter_messages(raw: str) -> list[tuple[str, str]]:
    messages: list[tuple[str, str]] = []
    for record in raw.split("\x1e"):
        record = record.strip("\n")
        if not record:
            continue
        commit, separator, message = record.partition("\x00")
        if not separator:
            raise ValueError(f"malformed git log record for {commit}")
        messages.append((commit, message.strip("\n")))
    return messages


def validate_message(commit: str, message: str) -> list[str]:
    errors: list[str] = []
    subject = message.splitlines()[0] if message else ""
    try:
        message.encode("ascii")
    except UnicodeEncodeError:
        errors.append("message must be ASCII English text")
    if not SUBJECT_RE.fullmatch(subject):
        errors.append(
            "subject must match Conventional Commits, for example "
            "`feat(runtime): add bounded scheduling evidence`"
        )
    return [f"{commit[:12]} {subject!r}: {error}" for error in errors]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--message-file",
        type=str,
        help="validate one commit message file, used by the pre-commit commit-msg hook",
    )
    parser.add_argument(
        "revisions",
        nargs="*",
        default=["HEAD"],
        help="git log revision arguments to validate, defaults to HEAD history",
    )
    args = parser.parse_args()

    if args.message_file:
        message = open(args.message_file, encoding="utf-8").read().strip("\n")
        failures = validate_message("COMMIT_MSG", message)
        if failures:
            sys.stderr.write("commit message check failed:\n")
            for failure in failures:
                sys.stderr.write(f"- {failure}\n")
            return 1
        return 0

    failures: list[str] = []
    for commit, message in iter_messages(run_git_log(args.revisions)):
        failures.extend(validate_message(commit, message))

    if failures:
        sys.stderr.write("commit message check failed:\n")
        for failure in failures:
            sys.stderr.write(f"- {failure}\n")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
