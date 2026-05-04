# FAQ

## What is TopoExec?

TopoExec is a small C++20 in-process stateful dataflow runtime. It focuses on explicit graph semantics: edge visibility, transactions, bounded channels, triggers, CompositeLoop ownership, metrics, and trace events.

## What is it not?

It is not a distributed runtime, ROS adapter, Python framework, GUI editor, or OpenTelemetry/Prometheus exporter. Those adapters are deferred until the core runtime API is stable.

## Do I need YAML?

No. Pure C++ users can build `GraphSpec` directly or use `GraphBuilder` and link only `topoexec::runtime`. YAML loading lives behind optional `topoexec::yaml`.

## Which edge kind should I use?

Use `immediate` for same-epoch DAG flow, `delay` for feedback that must wait until the next epoch, `state` for retained cross-epoch state, and `async` for deferred completion-style delivery. Only `immediate` edges participate in immediate SCC rejection.

## Does `publish()` execute downstream components recursively?

No. `GraphContext::publish()` stages output. The runtime commits staged publications according to edge kind and compiled region order.

## Is `thread_pool` real?

Not in `v0.1.0-alpha`. It is accepted by schema for forward compatibility, but `RuntimeRunner` rejects `thread_pool` lanes in `run` mode. Use `event_loop` for runnable alpha graphs.

## Is async max-inflight implemented?

No dedicated async admission controller exists yet. Current async backpressure uses bounded async channels and overflow policy. A future worker/admission path should enforce `max_inflight` independently of channel capacity.

## How do components report errors?

Existing components may throw from `configure()`, `activate()`, `execute()`, or `deactivate()`. Components that do not want exceptions can override `configure_status()`, `activate_status()`, `execute_status()`, or `deactivate_status()` and return `Status::error(...)`.
