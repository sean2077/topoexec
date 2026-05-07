# Minimal pipeline

## What this example demonstrates

Small source-transform-sink graph for the first validate, plan, render, and run commands.

## Graph structure

Primary graph: [`minimal.yaml`](minimal.yaml).

- Category: Getting Started
- Difficulty: beginner
- Tags: minimal, quick-start, dataflow

## How to run

```bash
./build/topoexec graph validate examples/00-getting-started/minimal.yaml
./build/topoexec graph plan examples/00-getting-started/minimal.yaml --format json
./build/topoexec graph render examples/00-getting-started/minimal.yaml --format mermaid
./build/topoexec graph run examples/00-getting-started/minimal.yaml --steps 1
```

## Expected result

`graph run` prints `ok`, the graph name, deterministic component order, and two committed publications.

## What to inspect

- `example.json` for the commands, generated asset expectations, and CI smoke flags.
- Generated graph asset under `docs/assets/generated/examples/minimal-pipeline/graph.mmd` after running the asset pipeline.
- Runtime metrics, trace, observe, or diagnostics output when the command list includes those surfaces.

## Related docs

- [`docs/11-user-guide/graph-spec.md`](../../docs/11-user-guide/graph-spec.md)
- [`docs/41-development-tools/cli.md`](../../docs/41-development-tools/cli.md)
