#!/usr/bin/env python3
"""Smoke-check live observe overhead against a local per-machine baseline.

The default mode is CI-safe: collect repeated timings, validate output/drop
contracts, and print JSON without enforcing global timing thresholds. Set
TOPOEXEC_LIVE_PERF_ENFORCE=1 to enforce conservative per-machine overhead
budgets on this host.
"""
from __future__ import annotations

import argparse
import json
import os
import statistics
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any


@dataclass
class RunResult:
    level: str
    elapsed_ms: float
    command: list[str]
    returncode: int
    stdout: str
    stderr: str


def run_observe(topoexec: Path, graph: Path, steps: int, level: str, capacity: int | None = None) -> RunResult:
    command = [
        str(topoexec),
        "graph",
        "observe",
        str(graph),
        "--steps",
        str(steps),
        "--observe-level",
        level,
        "--format",
        "json-summary",
    ]
    if capacity is not None:
        command.extend(["--event-buffer-capacity", str(capacity)])
    start = time.perf_counter()
    completed = subprocess.run(
        command,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    elapsed_ms = (time.perf_counter() - start) * 1000.0
    return RunResult(level, elapsed_ms, command, completed.returncode, completed.stdout, completed.stderr)


def parse_summary(result: RunResult) -> dict[str, Any]:
    if result.returncode != 0:
        raise RuntimeError(
            f"command failed ({result.returncode}): {' '.join(result.command)}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
    try:
        data = json.loads(result.stdout)
    except json.JSONDecodeError as error:
        raise RuntimeError(f"invalid JSON from {' '.join(result.command)}: {error}\n{result.stdout}") from error
    if not isinstance(data, dict):
        raise RuntimeError("json-summary output must be an object")
    final = data.get("final_summary") if isinstance(data.get("final_summary"), dict) else data
    if final.get("runtime_ok") is not True:
        raise RuntimeError(f"runtime_ok drifted for {result.level}: {data}")
    return final


def median(values: list[float]) -> float:
    return float(statistics.median(values))


def percentile(values: list[float], pct: float) -> float:
    ordered = sorted(values)
    if not ordered:
        return 0.0
    index = min(len(ordered) - 1, max(0, round((pct / 100.0) * (len(ordered) - 1))))
    return float(ordered[index])


def env_float(name: str, default: float) -> float:
    raw = os.getenv(name)
    if raw is None or raw == "":
        return default
    return float(raw)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--topoexec", required=True, type=Path)
    parser.add_argument("--graph", required=True, type=Path)
    parser.add_argument("--steps", type=int, default=int(os.getenv("TOPOEXEC_LIVE_PERF_STEPS", "50")))
    parser.add_argument("--runs", type=int, default=int(os.getenv("TOPOEXEC_LIVE_PERF_RUNS", "5")))
    parser.add_argument("--overflow-capacity", type=int, default=1)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--enforce", action="store_true", default=os.getenv("TOPOEXEC_LIVE_PERF_ENFORCE") == "1")
    args = parser.parse_args(argv)

    if args.steps <= 0 or args.runs <= 0:
        raise SystemExit("--steps and --runs must be positive")
    if not args.topoexec.exists():
        raise SystemExit(f"topoexec binary not found: {args.topoexec}")
    if not args.graph.exists():
        raise SystemExit(f"graph file not found: {args.graph}")

    levels = ["off", "summary", "detailed", "debug"]
    measurements: dict[str, list[float]] = {level: [] for level in levels}
    summaries: dict[str, dict[str, Any]] = {}

    for _ in range(args.runs):
        for level in levels:
            result = run_observe(args.topoexec, args.graph, args.steps, level)
            summaries[level] = parse_summary(result)
            measurements[level].append(result.elapsed_ms)

    overflow = run_observe(args.topoexec, args.graph, args.steps, "summary", args.overflow_capacity)
    overflow_summary = parse_summary(overflow)
    if int(overflow_summary.get("observer_dropped_event_count", 0)) <= 0:
        raise SystemExit("overflow smoke did not report observer_dropped_event_count > 0")

    baseline = median(measurements["off"])
    if baseline <= 0:
        raise SystemExit("disabled baseline median must be positive")

    modes: dict[str, Any] = {}
    for level in levels:
        med = median(measurements[level])
        overhead_percent = ((med - baseline) / baseline) * 100.0
        modes[level] = {
            "runs": args.runs,
            "elapsed_ms": measurements[level],
            "median_elapsed_ms": med,
            "p95_elapsed_ms": percentile(measurements[level], 95.0),
            "overhead_vs_off_percent": overhead_percent,
            "observer_dropped_event_count": summaries[level].get("observer_dropped_event_count", 0),
        }

    thresholds = {
        "off_self_overhead_percent": env_float("TOPOEXEC_LIVE_OFF_OVERHEAD_PERCENT", 0.5),
        "summary_overhead_percent": env_float("TOPOEXEC_LIVE_SUMMARY_OVERHEAD_PERCENT", 2.0),
        "detailed_overhead_percent": env_float("TOPOEXEC_LIVE_DETAILED_OVERHEAD_PERCENT", 5.0),
    }
    report: dict[str, Any] = {
        "live_perf_schema": 1,
        "ok": True,
        "enforced": bool(args.enforce),
        "policy": {
            "default_ci": "smoke_and_output_contract_only",
            "thresholds": "opt-in per-machine; set TOPOEXEC_LIVE_PERF_ENFORCE=1",
            "debug": "intrusive_no_hard_threshold",
        },
        "params": {
            "graph": str(args.graph),
            "steps": args.steps,
            "runs": args.runs,
            "overflow_capacity": args.overflow_capacity,
        },
        "thresholds": thresholds,
        "modes": modes,
        "overflow": {
            "observer_dropped_event_count": overflow_summary.get("observer_dropped_event_count", 0),
            "elapsed_ms": overflow.elapsed_ms,
            "returncode": overflow.returncode,
        },
        "failures": [],
    }

    if args.enforce:
        if abs(modes["off"]["overhead_vs_off_percent"]) > thresholds["off_self_overhead_percent"]:
            report["failures"].append("off baseline self-check exceeded tolerance")
        if modes["summary"]["overhead_vs_off_percent"] > thresholds["summary_overhead_percent"]:
            report["failures"].append("summary overhead exceeded threshold")
        if modes["detailed"]["overhead_vs_off_percent"] > thresholds["detailed_overhead_percent"]:
            report["failures"].append("detailed overhead exceeded threshold")

    report["ok"] = not report["failures"]
    output = json.dumps(report, indent=2, sort_keys=True)
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output + "\n", encoding="utf-8")
    print(output)
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
