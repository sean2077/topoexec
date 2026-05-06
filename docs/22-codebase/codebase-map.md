# Codebase Map

This page maps source ownership for maintainers.

## Public Headers

- `include/topoexec/runtime/graph.hpp`: graph model, schema/semantic constants, validation results, compiled plan, dry-run result, and graph loading declarations.
- `include/topoexec/runtime/component.hpp`: component API, descriptors, ports, payload invocation, `GraphContext`, lifecycle hooks, and state snapshots.
- `include/topoexec/runtime/runtime_runner.hpp`: `RuntimeRunner`, run options, result shape, runtime observer/sink APIs, trace events, runtime errors, and stop semantics.
- `include/topoexec/runtime/channel.hpp`: low-level channel bus and publication routing internals; prefer `RuntimeRunner` and `GraphContext` for stable embedding.
- `include/topoexec/runtime/scheduler.hpp`: scheduler lane config, metrics, stop token/source, and run result.
- `include/topoexec/runtime/graph_builder.hpp`: convenience builder over `GraphSpec`.
- `include/topoexec/adapters/*.hpp`, `include/topoexec/c_api/topoexec.h`, and `include/topoexec/plugins/loader.hpp`: optional preview/adapter surfaces.

## Source Modules

- `src/graph.cpp`: validation, diagnostics, SCC/CompositeLoop compilation, plan text/JSON/Mermaid rendering.
- `src/graph_io.cpp`: YAML/schema-v1 loading, input limits, hierarchy/template expansion, and schema parsing.
- `src/runtime_runner.cpp`: lifecycle orchestration, state/config stores, runner result aggregation, trace/metric/health/error collection, and observer delivery.
- `src/event_runtime.cpp`: scheduler loop, trigger readiness, direct/thread-pool component execution, fixed-rate cadence, CompositeLoop iteration, config transaction application, and runtime metrics.
- `src/channel.cpp`: channel storage, reader semantics, backpressure/deadline/stale handling, epoch advancement, and publication routing.
- `src/trigger_policy.cpp`: event source and trigger policy readiness logic.
- `src/task_executor.cpp`: deterministic and threaded task executor helpers.
- `src/state.cpp`: runtime state and config snapshot stores.
- `src/metrics.cpp`, `src/metric_schema.cpp`, `src/trace.cpp`, `src/diagnostics.cpp`, `src/logging.cpp`: observability and diagnostics support.
- `src/c_api.cpp` and `src/plugin_loader.cpp`: optional preview targets.

## Tooling And Examples

- `tools/topoexec/main.cpp`: CLI command surface for validate, plan, render, run, metrics, trace, lint, explain, diff-plan, bench, schema, and doctor.
- `examples/`: YAML fixtures plus runnable C++ apps; app READMEs explain graph shape and expected output.
- `benchmarks/`: benchmark graph cases and task-executor benchmark surface.
- `scripts/`: agent gate, goal-specific checks, fuzz/stress/bench/release helpers, and format checking.
- `tests/`: unit, CLI, schema, docs, package, policy, fuzz, stress, bench, release, and CMake downstream smokes.

## Test Ownership

- Runtime behavior belongs in `tests/test_runtime.cpp`, `tests/test_graph.cpp`, `tests/test_channel.cpp`, `tests/test_state.cpp`, or focused preview tests.
- CLI JSON and schema drift belong in `tests/golden/` plus schema/CLI smoke tests.
- Documentation command markers are checked by `tests/docs/check_docs.py`.
- Architecture and dependency boundaries are checked by `tests/policy/check_no_adapter_deps.py`.
