# Benchmark Cases

These YAML graphs are deterministic benchmark workloads for `topoexec graph bench`. They are correctness-gated only; they are not performance gates and make no cross-machine claims.

Run one case:

```bash
topoexec graph bench benchmarks/immediate_chain.yaml --steps 10 --runs 20 --format json
```

Cases:

| File | Runtime path covered |
| --- | --- |
| `single_component.yaml` | single component invocation overhead |
| `immediate_chain.yaml` | immediate chain length and transaction commit overhead |
| `latest_vs_queue.yaml` | latest and bounded queue channel policies |
| `deferred_edges.yaml` | delay, state snapshot, and async admission epoch boundaries |
| `thread_pool.yaml` | bounded `thread_pool` lane execution path |

The JSON output includes `case`, `params`, `runs`, aggregate elapsed time, per-run elapsed samples, p50/p95/p99 run latency, throughput, and an environment summary. Future regression thresholds should be defined per machine/CI class rather than hard-coded globally.
