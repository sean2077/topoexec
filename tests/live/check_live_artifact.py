#!/usr/bin/env python3
from __future__ import annotations

import json
import sys
from pathlib import Path

REQUIRED = [
    "manifest.json",
    "graph.yaml",
    "graph.normalized.json",
    "plan.json",
    "render.mmd",
    "observe.ndjson",
    "observe.summary.json",
    "assertions.yaml",
    "assertion_result.json",
    "metrics.final.json",
    "trace.final.json",
    "trace.chrome.json",
    "health.final.json",
    "dashboard.html",
]


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check_live_artifact.py <artifact-dir>", file=sys.stderr)
        return 1
    root = Path(sys.argv[1])
    missing = [name for name in REQUIRED if not (root / name).exists()]
    if missing:
        print(f"missing artifact files: {missing}", file=sys.stderr)
        return 1
    manifest = json.loads((root / "manifest.json").read_text(encoding="utf-8"))
    assert manifest["artifact_schema_version"] == "1"
    assert manifest["observe_schema_version"] == "1"
    assert manifest["summary"]["runtime_ok"] is True
    records = [json.loads(line) for line in (root / "observe.ndjson").read_text(encoding="utf-8").splitlines() if line.strip()]
    assert records[0]["kind"] == "symbol_table"
    assert records[-1]["kind"] == "final_summary"
    assert json.loads((root / "assertion_result.json").read_text(encoding="utf-8"))["ok"] is True
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
