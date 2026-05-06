# Runtime Semantic Contract

TopoExec exposes two related but separate compatibility axes:

- **Schema version** (`schema_version: 1`) describes the accepted graph file shape.
- **Runtime semantic contract version** (`semantic_contract_version: 0.2`) describes the meaning of accepted graphs at runtime.

The current semantic contract version is exposed in:

- `topoexec doctor --format json` as `semantic_contract_version`;
- `topoexec doctor` text output;
- `topoexec schema dump --format json` as the schema annotation `x-topoexec-semantic_contract_version`.

This document is the reference target for future goals that change runtime behavior. A field can be schema-v1 compatible while still changing runtime meaning; such changes must update this contract, `docs/versioning.md`, tests/goldens, and `CHANGELOG.md`.

## Contract status levels

| Status | Meaning |
| --- | --- |
| `v0.1 stable` | Semantics shipped with the initial alpha line and should not change in patch releases. |
| `v0.2 stable` | Semantics stabilized for the next architecture-stabilization alpha. Additive metadata is allowed; existing meaning should not silently change. |
| `experimental` | Implemented or public but still expected to change before beta. Changes still need docs/tests. |
| `future extension` | Planned or documented as a non-goal; do not claim implemented behavior. |

## Semantic contract table

| Semantic | Status | Contract |
| --- | --- | --- |
| Epoch | `v0.1 stable` | An epoch is one bounded runtime step, event-loop iteration, or simulated tick. Delay/state/async visibility crosses epoch boundaries. |
| Transaction | `v0.2 stable` | A transaction is the set of component executions and staged publications processed under one scheduler decision boundary inside an epoch. Runtime staging prevents recursive downstream calls. |
| Staged publication | `v0.1 stable` | `GraphContext::publish()` and `publish_shared()` submit output to runtime-owned staging/routing and return a publication result; components must not directly execute downstream components. |
| Commit | `v0.1 stable` | Commit is the point where staged publications become visible according to edge kind. Commit metrics count accepted publications that reached their visibility boundary. |
| Immediate edge visibility | `v0.1 stable` | `immediate` edges are same-transaction dependencies in the compiled immediate graph. Nontrivial immediate SCCs are rejected unless exactly owned by a `CompositeLoop`. |
| Delay edge visibility | `v0.1 stable` | `delay` edges break immediate feedback and make publications visible at the next epoch boundary. |
| State edge visibility | `v0.2 stable` | `state` publications are staged during the current epoch and committed at the next epoch boundary; readers observe the previously committed snapshot during the publishing epoch. |
| Async edge visibility | `v0.2 stable` | `async` publications are deferred to the next epoch boundary and cannot cause same-call-stack recursive execution. `policy.max_inflight` applies before channel capacity. |
| Trigger readiness ownership | `v0.2 stable` | Readiness belongs to the runtime trigger engine, not component code. Manual/timer/message/any/all/time-sync/batch/request/task/future-ready paths produce explicit invocation event/trigger metadata. |
| Invocation metadata | `experimental` | Correlation, causation, epoch, transaction, source endpoint, and trigger-kind metadata flow through publication, channels, trigger collection, invocations, task completions, and CompositeLoop external commits. Trace events may include these fields; metrics do not use them as default labels. |
| Runtime health events | `experimental` | Bounded observer-only health events capture channel overflow/stale/deadline/high-watermark, task reject, and scheduler reject paths. They are exposed through `RuntimeRunnerResult`, CLI JSON, and trace entries; they do not trigger components or alter scheduling. |
| Channel capacity and overflow | `v0.2 stable` | Channels and async admission are bounded. Overflow/drop/reject/deadline/stale outcomes are observable through metrics, health counters, and optional bounded health events, not hidden blocking or recursive control flow. |
| Payload ownership | `v0.2 stable` | Built-in text/frame/blob/opaque payloads use explicit copy/shared/loaned/move policies. Large copy and multi-reader move-only misuse are rejected or degraded with observable diagnostics/metrics. |
| CompositeLoop ownership | `v0.1 stable` | A `CompositeLoop` owns an immediate cyclic SCC only when its component set exactly matches the SCC. Partial loop declarations are invalid. |
| CompositeLoop external output commit | `v0.2 stable` | External outputs from loop-owned components are staged until the loop region finishes successfully. Internal failure suppresses half-updated external commits and records loop error evidence. |
| Scheduler event-loop lane | `v0.1 stable` | `event_loop` is the deterministic default lane. It executes compiled ready work with runtime priority ordering for independent ready regions and without OS scheduler claims. |
| Scheduler fixed-rate lane | `experimental` | Current behavior is deterministic/simulated over bounded runtime ticks by default, with runtime priority ordering for independent ready regions and opt-in cooperative wall-clock sleeping via `wall_clock_enabled`, `period_ms`/`hz`, and `overrun_policy`. Tick, overrun, skipped, jitter, blocked-duration, and max-lateness metrics are observable. Independent lane threads, OS jitter control, and hard real-time guarantees are future work. |
| Scheduler thread-pool lane | `experimental` | Current behavior is run-scoped persistent worker-pool execution for ready invocations with bounded runtime-priority queue admission, cooperative cancellation/timeout-budget observation, non-reentrant serialization, worker-id trace attributes, and queue/worker/priority metrics. OS scheduling policy, hard timeout preemption, and advanced starvation aging are future work. |
| Runtime metrics | `v0.2 stable` | `RuntimeRunnerResult::runtime_metrics` and CLI metrics JSON expose counters/gauges/histograms for runtime decisions. Metric schema/cardinality v2 remains future work. |
| Runtime trace | `v0.2 stable` | `RuntimeRunnerResult::trace`, CLI trace JSON, and Chrome trace output expose runtime events/spans. Causality/timeline v2 remains future work. |
| Structured runtime errors | `v0.2 stable` | `RuntimeRunnerResult::runtime_errors` records phase/component/lane/code/message/trace/fatality while preserving legacy `errors` strings. |
| State/config snapshot stores | `experimental` | Stores are epoch-boundary committed and observable; component config hot reload uses validate/apply transactions with rollback/fail-fast semantics. |
| Component reset/snapshot/restore | `experimental` | Components may opt into start-epoch reset/restore and post-run snapshot capture through `RuntimeRunnerOptions`; hooks do not interleave with component execution, and pause/resume policy remains future work. |
| TaskExecutor deterministic/threaded helpers | `experimental` | `ITaskExecutor` is the embedder interface, `TaskExecutor` remains the deterministic compatibility path, and `ThreadedTaskExecutor` is an opt-in bounded worker preview. Pending cancellation is cooperative, active work is not forcibly killed, completion callbacks route through publisher/channel boundaries, and threaded details may change before beta. |
| Cooperative cancellation and timeout | `experimental` | `CancellationToken`/`CancellationSource`, `GraphContext::cancel_requested()`, `Invocation::cancel_requested()`, component timeout-budget metrics, CompositeLoop between-iteration cancellation, and TaskExecutor pending-task cancellation are implemented. No hard thread termination or timeout preemption is implemented. |
| Runtime observer API | `v0.2 stable` | `RuntimeRunnerOptions::observers` delivers best-effort result/metric/trace/health/error records after run assembly; observer failures are non-fatal diagnostics. Exporter adapters remain future work. |

## Schema v1 relationship

Schema v1 controls graph shape and strict field validation. The semantic contract controls runtime meaning. Compatibility rules:

1. A schema-v1 additive field is allowed only when old graphs keep the same runtime meaning.
2. Changing the meaning of an existing schema-v1 field requires a semantic-contract update and may require schema v2 even if the JSON/YAML shape is unchanged.
3. A graph accepted under schema v1 must not silently acquire unsafe feedback, unbounded backlog, hidden recursion, or adapter-specific behavior.
4. Schema annotations such as `x-topoexec-semantic_contract_version` describe the runtime contract and are not graph input fields.
5. CLI JSON and C++ result fields are part of the observable contract when they describe semantic behavior.

## Change protocol

For any semantic change:

1. Update this document and mark the affected semantic status.
2. Update `docs/versioning.md` and `CHANGELOG.md`.
3. Add or update focused tests, schema/golden checks, or runtime invariant coverage.
4. Preserve schema v1 compatibility or explicitly open schema v2 design.
5. Keep deferred behavior explicit; do not use optimistic docs to imply implementation.
