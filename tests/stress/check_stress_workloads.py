#!/usr/bin/env python3
"""Bounded stress and soak workloads for TopoExec runtime surfaces.

The default profile is intentionally short enough for CTest. The soak profile is
opt-in and repeats bounded-step graph runs for a caller-selected duration.
"""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import sys
import tempfile
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


@dataclass(frozen=True)
class Workload:
    name: str
    graph_text: str
    steps: int
    expected_channel_drops: int = 0
    min_scheduler_rejections: int = 0
    max_component_count: int | None = None


class StressFailure(AssertionError):
    pass


def indented_lines(lines: Iterable[str], spaces: int = 2) -> str:
    prefix = " " * spaces
    return "\n".join(prefix + line if line else line for line in lines)


def input_component(component_id: str, descriptor: str | None = None) -> list[str]:
    descriptor = descriptor or component_id
    return [
        f"- id: {component_id}",
        "  type: topoexec.boundary.Input",
        f"  boundary: {{role: input, descriptor: {descriptor}}}",
        "  event_sources: [{type: manual}]",
        "  trigger_policy: {type: manual}",
        "  execution: {lane: main}",
    ]


def output_component(component_id: str, lane: str = "main") -> list[str]:
    return [
        f"- id: {component_id}",
        "  type: topoexec.boundary.Output",
        f"  boundary: {{role: output, descriptor: {component_id}}}",
        "  event_sources: [{type: message, inputs: [in]}]",
        "  trigger_policy: {type: any_input, inputs: [in]}",
        f"  execution: {{lane: {lane}}}",
    ]


def identity_component(component_id: str, lane: str = "main", *, priority: str | None = None) -> list[str]:
    execution_fields = [f"lane: {lane}"]
    if priority is not None:
        execution_fields.append(f"priority: {priority}")
    return [
        f"- id: {component_id}",
        "  type: topoexec.transforms.Identity",
        "  event_sources: [{type: message, inputs: [in]}]",
        "  trigger_policy: {type: any_input, inputs: [in]}",
        f"  execution: {{{', '.join(execution_fields)}}}",
    ]


def join_component(component_id: str, lane: str = "main") -> list[str]:
    return [
        f"- id: {component_id}",
        "  type: topoexec.transforms.Join",
        "  event_sources: [{type: message, inputs: [left, right]}]",
        "  trigger_policy: {type: time_sync, inputs: [left, right], sync_slop_ms: 5}",
        f"  execution: {{lane: {lane}}}",
    ]


def async_worker_component(component_id: str, lane: str = "main") -> list[str]:
    return [
        f"- id: {component_id}",
        "  type: topoexec.transforms.AsyncWorker",
        "  event_sources: [{type: task_ready, inputs: [ready]}]",
        "  trigger_policy: {type: task_ready, inputs: [ready]}",
        f"  execution: {{lane: {lane}}}",
    ]


def graph_document(name: str, lanes: list[str], components: list[list[str]], edges: list[str]) -> str:
    component_lines: list[str] = []
    for component in components:
        component_lines.extend(component)
    return "\n".join(
        [
            "schema_version: 1",
            f"graph: {{name: {name}, kind: runnable}}",
            "lanes:",
            *indented_lines(lanes, 2).splitlines(),
            "components:",
            *indented_lines(component_lines, 2).splitlines(),
            "edges:",
            *indented_lines(edges, 2).splitlines(),
            "",
        ]
    )


def queue_policy(capacity: int = 8) -> str:
    return f"{{mode: queue, capacity: {capacity}, overflow: drop_oldest, copy_policy: shared_view}}"


def latest_policy() -> str:
    return "{mode: latest, copy_policy: shared_view}"


def high_fan_out(scale: int, steps: int) -> Workload:
    fanout = max(4, scale)
    components = [input_component("source")]
    edges: list[str] = []
    for index in range(fanout):
        sink = f"sink_{index:03d}"
        components.append(output_component(sink))
        edges.append(
            f"- {{id: source_to_{sink}, kind: immediate, from: source.out, to: {sink}.in, policy: {queue_policy(16)}}}"
        )
    return Workload("high_fan_out", graph_document("stress_high_fan_out", ["main: {type: event_loop}"], components, edges), steps)


def high_fan_in(scale: int, steps: int) -> Workload:
    leaf_count = 1 << max(2, math.ceil(math.log2(max(4, scale))))
    components: list[list[str]] = []
    edges: list[str] = []
    current: list[str] = []
    for index in range(leaf_count):
        source = f"source_{index:03d}"
        components.append(input_component(source))
        current.append(source)

    level = 0
    while len(current) > 1:
        next_level: list[str] = []
        for pair_index in range(0, len(current), 2):
            left = current[pair_index]
            right = current[pair_index + 1]
            join = f"join_{level:02d}_{pair_index // 2:03d}"
            components.append(join_component(join))
            edges.append(f"- {{id: {left}_to_{join}_left, kind: immediate, from: {left}.out, to: {join}.left, policy: {queue_policy(16)}}}")
            edges.append(f"- {{id: {right}_to_{join}_right, kind: immediate, from: {right}.out, to: {join}.right, policy: {queue_policy(16)}}}")
            next_level.append(join)
        current = next_level
        level += 1

    components.append(output_component("sink"))
    edges.append(f"- {{id: root_to_sink, kind: immediate, from: {current[0]}.out, to: sink.in, policy: {queue_policy(16)}}}")
    return Workload("high_fan_in", graph_document("stress_high_fan_in", ["main: {type: event_loop}"], components, edges), steps)


def long_chain(scale: int, steps: int) -> Workload:
    length = max(8, scale * 2)
    components = [input_component("source")]
    edges: list[str] = []
    previous = "source"
    for index in range(length):
        node = f"id_{index:03d}"
        components.append(identity_component(node))
        edges.append(f"- {{id: {previous}_to_{node}, kind: immediate, from: {previous}.out, to: {node}.in, policy: {queue_policy(8)}}}")
        previous = node
    components.append(output_component("sink"))
    edges.append(f"- {{id: final_to_sink, kind: immediate, from: {previous}.out, to: sink.in, policy: {queue_policy(16)}}}")
    return Workload("long_chain", graph_document("stress_long_chain", ["main: {type: event_loop}"], components, edges), steps)


def mixed_edges(scale: int, steps: int) -> Workload:
    capacity = max(8, scale)
    components = [
        input_component("source"),
        identity_component("immediate_transform"),
        output_component("immediate_sink"),
        output_component("delay_sink"),
        output_component("state_sink"),
        async_worker_component("async_worker"),
        output_component("async_sink"),
    ]
    edges = [
        f"- {{id: source_to_immediate, kind: immediate, from: source.out, to: immediate_transform.in, policy: {queue_policy(capacity)}}}",
        f"- {{id: immediate_to_sink, kind: immediate, from: immediate_transform.out, to: immediate_sink.in, policy: {queue_policy(capacity)}}}",
        f"- {{id: source_to_delay, kind: delay, from: source.out, to: delay_sink.in, policy: {queue_policy(capacity)}}}",
        f"- {{id: source_to_state, kind: state, from: source.out, to: state_sink.in, policy: {latest_policy()}}}",
        f"- {{id: source_to_async, kind: async, from: source.out, to: async_worker.ready, policy: {{mode: queue, capacity: {capacity}, max_inflight: {capacity}, overflow: drop_oldest, copy_policy: shared_view}}}}",
        f"- {{id: async_to_sink, kind: immediate, from: async_worker.out, to: async_sink.in, policy: {queue_policy(capacity)}}}",
    ]
    mixed_steps = max(2, steps)
    return Workload(
        "mixed_immediate_delay_state_async",
        graph_document("stress_mixed_edges", ["main: {type: event_loop}"], components, edges),
        mixed_steps,
        expected_channel_drops=max(0, mixed_steps - 2),
    )


def thread_pool_bounded_fanout(scale: int, steps: int) -> Workload:
    worker_count = max(8, scale)
    components = [input_component("source")]
    edges: list[str] = []
    for index in range(worker_count):
        worker = f"worker_{index:03d}"
        sink = f"sink_{index:03d}"
        components.append(identity_component(worker, lane="pool", priority="low"))
        components.append(output_component(sink))
        edges.append(f"- {{id: source_to_{worker}, kind: immediate, from: source.out, to: {worker}.in, policy: {queue_policy(worker_count)}}}")
        edges.append(f"- {{id: {worker}_to_{sink}, kind: immediate, from: {worker}.out, to: {sink}.in, policy: {queue_policy(worker_count)}}}")
    return Workload(
        "thread_pool_bounded_fanout",
        graph_document(
            "stress_thread_pool_bounded_fanout",
            ["main: {type: event_loop}", "pool: {type: thread_pool, max_threads: 1, queue_capacity: 2, overflow: drop_newest}"],
            components,
            edges,
        ),
        steps,
    )


def build_workloads(scale: int, steps: int) -> list[Workload]:
    return [
        high_fan_out(scale, steps),
        high_fan_in(scale, steps),
        long_chain(scale, steps),
        mixed_edges(scale, steps),
        thread_pool_bounded_fanout(scale, steps),
    ]


def metric_values(result: dict, name: str, lane: str | None = None) -> list[float]:
    values: list[float] = []
    for metric in result.get("metrics", []):
        if metric.get("name") != name:
            continue
        if lane is not None and metric.get("lane") != lane:
            continue
        try:
            values.append(float(metric.get("value", 0)))
        except (TypeError, ValueError):
            raise StressFailure(f"metric {name} has non-numeric value: {metric!r}") from None
    return values


def metric_sum(result: dict, name: str) -> float:
    return sum(metric_values(result, name))


def metric_by_lane(result: dict, name: str) -> dict[str, float]:
    values: dict[str, float] = {}
    for metric in result.get("metrics", []):
        if metric.get("name") == name:
            values[str(metric.get("lane", ""))] = float(metric.get("value", 0))
    return values


def assert_runtime_ok(workload: Workload, result: dict) -> None:
    if not result.get("ok", False):
        raise StressFailure(f"{workload.name}: runtime returned ok=false: {result.get('errors') or result.get('runtime_errors')}")
    if result.get("errors"):
        raise StressFailure(f"{workload.name}: unexpected errors: {result['errors']}")
    runtime_errors = result.get("runtime_errors") or []
    if runtime_errors:
        raise StressFailure(f"{workload.name}: unexpected runtime_errors: {runtime_errors}")
    if int(result.get("channel_drop_count", 0)) != workload.expected_channel_drops:
        raise StressFailure(
            f"{workload.name}: expected channel_drop_count={workload.expected_channel_drops}, got {result.get('channel_drop_count')}"
        )
    if workload.max_component_count is not None and int(result.get("component_count", 0)) > workload.max_component_count:
        raise StressFailure(f"{workload.name}: component count unexpectedly high: {result.get('component_count')}")

    queue_depths = metric_by_lane(result, "runtime.scheduler.queue_depth")
    queue_capacities = metric_by_lane(result, "runtime.scheduler.queue_capacity")
    for lane, depth in queue_depths.items():
        capacity = queue_capacities.get(lane, 0.0)
        if capacity == 0.0 and depth != 0.0:
            raise StressFailure(f"{workload.name}: lane {lane} queue depth {depth} should be zero without queue capacity")
        if capacity > 0.0 and depth > capacity:
            raise StressFailure(f"{workload.name}: lane {lane} queue depth {depth} exceeded capacity {capacity}")

    rejected = metric_sum(result, "runtime.scheduler.rejected_count")
    if rejected < workload.min_scheduler_rejections:
        raise StressFailure(
            f"{workload.name}: expected at least {workload.min_scheduler_rejections} scheduler rejections, got {rejected}"
        )


def run_topoexec_metrics(topoexec: Path, graph_path: Path, steps: int, timeout: float) -> dict:
    completed = subprocess.run(
        [str(topoexec), "graph", "metrics", str(graph_path), "--steps", str(steps), "--format", "json"],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=timeout,
        check=False,
    )
    if completed.returncode != 0:
        raise StressFailure(
            f"{graph_path.name}: topoexec failed with {completed.returncode}\nstdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )
    try:
        return json.loads(completed.stdout)
    except json.JSONDecodeError as exc:
        raise StressFailure(f"{graph_path.name}: invalid JSON metrics output: {exc}\n{completed.stdout}") from exc


def run_suite(topoexec: Path, workloads: list[Workload], work_dir: Path, timeout: float, iteration: int) -> list[str]:
    passed: list[str] = []
    for workload in workloads:
        graph_path = work_dir / f"{workload.name}-{iteration}.yaml"
        graph_path.write_text(workload.graph_text, encoding="utf-8")
        result = run_topoexec_metrics(topoexec, graph_path, workload.steps, timeout)
        assert_runtime_ok(workload, result)
        passed.append(workload.name)
    return passed


def positive_int(value: str) -> int:
    parsed = int(value)
    if parsed <= 0:
        raise argparse.ArgumentTypeError("value must be positive")
    return parsed


def non_negative_float(value: str) -> float:
    parsed = float(value)
    if parsed < 0:
        raise argparse.ArgumentTypeError("value must be non-negative")
    return parsed


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--topoexec", required=True, type=Path, help="Path to the topoexec CLI executable")
    parser.add_argument("--profile", choices=("smoke", "soak"), default="smoke")
    parser.add_argument("--scale", type=positive_int, help="Approximate graph width/depth scale")
    parser.add_argument("--steps", type=positive_int, help="Runtime steps per graph execution")
    parser.add_argument("--duration-seconds", type=non_negative_float, help="Repeat bounded graph runs until this duration elapses")
    parser.add_argument("--max-iterations", type=positive_int, help="Maximum suite repetitions when duration is set")
    parser.add_argument("--timeout-seconds", type=positive_int, default=30, help="Per topoexec command timeout")
    return parser.parse_args()


def defaults(args: argparse.Namespace) -> tuple[int, int, float, int]:
    if args.profile == "soak":
        duration_seconds = 30.0 if args.duration_seconds is None else args.duration_seconds
        return (
            args.scale or 48,
            args.steps or 64,
            duration_seconds,
            args.max_iterations or max(1, int(duration_seconds * 100.0) + 1),
        )
    return (
        args.scale or 12,
        args.steps or 4,
        0.0 if args.duration_seconds is None else args.duration_seconds,
        args.max_iterations or 1,
    )


def main() -> int:
    args = parse_args()
    topoexec = args.topoexec.resolve()
    if not topoexec.exists():
        raise StressFailure(f"topoexec executable does not exist: {topoexec}")
    if not topoexec.is_file():
        raise StressFailure(f"topoexec path is not a file: {topoexec}")

    scale, steps, duration_seconds, max_iterations = defaults(args)
    workloads = build_workloads(scale, steps)
    started = time.monotonic()
    iterations = 0
    passed: list[str] = []
    with tempfile.TemporaryDirectory(prefix="topoexec-stress-") as tmp:
        work_dir = Path(tmp)
        while iterations < max_iterations:
            iterations += 1
            passed = run_suite(topoexec, workloads, work_dir, float(args.timeout_seconds), iterations)
            if duration_seconds <= 0:
                break
            if time.monotonic() - started >= duration_seconds:
                break
    elapsed = time.monotonic() - started
    print(
        json.dumps(
            {
                "ok": True,
                "profile": args.profile,
                "scale": scale,
                "steps": steps,
                "iterations": iterations,
                "elapsed_seconds": round(elapsed, 3),
                "workloads": passed,
            },
            indent=2,
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (StressFailure, subprocess.TimeoutExpired) as exc:
        print(f"stress check failed: {exc}", file=sys.stderr)
        raise SystemExit(1)
