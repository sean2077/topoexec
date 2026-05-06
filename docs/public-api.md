# Public C++ API

TopoExec is pre-1.0, but embedders should still know which headers are intended product surface and which ones are low-level runtime machinery. Prefer `topoexec::runtime` for embedded C++ applications and add `topoexec::yaml` only when YAML loading or JSON/Mermaid plan helpers are needed.

## CMake Targets

| Target | Intended use | Dependency boundary |
| --- | --- | --- |
| `topoexec::core` | Header-only value types shared by runtime users. | No YAML, CLI, adapter, or tool dependency. |
| `topoexec::runtime` | Components, C++ graph construction, validation, runtime execution, payload helpers, metrics, and traces. | No YAML parser or CLI dependency for pure C++ embedding. |
| `topoexec::yaml` | YAML `schema_version: 1` loading and optional JSON/Mermaid plan helpers. | Depends on `topoexec::runtime` and parser/JSON libraries. |
| `topoexec::adapter_sdk` | Header-only adapter SDK v0 boundary for future adapter packages. | Depends on `topoexec::runtime`; no YAML, CLI, ROS, OpenTelemetry, Prometheus, Python, Perfetto, or plugin-loader dependency. |

Installed package config metadata exposes `TOPOEXEC_VERSION`,
`TOPOEXEC_SCHEMA_VERSION`, `TOPOEXEC_SEMANTIC_CONTRACT_VERSION`,
`TOPOEXEC_HAS_RUNTIME`, `TOPOEXEC_HAS_ADAPTER_SDK`, `TOPOEXEC_HAS_YAML`,
`TOPOEXEC_HAS_CLI`, and `TOPOEXEC_HAS_EXAMPLES` so downstream projects can assert package capabilities
at configure time.

## Stability markers

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
| `topoexec/runtime/component.hpp` | `Component`, `ComponentDescriptor`, `PortDescriptor`, `PortMultiplicity`, `GraphContext`, `Invocation`, `InvocationMetadata`, `InputView`, `ConfigView`, publication result | `GraphContext::publish()` stages through the runtime publisher and never calls downstream components directly; descriptor port metadata is optional and semantic-validation only. |
| `topoexec/runtime/component_registry.hpp` | `ComponentRegistry`, `ComponentRegistration`, `ComponentFactory` | Stable registry entry point for embedders and examples. |
| `topoexec/runtime/diagnostics.hpp` | `GraphDiagnosticDescriptor`, `kGraphDiagnosticSchemaVersion`, `graph_diagnostic_registry()` | Stable diagnostic code/category/severity descriptors for editor/tooling integrations. |
| `topoexec/runtime/graph.hpp` | `GraphSpec`, edge/channel/trigger policy specs, validation and compile result structs, runtime metric samples, `GraphInputLimits` | The C++ graph model is stable; YAML loader declarations in this header require linking `topoexec::yaml` when called. |
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
| `topoexec/runtime/metric_schema.hpp` | Runtime metric descriptor registry, schema version, and sample validation helpers for exporter-safe cardinality. |
| `topoexec/runtime/state.hpp` | Namespaced blackboard and graph/component config snapshot stores with epoch-boundary commits and experimental config transaction metadata. |
| `topoexec/runtime/task_executor.hpp` | `ITaskExecutor`, `DeterministicTaskExecutor`, compatibility `TaskExecutor`, and opt-in `ThreadedTaskExecutor` preview. |
| `topoexec/runtime/trigger_policy.hpp` | Trigger engine internals and readiness helpers. |
| `topoexec/adapters/sdk.hpp` | Adapter SDK v0 preview: observer aliases, `BoundaryBridge`, and `ComponentFactoryProvider` for dependency-free future adapter packages. |
| `topoexec/common/metrics.hpp` | Small metrics registry/value helpers; runtime metric schema descriptors live in `topoexec/runtime/metric_schema.hpp`. |
| `topoexec/common/logging.hpp` | Structured logging helper; adapter/exporter boundary is not stable yet. |
| `topoexec/common/trace.hpp` | Trace collection helper used by the runtime; public timeline fields are exposed through `RuntimeTraceEvent`. |

No installed header is intentionally `internal-use-only`. If future work needs internal-only declarations, place them outside `include/` or stop installing them.

## Function and class stability

| Surface | Stability | Compatibility expectation |
| --- | --- | --- |
| `Component::configure/activate/execute/deactivate` plus status variants | stable-v0.2 | Existing hook meanings should not silently change. G43 reset/pause/resume/snapshot/restore hooks and G44 validate/apply config hooks are additive and experimental until lifecycle/config policy is stable. |
| `GraphContext::publish()` and `publish_shared()` | stable-v0.2 | Publication remains staged/routed by runtime; no direct downstream calls. |
| `Invocation`, `InvocationMetadata`, `InputView`, typed payload helpers | stable-v0.2 | Existing payload lookup and typed access behavior should remain source-compatible; metadata fields are additive trace/debug context and `Invocation::cancel_requested()` is cooperative. |
| `CancellationToken` / `CancellationSource` | stable-v0.2 | Cancellation requests are observable by components, tasks, and loops; observation is metric/trace evidence, not forced termination. |
| `ComponentRegistry::register_component/create/metadata/types` | stable-v0.2 | Registration metadata can gain additive fields. |
| `GraphSpec`, `LaneSpec`, `EdgeSpec`, `TriggerPolicySpec`, `CompositeLoopSpec` | stable-v0.2 for current fields | Additive fields are allowed only when schema/runtime meaning stays compatible. Trigger v2 `watermark`, `condition`, `debounce`, and `rate_limit` policy fields are additive preview fields and may be refined before beta. |
| `GraphInputLimits`, `default_graph_input_limits()`, `load_graph_text(..., limits)`, `load_graph_file(..., limits)` | stable-v0.2 | Parser-limit fields can be tightened by embedders and CLI tooling; defaults should remain conservative and source-compatible. |
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
| `topoexec::adapters::BoundaryBridge`, `BoundaryMessage`, `BoundaryPollResult`, `BoundaryBridgeStatus`, `ComponentFactoryProvider` | experimental | Adapter SDK v0 is a header-only boundary. Bridges are bounded/best-effort and providers register components explicitly; concrete adapter packages and dynamic discovery remain future work. |

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

## CLI JSON Compatibility

CLI JSON fields are part of the user-facing tooling contract even though the CLI implementation is not an embedder API.

- Stable commands: `plan`, `metrics`, `trace`, `schema dump`, `schema check`, `doctor`, and `diff-plan` should keep existing field names and JSON value types within a minor release. `metrics` and `trace` JSON include explicit schema-version fields for exporter compatibility.
- `bench` JSON is machine-readable but still experimental; add fields instead of changing existing field meanings where practical.
- New fields are allowed. Removing or renaming fields requires a changelog note and, when schema-related, a versioning note.
- Human-readable text output is allowed to evolve more freely than JSON.
- Golden drift coverage lives in `tests/golden/` for plan, metrics, trace, Chrome trace, render, schema dump, and doctor JSON. Benchmark JSON has separate output-contract coverage in `tests/bench/check_bench_contract.py` because timings are intentionally volatile.

## Adapter-preview stability

G57 establishes Adapter SDK v0 as a preview/dependency-free boundary:

- Core/runtime headers must not include ROS 2, OpenTelemetry, Prometheus, Python, Perfetto, dynamic plugin SDK headers, or `topoexec/adapters/*`.
- `ResultSink`, `RuntimeObserver`, `MetricSink`, `TraceSink`, and `InMemoryRuntimeObserver` remain the stable-v0.2 in-process observer surface; `topoexec/adapters/sdk.hpp` re-exports them under `topoexec::adapters` for adapter authors.
- `topoexec::adapter_sdk` is a header-only interface target that depends on `topoexec::runtime`; `topoexec::runtime` does not depend on it.
- `BoundaryBridge` is bounded/best-effort and must not directly affect runtime scheduling. Bridge failures are adapter health/diagnostic evidence unless represented as ordinary graph boundary input/output.
- `ComponentFactoryProvider` registers explicit in-process factories into `ComponentRegistry`; dynamic discovery, ABI policy, sandboxing, and concrete exporters remain future work.

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

`GraphValidationResult` and `GraphCompileResult` preserve legacy `errors` strings and also expose `diagnostics[]` with `code`, `severity`, `category`, `message`, optional graph path/involved ids, and `suggested_fix`. New tooling should prefer diagnostics while keeping `errors` for human-readable compatibility. Stable code descriptors and diagnostic schema version `1` live in `topoexec/runtime/diagnostics.hpp` and are documented in [diagnostics.md](diagnostics.md).

## API change checklist

Use [api-change-checklist.md](api-change-checklist.md) before modifying installed headers, CLI JSON, schema fields, or adapter-preview boundaries.
