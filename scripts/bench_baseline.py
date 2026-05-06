#!/usr/bin/env python3
"""Generate or optionally compare local TopoExec benchmark baselines.

This script intentionally treats regression checks as opt-in and per-machine. CI
should use the benchmark contract smoke instead of global timing thresholds.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import UTC, datetime
from pathlib import Path
from typing import Any

BENCHMARK_CASES = [
    "single_component.yaml",
    "immediate_chain.yaml",
    "fan_out.yaml",
    "fan_in.yaml",
    "latest_vs_queue.yaml",
    "deferred_edges.yaml",
    "thread_pool.yaml",
    "composite_loop_iterations.yaml",
    "payload_policies.yaml",
]


def run_json(command: list[str], cwd: Path, timeout_seconds: float = 30.0) -> dict[str, Any]:
    completed = subprocess.run(
        command,
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout_seconds,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed with exit {completed.returncode}: {' '.join(command)}\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )
    data = json.loads(completed.stdout)
    if not isinstance(data, dict):
        raise RuntimeError(f"command did not return a JSON object: {' '.join(command)}")
    return data


def collect(parsed: argparse.Namespace) -> dict[str, Any]:
    cases: list[dict[str, Any]] = []
    for case_file in BENCHMARK_CASES:
        path = parsed.source_dir / "benchmarks" / case_file
        cases.append(
            run_json(
                [
                    str(parsed.topoexec),
                    "graph",
                    "bench",
                    str(path),
                    "--steps",
                    str(parsed.steps),
                    "--runs",
                    str(parsed.runs),
                    "--format",
                    "json",
                ],
                cwd=parsed.source_dir,
            )
        )

    task_executor: dict[str, Any] | None = None
    if parsed.task_executor_bench is not None:
        task_executor = run_json(
            [
                str(parsed.task_executor_bench),
                "--tasks",
                str(parsed.task_executor_tasks),
                "--runs",
                str(parsed.runs),
                "--format",
                "json",
            ],
            cwd=parsed.source_dir,
        )

    return {
        "benchmark_schema": 2,
        "generated_at_utc": datetime.now(UTC).replace(microsecond=0).isoformat(),
        "policy": {
            "regression_thresholds": "opt-in per-machine only",
            "default_ci": "output_contract_only",
            "comparison_metric": "p95_run_elapsed_ms",
        },
        "params": {
            "steps": parsed.steps,
            "runs": parsed.runs,
            "task_executor_tasks": parsed.task_executor_tasks if task_executor is not None else None,
        },
        "cases": cases,
        "task_executor": task_executor,
    }


def case_map(baseline: dict[str, Any]) -> dict[str, dict[str, Any]]:
    mapped: dict[str, dict[str, Any]] = {}
    for case in baseline.get("cases", []):
        if isinstance(case, dict) and isinstance(case.get("case"), str):
            mapped[case["case"]] = case
    if isinstance(baseline.get("task_executor"), dict):
        mapped["task_executor"] = baseline["task_executor"]
    return mapped


def metric_for(case: dict[str, Any]) -> float:
    if case.get("case") == "task_executor":
        threaded = case.get("threaded", {})
        return float(threaded.get("p95_run_elapsed_ms", 0.0))
    return float(case.get("p95_run_elapsed_ms", 0.0))


def compare(current: dict[str, Any], baseline: dict[str, Any], threshold_percent: float) -> list[str]:
    allowed_factor = 1.0 + (threshold_percent / 100.0)
    current_cases = case_map(current)
    baseline_cases = case_map(baseline)
    failures: list[str] = []
    for name, current_case in sorted(current_cases.items()):
        baseline_case = baseline_cases.get(name)
        if baseline_case is None:
            failures.append(f"missing baseline case: {name}")
            continue
        current_metric = metric_for(current_case)
        baseline_metric = metric_for(baseline_case)
        allowed = baseline_metric * allowed_factor
        if current_metric > allowed:
            failures.append(
                f"{name} p95_run_elapsed_ms {current_metric:.6f} exceeded baseline "
                f"{baseline_metric:.6f} by threshold {threshold_percent:.2f}%"
            )
    return failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--topoexec", required=True, type=Path)
    parser.add_argument("--source-dir", required=True, type=Path)
    parser.add_argument("--task-executor-bench", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--runs", type=int, default=5)
    parser.add_argument("--steps", type=int, default=10)
    parser.add_argument("--task-executor-tasks", type=int, default=32)
    parser.add_argument("--baseline-in", type=Path)
    parser.add_argument("--threshold-percent", type=float)
    parsed = parser.parse_args()

    if parsed.runs <= 0 or parsed.steps <= 0 or parsed.task_executor_tasks <= 0:
        raise SystemExit("--runs, --steps, and --task-executor-tasks must be positive")

    current = collect(parsed)
    parsed.output.parent.mkdir(parents=True, exist_ok=True)
    parsed.output.write_text(json.dumps(current, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"wrote local benchmark baseline: {parsed.output}")

    if parsed.baseline_in is None or parsed.threshold_percent is None:
        print("no regression threshold applied; timing comparison is opt-in and per-machine")
        return 0

    baseline = json.loads(parsed.baseline_in.read_text(encoding="utf-8"))
    failures = compare(current, baseline, parsed.threshold_percent)
    for failure in failures:
        print(failure, file=sys.stderr)
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
