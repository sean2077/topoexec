# CompositeLoop iterative solver

## What this example demonstrates

A declared estimator/controller feedback SCC with fixed-point policy, convergence label, and iteration budget.

## Graph structure

Primary graph: [`composite_loop_solver.yaml`](composite_loop_solver.yaml).

- Category: Advanced Semantics
- Difficulty: advanced
- Tags: loop, fixed-point, feedback

## How to run

```bash
./build/topoexec graph validate examples/40-composite-loop/composite_loop_solver.yaml
./build/topoexec graph plan examples/40-composite-loop/composite_loop_solver.yaml --format json
./build/topoexec graph render examples/40-composite-loop/composite_loop_solver.yaml --format mermaid
./build/topoexec graph run examples/40-composite-loop/composite_loop_solver.yaml --steps 2
```

## Expected result

Immediate feedback is accepted because the SCC is explicitly owned by `composite_loops`; plan output includes the loop policy.

## What to inspect

- `example.json` for the commands, generated asset expectations, and CI smoke flags.
- Generated graph asset under `docs/assets/generated/examples/composite-loop-solver/graph.mmd` after running the asset pipeline.
- Runtime metrics, trace, observe, or diagnostics output when the command list includes those surfaces.

## Related docs

- [`docs/21-architecture/runtime-invariants.md`](../../docs/21-architecture/runtime-invariants.md)
- [`docs/11-user-guide/cookbook.md`](../../docs/11-user-guide/cookbook.md)
