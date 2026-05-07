#!/usr/bin/env python3
"""Check adoption feedback and triage surfaces stay usable."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REQUIRED_DOC = [
    "## First-user path",
    "## Debug pack",
    "## Label taxonomy",
    "## Triage states",
    "## Response checklist",
    "production deployment claim",
]

TEMPLATE_MARKERS = {
    ".github/ISSUE_TEMPLATE/bug_report.md": ["## Debug pack", "TopoExec commit/tag", "focused gate"],
    ".github/ISSUE_TEMPLATE/semantic_mismatch.md": ["## Debug pack", "minimal graph", "focused gate"],
    ".github/ISSUE_TEMPLATE/performance_issue.md": ["## Debug pack", "machine context", "benchmark JSON"],
    ".github/ISSUE_TEMPLATE/feature_request.md": ["## Adoption impact", "first-user path"],
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True, type=Path)
    args = parser.parse_args()
    root = args.source_dir
    try:
        doc = (root / "docs/44-coding-standards/feedback-and-triage.md").read_text(encoding="utf-8")
        for phrase in REQUIRED_DOC:
            require(phrase in doc, f"feedback doc missing {phrase}")
        for relative, markers in TEMPLATE_MARKERS.items():
            text = (root / relative).read_text(encoding="utf-8")
            lowered = text.lower()
            for marker in markers:
                require(marker.lower() in lowered, f"{relative} missing {marker}")
        docs_readme = (root / "docs/README.md").read_text(encoding="utf-8")
        require("feedback-and-triage.md" in docs_readme, "docs map must link feedback workflow")
        pr = (root / ".github/PULL_REQUEST_TEMPLATE.md").read_text(encoding="utf-8")
        require("Known validation gaps" in pr, "PR template must keep validation gaps")
    except Exception as exc:  # noqa: BLE001
        print(f"adoption feedback check failed: {exc}", file=sys.stderr)
        return 1
    print(json.dumps({"ok": True, "templates_checked": len(TEMPLATE_MARKERS)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
