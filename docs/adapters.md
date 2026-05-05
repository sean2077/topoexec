# Adapter Boundaries

Adapters are intentionally deferred until the core runtime and API stabilize. This
page defines the preview architecture so future adapter work consumes stable
runtime surfaces instead of leaking adapter assumptions into `topoexec::runtime`.

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

Expected future CMake shape:

```text
topoexec::runtime          # no adapter dependencies
topoexec::yaml             # optional graph loading
topoexec_adapters::otel    # optional exporter target
topoexec_adapters::ros2    # optional package, built separately
topoexec_adapters::plugins # optional dynamic loading package
```

No `topoexec_adapters::*` target should be a transitive dependency of
`topoexec::runtime`.

## Adapter interface concepts

These are interface concepts, not committed C++ APIs yet.

### Result sink

Consumes one completed `RuntimeRunnerResult` after a run/tick/batch:

```text
on_result(const RuntimeRunnerResult& result)
```

Use for metrics exporters, trace exporters, local JSON snapshots, release smoke
reporting, or test harnesses.

### Runtime observer

Observes append-only runtime events without influencing scheduling or trigger
readiness:

```text
on_metric_sample(name, labels, value)
on_trace_event(const TraceEvent& event)
on_runtime_error(const RuntimeError& error)
```

Observers must be best-effort and bounded. Export failure should be reported as
adapter health, not as a hidden runtime semantic change.

### Boundary bridge

Connects app-owned external I/O to boundary components:

```text
poll_external_inputs() -> RuntimePayload
publish_boundary_output(RuntimePayload)
```

Boundary bridges translate at graph boundaries only. Internal `EdgePolicy`,
trigger policy, and payload ownership remain TopoExec-owned.

### Component factory provider

Registers app or plugin factories into a `ComponentRegistry`:

```text
register_components(ComponentRegistry& registry)
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

## Stub examples

Preview-only stub notes live under `examples/adapters/`. They are intentionally
not built and contain no external SDK includes. The runnable boundary pattern is
`examples/boundary_adapter_pattern.yaml`.

## Policy checks

`policy_no_core_adapter_deps` scans core/source/build files for accidental adapter
SDK symbols such as `rclcpp`, OpenTelemetry, Prometheus, Perfetto, `pybind11`, or
`Python.h`. It intentionally ignores docs and preview stub notes where those
names are discussed as deferred dependencies.
