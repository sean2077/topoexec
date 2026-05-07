#!/usr/bin/env python3
"""Smoke-check TopoExec observe NDJSON schema v1 output."""
from __future__ import annotations

import json
import sys
from pathlib import Path

REQUIRED_EVENT_FIELDS = {
    "observe_schema_version",
    "run_id",
    "display_seq",
    "stream_id",
    "local_seq",
    "kind",
    "severity",
    "exactness",
    "mono_ns",
}
EXACTNESS = {"exact", "aggregated", "sampled", "lossy", "partial"}


def fail(message: str) -> int:
    print(f"observe schema error: {message}", file=sys.stderr)
    return 1


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        return fail("usage: check_observe_schema.py <observe.ndjson>")
    path = Path(argv[1])
    records = []
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        if not line.strip():
            continue
        try:
            record = json.loads(line)
        except json.JSONDecodeError as exc:
            return fail(f"line {number} is not JSON: {exc}")
        if record.get("observe_schema_version") != "1":
            return fail(f"line {number} missing observe_schema_version=1")
        records.append(record)
    if not records:
        return fail("no records")
    if records[0].get("kind") != "symbol_table":
        return fail("first record must be symbol_table")
    kinds = {record.get("kind") for record in records}
    for expected in ("graph_validated", "plan_ready", "run_started", "run_finished", "final_summary"):
        if expected not in kinds:
            return fail(f"missing {expected}")
    for record in records:
        if "display_seq" not in record:
            continue
        missing = REQUIRED_EVENT_FIELDS.difference(record)
        if missing:
            return fail(f"event missing fields: {sorted(missing)}")
        if record["exactness"] not in EXACTNESS:
            return fail(f"invalid exactness: {record['exactness']}")
    final = records[-1]
    if final.get("kind") != "final_summary" or final.get("runtime_ok") is not True:
        return fail("final summary must be last and runtime_ok=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
