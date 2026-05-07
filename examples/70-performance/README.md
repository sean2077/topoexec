# Minimal performance benchmark

## What this example demonstrates

Small graph bench command for local performance awareness without cross-machine timing claims.

## Graph structure

Primary graph: [`minimal_benchmark.yaml`](minimal_benchmark.yaml).

- Category: Performance
- Difficulty: intermediate
- Tags: benchmark, performance, regression

## How to run

```bash
./build/topoexec graph validate examples/70-performance/minimal_benchmark.yaml
./build/topoexec graph bench examples/70-performance/minimal_benchmark.yaml --steps 1 --runs 2 --format json
```

## Expected result

Benchmark JSON reports environment metadata, run count, percentiles, and throughput; README should not compare machines from this local smoke.

## What to inspect

- `example.json` for the commands, generated asset expectations, and CI smoke flags.
- Generated graph asset under `docs/assets/generated/examples/minimal-benchmark/graph.mmd` after running the asset pipeline.
- Runtime metrics, trace, observe, or diagnostics output when the command list includes those surfaces.

## Related docs

- [`benchmarks/README.md`](../../benchmarks/README.md)
- [`docs/24-testing/performance-baselines.md`](../../docs/24-testing/performance-baselines.md)
