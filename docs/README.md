# TopoExec Documentation

This directory is organized into four reading paths. Start with the tutorial path
when learning the project, then use the reference pages when implementing or
reviewing changes.

## Tutorial path

1. [Getting started](getting-started.md): build, run, inspect, and debug a graph.
2. [Concepts](concepts.md): components, edges, triggers, lanes, and observability.
3. [Components](components.md): write a component, describe ports, and publish data.
4. [Graph spec](graph-spec.md): author YAML or C++ graphs that validate.
5. [CLI](cli.md): validate, plan, run, inspect metrics, and debug trace output.
6. [Examples](examples.md): concrete examples for each core semantic.
7. [Testing strategy](testing-strategy.md): test layers, fuzz/stress smoke, and sanitizer gates.

A new user should be able to build the repository, run the C++ builder app, and
inspect a YAML graph in less than 30 minutes by following these pages in order.

## Reference path

- [Public API stability](public-api.md)
- [Schema v1](schema-v1.md)
- [Runtime semantics](runtime-semantics.md)
- [Runtime semantic contract](semantic-contract.md)
- [Runtime invariant coverage](runtime-invariants.md)
- [Metrics](metrics.md)
- [Trace events](trace-events.md)
- [Diagnostics](diagnostics.md)
- [Defensive input handling](defensive-input.md)
- [Coverage-guided fuzzing](fuzzing.md)
- [Stress and soak testing](stress-testing.md)
- [Versioning](versioning.md)

## Design path

- [Channels and backpressure](channels.md)
- [Triggers](triggers.md)
- [Scheduler](scheduler.md)
- [Concurrency](concurrency.md)
- [CompositeLoop regions](composite-loops.md)
- [Payloads and ownership](payloads.md)
- [Memory and buffer pools](memory.md)
- [State and config snapshots](state.md)
- [Async tasks](async-tasks.md)

## Architecture and project path

- [Architecture guardrails](architecture-guardrails.md)
- [Adapter boundaries](adapters.md)
- [ROS 2 adapter plan](adapters/ros2.md)
- [Performance baselines](performance-baselines.md)
- [Testing strategy](testing-strategy.md)
- [Coverage-guided fuzzing](fuzzing.md)
- [Stress and soak testing](stress-testing.md)
- [Build and package](build-and-package.md)
- [Release checklist](release-checklist.md)
- [Release progression](release-progression.md)
- [Contributing](contributing.md)
- [Agent goals](agent-goals.md)
- [Current baseline](current-baseline.md)
- [FAQ](faq.md)

## Docs validation

The `docs_command_smoke` CTest runs selected commands embedded as
`topoexec-doc-test` markers in tutorial/reference pages. The package smoke and
C++ app smokes cover compileable snippets by building `examples/apps/*` and the
downstream `find_package(topoexec)` runtime-only example.
