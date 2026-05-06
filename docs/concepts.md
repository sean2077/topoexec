# Concepts

TopoExec is a C++20 in-process semantic execution graph runtime. It is not a
distributed scheduler, ROS 2 adapter, Python runtime, GUI editor, or metrics
exporter. Adapter packages are deferred until the core runtime API is stable.

## Runtime objects

- **Component**: app-owned code with lifecycle hooks and an execution entry
  point. Components publish outputs through `GraphContext`.
- **ComponentRegistry**: maps type names to factories. Embedded apps own their
  registry; the CLI has a tiny demo registry for examples.
- **GraphSpec**: lanes, components, edges, triggers, optional graph config, and
  optional CompositeLoop declarations.
- **CompiledPlan**: validated graph regions, SCC ownership, trigger wiring, and
  deterministic region order.
- **RuntimeRunner**: executes the compiled graph, owns publication routing, and
  returns errors, metrics, trace events, and stop reason.

## Edge visibility

- `immediate`: downstream consumers can observe committed output in the same
  bounded run step if the graph remains feed-forward or CompositeLoop-owned.
- `delay`: publication is staged for a later epoch, which safely breaks feedback.
- `state`: consumers observe the last committed snapshot, not hidden mutable
  state inside the current epoch.
- `async`: completion is deferred and bounded by channel/admission policy.

## Triggers

Trigger readiness is runtime-owned. Current trigger policies include manual,
any/all input, time-sync, batch, request, and task-ready paths. Components should
not poll arbitrary global state to decide readiness.

## Lanes and concurrency

`event_loop` is the deterministic single-thread lane. `thread_pool` supports the
bounded persistent worker-pool v1 documented in [Scheduler](scheduler.md) and
[Concurrency](concurrency.md), including FIFO queue admission, worker-id trace
attributes, and non-reentrant serialization. TopoExec does not claim hard
real-time behavior.

## Overload and payloads

Every channel has explicit capacity, overflow, and copy policy. Payload ownership
is visible through `TextPayload`, `BinaryBlobPayload`, `FrameView`,
`OpaquePayload`, shared/loaned views, and BufferPool metrics.

## Observability

Metrics, trace events, diagnostics, plan JSON, lint/explain output, and benchmark
JSON are scriptable surfaces. They are designed for CI and debugging, not for
external exporter integration yet.
