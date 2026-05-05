#!/usr/bin/env python3
"""Cheap schema contract smoke without adding a JSON Schema dependency."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--schema", required=True, type=Path)
    parser.add_argument("--topoexec", required=True, type=Path)
    parser.add_argument("--source-dir", required=True, type=Path)
    args = parser.parse_args()

    schema = json.loads(args.schema.read_text(encoding="utf-8"))
    require(schema.get("x-topoexec-semantic_contract_version") == "0.2",
            "semantic contract version annotation drifted")
    require(schema.get("additionalProperties") is False, "root must be strict")
    require(schema.get("required") == ["schema_version", "graph", "lanes", "components", "edges"],
            "root required fields drifted")
    defs = schema["$defs"]
    require(defs["edge"]["additionalProperties"] is False, "edge schema must be strict")
    require(schema["properties"]["lanes"]["maxProperties"] == 256, "lane count limit drifted")
    require(schema["properties"]["components"]["maxItems"] == 4096, "component count limit drifted")
    require(schema["properties"]["edges"]["maxItems"] == 8192, "edge count limit drifted")
    require(schema["properties"]["composite_loops"]["maxItems"] == 1024, "loop count limit drifted")
    require(defs["id"]["maxLength"] == 128, "id length limit drifted")
    require(defs["edge"]["properties"]["kind"]["enum"] == ["immediate", "delay", "state", "async"],
            "edge kind enum drifted")
    require("thread_pool" in defs["lane"]["properties"]["type"]["enum"], "thread_pool lane missing")
    require("time_sync" in defs["trigger_policy"]["properties"]["type"]["enum"], "time_sync trigger missing")
    require(defs["execution"]["properties"]["on_error"]["enum"] == ["fail_fast", "continue", "isolate"],
            "execution.on_error enum drifted")
    require("loaned_view" in defs["edge_policy"]["properties"]["copy_policy"]["enum"],
            "loaned_view copy policy missing")

    valid_examples = [
        "examples/minimal.yaml",
        "examples/control_feedback_delay.yaml",
        "examples/composite_loop.yaml",
        "examples/large_payload_copy.yaml",
        "examples/state_config_snapshot.yaml",
        "examples/batch_time_sync.yaml",
        "examples/service_pipeline.yaml",
        "examples/boundary_adapter_pattern.yaml",
    ]
    for example in valid_examples:
        completed = subprocess.run(
            [str(args.topoexec), "graph", "validate", example],
            cwd=args.source_dir,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if completed.returncode != 0:
            sys.stderr.write(f"{example} failed validation\n{completed.stdout}\n{completed.stderr}\n")
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
