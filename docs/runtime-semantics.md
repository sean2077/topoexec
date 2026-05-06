# Runtime Semantics

TopoExec is a single-process, in-process semantic graph runtime. A graph is compiled before execution, and the compiled plan is the runtime source of truth for component order, CompositeLoop ownership, and immediate-cycle validation.

This document explains runtime behavior. The versioned compatibility surface is summarized in [semantic-contract.md](semantic-contract.md), currently `semantic_contract_version: 0.2`. Some runtime behaviors are still being implemented; those gaps should stay visible in tests, examples, and release notes rather than being hidden behind compatibility aliases or optimistic examples.

## Time And Commit Model

An epoch is one bounded runtime step, usually one event-loop iteration or one simulated tick in tests.

A transaction is the set of component executions and staged publications processed under one scheduler decision boundary inside an epoch.

A commit is the point where staged publications become visible according to their edge kind. `GraphContext::publish()` records output through the runtime-owned publication path; it must not call downstream component `execute()` directly.

Runtime execution is bounded by steps, duration, stop token, or idle detection. `run_until_idle` still uses the configured step bound as a safety limit, but stops early once a full event-loop iteration executes no components.

## Edge Visibility

`immediate` edges are same-transaction dependencies. They participate in immediate dependency SCC analysis, and a nontrivial immediate SCC is rejected unless it exactly matches one declared `composite_loops[]` entry. In a DAG, immediate publications may become visible to downstream components during the same epoch according to compiled region order.

`delay` edges break immediate feedback at compile time: they do not participate in immediate SCC rejection. At runtime, publications on delay edges are staged by the runtime-owned publication router and committed at the next epoch boundary. Use this for previous-frame feedback, sample-and-hold control, or any feedback path that must not recursively execute in the same transaction.

`state` edges do not participate in immediate SCC rejection. Runtime state-edge publications are staged during the current epoch and committed at the next epoch boundary, so readers continue to see the previously committed snapshot during the publishing epoch. Use this for blackboard-like state, configuration snapshots, and slow-to-fast crossings. Multiple state writers to the same target are rejected at validation time until a richer explicit merge policy exists.

`async` edges do not participate in immediate SCC rejection. Runtime async-edge publications are deferred to the next epoch boundary, which prevents same-call-stack or same-transaction recursive execution. Use this for worker completion, future-ready events, diagnostics, and lower-priority notifications. Components with `task_ready`, `future_ready`, or `request` event sources receive matching invocation event kinds once their input event is observed. Async `policy.max_inflight` limits deferred completions before channel capacity is considered; channel capacity and overflow policy still apply when admitted completions are committed to the runtime channel.

## Channel Policy

Runtime channels are bounded. `latest` with `overwrite` and `capacity=1` is the low-latency default. `queue` with explicit capacity is for ordered event or command streams. `latched` keeps the last committed value for late readers. `previous_tick` exposes only the prior epoch's value. `barrier` with capacity N waits until N queued messages are available before delivering the synchronized batch.

Overflow behavior must be explicit. Dropped, overwritten, blocked, or rejected publications must be observable through channel metrics. Blocking overflow reports a would-block result on the current non-blocking runtime path and is not allowed to silently block a single-thread event loop. Queue readers are single-reader by default; explicit `readers: multi` / `readers: multiple` uses per-reader cursors over bounded retained history, so a slow reader can still miss messages dropped by overflow.

Read policy is part of the runtime contract: low-level APIs distinguish peek, snapshot, bounded drain, and per-reader drain. Copy policy is part of the runtime contract. `copy` owns a copied payload and rejects large payloads that cannot be copied safely. `shared_view` shares immutable payload storage. `loaned_view` preserves `BufferPool` / `LoanedFrame` frame buffers without copying and publishes them as immutable runtime payloads. `move_only` is accepted only for `readers: single`; multi-reader move-only edges are invalid.

## State And Config Snapshots

State is never a hidden mutable global. `RuntimeStateStore` exposes an optional namespaced blackboard with immutable `RuntimeStateSnapshot` reads and explicit `component.port` writers. Writes are staged and become current only when the runtime crosses an epoch boundary. The initial merge policy is single-writer per namespace/key; a second writer is rejected until a future explicit merge function exists.

Graph-level `graph.config` values are parsed into `GraphSpec::config` and copied into `ConfigSnapshotStore` by `RuntimeRunner`. Component configs are also snapshotted there. A component may stage a component config update through `ConfigSnapshotStore`; by default it applies on the next epoch boundary, so other components in the publishing epoch still observe the previously committed config snapshot. This API does not re-run `configure()` automatically; it is a runtime snapshot/config-data surface.

## Trigger Readiness

Components implement one `execute()` method. Readiness belongs to the trigger engine, not to component code.

`manual` triggers are explicitly scheduled. `timer` / `on_tick` triggers fire from timer event sources. `any_input` fires when at least one configured input has an update. `all_inputs` waits until every configured input has a message, then consumes one from each input in deterministic input order. `time_sync` waits for every configured input and, when all front messages carry comparable event timestamps, enforces `sync_slop_ms` by dropping the oldest out-of-window sample until the front messages align. Messages without comparable timestamps fall back to all-input readiness but still mark the invocation as `kTimeSync`. `batch` fires when its configured batch size is available, or flushes the available partial batch after `batch_window_ms` expires. `request`, `task_ready`, and `future_ready` are input-driven and produce matching invocation events where implemented. Coalescing merges pending updates into one invocation, `min_interval_ms` suppresses repeated invocations inside the interval, and `max_latency_ms` drops expired pending trigger messages before readiness is evaluated.

An `Invocation` carries event kind, trigger kind, channel id, local correlation id, structured `InvocationMetadata`, ready input names, payloads by port, batch payloads when relevant, timing fields, sequence, budget, lane, priority, deadline metadata, cooperative cancellation token access, and legacy stop-token access. `InvocationMetadata` includes `correlation_id`, `causation_id`, `epoch_id`, `transaction_id`, source component/port, and trigger kind so trace events can explain why a component ran without adding high-cardinality metric labels by default.

## CompositeLoop Ownership

A `composite_loops[]` entry owns an immediate cyclic SCC only when its `components` set exactly matches that SCC. Partial declarations and decorative loop declarations are invalid. Runtime external outputs from loop-owned components are staged until the loop finishes successfully; internal failure records loop error metrics and does not commit half-updated external outputs.

At runtime, a CompositeLoop compiled region owns its internal component scheduling. For `loop_policy.type: fixed_point`, the current runtime executes internal components in deterministic compiled order up to `max_iterations` and emits loop iteration metrics. `loop_policy.convergence: single_pass` stops after the first iteration and records convergence. `budget_ms` stops the loop when elapsed loop time exceeds the budget and records a budget overrun. A requested cancellation is observed between loop iterations, records loop cancellation metrics, and stops without hard-preempting the component currently running. Publications from loop-internal components to components outside the loop are staged and committed only when the CompositeLoop region finishes.

## Example Graph Shapes

### DAG Pipeline

```text
source --immediate--> transform --immediate--> sink
```

The immediate graph is acyclic. The compiler produces component regions and a deterministic topological `region_order`. Runtime execution can make `source` output visible to `transform`, then `transform` output visible to `sink`, without recursive calls from `publish()`.

### Delay Feedback

```text
sensor --immediate--> estimator --immediate--> controller --immediate--> actuator
controller --delay--> estimator
```

The feedback edge is not part of the immediate dependency graph. The compiler accepts the graph without a CompositeLoop. The controller correction becomes visible to the estimator only at the next delay boundary, so the current transaction remains acyclic.

### CompositeLoop Feedback

```text
estimator --immediate--> controller
controller --immediate--> estimator
```

Without a matching `composite_loops[]` entry, this graph is invalid. With `components: [estimator, controller]` and a `loop_policy`, the compiler condenses the SCC into one CompositeLoop region and the runtime executes that region through the loop owner.

## Runtime Error Propagation

Runtime execution reports failures in two compatible forms:

- `RuntimeRunnerResult::errors` keeps legacy human-readable strings for CLI and existing tests.
- `RuntimeRunnerResult::runtime_errors` records structured `RuntimeError` entries with `phase`, `component_id`, `lane`, `message`, `code`, `trace_id`, and `fatal`.

Lifecycle phases use `configure`, `activate`, and `deactivate`. Component invocation failures use `execute`; runtime/compiler failures use `validate`, `dry_run`, or `runtime`. Deactivate errors are recorded with `fatal: false` when they are cleanup follow-ons so they do not hide the original fatal error. The default runtime policy remains fail-fast; non-fail-fast policies are still future work and must not be silently emulated.
