# FAQ

## What is TopoExec?

TopoExec is a small C++20 in-process stateful dataflow runtime. It focuses on explicit graph semantics: edge visibility, transactions, bounded channels, triggers, CompositeLoop ownership, metrics, and trace events.

## What is it not?

It is not a distributed runtime, production ROS adapter, Python framework, GUI editor, or production OpenTelemetry/Prometheus exporter. Those adapters are deferred until the core runtime API is stable; the G58/G59 telemetry targets, G60 ROS 2 target, G61 C API target, and G62 Python automation package are dependency-free or unstable previews only.

## Do I need YAML?

No. Pure C++ users can build `GraphSpec` directly or use `GraphBuilder` and link only `topoexec::runtime`. YAML loading lives behind optional `topoexec::yaml`.

## Which edge kind should I use?

Use `immediate` for same-epoch DAG flow, `delay` for feedback that must wait until the next epoch, `state` for retained cross-epoch state, and `async` for deferred completion-style delivery. Only `immediate` edges participate in immediate SCC rejection.

## Does `publish()` execute downstream components recursively?

No. `GraphContext::publish()` stages output. The runtime commits staged publications according to edge kind and compiled region order.

## Is `thread_pool` real?

Yes, as a bounded persistent worker-pool v1. `RuntimeRunner` starts run-scoped lane workers up to the lane `max_threads` value, admits ready invocations through bounded runtime-priority queue policy, then waits at the component/region barrier before downstream regions run. `execution.reentrant: false` still serializes a component's invocations.

## Is async max-inflight implemented?

Yes for async edges. Set `policy.max_inflight` on an `async` edge to limit outstanding deferred completions before channel capacity is considered. It is not itself a task executor; it controls admission of async completion events. Optional `TaskExecutor` / `ThreadedTaskExecutor` helpers are separate surfaces that publish completions back through normal graph edges.

## How do components report errors?

Existing components may throw from `configure()`, `activate()`, `execute()`, or `deactivate()`. Components that do not want exceptions can override `configure_status()`, `activate_status()`, `execute_status()`, or `deactivate_status()` and return `Status::error(...)`.
