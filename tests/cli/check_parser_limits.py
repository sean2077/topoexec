#!/usr/bin/env python3
"""Check CLI parser-limit and error-path failures stay bounded and clear."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def run(args: list[str], *, cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        args,
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def require_fails_with(completed: subprocess.CompletedProcess[str], expected: str, label: str) -> None:
    if completed.returncode == 0:
        raise AssertionError(f"{label} unexpectedly passed")
    combined = completed.stdout + completed.stderr
    if expected not in combined:
        raise AssertionError(f"{label} missing expected diagnostic {expected!r}\n{combined}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--topoexec", required=True, type=Path)
    parser.add_argument("--source-dir", required=True, type=Path)
    args = parser.parse_args()

    completed = run(
        [
            str(args.topoexec),
            "graph",
            "validate",
            str(args.source_dir / "examples" / "minimal.yaml"),
            "--max-graph-input-bytes",
            "64",
            "--format",
            "json",
        ],
    )
    if completed.returncode == 0:
        sys.stderr.write("parser-limit override unexpectedly passed\n")
        sys.stderr.write(completed.stdout + completed.stderr)
        return 1
    try:
        payload = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        sys.stderr.write(f"expected JSON stdout, got decode error: {error}\n")
        sys.stderr.write(completed.stdout + completed.stderr)
        return 1
    errors = payload.get("errors", [])
    if payload.get("ok") is not False or not any("graph input size exceeds limit 64" in item for item in errors):
        sys.stderr.write("parser-limit override did not report the expected diagnostic\n")
        sys.stderr.write(json.dumps(payload, indent=2) + "\n")
        sys.stderr.write(completed.stderr)
        return 1
    print("ok parser limit override rc=%s" % completed.returncode)

    malformed = tempfile.NamedTemporaryFile("w", suffix=".yaml", encoding="utf-8", delete=False)
    try:
        with malformed:
            malformed.write("{not: yaml: [\n")
        completed = run(
            [
                str(args.topoexec),
                "graph",
                "validate",
                malformed.name,
                "--format",
                "json",
            ]
        )
        if completed.returncode == 0:
            sys.stderr.write("malformed YAML unexpectedly passed\n")
            sys.stderr.write(completed.stdout + completed.stderr)
            return 1
        payload = json.loads(completed.stdout)
        if payload.get("ok") is not False or not payload.get("errors"):
            sys.stderr.write("malformed YAML did not produce machine-readable errors\n")
            sys.stderr.write(completed.stdout + completed.stderr)
            return 1
    finally:
        Path(malformed.name).unlink(missing_ok=True)
    print("ok malformed YAML json error rc=%s" % completed.returncode)

    minimal = str(args.source_dir / "examples" / "minimal.yaml")
    error_cases = [
        (
            [
                str(args.topoexec),
                "graph",
                "run",
                minimal,
                "--format",
                "xml",
            ],
            "--format: xml not in",
            "invalid run format",
        ),
        (
            [
                str(args.topoexec),
                "graph",
                "run",
                minimal,
                "--steps",
                "-1",
            ],
            "Could not convert: --steps = -1",
            "negative run steps",
        ),
        (
            [
                str(args.topoexec),
                "graph",
                "validate",
                minimal,
                "--schema-only",
                "--semantic",
            ],
            "--schema-only and --semantic are mutually exclusive",
            "mutually exclusive validation modes",
        ),
        (
            [
                str(args.topoexec),
                "graph",
                "observe",
                minimal,
                "--payload-preview-bytes",
                "16",
                "--format",
                "json-summary",
            ],
            "--payload-preview-bytes requires --observe-level debug",
            "observe preview without debug level",
        ),
        (
            [
                str(args.topoexec),
                "graph",
                "observe",
                minimal,
                "--sample-event",
                "component_begin:0",
                "--format",
                "json-summary",
            ],
            "--sample-event ratio must be positive",
            "observe invalid sampling ratio",
        ),
    ]
    for command, expected, label in error_cases:
        require_fails_with(run(command, cwd=args.source_dir), expected, label)
        print(f"ok {label}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
