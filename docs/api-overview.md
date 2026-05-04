# API Overview

TopoExec's public C++ surface is split into two CMake targets:

- `topoexec::runtime`: component, graph model, channel, scheduler, runtime runner, metrics, trace, and the C++ graph builder.
- `topoexec::yaml`: optional YAML graph loading and JSON plan rendering helpers.

Use `topoexec::runtime` when an application builds `GraphSpec` directly in C++ and does not need YAML parsing.

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
