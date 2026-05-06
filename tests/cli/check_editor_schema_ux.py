#!/usr/bin/env python3
"""Verify editor-facing schema discovery and diagnostic JSON fields."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


def run_json(argv: list[str], *, cwd: Path, expect_success: bool = True) -> dict:
    completed = subprocess.run(argv, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    if expect_success and completed.returncode != 0:
        sys.stderr.write(f"command failed: {' '.join(argv)}\n")
        sys.stderr.write(completed.stdout)
        sys.stderr.write(completed.stderr)
        raise SystemExit(completed.returncode)
    if not expect_success and completed.returncode == 0:
        sys.stderr.write(f"command unexpectedly passed: {' '.join(argv)}\n")
        sys.stderr.write(completed.stdout)
        sys.stderr.write(completed.stderr)
        raise SystemExit(1)
    return json.loads(completed.stdout)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--topoexec", required=True, type=Path)
    parser.add_argument("--source-dir", required=True, type=Path)
    args = parser.parse_args()

    doctor = run_json([str(args.topoexec), "doctor", "--format", "json"], cwd=args.source_dir)
    require(doctor.get("schema_found") is True, "doctor must report schema_found")
    require(str(doctor.get("schema_path", "")).endswith("schema/topoexec.schema.v1.json"),
            "doctor schema_path must point at topoexec.schema.v1.json")

    schema = run_json([str(args.topoexec), "schema", "dump", "--format", "json"], cwd=args.source_dir)
    require(schema.get("$id") == "https://topoexec.dev/schema/topoexec.schema.v1.json",
            "schema dump id drifted")
    require(schema.get("properties", {}).get("schema_version") == {"const": 1},
            "schema dump must remain schema_version const 1")

    validation = run_json(
        [str(args.topoexec), "graph", "validate", "examples/diagnostic_warnings.yaml", "--format", "json"],
        cwd=args.source_dir,
    )
    require(validation.get("diagnostics_schema_version") == "1", "diagnostics schema version drifted")
    diagnostics = validation.get("diagnostics")
    require(isinstance(diagnostics, list) and diagnostics, "expected editor diagnostics")
    for diagnostic in diagnostics:
        for field in ["code", "severity", "category", "graph_path", "message", "suggested_fix"]:
            require(isinstance(diagnostic.get(field), str) and diagnostic[field], f"diagnostic missing {field}")
        require(isinstance(diagnostic.get("involved_components"), list), "diagnostic missing involved_components")
        require(isinstance(diagnostic.get("involved_edges"), list), "diagnostic missing involved_edges")
    capacity = next((item for item in diagnostics if item.get("code") == "high_queue_depth_latency_risk"), None)
    require(capacity is not None, "missing high_queue_depth_latency_risk diagnostic")
    require(capacity.get("graph_path") == "edges.deep_blocking_queue.policy.capacity",
            "capacity diagnostic path drifted")
    require("Reduce capacity" in capacity.get("suggested_fix", ""), "capacity suggested fix drifted")

    schema_check = run_json(
        [str(args.topoexec), "schema", "check", "examples/invalid_unknown_field.yaml", "--format", "json"],
        cwd=args.source_dir,
        expect_success=False,
    )
    require(schema_check.get("ok") is False, "invalid schema check should fail")
    require(any("runtime graph.graph_version" in error for error in schema_check.get("errors", [])),
            "schema check error should carry the offending path")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
