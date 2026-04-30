# TopoExec

TopoExec is a C++20 single-process stateful dataflow runtime. It focuses on a declarative graph contract, validation, channel policy, trigger policy, compiled execution regions, and runtime observability for applications that need reusable in-process orchestration.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Dependencies are CMake, a C++20 compiler, `yaml-cpp`, `CLI11`, `nlohmann_json`, and GTest for tests. If GTest is not installed, the build fetches it through CMake `FetchContent`.

## CLI

```bash
topoexec graph validate examples/minimal.yaml
topoexec graph plan examples/composite_loop.yaml --format json
topoexec graph render examples/composite_loop.yaml --format mermaid
```

The CLI validates `schema_version: 1` graphs and can emit text, JSON, or Mermaid views of components, edges, CompositeLoop regions, region order, boundary roles, channel policy, and validation errors.

## Included

- `include/topoexec/common/`: logging, metrics, trace.
- `include/topoexec/runtime/`: graph, component, static registry, channel, payload, trigger policy, event runtime, scheduler, runner.
- `tools/topoexec/`: C++ CLI for graph validation, plan output, and Mermaid rendering.
- `examples/`: minimal graph, CompositeLoop graph, and an invalid-schema fixture.
