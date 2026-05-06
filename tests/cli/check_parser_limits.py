#!/usr/bin/env python3
"""Check CLI parser-limit failures stay bounded and machine-readable."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--topoexec", required=True, type=Path)
    parser.add_argument("--source-dir", required=True, type=Path)
    args = parser.parse_args()

    completed = subprocess.run(
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
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
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
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
