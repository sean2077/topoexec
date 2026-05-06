# Project Map

TopoExec is a small, embeddable graph runtime. The core project boundary is C++20 runtime semantics, not production adapters or a distributed execution platform.

## What To Read First

- Build and run the project through [Getting started](../01-quickstart/getting-started.md).
- Learn the vocabulary through [Concepts](../10-user-overview/concepts.md).
- Understand the implementation through [Runtime architecture](../21-architecture/runtime-architecture.md).
- Find source files through [Codebase map](../22-codebase/codebase-map.md).
- Maintain or release the project through [Maintainer map](../20-development-overview/maintainer-map.md).

## Current Architecture In One Pass

Graph authors provide a schema-v1 `GraphSpec` with lanes, components, edges, optional hierarchy/template expansion, and optional CompositeLoop regions. Validation checks structural, scheduler, trigger, port, payload, and cycle contracts. The compiler condenses immediate-edge strongly connected components into ordered regions; cyclic immediate regions must be owned by an exact `CompositeLoop`.

`RuntimeRunner` owns lifecycle, state/config stores, channel bus, publication router, metrics, trace, health events, and observers. `EventRuntime` executes compiled regions by scheduler lane, collects ready invocations through the trigger engine, commits immediate publications between components, and advances delay/state/async visibility across epoch boundaries.

## Non-Goals To Keep Visible

- No new runtime dependency without an explicit documented reason.
- No production ROS 2, OpenTelemetry, Prometheus, Python binding, or plugin ecosystem claim from preview/boundary docs.
- No hard real-time, thread preemption, distributed scheduling, or schema v2 implementation claim from current runtime behavior.
