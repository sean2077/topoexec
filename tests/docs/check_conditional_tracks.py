#!/usr/bin/env python3
"""Check conditional G82x/G83 tracks stay blocked until explicitly opened."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

BLOCKERS = [
    "g82a-production-observability-exporter.md",
    "g82b-native-python-binding.md",
    "g82c-real-ros2-adapter.md",
    "g82d-editor-lsp.md",
    "g82e-package-registry-publication.md",
    "g83-schema-v2-migration.md",
]

FORBIDDEN = ["pybind11", "Python.h", "find_package(rclcpp", "ament_", "rosidl", "find_package(OpenTelemetry"]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True, type=Path)
    args = parser.parse_args()
    root = args.source_dir
    try:
        ledger = (root / "docs/31-planning-roadmap/conditional-tracks-ledger.md").read_text(encoding="utf-8")
        for blocker in BLOCKERS:
            require(blocker in ledger, f"conditional ledger missing {blocker}")
            text = (root / "docs/31-planning-roadmap/goals/blockers" / blocker).read_text(encoding="utf-8")
            for phrase in ["Status: blocked/deferred.", "## Decision needed", "## Recommendation", "## Required evidence before opening", "## Safe independent work"]:
                require(phrase in text, f"{blocker} missing {phrase}")
        docs_readme = (root / "docs/README.md").read_text(encoding="utf-8")
        require("conditional-tracks-ledger.md" in docs_readme, "docs map must link conditional tracks ledger")
        cmake = (root / "CMakeLists.txt").read_text(encoding="utf-8")
        for token in FORBIDDEN:
            require(token.lower() not in cmake.lower(), f"forbidden conditional implementation token present: {token}")
    except Exception as exc:  # noqa: BLE001
        print(f"conditional tracks check failed: {exc}", file=sys.stderr)
        return 1
    print(json.dumps({"ok": True, "blocked_tracks": len(BLOCKERS)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
