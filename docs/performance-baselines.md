# Performance Baselines

TopoExec keeps benchmark support lightweight. The CLI benchmark command is machine-readable and is not part of normal correctness CI:

```bash
topoexec graph bench benchmarks/immediate_chain.yaml --steps 10 --runs 20 --format json
```

Output shape:

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
  "tick_calls": 800,
  "elapsed_ms": 12,
  "run_elapsed_ms": [0.6, 0.5],
  "p50_run_elapsed_ms": 0.55,
  "p95_run_elapsed_ms": 0.595,
  "p99_run_elapsed_ms": 0.599,
  "throughput_tick_calls_per_sec": 64000.0,
  "throughput_runs_per_sec": 1600.0,
  "environment": {
    "benchmark_schema": 1,
    "clock": "steady_clock",
    "runtime": "RuntimeRunner"
  },
  "errors": []
}
```

Numbers above are illustrative only. TopoExec does not publish global performance claims before stable baselines exist for specific machines and build profiles.

## v1 Benchmark Cases

The deterministic cases live in [`benchmarks/`](../benchmarks/):

| Case | Runtime path covered |
| --- | --- |
| `single_component.yaml` | single component invocation overhead |
| `immediate_chain.yaml` | immediate chain length and transaction commit overhead |
| `latest_vs_queue.yaml` | latest and bounded queue channel policies |
| `deferred_edges.yaml` | delay, state snapshot, and async admission epoch boundaries |
| `thread_pool.yaml` | bounded `thread_pool` lane execution path |

Existing app/example graphs still cover CompositeLoop and larger payload policy paths and can be passed directly to the same `bench` command.

## CI Contract

CI runs benchmark commands as correctness smokes only. It checks output shape and successful execution, not elapsed-time thresholds. Future nightly performance jobs may store machine-specific baselines and compare regressions using per-case thresholds.
