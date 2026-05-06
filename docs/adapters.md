# Adapter Boundaries

Concrete adapters are intentionally deferred until the core runtime and API stabilize.
G57 adds a dependency-free Adapter SDK v0 boundary so future adapter packages can
consume stable runtime surfaces without leaking adapter assumptions into
`topoexec::runtime`.

## Core boundary

The core/runtime layer owns:

- `GraphSpec` and graph validation/compilation.
- Component lifecycle and invocation.
- Edge visibility, channel policy, payload ownership, and publication routing.
- Scheduler lanes, trigger readiness, CompositeLoop ownership, state/config
  snapshots, metrics, trace events, and diagnostics.
- `RuntimeRunnerResult` as the in-process observation surface.

The core/runtime layer must not depend on ROS, OpenTelemetry, Prometheus,
Python, Perfetto SDKs, service-specific client libraries, or dynamic plugin
loader frameworks.

## Future namespace and targets

Future adapter code should live outside core under a namespace such as:

```text
topoexec_adapters::otel
topoexec_adapters::prometheus
topoexec_adapters::perfetto
topoexec_adapters::ros2
topoexec_adapters::python
topoexec_adapters::c_api
topoexec_adapters::plugins
```

CMake shape after G57:

```text
topoexec::runtime          # no adapter dependencies
topoexec::adapter_sdk      # header-only SDK v0, depends on runtime
topoexec::yaml             # optional graph loading
topoexec_adapters::otel    # future optional exporter target
topoexec_adapters::ros2    # future optional package, built separately
topoexec_adapters::plugins # future optional dynamic loading package
```

No `topoexec_adapters::*` target or `topoexec::adapter_sdk` dependency should be
a transitive dependency of `topoexec::runtime`. Adapter packages depend outward
on `topoexec::adapter_sdk`, not inward from runtime.

## Adapter interface concepts

Include Adapter SDK v0 with:

```cpp
#include "topoexec/adapters/sdk.hpp"
```

Concrete adapter targets remain deferred, but future exporters should consume
these runtime surfaces rather than adding SDK dependencies to core.

### Result sink

Consumes one completed `RuntimeRunnerResult` after a run/tick/batch:

```cpp
topoexec::Status on_result(const topoexec::RuntimeRunnerResult& result)
```

Use for metrics exporters, trace exporters, local JSON snapshots, release smoke
reporting, or test harnesses.

### Runtime observer

Observes append-only runtime events without influencing scheduling or trigger
readiness:

```cpp
topoexec::Status on_metric(const topoexec::RuntimeMetricSample& metric)
topoexec::Status on_trace_event(const topoexec::RuntimeTraceEvent& event)
topoexec::Status on_health_event(const topoexec::HealthEvent& event)
topoexec::Status on_runtime_error(const topoexec::RuntimeError& error)
```

Register observers with `RuntimeRunnerOptions::observers`. The default is no
observer; `NoopRuntimeObserver` is explicit no-op behavior, and
`InMemoryRuntimeObserver` is a bounded recorder for tests and embedders.
Observers must be best-effort and bounded. Callback failure is recorded as
`RuntimeRunnerResult::observer_failure_count`, non-fatal `observer_failure`
diagnostics, and `runtime.observer.*` metrics; it does not change graph runtime
semantics.

Metrics exporters should also read `runtime_metric_descriptors()` and verify
`metric_schema_version` before mapping names, units, and bounded labels.

### Boundary bridge

Connects app-owned external I/O to boundary components through
`topoexec::adapters::BoundaryBridge`:

```cpp
topoexec::adapters::BoundaryPollResult poll_input();
topoexec::Status publish_output(const topoexec::adapters::BoundaryMessage& message);
```

`poll_input()` is a non-blocking/best-effort boundary poll: `ready=false` means no
input is available, and `reason` carries adapter-side failure text.
`publish_output()` reports adapter-side output failures as `Status`; it must not
call downstream runtime components directly. Boundary bridges translate at graph
boundaries only. Internal `EdgePolicy`, trigger policy, and payload ownership
remain TopoExec-owned.

### Component factory provider

Registers app or plugin factories into a `ComponentRegistry` through
`topoexec::adapters::ComponentFactoryProvider`:

```cpp
topoexec::Status register_components(topoexec::ComponentRegistry& registry);
```

Dynamic discovery, ABI policy, version negotiation, and sandboxing are future
plugin-package concerns. The current stable path is explicit in-process factory
registration.

## Schema policy

Adapter-specific fields should not enter schema v1. Prefer existing generic
fields first:

- `ComponentNodeSpec.boundary` for external input/output role and descriptor.
- `EdgePolicy` for internal channel capacity, overflow, and copy policy.
- `trigger_policy` for readiness semantics.
- graph-level `config` for app-owned configuration snapshots.

A schema field is acceptable only if it is useful without a specific adapter.
Examples: a generic boundary descriptor is acceptable; a ROS topic QoS field is
not core schema and belongs in a ROS adapter config layer.

## Candidate adapter contracts

| Adapter | Contract | Explicit non-goal for core |
| --- | --- | --- |
| OpenTelemetry | Export existing metrics/trace/error data from `RuntimeRunnerResult` or observer events. | No OTel SDK dependency in core; no OTel-specific schema fields. |
| Prometheus | Expose existing metrics through a scrape endpoint owned by the adapter. | Core does not run HTTP servers or Prometheus registries. |
| Perfetto | Convert trace events to richer Perfetto output. | Core keeps Chrome trace JSON as dependency-free output. |
| ROS 2 | Translate topics/services/actions at boundary components. | Core does not include `rclcpp`, ROS executors, or ROS QoS fields. |
| Python | Configuration, tests, and scripting first. | Python is not the high-performance payload path. |
| C API | Stable FFI boundary over runtime/result/config primitives. | No premature ABI freeze before C++ API and schema stabilize. |
| Plugin loader | Optional dynamic component discovery. | Current core uses explicit `ComponentRegistry` factories only. |

## Detailed adapter plans

- [ROS 2 adapter plan](adapters/ros2.md) documents boundary mapping, QoS separation,
  executor interaction, threading, lifecycle, parameters, diagnostics, tracing,
  and fake-boundary-first tests without adding ROS dependencies.

## Stub examples

Preview-only stub notes live under `examples/adapters/`. They are intentionally
not built and contain no external SDK includes. The runnable boundary pattern is
`examples/boundary_adapter_pattern.yaml`.

## Policy checks

`policy_no_core_adapter_deps` scans core/source/build files for accidental adapter
SDK symbols such as `rclcpp`, OpenTelemetry, Prometheus, Perfetto, `pybind11`, or
`Python.h`; it also checks that `topoexec_runtime` does not link
`topoexec_adapter_sdk` and that `topoexec_adapter_sdk` depends only on runtime. It
intentionally ignores docs and preview stub notes where those names are discussed
as deferred dependencies.
