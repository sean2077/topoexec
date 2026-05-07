# Public C++ API

TopoExec is pre-1.0, but embedders should still know which headers are intended product surface and which ones are low-level runtime machinery. Prefer `topoexec::runtime` for embedded C++ applications and add `topoexec::yaml` only when YAML loading or JSON/Mermaid plan helpers are needed.

## CMake Targets

| Target | Intended use | Dependency boundary |
| --- | --- | --- |
| `topoexec::core` | Header-only value types shared by runtime users. | No YAML, CLI, adapter, or tool dependency. |
| `topoexec::runtime` | Components, C++ graph construction, validation, runtime execution, payload helpers, metrics, and traces. | No YAML parser or CLI dependency for pure C++ embedding. |
| `topoexec::yaml` | YAML `schema_version: 1` loading and optional JSON/Mermaid plan helpers. | Depends on `topoexec::runtime` and parser/JSON libraries. |
| `topoexec::adapter_sdk` | Header-only adapter SDK v0 boundary for future adapter packages. | Depends on `topoexec::runtime`; no YAML, CLI, ROS, OpenTelemetry, Prometheus, Python, Perfetto, or plugin-loader dependency. |
| `topoexec::c_api` | Optional unstable C API/FFI preview target. | Depends on `topoexec::runtime`; no YAML, CLI, adapters, Python, dynamic plugin, or ABI-stability promise. Default OFF. |
| `topoexec::plugin_loader` | Optional trusted-native dynamic component loader preview target. | Depends on `topoexec::runtime`; no YAML, CLI, adapters, Python, sandbox, graph discovery, or stable ABI promise. Default OFF. |
| `topoexec_adapters::otel` | Optional OTel exporter preview target. | Depends on `topoexec::adapter_sdk`; no core/runtime reverse dependency and no external telemetry SDK. Default OFF. |
| `topoexec_adapters::prometheus` | Optional Prometheus text exporter preview target. | Depends on `topoexec::adapter_sdk`; no core/runtime reverse dependency, no HTTP server, and no external Prometheus library. Default OFF. |
| `topoexec_adapters::ros2` | Optional ROS 2 fake-boundary preview target. | Depends on `topoexec::adapter_sdk`; no core/runtime reverse dependency, no ROS client library, no executor, and no schema fields. Default OFF. |

Installed package config metadata exposes `TOPOEXEC_VERSION`,
`TOPOEXEC_SCHEMA_VERSION`, `TOPOEXEC_SEMANTIC_CONTRACT_VERSION`,
`TOPOEXEC_HAS_RUNTIME`, `TOPOEXEC_HAS_ADAPTER_SDK`, `TOPOEXEC_HAS_C_API`,
`TOPOEXEC_HAS_PYTHON_PREVIEW`, `TOPOEXEC_HAS_PLUGIN_LOADER`,
`TOPOEXEC_HAS_YAML`, `TOPOEXEC_HAS_OTEL_ADAPTER`, `TOPOEXEC_HAS_PROMETHEUS_ADAPTER`,
`TOPOEXEC_HAS_ROS2_ADAPTER`, `TOPOEXEC_HAS_CLI`, and
`TOPOEXEC_HAS_EXAMPLES` so downstream projects can assert package capabilities
at configure time. The Python automation preview is not a C++ target or native
extension; when enabled, it installs `topoexec_preview` for CLI-backed
automation only. The plugin loader preview is a C++ target and header, but it is
trusted-native-code only and does not promise sandboxing or ABI stability.

## Stability markers

Generated Doxygen output is an optional lookup view over these same installed
headers; it does not change the stability class of any API. See
[doxygen.md](doxygen.md) for the local build target and the Pages `/api/`
publication boundary.

Each installed public header now starts with one of these markers:

- `API stability: stable-v0.2` — intended embedder API for the next `v0.2.0-alpha.0` line. Source compatibility is best-effort through `0.x`; breaking changes need a changelog and versioning note.
- `API stability: mixed` — the header contains a stable subset used by stable APIs plus experimental low-level types.
- `API stability: experimental` — public because tests, advanced examples, or future extension points need it, but normal embedders should avoid depending on details before beta.
- `internal-use-only` — must not be installed under `include/`. Internal headers belong in `src/`, `tools/`, or private build trees.

## Header stability matrix

### Stable-v0.2 headers

These headers are safe for ordinary runtime users to include directly.

| Header | Main surface | Notes |
| --- | --- | --- |
| `topoexec/runtime/status.hpp` | `Status`, `Result<T>` | Status-returning hooks use this instead of exceptions when failures are expected. |
| `topoexec/runtime/clock.hpp` | `TimestampDomain`, `EventTimestamp` | Stable timestamp value types for event-time payloads and policies. |
| `topoexec/runtime/payload.hpp` | Built-in payload variants, schema constants, typed access helpers, payload schema summaries | Custom `OpaquePayload`/`make_custom_payload` are stable enough for in-process embedding; external shared-memory zero-copy remains out of scope. |
| `topoexec/runtime/cancellation.hpp` | `CancellationToken`, `CancellationSource` | Stable cooperative cancellation value types; they report requests and observations without hard preemption. |
| `topoexec/runtime/component.hpp` | `Component`, `ComponentDescriptor`, `PortDescriptor`, `PortMultiplicity`, `GraphContext`, `Invocation`, `InvocationMetadata`, `InputView`, `ConfigView`, `LoopIterationContext`, `LoopConvergenceReport`, publication result | `GraphContext::publish()` stages through the runtime publisher and never calls downstream components directly; descriptor port metadata is optional and semantic-validation only. Solver-style CompositeLoops can report convergence through `GraphContext::report_loop_convergence()`. |
| `topoexec/runtime/component_registry.hpp` | `ComponentRegistry`, `ComponentRegistration`, `ComponentFactory` | Stable registry entry point for embedders and examples. |
| `topoexec/runtime/diagnostics.hpp` | `GraphDiagnosticDescriptor`, `kGraphDiagnosticSchemaVersion`, `graph_diagnostic_registry()` | Stable diagnostic code/category/severity descriptors for editor/tooling integrations. |
| `topoexec/runtime/graph.hpp` | `GraphSpec`, `GraphHierarchyEntry`, edge/channel/trigger policy specs, validation and compile result structs, runtime metric samples, `GraphInputLimits` | The C++ graph model is stable; YAML loader declarations in this header require linking `topoexec::yaml` when called. |
| `topoexec/runtime/graph_builder.hpp` | `GraphBuilder` and helper constructors | Convenience only; it does not create a second graph model. |
| `topoexec/runtime/runtime_runner.hpp` | `RuntimeRunner`, `RuntimeRunnerOptions`, `RuntimeRunnerResult`, `RuntimeTraceEvent`, `RuntimeError`, health event result fields | Primary execution API for embedded applications. |

### Mixed stability headers

| Header | Stable subset | Experimental subset |
| --- | --- | --- |
| `topoexec/runtime/buffer.hpp` | `SharedBuffer`, `FrameView`, `BinaryBlobPayload` support types | `BufferPool`, `BufferPoolConfig`, `LoanedFrame`, and pool metrics are in-process helpers; external allocator/SHM ownership remains out of scope. |
| `topoexec/runtime/scheduler.hpp` | `SchedulerStopToken` and `SchedulerStopReason` as used by `RuntimeRunnerOptions`/`RuntimeRunnerResult` | Direct scheduler classes, lane metric structs, worker-loop details, and lane implementation hooks may change before beta. |

### Experimental headers

| Header | Reason |
| --- | --- |
| `topoexec/runtime/channel.hpp` | Low-level bounded channel bus, publication router, channel read APIs, and channel metrics. Prefer `RuntimeRunner`/`GraphContext` for ordinary embedding. |
| `topoexec/runtime/event_runtime.hpp` | Lower-level event runtime surface used by tests and advanced embedders. |
| `topoexec/runtime/health.hpp` | `HealthEvent` and bounded `HealthEventSink` helpers used by the runtime observer surface. |
| `topoexec/runtime/live_event.hpp` | Low-level fixed-size live observe event records and numeric event identifiers. CLI/live observe JSON schema remains the user-facing contract. |
| `topoexec/runtime/live_observe.hpp` | Low-level bounded live observe transport/session helpers. `RuntimeRunnerOptions::live_observe` and `topoexec graph observe` are the supported integration path. |
| `topoexec/runtime/metric_schema.hpp` | Runtime metric descriptor registry, schema version, and sample validation helpers for exporter-safe cardinality. |
| `topoexec/runtime/state.hpp` | Namespaced blackboard and graph/component config snapshot stores with epoch-boundary commits and experimental config transaction metadata. |
| `topoexec/runtime/task_executor.hpp` | `ITaskExecutor`, `DeterministicTaskExecutor`, compatibility `TaskExecutor`, and opt-in `ThreadedTaskExecutor` preview. |
| `topoexec/runtime/trigger_policy.hpp` | Trigger engine internals and readiness helpers. |
| `topoexec/c_api/topoexec.h` | C API/FFI preview: opaque handles, create/run/destroy, borrowed error strings, and metric iteration for downstream C smoke tests. |
| `topoexec/plugins/loader.hpp` | Dynamic plugin loader preview: manifest/version/schema views, load options/result errors, and trusted native loader entry point. |
| `topoexec/adapters/sdk.hpp` | Adapter SDK v0 preview: observer aliases, `BoundaryBridge`, and `ComponentFactoryProvider` for dependency-free future adapter packages. |
| `topoexec/adapters/otel.hpp` | OTel exporter preview: dependency-free in-memory mapping records over runtime metrics, trace, health, and errors. |
| `topoexec/adapters/prometheus.hpp` | Prometheus exporter preview: dependency-free text exposition mapping over runtime metric descriptors and histogram summaries. |
| `topoexec/adapters/ros2.hpp` | ROS 2 adapter preview: dependency-free topic/service/action endpoint mapping and fake boundary bridges over Adapter SDK types. |
| `topoexec/common/metrics.hpp` | Small metrics registry/value helpers; runtime metric schema descriptors live in `topoexec/runtime/metric_schema.hpp`. |
| `topoexec/common/logging.hpp` | Structured logging helper; adapter/exporter boundary is not stable yet. |
| `topoexec/common/trace.hpp` | Trace collection helper used by the runtime; public timeline fields are exposed through `RuntimeTraceEvent`. |

No installed header is intentionally `internal-use-only`. If future work needs internal-only declarations, place them outside `include/` or stop installing them.

## Function and class stability

| Surface | Stability | Compatibility expectation |
| --- | --- | --- |
| `Component::configure/activate/execute/deactivate` plus status variants | stable-v0.2 | Existing hook meanings should not silently change. G43 reset/pause/resume/snapshot/restore hooks and G44 validate/apply config hooks are additive and experimental until lifecycle/config policy is stable. |
| `GraphContext::publish()` and `publish_shared()` | stable-v0.2 | Publication remains staged/routed by runtime; no direct downstream calls. |
| `GraphContext::loop_iteration` and `report_loop_convergence()` | experimental | In-process solver-style CompositeLoop hook. Components can report `converged`, optional `residual`, and a bounded reason string; dynamic solver plugins remain out of scope. |
| `Invocation`, `InvocationMetadata`, `InputView`, typed payload helpers | stable-v0.2 | Existing payload lookup and typed access behavior should remain source-compatible; metadata fields are additive trace/debug context and `Invocation::cancel_requested()` is cooperative. |
| `CancellationToken` / `CancellationSource` | stable-v0.2 | Cancellation requests are observable by components, tasks, and loops; observation is metric/trace evidence, not forced termination. |
| `ComponentRegistry::register_component/create/metadata/types` | stable-v0.2 | Registration metadata can gain additive fields. |
| `GraphSpec`, `LaneSpec`, `EdgeSpec`, `TriggerPolicySpec`, `CompositeLoopSpec`, `GraphHierarchyEntry` | stable-v0.2 for current fields | Additive fields are allowed only when schema/runtime meaning stays compatible. `LoopPolicySpec::solver_iteration`, residual threshold, and partial-success fields are experimental G45 additions. `GraphHierarchyEntry` records compile-time `subgraphs[]` expansion metadata only; it does not imply runtime nesting. Trigger v2 `watermark`, `condition`, `debounce`, and `rate_limit` policy fields are additive preview fields and may be refined before beta. |
| `GraphInputLimits`, `default_graph_input_limits()`, `load_graph_text(..., limits)`, `load_graph_file(..., limits)` | stable-v0.2 | Parser-limit fields can be tightened by embedders and CLI tooling; defaults should remain conservative and source-compatible. |
| YAML `templates[]` / `template_instances[]` | stable-v0.2 schema-loader surface | Template definitions are not retained in `GraphSpec`; they expand through strict parameter substitution before validation/runtime execution. |
| `validate_graph`, `compile_graph`, `GraphDiagnostic` | stable-v0.2 | New diagnostics may be added; existing codes should keep meanings. |
| `RuntimeRunner::run()`, `RuntimeRunnerOptions`, and `RuntimeRunnerResult` | stable-v0.2 | New result fields may be added; existing counters, observer registration, trace vectors, metric vectors, health event vectors, and error fields should keep meanings. |
| `RuntimeObserver`, `ResultSink`, `MetricSink`, `TraceSink`, `NoopRuntimeObserver`, `InMemoryRuntimeObserver` | stable-v0.2 | Observer callbacks are best-effort result/metric/trace/health/error delivery for adapters. Callback failures are reported as observer diagnostics, not runtime semantic failures. |
| `RuntimeMetricDescriptor`, `runtime_metric_descriptors()`, `validate_runtime_metric_samples()` | stable-v0.2 | Metric names, kind/unit metadata, allowed labels, and descriptor schema version are the exporter-safe contract. Add names rather than changing meanings. |
| `RuntimeTraceEvent` and `kRuntimeTraceSchemaVersion` | stable-v0.2 | Trace schema version `1` events expose ordered timeline fields plus phase/component/channel/lane/worker/epoch/transaction/correlation/causation identifiers. A compatibility constructor preserves the prior name/trace-id/timing/attributes shape; add fields or event names rather than changing existing meanings. |
| `SchedulerStopSource`/`SchedulerStopToken` | stable-v0.2 through runner options | Direct scheduler registry/metrics internals remain experimental. |
| `ComponentStateSnapshot`, reset/snapshot/restore runner options/results | experimental | Stateful lifecycle support is start/end-boundary only; pause/resume policy and hot live control may change before beta. |
| `RuntimeStateStore`, `ConfigSnapshotStore` | experimental | State snapshots and config transactions are epoch-boundary, observable APIs; transaction metadata and immediate-update escape hatches may be reshaped before beta. |
| `ITaskExecutor`, `DeterministicTaskExecutor`, `TaskExecutor`, `ThreadedTaskExecutor` | experimental | The deterministic compatibility name remains available; threaded executor preview shutdown/admission details may change before beta. |
| `RuntimeChannelBus`, `RuntimePublicationRouter`, `TriggerPolicyEngine`, `EventRuntime` | experimental | Advanced runtime internals may change as scheduler/channel/trigger v2 goals land. |
| `topoexec/c_api/topoexec.h` opaque handles and functions | experimental | G61 C API/FFI preview, ABI version `0`. Names, ownership details, and exported functions may change before any stable ABI promise. |
| `topoexec::plugins::load_plugin`, `LoadedPlugin`, and manifest view structs | experimental | G63 plugin loader preview, plugin API version `0`. The loader requires explicit paths and trusted native code, validates manifest/schema/component descriptors, defaults to no `dlclose`, and may change before any stable plugin ABI. |
| `topoexec::adapters::BoundaryBridge`, `BoundaryMessage`, `BoundaryPollResult`, `BoundaryBridgeStatus`, `ComponentFactoryProvider` | experimental | Adapter SDK v0 is a header-only boundary. Bridges are bounded/best-effort and providers register components explicitly; concrete adapter packages and dynamic discovery remain future work. |
| `topoexec::adapters::otel::ExporterPreview` and preview record structs | experimental | G58 dependency-free OTel-shaped mapping over the observer API. Record names and options may change before production exporter work. |
| `topoexec::adapters::prometheus::TextExporterPreview` | experimental | G59 dependency-free Prometheus text exposition mapping over metric descriptors and custom histogram summaries. Text names/options may change before production exporter work. |
| `topoexec::adapters::ros2::*` preview endpoint and fake bridge types | experimental | G60 dependency-free ROS 2 boundary mapping preview. Names/options may change before any real ROS package or client-library integration. |

## Compatibility Expectations

- Source compatibility in `0.x` is best-effort for `stable-v0.2` headers.
- Binary compatibility is not promised before `1.0.0`; rebuild downstream applications when upgrading.
- Stable enum names, field names, and result field meanings should not change inside a patch release.
- Additive fields and metrics may appear in minor releases.
- Experimental headers may change in minor releases, but changes should still be documented.

## Deprecation policy

Before `1.0.0`, TopoExec still optimizes for semantic clarity over permanent
compatibility. The beta-readiness bar is therefore a visible deprecation path,
not an ABI-freeze claim:

- `stable-v0.2` headers and documented CLI JSON/schema fields should receive at
  least one prerelease/minor-line deprecation note before removal or rename when
  practical. Immediate removal is reserved for security, data-corruption,
  unsound semantic, or build-breaking defects.
- Deprecated stable APIs should name the replacement in `CHANGELOG.md` and this
  API map or the relevant reference page. Keep source compatibility through the
  next prerelease line when the compatibility shim is small and behaviorally
  honest.
- Mixed headers follow the stable policy for their stable subset; explicitly
  experimental classes, fields, low-level scheduler/channel hooks, lifecycle
  extensions, and adapter-preview helpers may change with a changelog note and
  no long deprecation window.
- CLI JSON and schema-compatible surfaces should prefer additive fields. Removing
  or renaming stable fields requires a changelog and versioning note; schema
  semantic changes require the schema/semantic-contract rules below.
- Human-readable CLI output, examples, and docs may evolve more freely, but
  changes that alter a documented behavior claim still need a release note.

## Schema and semantic compatibility

Schema v1 is strict and compatibility-preserving:

- Additive fields are allowed only when they do not change v1 meaning.
- Unknown fields remain invalid.
- Breaking semantic changes require a schema version bump.
- New adapter-specific fields should not be added to core schema v1 unless they are useful without that adapter.

A schema version bump is required when a graph that was valid under the old schema would be rejected or would mean something different because of:

- a removed or renamed root field;
- a removed or renamed enum value;
- a new required field without a backward-compatible default;
- changed edge visibility, trigger readiness, channel overflow, or loop ownership semantics;
- incompatible validation behavior for existing graph files.

Additive optional fields with documented defaults can remain in schema v1.
[Schema v2 notes](../33-specs-rfcs/schema-v2-notes.md) are the review gate for candidates such
as runtime-nested subgraphs, YAML-authored typed ports, graph-declared adapter
or plugin/package discovery, arbitrary trigger expressions, and schema-selected
runtime behavior.

## CLI JSON Compatibility

CLI JSON fields are part of the user-facing tooling contract even though the CLI implementation is not an embedder API.

- Stable commands: `plan`, `metrics`, `trace`, `schema dump`, `schema check`, `doctor`, and `diff-plan` should keep existing field names and JSON value types within a minor release. `metrics` and `trace` JSON include explicit schema-version fields for exporter compatibility.
- `bench` JSON is machine-readable but still experimental; add fields instead of changing existing field meanings where practical.
- New fields are allowed. Removing or renaming fields requires a changelog note and, when schema-related, a versioning note.
- Human-readable text output is allowed to evolve more freely than JSON.
- Golden drift coverage lives in `tests/golden/` for plan, metrics, trace, Chrome trace, render, schema dump, and doctor JSON. Benchmark JSON has separate output-contract coverage in `tests/bench/check_bench_contract.py` because timings are intentionally volatile.

## Adapter-preview stability

G57 establishes Adapter SDK v0 as a preview/dependency-free boundary:

- Core/runtime headers must not include ROS 2 client-library, OpenTelemetry, Prometheus, Python, Perfetto, dynamic-loader APIs, plugin-loader headers, or `topoexec/adapters/*`.
- `ResultSink`, `RuntimeObserver`, `MetricSink`, `TraceSink`, and `InMemoryRuntimeObserver` remain the stable-v0.2 in-process observer surface; `topoexec/adapters/sdk.hpp` re-exports them under `topoexec::adapters` for adapter authors.
- `topoexec::adapter_sdk` is a header-only interface target that depends on `topoexec::runtime`; `topoexec::runtime` does not depend on it.
- `topoexec_adapters::otel` is a default-off preview target that depends on
  `topoexec::adapter_sdk` and maps observer/result records without linking an
  external telemetry SDK.
- `topoexec_adapters::prometheus` is a default-off preview target that depends
  on `topoexec::adapter_sdk` and renders metric records as text exposition
  without starting an HTTP server or linking an external Prometheus library.
- `topoexec_adapters::ros2` is default-off and only validates adapter-side
  endpoint/QoS mapping plus fake boundary bridges; it does not create ROS nodes
  or link ROS packages.
- `topoexec::plugin_loader` is default-off and only loads trusted native plugins
  from explicit paths after manifest, plugin API version, schema version, and
  descriptor checks; it does not add schema fields, sandboxing, package
  discovery, or stable ABI promises.
- `BoundaryBridge` is bounded/best-effort and must not directly affect runtime scheduling. Bridge failures are adapter health/diagnostic evidence unless represented as ordinary graph boundary input/output.
- `ComponentFactoryProvider` registers explicit in-process factories into
  `ComponentRegistry`. G63 dynamic loading is a preview alternative for trusted
  native components only; production package discovery, sandboxing, stable ABI,
  telemetry exporters, and network transports remain future work.

## Pure runtime embedding smoke

The pure C++ path links only `topoexec::runtime`:

```cmake
find_package(topoexec CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE topoexec::runtime)
```

Use `GraphBuilder` or direct `GraphSpec` construction, register components in a `ComponentRegistry`, then call `RuntimeRunner::run()`. The package smoke under `tests/cmake/runtime_smoke` compiles this path against only `topoexec::runtime` after install and now verifies:

- minimal app graph construction;
- component registry registration/creation;
- GraphBuilder helpers;
- typed payload publication and consumption;
- RuntimeRunner execution;
- `RuntimeRunnerResult` metrics and trace consumption.

## Example header boundary

Ordinary embedding examples should include only stable headers:

- `examples/apps/minimal_pipeline`
- `examples/apps/control_feedback_delay`
- `examples/apps/composite_loop_fixed_point`
- `examples/apps/async_worker`
- `examples/apps/cpp_builder_minimal`

`examples/apps/overload_latest_vs_queue` is intentionally marked as an advanced low-level channel-policy tutorial because it uses `topoexec/runtime/channel.hpp` directly.
`examples/apps/robot_cell_pilot` is an advanced pilot case study: it still links
only `topoexec::runtime`, but it intentionally composes stable, mixed, and
experimental runtime surfaces such as `BufferPool` and config/state snapshots.

## Component failure model

Existing components can keep overriding the original `void` hooks:

```cpp
void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override;
void execute(const topoexec::Invocation&, topoexec::GraphContext&) override;
```

For non-exception error reporting, override the status hooks:

```cpp
topoexec::Status execute_status(const topoexec::Invocation& invocation,
                                topoexec::GraphContext& ctx) override {
  if (bad_input) {
    return topoexec::Status::error("bad input");
  }
  return topoexec::Status::success();
}
```

`RuntimeRunnerResult::errors` preserves legacy human-readable configure, activate, execute, and deactivate failure strings. `RuntimeRunnerResult::runtime_errors` is the structured API for new callers and records phase, component id, lane when known, message, code, trace id when known, and fatality. The runner deactivates already-started components after stop-token shutdown and component errors.

## Graph diagnostics

`GraphValidationResult` and `GraphCompileResult` preserve legacy `errors` strings and also expose `diagnostics[]` with `code`, `severity`, `category`, `message`, optional graph path/involved ids, and `suggested_fix`. New tooling should prefer diagnostics while keeping `errors` for human-readable compatibility. Stable code descriptors and diagnostic schema version `1` live in `topoexec/runtime/diagnostics.hpp` and are documented in [diagnostics.md](../62-schemas-protocols/diagnostics.md).

## API change checklist

Use [api-change-checklist.md](api-change-checklist.md) before modifying installed headers, CLI JSON, schema fields, or adapter-preview boundaries; schema-field proposals must also classify v1 vs v2 impact in [schema-v2-notes.md](../33-specs-rfcs/schema-v2-notes.md).
