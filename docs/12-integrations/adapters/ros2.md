# ROS 2 Adapter Preview

G60 adds a dependency-free ROS 2 boundary-mapping preview. It proves that a
future ROS package can embed TopoExec as an in-process semantic runtime while the
core remains a normal C++ package with no ROS client-library dependency.

## Status

- Target: `topoexec_adapters::ros2`
- Header: `topoexec/adapters/ros2.hpp`
- Build option: `TOPOEXEC_BUILD_ROS2_ADAPTER=ON`
- Package metadata: `TOPOEXEC_HAS_ROS2_ADAPTER`
- Contract version: `topoexec::adapters::ros2::kRos2AdapterPreviewContractVersion`
- Default build: off

This is a fake-boundary preview, not a real ROS package. It does not create
nodes, executors, publishers, subscriptions, messages, actions, or services. It
does not call `colcon`, generate message bindings, or link any ROS libraries.

## Target model

A future adapter package can run one TopoExec graph inside one ROS process or
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

Internal components should not need a ROS node handle. Components receive
TopoExec payloads and config snapshots through normal runtime APIs.

## Preview API

The preview exposes adapter-side endpoint descriptors and a fake bridge:

```cpp
#include "topoexec/adapters/ros2.hpp"

topoexec::adapters::ros2::BoundaryEndpoint endpoint;
endpoint.kind = topoexec::adapters::ros2::EndpointKind::kSubscription;
endpoint.external_name = "/camera";
endpoint.boundary_id = "camera_in";
endpoint.port = "out";
endpoint.qos.depth = 2; // adapter config, not core schema

topoexec::adapters::ros2::FakeRos2BoundaryBridge bridge({endpoint}, 8);
bridge.receive(endpoint, topoexec::make_shared_payload(topoexec::make_text_payload("frame")));
```

The fake bridge implements `topoexec::adapters::BoundaryBridge` so tests can
exercise boundary injection and output publication without ROS runtime state.
`validate_boundary_mapping(...)` checks that inbound endpoints map to input
boundary components and outbound endpoints map to output boundary components.

## Boundary mapping

| ROS concept | Preview endpoint kind | Adapter responsibility | TopoExec core responsibility |
| --- | --- | --- | --- |
| Subscription | `kSubscription` | Deserialize message, create payload, enqueue boundary input. | Route payload through declared edge/trigger policy. |
| Publisher | `kPublisher` | Consume boundary output payload and serialize message. | Produce output at the declared boundary component. |
| Service request | `kServiceRequest` | Assign adapter correlation id and enqueue request payload. | Use request/task-ready/async semantics internally. |
| Service response | `kServiceResponse` | Match correlation id and send response through adapter. | Expose boundary output payload and runtime trace/metrics. |
| Action goal/cancel | `kActionGoal`, `kActionCancel` | Split action state into adapter-owned boundary flows. | Keep graph semantics independent of ROS action state machines. |
| Action feedback/result | `kActionFeedback`, `kActionResult` | Publish feedback/result from boundary outputs. | Produce payloads and correlation metadata through normal runtime APIs. |
| Parameters | Future adapter policy | Translate selected parameters into graph config update events. | Apply config snapshots at epoch boundaries. |

Use `ComponentNodeSpec.boundary` descriptors to identify graph boundary nodes.
Do not add ROS topic names or QoS fields to schema v1.

## QoS mapping

ROS QoS belongs at the adapter boundary only. Internal TopoExec channel policy
stays TopoExec-owned:

- ROS reliability/durability/deadline/lifespan configure the adapter transport.
- TopoExec `EdgePolicy` capacity/overflow/lifespan/deadline configure internal
  graph behavior.
- Mapping can be documented per `BoundaryEndpoint`, but it must not silently
  rewrite internal `EdgePolicy`.

If a ROS deadline miss occurs before payload injection, report adapter health and
possibly a boundary metric. If a TopoExec deadline/lifespan policy drops an
internal payload, report runtime channel metrics. Do not merge the two concepts.

## Executor interaction

Initial design should avoid depending on a particular ROS executor strategy:

1. Adapter callbacks enqueue boundary input events into an adapter-owned bounded
   queue.
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

## Validation

G60 coverage:

- `test_ros2_adapter` validates topic/service/action endpoint mapping, inbound
  and outbound boundary-role checks, fake subscription injection, fake publisher
  output capture, adapter-side correlation metadata, and QoS remaining outside
  `BoundaryMessage`/core schema fields.
- `cmake_ros2_adapter_options_smoke` configures a runtime-only build with
  `TOPOEXEC_BUILD_ROS2_ADAPTER=ON`, installs it, and verifies a downstream
  `find_package(topoexec COMPONENTS ros2)` consumer links
  `topoexec_adapters::ros2`.
- `policy_no_core_adapter_deps` keeps runtime/core free of adapter dependencies
  and rejects accidental ROS package discovery in the preview target.

Only after fake-boundary tests pass should a separate ROS package prototype one
input topic and one output topic. Actions/services should remain later work.

## Non-goals for core

- No ROS client-library include or link dependency in core targets.
- No ROS executor assumptions in scheduler lanes.
- No ROS QoS fields in schema v1.
- No component requirement to own or receive a ROS node handle.
- No replacement for ROS 2; TopoExec remains an in-process semantic graph
  runtime embedded behind adapter boundaries.
