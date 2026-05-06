#!/usr/bin/env python3
"""Smoke-test release_prepare.sh without creating release artifacts."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True, type=Path)
    parser.add_argument("--build-dir", required=True, type=Path)
    args = parser.parse_args()

    smoke_dir = args.build_dir / "release-prepare-smoke"
    notes = smoke_dir / "notes.md"
    command = [
        str(args.source_dir / "scripts/release_prepare.sh"),
        "--version",
        "v0.2.0-alpha.0",
        "--dry-run",
        "--allow-dirty",
        "--artifacts-dir",
        str(smoke_dir),
        "--notes-out",
        str(notes),
    ]
    completed = subprocess.run(
        command,
        cwd=args.source_dir,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        sys.stderr.write(completed.stdout)
        sys.stderr.write(completed.stderr)
        return completed.returncode

    require(notes.exists(), "release notes draft was not written")
    require((smoke_dir / "tag-command-v0.2.0-alpha.0.txt").exists(), "tag command draft missing")
    text = notes.read_text(encoding="utf-8")
    require("Candidate commit:" in text, "notes missing candidate commit")
    require("Human approval checklist" in text, "notes missing human approval checklist")
    require("Annotated tag" in text, "notes missing annotated tag reminder")
    require("dry-run" in completed.stdout, "stdout should make dry-run behavior visible")
    print("ok: release_prepare dry-run smoke")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except AssertionError as error:
        print(f"release_prepare smoke failed: {error}", file=sys.stderr)
        raise SystemExit(1)
