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
topoexec graph run examples/minimal.yaml --steps 1
topoexec graph run examples/minimal.yaml --steps 10 --until-idle
topoexec graph metrics examples/minimal.yaml --steps 1 --format json
topoexec graph trace examples/minimal.yaml --steps 1
topoexec graph lint examples/control_feedback_delay.yaml
topoexec graph explain examples/minimal.yaml
topoexec graph diff-plan examples/minimal.yaml examples/control_feedback_delay.yaml
topoexec graph bench examples/minimal.yaml --steps 1 --runs 2
```

The CLI validates `schema_version: 1` graphs, emits text/JSON/Mermaid views, runs demo graphs, prints metrics/trace events, and provides lightweight lint/explain/diff/bench output derived from the runtime contract.

## Runtime semantics

TopoExec's user-visible execution contract is documented in [docs/runtime-semantics.md](docs/runtime-semantics.md). Schema v1 details are in [docs/schema-v1.md](docs/schema-v1.md), and the `docs/spec.md` implementation audit is captured in [docs/spec-implementation-audit.md](docs/spec-implementation-audit.md).

## Included

- `include/topoexec/common/`: logging, metrics, trace.
- `include/topoexec/runtime/`: graph, component, static registry, channel, payload, trigger policy, event runtime, scheduler, runner.
- `tools/topoexec/`: C++ CLI for validation, plan/render output, runtime runs, metrics, trace, lint, explain, diff, and bench.
- `examples/`: minimal graph, CompositeLoop graph, delay-feedback control graph, invalid-schema fixture, and runnable apps for minimal pipeline, latest-vs-queue overload behavior, delayed control feedback, CompositeLoop fixed-point ownership, and async task-ready delivery.
