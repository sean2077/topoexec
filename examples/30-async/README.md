# Async worker with bounded inflight

## What this example demonstrates

Request validation followed by an async worker edge with max_inflight=2 and bounded queue capacity.

## Graph structure

Primary graph: [`async_bounded_inflight.yaml`](async_bounded_inflight.yaml).

- Category: Execution Semantics
- Difficulty: intermediate
- Tags: async, max-inflight, task-ready

## How to run

```bash
./build/topoexec graph validate examples/30-async/async_bounded_inflight.yaml
./build/topoexec graph run examples/30-async/async_bounded_inflight.yaml --steps 4
./build/topoexec graph trace examples/30-async/async_bounded_inflight.yaml --steps 4 --format json
```

## Expected result

Async publications are deferred and bounded; trace output includes runtime events without changing graph semantics.

## What to inspect

- `example.json` for the commands, generated asset expectations, and CI smoke flags.
- Generated graph asset under `docs/assets/generated/examples/async-bounded-inflight/graph.mmd` after running the asset pipeline.
- Runtime metrics, trace, observe, or diagnostics output when the command list includes those surfaces.

## Related docs

- [`docs/21-architecture/runtime-semantics.md`](../../docs/21-architecture/runtime-semantics.md)
- [`docs/62-schemas-protocols/trace-events.md`](../../docs/62-schemas-protocols/trace-events.md)
