#!/usr/bin/env python3
"""Validate benchmark output contracts without enforcing timing thresholds."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
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

ENVIRONMENT_KEYS = {
    "benchmark_schema",
    "clock",
    "runtime",
    "compiler",
    "compiler_version",
    "cpp_standard",
    "build_type",
    "cpu_model",
    "cpu_threads",
    "commit",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def run_json(command: list[str], cwd: Path, timeout_seconds: float = 15.0) -> dict[str, Any]:
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
    try:
        data = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        raise RuntimeError(f"invalid JSON from {' '.join(command)}: {error}\n{completed.stdout}") from error
    require(isinstance(data, dict), "benchmark output must be a JSON object")
    return data


def assert_number(data: dict[str, Any], key: str) -> None:
    require(key in data, f"missing {key}")
    require(isinstance(data[key], (int, float)), f"{key} must be numeric")


def check_cli_case(topoexec: Path, source_dir: Path, case_file: str) -> None:
    path = source_dir / "benchmarks" / case_file
    data = run_json(
        [str(topoexec), "graph", "bench", str(path), "--steps", "2", "--runs", "2", "--format", "json"],
        cwd=source_dir,
    )
    expected_case = path.stem
    require(data.get("ok") is True, f"{case_file} did not report ok")
    require(data.get("case") == expected_case, f"{case_file} case name drifted")
    require(data.get("runs") == 2, f"{case_file} runs drifted")
    require(data.get("ok_runs") == 2, f"{case_file} ok_runs drifted")
    require(data.get("steps") == 2, f"{case_file} steps drifted")
    require(isinstance(data.get("graph_hash"), str) and data["graph_hash"].startswith("fnv1a64:"),
            f"{case_file} graph_hash missing")
    require(data.get("params", {}).get("file") == str(path), f"{case_file} params.file drifted")
    require(isinstance(data.get("run_elapsed_ms"), list) and len(data["run_elapsed_ms"]) == 2,
            f"{case_file} run_elapsed_ms shape drifted")
    for key in [
        "tick_calls",
        "elapsed_ms",
        "p50_run_elapsed_ms",
        "p95_run_elapsed_ms",
        "p99_run_elapsed_ms",
        "throughput_tick_calls_per_sec",
        "throughput_runs_per_sec",
    ]:
        assert_number(data, key)
    require(isinstance(data.get("errors"), list) and not data["errors"], f"{case_file} errors not empty")

    environment = data.get("environment")
    require(isinstance(environment, dict), f"{case_file} environment missing")
    missing = sorted(ENVIRONMENT_KEYS - set(environment))
    require(not missing, f"{case_file} environment keys missing: {missing}")
    require(environment["benchmark_schema"] == 2, f"{case_file} schema version drifted")
    require(environment["clock"] == "steady_clock", f"{case_file} clock drifted")
    require(environment["runtime"] == "RuntimeRunner", f"{case_file} runtime drifted")
    require(isinstance(environment["compiler"], str) and environment["compiler"], f"{case_file} compiler missing")
    require(isinstance(environment["compiler_version"], str) and environment["compiler_version"],
            f"{case_file} compiler_version missing")
    require(isinstance(environment["cpp_standard"], str) and environment["cpp_standard"],
            f"{case_file} cpp_standard missing")
    require(isinstance(environment["build_type"], str) and environment["build_type"],
            f"{case_file} build_type missing")
    require(isinstance(environment["cpu_model"], str) and environment["cpu_model"],
            f"{case_file} cpu_model missing")
    require(isinstance(environment["cpu_threads"], int), f"{case_file} cpu_threads missing")
    require(isinstance(environment["commit"], str) and environment["commit"], f"{case_file} commit missing")


def check_task_executor(path: Path, source_dir: Path) -> None:
    data = run_json([str(path), "--tasks", "8", "--runs", "2", "--format", "json"], cwd=source_dir)
    require(data.get("ok") is True, "task_executor did not report ok")
    require(data.get("case") == "task_executor", "task_executor case drifted")
    require(data.get("benchmark_schema") == 2, "task_executor schema drifted")
    require(data.get("params", {}).get("tasks") == 8, "task_executor tasks drifted")
    require(data.get("params", {}).get("runs") == 2, "task_executor runs drifted")
    for section_name in ["deterministic", "threaded"]:
        section = data.get(section_name)
        require(isinstance(section, dict), f"task_executor {section_name} missing")
        require(section.get("completed") == 16, f"task_executor {section_name} completed drifted")
        require(section.get("rejected") == 0, f"task_executor {section_name} rejected drifted")
        require(section.get("failed") == 0, f"task_executor {section_name} failed drifted")
        require(isinstance(section.get("p50_run_elapsed_ms"), (int, float)),
                f"task_executor {section_name} p50 missing")
        require(isinstance(section.get("p95_run_elapsed_ms"), (int, float)),
                f"task_executor {section_name} p95 missing")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--topoexec", required=True, type=Path)
    parser.add_argument("--source-dir", required=True, type=Path)
    parser.add_argument("--task-executor-bench", required=True, type=Path)
    parsed = parser.parse_args()

    failures = 0
    for case_file in BENCHMARK_CASES:
        try:
            check_cli_case(parsed.topoexec, parsed.source_dir, case_file)
        except Exception as error:  # noqa: BLE001 - CTest should print every case failure.
            print(f"bench contract failed for {case_file}: {error}", file=sys.stderr)
            failures += 1
    try:
        check_task_executor(parsed.task_executor_bench, parsed.source_dir)
    except Exception as error:  # noqa: BLE001
        print(f"bench contract failed for task_executor: {error}", file=sys.stderr)
        failures += 1
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
