#!/usr/bin/env python3
"""Check G85 keeps v1.0 readiness deferred and criteria-based."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REQUIRED = [
    "deferred criteria only",
    "does not declare TopoExec ready for",
    "not v1.0 ready",
    "post-beta adoption evidence",
    "Stable-surface expectations",
    "Scheduler and runtime limitation policy",
    "Package and adoption maturity criteria",
    "Owner decision path",
    "G81/G82/G83 blockers",
    "package registry publication decision",
    "./scripts/goal_check.sh v1",
    "./scripts/agent_check.sh",
    "release_prepare.sh --version v1.0.0",
]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True, type=Path)
    args = parser.parse_args()
    root = args.source_dir
    try:
        page = (root / "docs/43-ci-build-release-tools/v1-readiness-program.md").read_text(encoding="utf-8")
        for phrase in REQUIRED:
            require(phrase.lower() in page.lower(), f"v1 readiness page missing {phrase}")
        release_progression = (root / "docs/43-ci-build-release-tools/release-progression.md").read_text(encoding="utf-8")
        require("v1-readiness-program.md" in release_progression, "release progression must link v1 readiness program")
        require("Not ready" in release_progression and "post-beta adoption" in release_progression, "release progression must keep v1 not-ready wording")
        release_checklist = (root / "docs/43-ci-build-release-tools/release-checklist.md").read_text(encoding="utf-8")
        require("v1-readiness-program.md" in release_checklist, "release checklist must require v1 readiness program for v1 wording")
        docs_readme = (root / "docs/README.md").read_text(encoding="utf-8")
        require("v1-readiness-program.md" in docs_readme, "docs map must link v1 readiness program")
        beta_page = (root / "docs/43-ci-build-release-tools/core-runtime-beta-candidate-g84.md").read_text(encoding="utf-8")
        require("not a v1.0 readiness claim" in beta_page, "G84 page must keep v1 deferral wording")
    except Exception as exc:  # noqa: BLE001
        print(f"v1 readiness deferral check failed: {exc}", file=sys.stderr)
        return 1
    print(json.dumps({"ok": True, "v1_status": "deferred"}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
