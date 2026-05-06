# TopoExec Documentation

This directory is organized as a learning and maintenance map. Start with the
learning path when onboarding, then use reference and project paths when changing
runtime semantics or release surfaces.

## Getting started

1. [Getting started](getting-started.md): build, run, inspect, and debug a graph.
2. [Concepts](concepts.md): components, edges, triggers, lanes, and observability.
3. [Cookbook](cookbook.md): executable recipes for common graph patterns.
4. [Examples](examples.md): concrete examples for each core semantic.
5. [Robot cell case study](case-study-robot-cell.md): composed real-world pilot
   app without adapter dependencies.

A new user should be able to build the repository, run the C++ builder app, and
inspect a YAML graph in less than 30 minutes by following this path.

## Concepts and runtime semantics

- [Robot cell pilot case study](case-study-robot-cell.md)
- [Runtime semantics](runtime-semantics.md)
- [Runtime semantic contract](semantic-contract.md)
- [Runtime invariant coverage](runtime-invariants.md)
- [Design principles](design-principles.md)
- [Architecture diagrams](architecture-diagrams.md)
- [Why not ...?](why-topoexec.md)

## API reference

- [API overview](api-overview.md)
- [Public API stability](public-api.md)
- [API change checklist](api-change-checklist.md)
- [Components](components.md)
- [Payloads and ownership](payloads.md)
- [Memory and buffer pools](memory.md)
- [Async tasks](async-tasks.md)
- [State and config snapshots](state.md)

## Graph schema and runtime features

- [Graph spec](graph-spec.md)
- [Schema v1](schema-v1.md)
- [Hierarchical graphs](hierarchical-graphs.md)
- [Channels and backpressure](channels.md)
- [Triggers](triggers.md)
- [Scheduler](scheduler.md)
- [Concurrency](concurrency.md)
- [CompositeLoop regions](composite-loops.md)

## Observability and diagnostics

- [Metrics](metrics.md)
- [Trace events](trace-events.md)
- [Diagnostics](diagnostics.md)
- [Defensive input handling](defensive-input.md)
- [CLI](cli.md)

## Adapters

- [Adapter boundaries](adapters.md)
- [ROS 2 adapter plan](adapters/ros2.md)

Adapters remain deferred/preview unless a later goal explicitly implements them.
Do not infer ROS 2, OpenTelemetry, Prometheus, Python, or dynamic plugin support
from docs that only describe boundaries.

## Testing and release

- [Testing strategy](testing-strategy.md)
- [Coverage-guided fuzzing](fuzzing.md)
- [Stress and soak testing](stress-testing.md)
- [Performance baselines](performance-baselines.md)
- [Build and package](build-and-package.md)
- [Release checklist](release-checklist.md)
- [Release runbook](release-runbook.md)
- [Release progression](release-progression.md)
- [Beta readiness review](beta-readiness-review.md)
- [Versioning](versioning.md)
- [Current baseline](current-baseline.md)

## Project maintenance

- [Architecture guardrails](architecture-guardrails.md)
- [Contributing](contributing.md)
- [Agent goals](agent-goals.md)
- [FAQ](faq.md)

## Docs validation

The `docs_command_smoke` CTest runs selected commands embedded as
`topoexec-doc-test` markers across the docs tree. It also checks that the G55
learning map, cookbook, diagrams, comparisons, and design-principle pages keep
required sections present. Package, C++ app, benchmark, fuzz, and stress smokes
cover compileable/runtime snippets outside docs.
