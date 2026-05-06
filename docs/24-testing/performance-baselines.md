# Performance Baselines

TopoExec keeps benchmark support lightweight and local-first. The benchmark
suite helps engineers compare runtime paths on the same machine and build
profile; it is not a source of global throughput or latency claims.

Run one graph case:

```bash
topoexec graph bench benchmarks/immediate_chain.yaml --steps 10 --runs 20 --format json
```

Generate a local baseline file:

```bash
./scripts/bench_baseline.sh
```

By default the script writes `benchmarks/local-baseline.json`, which is ignored
by git. Override it with `TOPOEXEC_BENCH_BASELINE_OUTPUT=/path/to/file.json`.
The script builds the CLI and the non-installed task-executor benchmark before
collecting results.

## Benchmark schema v2

Graph benchmark JSON uses schema version 2:

```json
{
  "ok": true,
  "case": "immediate_chain",
  "runs": 20,
  "ok_runs": 20,
  "steps": 10,
  "params": {
    "file": "benchmarks/immediate_chain.yaml",
    "steps": 10,
    "runs": 20
  },
  "graph_hash": "fnv1a64:<hash>",
  "tick_calls": 800,
  "elapsed_ms": 12,
  "run_elapsed_ms": [0.6, 0.5],
  "p50_run_elapsed_ms": 0.55,
  "p95_run_elapsed_ms": 0.595,
  "p99_run_elapsed_ms": 0.599,
  "throughput_tick_calls_per_sec": 64000.0,
  "throughput_runs_per_sec": 1600.0,
  "environment": {
    "benchmark_schema": 2,
    "clock": "steady_clock",
    "runtime": "RuntimeRunner",
    "compiler": "gcc",
    "compiler_version": "13.3.0",
    "cpp_standard": "202002",
    "build_type": "RelWithDebInfo",
    "cpu_model": "example cpu",
    "cpu_threads": 20,
    "commit": "abcdef123456"
  },
  "errors": []
}
```

Numbers above are illustrative only.

## Benchmark cases

The deterministic graph cases live in [`benchmarks/`](../../benchmarks/):

| Case | Runtime path covered |
| --- | --- |
| `single_component.yaml` | single manually-triggered component dispatch overhead |
| `immediate_chain.yaml` | immediate chain length and transaction commit overhead |
| `fan_out.yaml` | multi-reader fan-out through queue/latest channels |
| `fan_in.yaml` | two-source fan-in into an `all_inputs` join |
| `latest_vs_queue.yaml` | latest and bounded queue channel policies |
| `deferred_edges.yaml` | delay, state snapshot, and async admission epoch boundaries |
| `thread_pool.yaml` | bounded `thread_pool` lane execution path |
| `composite_loop_iterations.yaml` | fixed-point CompositeLoop iteration region path |
| `payload_policies.yaml` | text payload copy/shared/loaned branches; no external zero-copy claim |
| `channel_modes.yaml` | latest, queue, latched, previous-tick, and barrier channel modes |
| `trigger_policies.yaml` | any/all/time-sync/batch/watermark/condition/debounce/rate-limit trigger paths |

The task-executor benchmark is a separate non-installed CTest binary rather than
a new public CLI command:

```bash
./build/topoexec_bench_task_executor --tasks 32 --runs 5 --format json
```

It reports deterministic and threaded task-executor completion counts and p50/p95
run summaries.

CLI path coverage remains command-smoke based rather than timing-threshold based:
`ctest` and `./scripts/goal_check.sh bench` cover `graph bench`, while the
ordinary CLI smokes cover validate, plan, run, metrics, and trace on
representative examples. Treat these as output-contract baselines, not public
latency guarantees.

## Local baseline workflow

`./scripts/bench_baseline.sh` is a convenience wrapper around
`scripts/bench_baseline.py`. Useful environment variables:

| Variable | Default | Purpose |
| --- | --- | --- |
| `TOPOEXEC_BENCH_BASELINE_OUTPUT` | `benchmarks/local-baseline.json` | Output file for the newly collected baseline. |
| `TOPOEXEC_BENCH_RUNS` | `5` | Runs per benchmark case. |
| `TOPOEXEC_BENCH_STEPS` | `10` | RuntimeRunner steps per graph case. |
| `TOPOEXEC_BENCH_TASKS` | `32` | Task count for the task-executor case. |
| `TOPOEXEC_BENCH_BASELINE_IN` | unset | Optional baseline to compare against. |
| `TOPOEXEC_BENCH_THRESHOLD_PERCENT` | unset | Optional p95 regression threshold percentage. |

Example opt-in per-machine comparison:

```bash
TOPOEXEC_BENCH_BASELINE_OUTPUT=/tmp/topoexec-current.json \
TOPOEXEC_BENCH_BASELINE_IN=benchmarks/local-baseline.json \
TOPOEXEC_BENCH_THRESHOLD_PERCENT=15 \
./scripts/bench_baseline.sh
```

Only use such thresholds for the same machine, compiler, build type, and workload
settings. Commit the code, not the local baseline, unless a human explicitly
chooses to maintain machine-specific baselines elsewhere.

## CI contract

CI and `./scripts/goal_check.sh bench` check benchmark output shape, required
metadata, and successful execution. They do not enforce elapsed-time thresholds.
This keeps normal CI stable while still catching schema drift, missing metadata,
and broken benchmark workloads.
