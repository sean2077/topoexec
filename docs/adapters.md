# Adapter Boundaries

Concrete production adapters are intentionally deferred until the core runtime
and API stabilize. G57 adds a dependency-free Adapter SDK v0 boundary so future
adapter packages can consume stable runtime surfaces without leaking adapter
assumptions into `topoexec::runtime`. G58 adds the first optional exporter
preview target, `topoexec_adapters::otel`, as a dependency-free OTel-shaped
mapping over the observer API; G59 adds `topoexec_adapters::prometheus` as a
dependency-free text exposition preview over the metric schema. G60 adds
`topoexec_adapters::ros2` as a dependency-free fake-boundary preview for
topics, services, actions, and adapter-side QoS. G63 adds `topoexec::plugin_loader`
as a separate default-off trusted-native component-loading preview. None of these
targets is a production SDK/server/client-library, sandbox, or stable plugin ABI
integration.

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
loader frameworks. The optional G63 loader depends outward on `topoexec::runtime`
and is not linked by the runtime target.

## Future namespace and targets

Adapter code lives outside core under namespaces such as:

```text
topoexec_adapters::otel       # optional dependency-free preview target
topoexec_adapters::prometheus # optional text exposition preview, default OFF
topoexec_adapters::ros2    # optional fake-boundary preview, default OFF
topoexec_adapters::perfetto
topoexec_adapters::python
topoexec_adapters::c_api
topoexec::plugins             # G63 loader preview helper namespace
```

CMake shape after G63:

```text
topoexec::runtime          # no adapter dependencies
topoexec::adapter_sdk      # header-only SDK v0, depends on runtime
topoexec::yaml             # optional graph loading
topoexec_adapters::otel    # optional OTel-shaped preview, default OFF
topoexec_adapters::prometheus # optional text exposition preview, default OFF
topoexec_adapters::ros2    # optional fake-boundary preview, default OFF
topoexec::plugin_loader     # optional trusted-native loader preview, default OFF
```

No `topoexec_adapters::*` target or `topoexec::adapter_sdk` dependency should be
a transitive dependency of `topoexec::runtime`. Adapter packages depend outward
on `topoexec::adapter_sdk`, not inward from runtime.

## Adapter interface concepts

Include Adapter SDK v0 with:

```cpp
#include "topoexec/adapters/sdk.hpp"
```

Production adapter targets remain deferred, but preview/exporter targets
should consume these runtime surfaces rather than adding SDK dependencies to
core.

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
`metric_schema_version` before mapping names, units, and bounded labels. The
G58 `topoexec/adapters/otel.hpp` preview demonstrates this rule by translating
metric descriptors, trace events, health events, and runtime errors into
bounded in-memory records without linking a telemetry SDK.
G59 `topoexec/adapters/prometheus.hpp` demonstrates the metrics-only style by
rendering counter, gauge, and custom histogram summary samples into bounded text
exposition without starting an HTTP server. G60
`topoexec/adapters/ros2.hpp` demonstrates the boundary-bridge style by mapping
topics, services, actions, and QoS into adapter-owned endpoint descriptors and
fake boundary messages without linking ROS libraries.

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

G63 provides a preview-only dynamic loader with manifest, plugin API version,
schema version, descriptor matching, and structured load errors. Plugins are
trusted native code and are not sandboxed. The current stable path remains
explicit in-process factory registration.

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
| OpenTelemetry | G58 preview maps existing metrics/trace/error/health data from `RuntimeRunnerResult` or observer events through `topoexec_adapters::otel`. | No OTel SDK dependency in core/runtime; no OTel-specific schema fields; no network exporter. |
| Prometheus | G59 preview renders existing metrics and custom histogram summaries as text exposition through `topoexec_adapters::prometheus`. | Core does not run HTTP servers or Prometheus registries; no unbounded labels. |
| Perfetto | Convert trace events to richer Perfetto output. | Core keeps Chrome trace JSON as dependency-free output. |
| ROS 2 | G60 preview maps topics/services/actions and adapter-side QoS through `topoexec_adapters::ros2` fake boundary bridges. | Core does not include ROS client libraries, ROS executors, or ROS QoS fields. |
| Python | G62 preview provides `topoexec_preview`, a stdlib-only CLI-backed automation package for validation, plan, run, metrics, and trace JSON. | Python is not a native component implementation or high-performance payload path. |
| C API | G61 preview exposes `topoexec::c_api` opaque handles, create/run/destroy, error strings, and metric iteration. | ABI version remains `0`; no Python binding, dynamic plugin, or stable ABI promise. |
| Plugin loader | G63 `topoexec::plugin_loader` loads trusted native components by explicit path with manifest/version/schema checks. | No default runtime dependency, no graph-driven discovery, no sandbox, no stable ABI, and explicit `ComponentRegistry` factories remain the stable path. |

## Detailed adapter plans

- [OTel exporter preview](adapters/otel.md) documents the dependency-free G58
  mapping target, build option, package metadata, and cardinality rules.
- [Prometheus exporter preview](adapters/prometheus.md) documents the
  dependency-free G59 text exposition target, build option, package metadata,
  and label/cardinality rules.
- [ROS 2 adapter preview](adapters/ros2.md) documents the dependency-free
  G60 fake-boundary target, topic/service/action mapping, QoS separation,
  executor interaction, lifecycle, diagnostics, tracing, and package smoke.
- [C API / FFI preview](c-api.md) documents the unstable G61 ABI-version-0
  target, opaque handles, ownership, error-string, and metric iteration rules.
- [Python automation preview](python-preview.md) documents the G62 default-off
  CLI-backed package, stdlib-only dependency model, and non-goals.
- [Dynamic plugin loader preview](plugin-loader.md) documents the G63 default-off
  trusted-native loader, manifest/export symbols, version checks, unload rules,
  and no-sandbox security model.

## Stub examples

Preview-only stub notes live under `examples/adapters/`. They are intentionally
not built and contain no external SDK includes. The runnable boundary pattern is
`examples/boundary_adapter_pattern.yaml`.

## Policy checks

`policy_no_core_adapter_deps` scans core/source/build files for accidental
adapter SDK symbols such as ROS client-library includes, OpenTelemetry, Prometheus, Perfetto,
`pybind11`, `Python.h`, or native Python bridge tokens in the preview package;
it also checks that `topoexec_runtime` does not link `topoexec_adapter_sdk`,
that `topoexec_adapter_sdk` depends only on runtime, and that optional preview
targets depend outward through the adapter SDK or CLI-backed preview boundary.
It also checks the optional Prometheus, ROS 2, C API, Python, and plugin-loader
preview option-smoke paths, and forbids POSIX dynamic-loader calls from runtime
sources. The policy intentionally ignores docs and preview stub notes
where those names are discussed as deferred dependencies.
