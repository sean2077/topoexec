# Metrics, trace, and live observe

## What this example demonstrates

Minimal graph run that produces metrics JSON, structured trace JSON, Chrome trace, and live observe NDJSON.

## Graph structure

Primary graph: [`minimal_observability.yaml`](minimal_observability.yaml).

- Category: Observability
- Difficulty: beginner
- Tags: metrics, trace, observe

## How to run

```bash
./build/topoexec graph metrics examples/50-observability/minimal_observability.yaml --steps 1 --format json
./build/topoexec graph trace examples/50-observability/minimal_observability.yaml --steps 1 --format json
./build/topoexec graph trace examples/50-observability/minimal_observability.yaml --steps 1 --format chrome
./build/topoexec graph observe examples/50-observability/minimal_observability.yaml --steps 3 --observe-level summary --format ndjson
```

## Expected result

Metrics and trace outputs are JSON-structured; live observe emits schema-versioned NDJSON for local debugging.

## What to inspect

- `example.json` for the commands, generated asset expectations, and CI smoke flags.
- Generated graph asset under `docs/assets/generated/examples/metrics-trace-observe/graph.mmd` after running the asset pipeline.
- Runtime metrics, trace, observe, or diagnostics output when the command list includes those surfaces.

## Related docs

- [`docs/62-schemas-protocols/metrics.md`](../../docs/62-schemas-protocols/metrics.md)
- [`docs/62-schemas-protocols/trace-events.md`](../../docs/62-schemas-protocols/trace-events.md)
- [`docs/41-development-tools/live-runtime-validation.md`](../../docs/41-development-tools/live-runtime-validation.md)
