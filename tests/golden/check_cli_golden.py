#!/usr/bin/env python3
"""Run stable CLI golden checks for TopoExec graph tooling.

The CLI intentionally emits runtime-specific durations, offsets and trace ids.
This checker normalizes those volatile fields while preserving semantic fields so
plan/metrics/trace/Chrome-trace/render/schema/doctor drift fails in CTest.
"""

from __future__ import annotations

import argparse
import difflib
import json
import subprocess
import sys
from pathlib import Path
from typing import Any

VOLATILE_JSON_KEYS = {"trace_id", "start_offset_ns", "duration_ns", "ts", "dur"}
VOLATILE_METRIC_SUFFIXES = ("duration_ns", "duration_ms", "latency_ms", "message_age_ms", "jitter_ms")


def normalize_json(value: Any) -> Any:
    if isinstance(value, dict):
        normalized: dict[str, Any] = {}
        metric_name = value.get("name")
        for key, child in value.items():
            if key == "value" and isinstance(metric_name, str) and metric_name.endswith(VOLATILE_METRIC_SUFFIXES):
                normalized[key] = 0.0
            elif key in VOLATILE_JSON_KEYS:
                normalized[key] = "<volatile>" if key == "trace_id" else 0
            else:
                normalized[key] = normalize_json(child)
        return normalized
    if isinstance(value, list):
        return [normalize_json(child) for child in value]
    return value


def normalize_text(output: str, fmt: str) -> str:
    if fmt == "json":
        data = json.loads(output)
        return json.dumps(normalize_json(data), indent=2, sort_keys=True) + "\n"
    return output if output.endswith("\n") else output + "\n"


def run_case(topoexec: Path, source_dir: Path, args: list[str], fmt: str) -> str:
    completed = subprocess.run(
        [str(topoexec), *args],
        cwd=source_dir,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed with exit {completed.returncode}: {' '.join([str(topoexec), *args])}\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )
    return normalize_text(completed.stdout, fmt)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--topoexec", required=True, type=Path)
    parser.add_argument("--source-dir", required=True, type=Path)
    parser.add_argument("--golden-dir", required=True, type=Path)
    parser.add_argument("--update", action="store_true")
    parsed = parser.parse_args()

    cases = [
        (
            "metrics_minimal.json",
            ["graph", "metrics", "examples/minimal.yaml", "--steps", "1", "--format", "json"],
            "json",
        ),
        (
            "trace_minimal.json",
            ["graph", "trace", "examples/minimal.yaml", "--steps", "1", "--format", "json"],
            "json",
        ),
        (
            "trace_minimal_chrome.json",
            ["graph", "trace", "examples/minimal.yaml", "--steps", "1", "--format", "chrome"],
            "json",
        ),
        (
            "plan_composite_loop.json",
            ["graph", "plan", "examples/composite_loop.yaml", "--format", "json"],
            "json",
        ),
        (
            "schema_dump.json",
            ["schema", "dump", "--format", "json"],
            "json",
        ),
        (
            "doctor.json",
            ["doctor", "--format", "json"],
            "json",
        ),
        (
            "render_minimal.mmd",
            ["graph", "render", "examples/minimal.yaml", "--format", "mermaid"],
            "text",
        ),
    ]

    failures = 0
    parsed.golden_dir.mkdir(parents=True, exist_ok=True)
    for filename, args, fmt in cases:
        actual = run_case(parsed.topoexec, parsed.source_dir, args, fmt)
        golden_path = parsed.golden_dir / filename
        if parsed.update:
            golden_path.write_text(actual, encoding="utf-8")
            continue
        if not golden_path.exists():
            print(f"missing golden file: {golden_path}", file=sys.stderr)
            failures += 1
            continue
        expected = golden_path.read_text(encoding="utf-8")
        if actual != expected:
            print(f"golden mismatch: {filename}", file=sys.stderr)
            print(
                "".join(
                    difflib.unified_diff(
                        expected.splitlines(keepends=True),
                        actual.splitlines(keepends=True),
                        fromfile=f"expected/{filename}",
                        tofile=f"actual/{filename}",
                    )
                ),
                file=sys.stderr,
            )
            failures += 1
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
