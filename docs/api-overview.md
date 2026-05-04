# API Overview

TopoExec's public C++ surface is split into two CMake targets:

- `topoexec::runtime`: component, graph model, channel, scheduler, runtime runner, metrics, trace, and the C++ graph builder.
- `topoexec::yaml`: optional YAML graph loading and JSON plan rendering helpers.

Use `topoexec::runtime` when an application builds `GraphSpec` directly in C++ and does not need YAML parsing.

For stability categories and lifecycle failure behavior, see [public-api.md](public-api.md). For payload ownership and typed access helpers, see [payloads.md](payloads.md).

## Minimal Runtime Shape

Application code usually provides three pieces:

1. `Component` subclasses with descriptors and `execute()` implementations.
2. A `ComponentRegistry` mapping descriptor types to factories.
3. A `GraphSpec`, either built directly in C++ or loaded through the optional YAML helpers.

The C++ builder API in `topoexec/runtime/graph_builder.hpp` is a thin convenience layer over `GraphSpec`. It does not hide the runtime model or introduce a second graph representation.

```cpp
auto graph = topoexec::GraphBuilder("pipeline")
                 .event_loop_lane("main")
                 .component(topoexec::component_node(
                     "source", "app.Source", {topoexec::manual_event_source()},
                     topoexec::manual_trigger(), topoexec::lane_execution("main")))
                 .component(topoexec::component_node(
                     "sink", "app.Sink", {topoexec::message_event_source({"in"})},
                     topoexec::any_input_trigger({"in"}), topoexec::lane_execution("main")))
                 .edge(topoexec::immediate_edge("source_sink", "source.out", "sink.in"))
                 .build();
```

`examples/apps/cpp_builder_minimal` is the runnable version of this pattern and links only `topoexec_runtime`.

## Failure Reporting

The original `void` lifecycle and `execute()` hooks remain supported. Components may also override status-returning hooks such as `execute_status()` to report failures without throwing:

```cpp
topoexec::Status execute_status(const topoexec::Invocation& invocation,
                                topoexec::GraphContext& ctx) override {
  if (invocation.payload == nullptr) {
    return topoexec::Status::error("missing payload");
  }
  return topoexec::Status::success();
}
```

`RuntimeRunnerResult::errors` records configure, activate, execute, and deactivate failures with component ids. Started components are deactivated on stop and component error paths.

## Payload Access

Components should prefer typed helpers over manual `std::variant` access:

```cpp
const auto& text = invocation.payload_as<topoexec::TextPayload>();
const auto* frame = invocation.try_payload_as<topoexec::FrameView>();
```

Bad typed access raises a clear `std::runtime_error`; status-returning components can translate that into `Status::error(...)`.
