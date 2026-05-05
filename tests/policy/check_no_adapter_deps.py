#!/usr/bin/env python3
"""Ensure core/build sources do not grow adapter SDK dependencies."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

BANNED = (
    "rclcpp",
    "opentelemetry",
    "prometheus",
    "perfetto",
    "pybind11",
    "Python.h",
)

SEARCH_ROOTS = (
    "CMakeLists.txt",
    "include",
    "src",
    "tools",
    "cmake",
)

SKIP_SUFFIXES = (
    ".md",
    ".txt",
)


def iter_files(root: Path):
    for entry in SEARCH_ROOTS:
        path = root / entry
        if path.is_file():
            yield path
            continue
        if path.is_dir():
            for child in path.rglob("*"):
                if child.is_file() and child.suffix not in SKIP_SUFFIXES:
                    yield child


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True, type=Path)
    args = parser.parse_args()

    violations: list[str] = []
    for path in iter_files(args.source_dir):
        text = path.read_text(encoding="utf-8", errors="ignore")
        lowered = text.lower()
        for token in BANNED:
            needle = token.lower()
            if needle in lowered:
                violations.append(f"{path.relative_to(args.source_dir)} contains adapter token {token!r}")

    if violations:
        sys.stderr.write("\n".join(violations) + "\n")
        return 1
    print("ok: no adapter SDK tokens in core/build sources")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
