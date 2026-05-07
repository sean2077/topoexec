#!/usr/bin/env python3
"""Check that channel drop and overwrite aggregates keep distinct semantics."""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path
from typing import Any


def run_json(argv: list[str], cwd: Path) -> dict[str, Any]:
    completed = subprocess.run(argv, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    if completed.returncode != 0:
        raise AssertionError(
            f"command failed ({completed.returncode}): {' '.join(argv)}\n{completed.stderr}\n{completed.stdout}"
        )
    data = json.loads(completed.stdout)
    if not isinstance(data, dict):
        raise AssertionError("expected JSON object")
    return data


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--topoexec", required=True, type=Path)
    parser.add_argument("--source-dir", required=True, type=Path)
    args = parser.parse_args()

    result = run_json(
        [
            str(args.topoexec),
            "graph",
            "run",
            "examples/40-composite-loop/composite_loop_solver.yaml",
            "--steps",
            "20",
            "--format",
            "json",
        ],
        cwd=args.source_dir,
    )

    total = int(result.get("channel_drop_count", 0))
    overwrite = int(result.get("channel_overwrite_count", 0))
    reject = int(result.get("channel_reject_count", 0))
    stale = int(result.get("channel_stale_drop_count", 0))

    require(total == 0, f"expected composite example drop total 0, got {total}")
    require(overwrite == 173, f"expected composite example overwrites 173, got {overwrite}")
    require(reject == 0, f"unexpected channel rejects: {reject}")
    require(stale == 0, f"unexpected stale drops: {stale}")
    require(result.get("channel_deadline_miss_count") == 0, "unexpected deadline miss count")

    metrics = result.get("metrics", [])
    by_channel = {
        sample.get("channel_id"): sample.get("value")
        for sample in metrics
        if sample.get("name") == "runtime.channel.overwrite_count"
    }
    require(by_channel.get("estimator_to_controller") == 78.0, "missing estimator latest overwrite evidence")
    require(by_channel.get("controller_to_actuator") == 18.0, "missing actuator queue overwrite/drop-oldest evidence")
    print(json.dumps({"ok": True, "channel_drop_count": total, "channel_overwrite_count": overwrite}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
