# Public C++ API

TopoExec is pre-1.0, but embedders should still know which headers are intended product surface and which ones are low-level runtime machinery. Prefer `topoexec::runtime` for embedded C++ applications and add `topoexec::yaml` only when YAML loading or JSON plan helpers are needed.

## CMake Targets

| Target | Intended use | Dependency boundary |
| --- | --- | --- |
| `topoexec::core` | Header-only value types shared by runtime users. | No YAML, CLI, adapter, or tool dependency. |
| `topoexec::runtime` | Components, C++ graph construction, validation, runtime execution, payload helpers, metrics, and traces. | No YAML parser or CLI dependency for pure C++ embedding. |
| `topoexec::yaml` | YAML `schema_version: 1` loading and optional JSON/Mermaid plan helpers. | Depends on `topoexec::runtime` and parser/JSON libraries. |

## Stable For 0.x Embedders

These headers are safe for ordinary runtime users to include directly. In `0.x`, source compatibility is best-effort and additive fields may appear, but existing behavior should not change without a changelog and versioning note.

| Header | Main surface | Notes |
| --- | --- | --- |
| `topoexec/runtime/status.hpp` | `Status`, `Result<T>` | Status-returning hooks use this instead of exceptions when failures are expected. |
| `topoexec/runtime/clock.hpp` | `TimestampDomain`, `EventTimestamp` | Stable timestamp value types for event-time payloads and policies. |
| `topoexec/runtime/payload.hpp` | Built-in payload variants, schema constants, typed access helpers | Uses `FrameView` and `SharedBuffer` from `buffer.hpp`; custom payload extension remains future work. |
| `topoexec/runtime/component.hpp` | `Component`, `ComponentDescriptor`, `GraphContext`, `Invocation`, `InputView`, publication result | `GraphContext::publish()` stages through the runtime publisher and returns an observable publish result. |
| `topoexec/runtime/component_registry.hpp` | Component factory registration and lookup | Stable registry entry point for embedders and examples. |
| `topoexec/runtime/graph.hpp` | `GraphSpec`, edge/channel/trigger policy specs, validation and compile result structs | C++ graph model is stable. YAML loader declarations in this header require linking `topoexec::yaml`. |
| `topoexec/runtime/graph_builder.hpp` | Thin C++ helpers over `GraphSpec` | Convenience only; it does not create a second graph model. |
| `topoexec/runtime/runtime_runner.hpp` | `RuntimeRunner`, `RuntimeRunnerOptions`, `RuntimeRunnerResult`, `RuntimeTraceEvent` | Primary execution API for embedded applications. |

## Mixed Stability Headers

These headers contain at least one stable type used by stable APIs plus experimental or low-level details.

| Header | Stable subset | Experimental subset |
| --- | --- | --- |
| `topoexec/runtime/buffer.hpp` | `SharedBuffer`, `FrameView` value shapes used by built-in payloads | `BufferPool` and `LoanedFrame` remain prototype-level until payload/loaned-buffer goals are complete. |
| `topoexec/runtime/scheduler.hpp` | `SchedulerStopToken`, `SchedulerStopReason`, lane metric structs observed through `RuntimeRunnerResult` | Direct scheduler classes, worker-loop details, and lane implementation hooks may change before beta. |

## Experimental Before Beta

These headers are public because tests, advanced examples, or future extension points need them, but normal embedders should avoid depending on their details unless they accept churn.

| Header | Reason |
| --- | --- |
| `topoexec/runtime/channel.hpp` | Low-level bounded channel bus, publication router, and channel metrics. The overload tutorial uses it as an advanced channel-policy example. |
| `topoexec/runtime/event_runtime.hpp` | Lower-level event runtime surface used by tests and advanced embedders. |
| `topoexec/runtime/trigger_policy.hpp` | Trigger engine internals and readiness helpers. |
| `topoexec/common/metrics.hpp` | Small metrics registry/value helpers that may gain sinks/exporters later. |
| `topoexec/common/logging.hpp` | Structured logging helper; adapter/exporter boundary is not stable yet. |
| `topoexec/common/trace.hpp` | Trace collection helper; exporter mapping remains optional tooling. |

## Internal Or Tooling-Only

- `src/*` files are implementation details.
- `tools/topoexec/*` is CLI implementation, not embedder API.
- YAML parser implementation details are internal to `topoexec::yaml`.
- `tests/*`, `examples/*`, and generated CMake package files are not API contracts.

## Compatibility Expectations

- Source compatibility in `0.x` is best-effort for stable headers.
- Binary compatibility is not promised before `1.0.0`; rebuild downstream applications when upgrading.
- Stable enum names, field names, and result field meanings should not change inside a patch release.
- Additive fields and metrics may appear in minor releases.
- Experimental headers may change in minor releases, but changes should still be documented.

## Schema Bump Rules

A schema version bump is required when a graph that was valid under the old schema would be rejected or would mean something different because of:

- a removed or renamed root field;
- a removed or renamed enum value;
- a new required field without a backward-compatible default;
- changed edge visibility, trigger readiness, channel overflow, or loop ownership semantics;
- incompatible validation behavior for existing graph files.

Additive optional fields with documented defaults can remain in schema v1.

## CLI JSON Compatibility

CLI JSON fields are part of the user-facing tooling contract even though the CLI implementation is not an embedder API.

- Stable commands: `plan`, `metrics`, `trace`, and `diff-plan` should keep existing field names and JSON value types within a minor release.
- `bench` JSON is machine-readable but still experimental; add fields instead of changing existing field meanings where practical.
- New fields are allowed. Removing or renaming fields requires a changelog note and, when schema-related, a versioning note.
- Human-readable text output is allowed to evolve more freely than JSON.

## Example Header Boundary

Ordinary embedding examples should include only stable headers:

- `examples/apps/minimal_pipeline`
- `examples/apps/control_feedback_delay`
- `examples/apps/composite_loop_fixed_point`
- `examples/apps/async_worker`
- `examples/apps/cpp_builder_minimal`

`examples/apps/overload_latest_vs_queue` is intentionally marked as an advanced low-level channel-policy tutorial because it uses `topoexec/runtime/channel.hpp` directly.

## Component Failure Model

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

## Pure Runtime Embedding

The pure C++ path links only `topoexec::runtime`:

```cmake
find_package(topoexec CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE topoexec::runtime)
```

Use `GraphBuilder` or direct `GraphSpec` construction, register components in a `ComponentRegistry`, then call `RuntimeRunner::run()`. The package smoke under `tests/cmake/runtime_smoke` compiles this path against only `topoexec::runtime` after install, and `examples/apps/cpp_builder_minimal` shows a larger app-local variant.


## Graph diagnostics

`GraphValidationResult` and `GraphCompileResult` preserve legacy `errors` strings and also expose `diagnostics[]` with `code`, `severity`, `message`, optional graph path/involved ids, and `suggested_fix`. New tooling should prefer diagnostics while keeping `errors` for human-readable compatibility.
