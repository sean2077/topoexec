# Trigger semantics showcase

## What this example demonstrates

One graph showing any_input, all_inputs, time_sync, batch, rate_limit, debounce, and condition trigger policies.

## Graph structure

Primary graph: [`trigger_semantics.yaml`](trigger_semantics.yaml).

- Category: Execution Semantics
- Difficulty: intermediate
- Tags: trigger, readiness, suppression

## How to run

```bash
./build/topoexec graph validate examples/20-triggers/trigger_semantics.yaml
./build/topoexec graph render examples/20-triggers/trigger_semantics.yaml --format mermaid
./build/topoexec graph run examples/20-triggers/trigger_semantics.yaml --steps 3
./build/topoexec graph explain examples/20-triggers/trigger_semantics.yaml
```

## Expected result

The graph validates all supported trigger policy shapes and a short run exposes suppressed/pending behavior through metrics if policies do not fire every tick.

## What to inspect

- `example.json` for the commands, generated asset expectations, and CI smoke flags.
- Generated graph asset under `docs/assets/generated/examples/trigger-semantics/graph.mmd` after running the asset pipeline.
- Runtime metrics, trace, observe, or diagnostics output when the command list includes those surfaces.

## Related docs

- [`docs/21-architecture/runtime-semantics.md`](../../docs/21-architecture/runtime-semantics.md)
- [`docs/62-schemas-protocols/diagnostics.md`](../../docs/62-schemas-protocols/diagnostics.md)
