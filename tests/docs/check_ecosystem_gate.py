#!/usr/bin/env python3
"""Check ecosystem decision gate keeps conditional tracks blocked."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REQUIRED_GATE = [
    "## Candidate G82 tracks",
    "G82a",
    "G82b",
    "G82c",
    "G82d",
    "G82e",
    "## Recommendation",
    "Do not implement any G82 track",
    "G82e package registry publication",
]

REQUIRED_BLOCKER = [
    "Status: blocked on adoption signal and human owner decision.",
    "## Decision needed",
    "## Options",
    "## Recommendation",
    "Do not implement G82a-e",
]

FORBIDDEN_IMPL_TOKENS = [
    "find_package(rclcpp",
    "pybind11_add_module",
    "find_package(OpenTelemetry",
    "find_package(prometheus",
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
        gate = (root / "docs/31-planning-roadmap/ecosystem-decision-gate.md").read_text(encoding="utf-8")
        blocker = (root / "docs/31-planning-roadmap/goals/blockers/g81-ecosystem-track-selection.md").read_text(encoding="utf-8")
        for phrase in REQUIRED_GATE:
            require(phrase in gate, f"ecosystem gate missing {phrase}")
        for phrase in REQUIRED_BLOCKER:
            require(phrase in blocker, f"G81 blocker missing {phrase}")
        cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
        for token in FORBIDDEN_IMPL_TOKENS:
            require(token.lower() not in cmake.lower(), f"unexpected ecosystem implementation token {token}")
        docs_readme = (root / "docs/README.md").read_text(encoding="utf-8")
        require("ecosystem-decision-gate.md" in docs_readme, "docs map must link ecosystem decision gate")
    except Exception as exc:  # noqa: BLE001
        print(f"ecosystem gate check failed: {exc}", file=sys.stderr)
        return 1
    print(json.dumps({"ok": True, "recommended_future_track": "G82e-package-registry-publication-after-owner-release"}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
