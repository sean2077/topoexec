# Runtime Architecture

This page summarizes the architecture as implemented in the current codebase. It is a maintainer-oriented map, not a replacement for the detailed semantic references.

## Target Boundary

TopoExec builds around these CMake targets:

- `topoexec::core`: installed headers and compile-feature boundary.
- `topoexec::runtime`: dependency-light in-process graph runtime.
- `topoexec::yaml`: optional YAML loading over the runtime.
- `topoexec` CLI: optional command surface over YAML/runtime.
- `topoexec::adapter_sdk` and adapter/FFI/Python/plugin preview targets: optional boundaries that must not pull dependencies into `topoexec::runtime`.

The enforced architecture policy is documented in [Architecture guardrails](architecture-guardrails.md).

## Main Runtime Flow

1. A graph enters as `GraphSpec`, either constructed in C++ or loaded from YAML.
2. Graph validation checks schema version, graph identity, lanes, components, edge endpoints, trigger policies, channel policy, payload/port compatibility, lifecycle dependencies, and CompositeLoop ownership.
3. The graph compiler computes immediate-edge strongly connected components, requires every immediate cycle to match exactly one `CompositeLoop`, condenses components into `CompiledGraphRegion` entries, and sorts the region DAG by dependency and runtime priority.
4. `RuntimeRunner` creates component instances from `ComponentRegistry`, configures and activates them, creates channel/state/config/metric/trace/health infrastructure, then hands the compiled plan to `EventRuntime`.
5. `EventRuntime` executes regions for bounded ticks, duration, idle stop, or external stop. It gathers ready invocations through `TriggerPolicyEngine`, runs components directly or through thread-pool lanes, commits publications, records metrics/trace, and stops on fatal runtime errors.
6. `RuntimeRunnerResult` returns validation, lifecycle counts, scheduler stop reason, runtime metrics, trace events, health events, errors, loop data, and optional component state snapshots.

## Core Objects

- `Component` is the user-code boundary. It describes ports/config, handles lifecycle, and executes one invocation at a time unless marked reentrant.
- `GraphContext` is the per-component runtime handle for publishing, reading inputs, submitting async tasks, using state/config stores, reporting loop convergence, and checking cancellation.
- `RuntimeChannelBus` owns bounded channel storage and reader semantics.
- `RuntimePublicationRouter` stages publication visibility across immediate, delay, state, async, and CompositeLoop boundaries.
- `TriggerPolicyEngine` translates channel/timer/request/task readiness into `Invocation` records.
- `EventRuntime` owns the scheduler loop over compiled regions.
- `RuntimeRunner` owns lifecycle, orchestration, result aggregation, and observer delivery.

## Visibility And Feedback

Immediate edges are visible in the current epoch by compiled region order. Delay, state, and async edges cross epoch or deferred boundaries. Immediate feedback cycles are rejected unless the exact cycle is owned by a `CompositeLoop`; loop policies control iteration, convergence, budget, and partial-success handling.

## Observability

Metrics, trace events, health events, diagnostics, and structured runtime errors are runtime outputs, not control inputs. Descriptor-backed metrics and trace schema fields are documented in [Metrics](../62-schemas-protocols/metrics.md) and [Trace events](../62-schemas-protocols/trace-events.md). Graph validation diagnostics are documented in [Diagnostics](../62-schemas-protocols/diagnostics.md).

## Deferred Or Preview Scope

Current preview surfaces for adapters, FFI, Python automation, plugin loading, and editor/schema UX are documented but not production ecosystem commitments. Schema v2 remains a planning boundary until a reviewed v2 loader and migration path exist.
