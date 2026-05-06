#!/usr/bin/env python3
"""Cheap schema contract smoke without adding a JSON Schema dependency."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
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
    require(schema["properties"]["schema_version"] == {"const": 1},
            "schema_version must remain v1-only until a reviewed v2 loader exists")
    require(schema.get("required") == ["schema_version", "graph", "lanes", "components", "edges"],
            "root required fields drifted")
    defs = schema["$defs"]
    require(defs["edge"]["additionalProperties"] is False, "edge schema must be strict")
    require(schema["properties"]["lanes"]["maxProperties"] == 256, "lane count limit drifted")
    require(schema["properties"]["components"]["maxItems"] == 4096, "component count limit drifted")
    require(schema["properties"]["edges"]["maxItems"] == 8192, "edge count limit drifted")
    require(schema["properties"]["composite_loops"]["maxItems"] == 1024, "loop count limit drifted")
    require(schema["properties"]["subgraphs"]["maxItems"] == 4096, "subgraph count limit drifted")
    require(schema["properties"]["templates"]["maxItems"] == 4096, "template count limit drifted")
    require(schema["properties"]["template_instances"]["maxItems"] == 4096,
            "template instance count limit drifted")
    require(defs["id"]["maxLength"] == 128, "id length limit drifted")
    require(defs["endpoint"]["maxLength"] == 4096, "endpoint string length limit drifted")
    require(defs["string_array"]["items"]["maxLength"] == 4096, "string array limit drifted")
    require(defs["component"]["properties"]["type"]["maxLength"] == 4096,
            "component type string limit drifted")
    require(defs["subgraph"]["additionalProperties"] is False, "subgraph schema must be strict")
    require(defs["subgraph"]["required"] == ["id", "components", "edges"],
            "subgraph required fields drifted")
    require(defs["subgraph"]["properties"]["components"]["minItems"] == 1,
            "subgraph components must stay non-empty")
    require(defs["graph_template"]["additionalProperties"] is False, "graph template schema must be strict")
    require(defs["graph_template"]["required"] == ["id", "components", "edges"],
            "graph template required fields drifted")
    require(defs["template_instance"]["additionalProperties"] is False,
            "template instance schema must be strict")
    require(defs["template_instance"]["required"] == ["id", "template"],
            "template instance required fields drifted")
    require(defs["edge"]["properties"]["kind"]["enum"] == ["immediate", "delay", "state", "async"],
            "edge kind enum drifted")
    require("thread_pool" in defs["lane"]["properties"]["type"]["enum"], "thread_pool lane missing")
    require("time_sync" in defs["trigger_policy"]["properties"]["type"]["enum"], "time_sync trigger missing")
    require("solver_iteration" in defs["loop_policy"]["properties"]["type"]["enum"],
            "solver_iteration loop policy missing")
    require(defs["loop_policy"]["properties"]["residual_threshold"]["minimum"] == 0,
            "loop residual threshold minimum drifted")
    require(defs["loop_policy"]["properties"]["partial_success"]["enum"] ==
            ["commit_outputs", "discard_outputs", "fail_run"],
            "loop partial_success enum drifted")
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
        "examples/template_source_transform_sink.yaml",
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

    with tempfile.NamedTemporaryFile("w", suffix=".yaml", encoding="utf-8", delete=False) as handle:
        handle.write(
            "schema_version: 2\n"
            "graph: {name: future_v2_sketch, kind: runnable}\n"
            "lanes: {main: {type: event_loop}}\n"
            "components: []\n"
            "edges: []\n"
        )
        v2_sketch = Path(handle.name)
    try:
        completed = subprocess.run(
            [str(args.topoexec), "schema", "check", str(v2_sketch), "--format", "json"],
            cwd=args.source_dir,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if completed.returncode == 0:
            sys.stderr.write("schema_version: 2 sketch was accepted by the v1 schema checker\n")
            sys.stderr.write(completed.stdout)
            sys.stderr.write(completed.stderr)
            return 1
    finally:
        v2_sketch.unlink(missing_ok=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
