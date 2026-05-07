# Validation diagnostics and golden-friendly output

## What this example demonstrates

A deterministic valid graph plus an invalid fixture for showing clear diagnostics and CI-friendly golden checks.

## Graph structure

Primary graph: [`golden_pipeline.yaml`](golden_pipeline.yaml).

- Category: Testing and Validation
- Difficulty: beginner
- Tags: diagnostics, golden, ci

## How to run

```bash
./build/topoexec graph validate examples/60-testing-validation/golden_pipeline.yaml --format json
./build/topoexec graph run examples/60-testing-validation/golden_pipeline.yaml --steps 1
./build/topoexec graph render examples/60-testing-validation/golden_pipeline.yaml --format mermaid
./build/topoexec graph validate examples/60-testing-validation/invalid_missing_edge_kind.yaml  # expected to fail
```

## Expected result

The valid graph produces stable JSON/text output; the invalid graph fails validation before runtime with a missing edge-kind diagnostic.

## What to inspect

- `example.json` for the commands, generated asset expectations, and CI smoke flags.
- Generated graph asset under `docs/assets/generated/examples/validation-and-golden/graph.mmd` after running the asset pipeline.
- Runtime metrics, trace, observe, or diagnostics output when the command list includes those surfaces.

## Related docs

- [`docs/62-schemas-protocols/diagnostics.md`](../../docs/62-schemas-protocols/diagnostics.md)
- [`docs/24-testing/testing-strategy.md`](../../docs/24-testing/testing-strategy.md)
