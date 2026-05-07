#!/usr/bin/env python3
"""Validate stable-v0.2 compatibility surfaces without freezing previews."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any

STABLE_DOC_SECTIONS = [
    "## Stability markers",
    "### Stable-v0.2 headers",
    "## CLI JSON Compatibility",
    "## Deprecation policy",
    "## Schema and semantic compatibility",
]

REQUIRED_JSON_FIELDS = {
    "doctor": ["version", "schema_version", "semantic_contract_version", "features", "graph_input_limits"],
    "schema": ["$id", "title", "properties"],
    "metrics": ["ok", "metric_schema_version", "trace_schema_version", "channel_publish_count"],
    "trace": ["ok", "trace_schema_version", "trace_event_count", "trace"],
    "observe": ["observe_schema_version", "final_summary", "event_kind_counts", "graph_validated"],
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def run_json(argv: list[str], *, cwd: Path) -> dict[str, Any]:
    completed = subprocess.run(argv, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    if completed.returncode != 0:
        raise AssertionError(f"command failed ({completed.returncode}): {' '.join(argv)}\n{completed.stderr}\n{completed.stdout}")
    try:
        data = json.loads(completed.stdout)
    except json.JSONDecodeError as exc:
        raise AssertionError(f"command did not emit JSON: {' '.join(argv)}\n{completed.stdout[:500]}") from exc
    require(isinstance(data, dict), f"JSON root must be object for {' '.join(argv)}")
    return data


def check_public_headers(root: Path) -> None:
    public_api = (root / "docs/61-api/public-api.md").read_text(encoding="utf-8")
    for section in STABLE_DOC_SECTIONS:
        require(section in public_api, f"public API doc missing {section}")
    headers = sorted((root / "include/topoexec").rglob("*.hpp")) + sorted((root / "include/topoexec").rglob("*.h"))
    require(headers, "no installed headers found")
    missing_marker: list[str] = []
    missing_doc: list[str] = []
    for header in headers:
        rel = header.relative_to(root / "include").as_posix()
        text = header.read_text(encoding="utf-8", errors="ignore")
        if "API stability:" not in text:
            missing_marker.append(rel)
        if rel not in public_api:
            missing_doc.append(rel)
    require(not missing_marker, "headers missing API stability marker: " + ", ".join(missing_marker))
    require(not missing_doc, "headers missing from public API inventory: " + ", ".join(missing_doc))


def check_docs(root: Path) -> None:
    versioning = (root / "docs/43-ci-build-release-tools/versioning.md").read_text(encoding="utf-8")
    for phrase in ["Stable-v0.2 Embedder Surface", "Deprecation and Removal Policy", "Schema Compatibility", "v0.2.0-alpha.0"]:
        require(phrase in versioning, f"versioning doc missing {phrase}")
    checklist = (root / "docs/61-api/api-change-checklist.md").read_text(encoding="utf-8")
    for phrase in ["CLI JSON removals/renames", "Schema v1 changes", "Runtime-only downstream consumers"]:
        require(phrase in checklist, f"API change checklist missing {phrase}")


def check_cli_json(root: Path, topoexec: Path) -> None:
    commands = {
        "doctor": [str(topoexec), "doctor", "--format", "json"],
        "schema": [str(topoexec), "schema", "dump", "--format", "json"],
        "metrics": [str(topoexec), "graph", "metrics", "examples/minimal.yaml", "--steps", "1", "--format", "json"],
        "trace": [str(topoexec), "graph", "trace", "examples/minimal.yaml", "--steps", "1", "--format", "json"],
        "observe": [str(topoexec), "graph", "observe", "examples/minimal.yaml", "--steps", "3", "--observe-level", "summary", "--format", "json-summary"],
    }
    for name, argv in commands.items():
        data = run_json(argv, cwd=root)
        for field in REQUIRED_JSON_FIELDS[name]:
            require(field in data, f"{name} JSON missing stable field {field}")
    require(run_json(commands["doctor"], cwd=root)["version"] == "0.2.0", "doctor version must match package metadata")


def check_goldens(root: Path) -> None:
    for relative in [
        "tests/golden/doctor.json",
        "tests/golden/schema_dump.json",
        "tests/golden/metrics_minimal.json",
        "tests/golden/trace_minimal.json",
        "tests/golden/trace_minimal_chrome.json",
        "tests/golden/plan_composite_loop.json",
    ]:
        path = root / relative
        require(path.exists(), f"missing golden {relative}")
        json.loads(path.read_text(encoding="utf-8"))
    render = root / "tests/golden/render_minimal.mmd"
    require(render.exists() and any(marker in render.read_text(encoding="utf-8") for marker in ("graph TD", "flowchart TD")), "render golden missing Mermaid graph")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-dir", required=True, type=Path)
    parser.add_argument("--topoexec", required=True, type=Path)
    args = parser.parse_args()
    root = args.source_dir
    try:
        check_public_headers(root)
        check_docs(root)
        check_cli_json(root, args.topoexec)
        check_goldens(root)
    except AssertionError as exc:
        print(f"compatibility contract failed: {exc}", file=sys.stderr)
        return 1
    print(json.dumps({"ok": True, "stable_contract": "v0.2", "checked": ["headers", "docs", "cli_json", "goldens"]}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
