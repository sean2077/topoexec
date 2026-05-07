# Branching fan-out and join

## What this example demonstrates

A non-linear source -> preprocess -> branch_a/branch_b -> merge -> sink graph.

## Graph structure

Primary graph: [`branching_fanout_join.yaml`](branching_fanout_join.yaml).

- Category: Basic Dataflow
- Difficulty: beginner
- Tags: branching, fan-out, join

## How to run

```bash
./build/topoexec graph validate examples/10-basic-dataflow/branching_fanout_join.yaml
./build/topoexec graph render examples/10-basic-dataflow/branching_fanout_join.yaml --format mermaid
./build/topoexec graph run examples/10-basic-dataflow/branching_fanout_join.yaml --steps 1
```

## Expected result

`merge` fires only after both branch outputs are ready, and the run completes without drops.

## What to inspect

- `example.json` for the commands, generated asset expectations, and CI smoke flags.
- Generated graph asset under `docs/assets/generated/examples/branching-fanout-join/graph.mmd` after running the asset pipeline.
- Runtime metrics, trace, observe, or diagnostics output when the command list includes those surfaces.

## Related docs

- [`docs/11-user-guide/examples.md`](../../docs/11-user-guide/examples.md)
- [`docs/21-architecture/runtime-semantics.md`](../../docs/21-architecture/runtime-semantics.md)
