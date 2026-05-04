# Performance Baselines

TopoExec keeps benchmark support lightweight. The CLI benchmark command is machine-readable and is not part of normal correctness CI:

```bash
topoexec graph bench examples/minimal.yaml --steps 1 --runs 10 --format json
```

Output shape:

```json
{
  "ok": true,
  "runs": 10,
  "ok_runs": 10,
  "steps": 1,
  "tick_calls": 30,
  "elapsed_ms": 1,
  "errors": []
}
```

## Alpha Baseline

The alpha baseline is intentionally modest:

- Graph: `examples/minimal.yaml`
- Workload: one source-transform-sink immediate chain.
- Command: `topoexec graph bench examples/minimal.yaml --steps 1 --runs 2 --format json`
- Contract: output includes `elapsed_ms`, `tick_calls`, `ok_runs`, and `errors`.

This proves the benchmark path is scriptable. It is not yet a statistically useful performance claim.

## Future Benchmark Cases

Before beta, add optional benchmarks for:

- single component invocation overhead;
- immediate chain length N;
- fan-out/fan-in graph;
- latest overwrite throughput;
- bounded queue throughput;
- delay edge epoch overhead;
- shared/loaned large payload no-copy path;
- CompositeLoop iteration overhead;
- trigger policies: any/all/batch/time-sync;
- worker-pool throughput once worker-pool scheduling exists.
