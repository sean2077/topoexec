# ROS 2 Adapter Plan

This is a deferred design for a future optional ROS 2 adapter. It is not an
implementation plan for the core runtime, and it must not introduce ROS symbols
or dependencies into `topoexec::runtime`.

## Target model

A future adapter package can run one TopoExec graph inside one ROS 2 process or
node-like adapter host:

```text
ROS subscription/service/action boundary
        |
        v
adapter-owned boundary bridge
        |
        v
TopoExec boundary component -- internal TopoExec graph -- boundary component
        |
        v
adapter-owned publisher/service/action response bridge
```

Internal components should not need an `rclcpp::Node`. Components receive
TopoExec payloads and config snapshots through normal runtime APIs.

## Boundary mapping

| ROS concept | Adapter responsibility | TopoExec core responsibility |
| --- | --- | --- |
| Subscription | Deserialize message, create payload, publish to input boundary. | Route payload through declared edge/trigger policy. |
| Publisher | Consume boundary output payload and serialize message. | Produce output at the declared boundary component. |
| Service request | Assign adapter correlation id, publish request payload. | Use request/task-ready/async semantics internally. |
| Service response | Match correlation id and publish response through adapter. | Expose boundary output payload and runtime trace/metrics. |
| Action | Split goal, feedback, result, and cancel into adapter-owned boundary flows. | Keep internal graph semantics independent of ROS action state machine. |
| Parameters | Translate selected ROS parameters into graph config update events. | Apply config snapshots at epoch boundaries. |

Use `ComponentNodeSpec.boundary` descriptors to identify graph boundary nodes.
Do not add ROS topic names or QoS fields to schema v1.

## QoS mapping

ROS QoS belongs at the ROS boundary only. Internal TopoExec channel policy stays
TopoExec-owned:

- ROS reliability/durability/deadline/lifespan configure the adapter transport.
- TopoExec `EdgePolicy` capacity/overflow/lifespan/deadline configure internal
  graph behavior.
- Mapping can be documented per boundary descriptor, but it must not silently
  rewrite internal `EdgePolicy`.

If a ROS deadline miss occurs before payload injection, report adapter health and
possibly a boundary metric. If a TopoExec deadline/lifespan policy drops an
internal payload, report runtime channel metrics. Do not merge the two concepts.

## Executor interaction

Initial design should avoid depending on a particular ROS executor strategy:

1. Adapter callbacks enqueue boundary input events into an adapter-owned buffer.
2. A TopoExec runtime tick drains bounded inputs according to graph policy.
3. Boundary output events are handed back to adapter-owned publishers/responders.
4. Shutdown coordinates adapter callback stop, runtime stop, and reverse
   component deactivation.

A future adapter package can offer executor integration choices, for example:

- single-threaded adapter host with explicit TopoExec ticks;
- ROS callback threads feeding bounded boundary queues;
- dedicated TopoExec runtime thread with explicit stop token.

Those choices must remain outside core runtime targets.

## Threading and backpressure

Backpressure is explicit at both boundaries:

- ROS transport backpressure is adapter-specific.
- Boundary bridge queues must be bounded and observable.
- TopoExec channel overflow is declared in graph `EdgePolicy`.
- Blocking inside a single-thread callback path should be avoided unless the
  adapter explicitly opts into it and documents shutdown behavior.

Thread-safety requirements for a future adapter:

- no direct component invocation from ROS callbacks;
- no hidden global state mutation;
- all boundary injections go through a runtime-owned or adapter-owned bounded
  queue;
- adapter failures surface as diagnostics/health, not silent dropped work.

## Lifecycle and shutdown

Recommended startup sequence:

1. Load graph/config through app or adapter package.
2. Register app component factories.
3. Validate/compile graph and boundary descriptors.
4. Configure and activate TopoExec runtime.
5. Start ROS subscriptions/services/actions/publishers.
6. Begin runtime tick loop or executor integration.

Recommended shutdown sequence:

1. Stop accepting new ROS boundary callbacks.
2. Drain or reject pending adapter boundary inputs according to policy.
3. Request TopoExec stop and wait for bounded in-flight tasks.
4. Deactivate components in runtime order.
5. Tear down ROS entities.

The adapter should expose shutdown timeout metrics and clear fatal errors when a
bounded shutdown cannot complete.

## Parameters and config

ROS parameters should map to graph-level config updates or component config
updates only through explicit adapter policy:

- `apply_on_epoch_boundary` semantics are preserved.
- Invalid parameter updates fail at the adapter boundary with diagnostics.
- Parameter names are adapter config, not core schema fields.

## Diagnostics, metrics, and tracing

A ROS 2 adapter can consume existing surfaces:

- `RuntimeRunnerResult::runtime_errors` for structured errors;
- metrics snapshots for runtime/channel/trigger/scheduler/loop health;
- trace events or Chrome trace JSON for timeline export;
- adapter health metrics for ROS-specific transport, callback, and QoS events.

ROS diagnostics should reference TopoExec component/edge ids so users can map
issues back to graph definitions.

## Fake-boundary-first tests

Before any ROS dependency is introduced, design tests with fake boundary bridges:

1. Fake subscription injects payloads into a boundary descriptor.
2. Runtime graph processes the payload through normal edge/trigger semantics.
3. Fake publisher records boundary output payloads.
4. QoS-like adapter drops are simulated outside the graph and reported as adapter
   health.
5. Internal TopoExec deadline/overflow drops are reported through runtime metrics.

Only after fake-boundary tests pass should a separate ROS package prototype one
input topic and one output topic. Actions/services should remain later work.

## Non-goals for core

- No `rclcpp` include or link dependency in core targets.
- No ROS executor assumptions in scheduler lanes.
- No ROS QoS fields in schema v1.
- No component requirement to own or receive a ROS node handle.
- No replacement for ROS 2; TopoExec remains an in-process semantic graph
  runtime embedded behind adapter boundaries.
