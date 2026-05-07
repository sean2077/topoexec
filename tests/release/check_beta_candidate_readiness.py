#!/usr/bin/env python3
"""Check beta readiness docs keep beta scope honest."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REQUIRED = [
    "readiness review only",
    "human release owner",
    "core-runtime beta",
    "not adapter/ecosystem beta readiness",
    "## Evidence Summary",
    "./scripts/goal_check.sh compat",
    "./scripts/goal_check.sh dogfood",
    "./scripts/goal_check.sh reliability",
    "./scripts/goal_check.sh conditional",
    "not production-proven",
    "v1.0 stable-compatibility claim",
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
        page = (root / "docs/43-ci-build-release-tools/core-runtime-beta-candidate.md").read_text(encoding="utf-8")
        lowered = page.lower()
        for phrase in REQUIRED:
            require(phrase.lower() in lowered, f"beta candidate page missing {phrase}")
        beta_review = (root / "docs/43-ci-build-release-tools/beta-readiness-review.md").read_text(encoding="utf-8")
        require("core-runtime beta candidate" in beta_review, "beta readiness review must keep core-runtime wording")
        release_progression = (root / "docs/43-ci-build-release-tools/release-progression.md").read_text(encoding="utf-8")
        require("v0.5.0-beta" in release_progression and "Conditional core-runtime review" in release_progression, "release progression must keep beta conditional")
        docs_readme = (root / "docs/README.md").read_text(encoding="utf-8")
        require("core-runtime-beta-candidate.md" in docs_readme, "docs map must link beta candidate page")
    except Exception as exc:  # noqa: BLE001
        print(f"beta candidate readiness check failed: {exc}", file=sys.stderr)
        return 1
    print(json.dumps({"ok": True, "beta_scope": "core-runtime-only"}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
