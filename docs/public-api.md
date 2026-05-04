# Public C++ API

TopoExec is pre-1.0, but the alpha API has explicit stability boundaries. Prefer `topoexec::runtime` for embedded C++ applications and add `topoexec::yaml` only when YAML loading or JSON plan helpers are needed.

## Stable For 0.1.x

- `topoexec/runtime/component.hpp`: `Component`, `ComponentDescriptor`, `GraphContext`, `Invocation`, lifecycle hooks, and status-returning lifecycle adapters.
- `topoexec/runtime/component_registry.hpp`: static component factories and metadata.
- `topoexec/runtime/graph.hpp`: `GraphSpec`, validation results, compiled plan structures, runtime metrics, and graph rendering helpers.
- `topoexec/runtime/graph_builder.hpp`: C++ builder helpers over `GraphSpec`.
- `topoexec/runtime/runtime_runner.hpp`: `RuntimeRunner`, `RuntimeRunnerOptions`, and `RuntimeRunnerResult`.
- `topoexec/runtime/payload.hpp`: built-in payload variants and typed access helpers.

These headers are safe for alpha users to include directly. Additive fields may appear in 0.1.x, but existing meaning should not change without a version note.

## Experimental Before Beta

- `topoexec/runtime/event_runtime.hpp`: lower-level event loop surface used by tests and advanced embedders.
- `topoexec/runtime/channel.hpp`: channel internals are usable, but publication routing and metrics may still grow.
- `topoexec/runtime/scheduler.hpp`: lane data and metrics are stable enough to observe; worker-pool execution is not implemented.
- `topoexec/common/metrics.hpp`, `logging.hpp`, and `trace.hpp`: small utility APIs that may gain exporters later.

## Internal Or Optional

- `topoexec::yaml` and `tools/topoexec` are optional adapter/tooling surfaces. They must not be required for pure C++ runtime embedding.
- `src/*` files are implementation details. Do not rely on concrete runtime classes beyond installed headers.

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

`RuntimeRunnerResult::errors` records configure, activate, execute, and deactivate failures with the component id. The runner deactivates already-started components after stop-token shutdown and component errors.

## Pure Runtime Embedding

The pure C++ path links only `topoexec::runtime`:

```cmake
find_package(topoexec CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE topoexec::runtime)
```

Use `GraphBuilder` or direct `GraphSpec` construction, register components in a `ComponentRegistry`, then call `RuntimeRunner::run()`. See `examples/apps/cpp_builder_minimal`.
