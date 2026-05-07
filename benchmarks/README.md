# Benchmark Cases

These workloads are deterministic benchmark inputs for `topoexec graph bench`
and the local benchmark-baseline scripts. They are correctness-gated only by
CTest; timing thresholds are intentionally opt-in and per machine.

Run one CLI case:

```bash
topoexec graph bench benchmarks/immediate_chain.yaml --steps 10 --runs 20 --format json
```

Generate a local baseline file:

```bash
./scripts/bench_baseline.sh
```

The default baseline output is `benchmarks/local-baseline.json`, which is ignored
by git. Set `TOPOEXEC_BENCH_BASELINE_OUTPUT` to write elsewhere.

## RuntimeRunner graph cases

| File | Runtime path covered |
| --- | --- |
| `single_component.yaml` | single manually-triggered component dispatch overhead |
| `immediate_chain.yaml` | immediate chain length and transaction commit overhead |
| `fan_out.yaml` | multi-reader fan-out through queue/latest channels |
| `fan_in.yaml` | two-source fan-in into an `all_inputs` join |
| `latest_vs_queue.yaml` | latest and bounded queue channel policies |
| `deferred_edges.yaml` | delay, state snapshot, and async admission epoch boundaries |
| `thread_pool.yaml` | bounded `thread_pool` lane execution path |
| `composite_loop_iterations.yaml` | fixed-point CompositeLoop iteration region path |
| `payload_policies.yaml` | text payload copy/shared/loaned policy branches without external zero-copy claims |
| `channel_modes.yaml` | latest, queue, latched, previous-tick, and barrier channel modes in one run |
| `trigger_policies.yaml` | any/all/time-sync/batch/watermark/condition/debounce/rate-limit trigger paths |
| `live_observe_minimal.yaml` | minimal live-observe runtime path for focused overhead smoke |
| `live_observe_high_frequency_channels.yaml` | high-frequency component/channel publish and commit live-observe workload |
| `live_observe_trigger_stress.yaml` | trigger-policy live-observe workload |
| `live_observe_thread_pool.yaml` | thread-pool lane live-observe workload |
| `live_observe_composite_loop.yaml` | CompositeLoop live-observe workload |

## Task executor case

`benchmarks/task_executor.cpp` builds the non-installed
`topoexec_bench_task_executor` test binary. It covers deterministic and threaded
task executor submission/completion overhead without adding a public CLI command:

```bash
./build/topoexec_bench_task_executor --tasks 32 --runs 5 --format json
```

## Output and policy

Benchmark JSON uses schema version 2. CLI graph benchmark output includes:

- `case`, `params`, `graph_hash`, `runs`, `ok_runs`, and `steps`;
- per-run elapsed samples plus p50/p95/p99 elapsed summaries;
- throughput counters useful for local comparison;
- environment metadata: compiler, compiler version, C++ standard, build type,
  CPU model, CPU thread count, commit, runtime, and clock.

Do not publish global performance claims from these cases. CI should check that
benchmark outputs are valid and complete; only local users should opt into
regression thresholds for a fixed machine/build profile.
