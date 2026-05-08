# Runtime Semantics

TopoExec is a single-process, in-process semantic graph runtime. A graph is compiled before execution, and the compiled plan is the runtime source of truth for component order, CompositeLoop ownership, and immediate-cycle validation.

This document explains runtime behavior. The versioned compatibility surface is summarized in [semantic-contract.md](semantic-contract.md), currently `semantic_contract_version: 0.2`. Some runtime behaviors are still being implemented; those gaps should stay visible in tests, examples, and release notes rather than being hidden behind compatibility aliases or optimistic examples.

## Time And Commit Model

An epoch is one bounded runtime step, usually one event-loop iteration or one simulated tick in tests.

A transaction is the set of component executions and staged publications processed under one scheduler decision boundary inside an epoch.

A commit is the point where staged publications become visible according to their edge kind. `GraphContext::publish()` records output through the runtime-owned publication path; it must not call downstream component `execute()` directly.

Runtime execution is bounded by steps, duration, stop token, or idle detection. `run_until_idle` still uses the configured step bound as a safety limit, but stops early once a full event-loop iteration executes no components.

## Hierarchical Graph Expansion

`subgraphs[]` are a schema-v1 organization aid. The YAML loader expands each
subgraph at load time into namespaced flat component, edge, and CompositeLoop ids
such as `cell.source`, `cell.source_sink`, and `cell.feedback`. The runtime does
not create nested schedulers or hidden CompositeComponents; validation,
compilation, scheduling, metrics, and trace all operate on the expanded flat
`GraphSpec`.

Endpoint parsing uses the last `.` as the component/port separator. This allows
expanded component ids such as `cell.sink` while preserving the normal
`component.port` contract. Immediate-cycle validation runs after expansion, so
subgraph boundaries cannot hide feedback loops.

`templates` and `template_instances` use the same compile-time expansion path
after strict parameter substitution. Missing or unknown template parameters fail
while loading; expanded components/edges are then validated exactly like
handwritten or subgraph-expanded graph entries.

## Edge Visibility

`immediate` edges are same-transaction dependencies. They participate in immediate dependency SCC analysis, and a nontrivial immediate SCC is rejected unless it exactly matches one declared `composite_loops[]` entry. In a DAG, immediate publications may become visible to downstream components during the same epoch according to compiled region order.

`delay` edges break immediate feedback at compile time: they do not participate in immediate SCC rejection. At runtime, publications on delay edges are staged by the runtime-owned publication router and committed at the next epoch boundary. Use this for previous-frame feedback, sample-and-hold control, or any feedback path that must not recursively execute in the same transaction.

`state` edges do not participate in immediate SCC rejection. Runtime state-edge publications are staged during the current epoch and committed at the next epoch boundary, so readers continue to see the previously committed snapshot during the publishing epoch. Use this for blackboard-like state, configuration snapshots, and slow-to-fast crossings. Multiple state writers to the same target are rejected at validation time until a richer explicit merge policy exists.

`async` edges do not participate in immediate SCC rejection. Runtime async-edge publications are deferred to the next epoch boundary, which prevents same-call-stack or same-transaction recursive execution. Use this for worker completion, future-ready events, diagnostics, and lower-priority notifications. Components with `task_ready`, `future_ready`, or `request` event sources receive matching invocation event kinds once their input event is observed. Async `policy.max_inflight` limits deferred completions before channel capacity is considered; channel capacity and overflow policy still apply when admitted completions are committed to the runtime channel.

## Channel Policy

Runtime channels are bounded. `latest` with `overwrite` and `capacity=1` is the low-latency default. `queue` with explicit capacity is for ordered event or command streams. `latched` keeps the last committed value for late readers. `previous_tick` stages a pending value and exposes it only at the next epoch boundary; update waiters are notified when that pending value becomes visible, not when it is first staged. `barrier` with capacity N waits until N queued messages are available before delivering the synchronized batch.

Overflow behavior must be explicit. Dropped, overwritten, blocked, or rejected publications must be observable through channel metrics and, when enabled, bounded runtime health events. Aggregate runtime results keep `channel_drop_count` for real drops only and expose overwrite, reject, stale-drop, and deadline-miss fields for user-facing explanations. Blocking overflow reports a would-block result on the current non-blocking runtime path and is not allowed to silently block a single-thread event loop. Queue readers are single-reader by default; explicit `readers: multi` / `readers: multiple` uses per-reader cursors over bounded retained history, so a slow reader can still miss messages overwritten by overflow.

Health events are observer records, not trigger inputs. `RuntimeRunnerOptions::emit_health_events` and graph-level `graph.config.emit_health_events` can disable event capture; `RuntimeRunnerOptions::health_event_capacity` and `graph.config.health_event_capacity` bound retained events. Edge-level `policy.emit_health_events: false` suppresses channel events for that edge while keeping degradation metrics. Channel hot paths build health records while mutating channel state, then emit them after releasing the channel bus lock; task executor reject paths follow the same observer-only pattern for threaded execution. Low-level `RuntimeChannelBus::set_health_event_sink()` and task-executor sink registration use non-owning pointers; embedders that bypass `RuntimeRunner` must keep the sink alive and must not replace or destroy it concurrently with publish, read, or submit operations. The runtime does not execute a component because a health event was recorded; future health-to-graph routing must use an explicit normal graph boundary.

Read policy is part of the runtime contract: low-level APIs distinguish peek, snapshot, bounded drain, and per-reader drain. Copy policy is part of the runtime contract. `copy` owns a copied payload and rejects large payloads that cannot be copied safely. `shared_view` shares immutable payload storage. `loaned_view` preserves `BufferPool` / `LoanedFrame` frame buffers without copying and publishes them as immutable runtime payloads. `move_only` is accepted only for `readers: single`; multi-reader move-only edges are invalid.

## State And Config Snapshots

State is never a hidden mutable global. `RuntimeStateStore` exposes an optional namespaced blackboard with immutable `RuntimeStateSnapshot` reads and explicit `component.port` writers. Writes are staged and become current only when the runtime crosses an epoch boundary. The initial merge policy is single-writer per namespace/key; a second writer is rejected until a future explicit merge function exists.

Graph-level `graph.config` values are parsed into `GraphSpec::config` and copied into `ConfigSnapshotStore` by `RuntimeRunner`. Component configs are also snapshotted there. A component may stage a component config update through `ConfigSnapshotStore`; by default it joins the pending transaction for the next epoch boundary, so other components in the publishing epoch still observe the previously committed config snapshot. At the boundary, pending component configs are validated with `Component::validate_config()`, applied with `Component::apply_config()`, and committed only after all apply hooks succeed. Validation or apply failure is fail-fast: pending updates are rolled back and the previous committed config remains active. The default `apply_config()` delegates to `configure_status()` for compatibility, but components that need live reload can override it with an idempotent boundary-safe update.

## Trigger Readiness

Components implement one `execute()` method. Readiness belongs to the trigger engine, not to component code.

`manual` triggers are explicitly scheduled. `timer` / `on_tick` triggers fire from timer event sources and cannot be mixed with input-driven event sources on the same component. `any_input` fires when at least one configured input has an update. `all_inputs` waits until every configured input has a message, then consumes one from each input in deterministic input order. `time_sync` waits for every configured input and, when all front messages carry comparable event timestamps, enforces `sync_slop_ms` by dropping the oldest out-of-window sample until the front messages align. Messages without comparable timestamps fall back to all-input readiness but still mark the invocation as `kTimeSync`. `batch` fires when its configured batch size is available, or flushes the available partial batch after `batch_window_ms` expires. `request`, `task_ready`, and `future_ready` are input-driven and produce matching invocation events where implemented; action goal/cancel readiness remains deferred from schema v1 runtime triggers. Trigger v2 preview policies remain declarative: `watermark` drops late timestamped messages relative to the observed per-domain watermark, `condition` supports only built-in readiness predicates, `debounce` coalesces pending messages at the scheduler check, and `rate_limit` uses `min_interval_ms` to suppress repeated ready checks. `debounce_window_ms` is reserved and must be zero in schema v1. Coalescing merges pending updates into one invocation, `min_interval_ms` suppresses repeated invocations inside the interval, and `max_latency_ms` drops expired pending trigger messages before readiness is evaluated. Pending trigger queues are bounded by the input channel capacity plus a small cushion, or by `batch_size` when that is larger; `debounce`/coalescing keep newest-per-input. Bound drops increment `runtime.trigger.pending_drop_count`. For `condition: event_timestamp_present`, a missing-timestamp head item is dropped and counted so a later timestamped item cannot be blocked indefinitely.

An `Invocation` carries event kind, trigger kind, channel id, local correlation id, structured `InvocationMetadata`, ready input names, payloads by port, batch payloads when relevant, timing fields, sequence, budget, lane, priority, deadline metadata, cooperative cancellation token access, and legacy stop-token access. `InvocationMetadata` includes `correlation_id`, `causation_id`, `epoch_id`, `transaction_id`, source component/port, and trigger kind so trace events can explain why a component ran without adding high-cardinality metric labels by default.

## CompositeLoop Ownership

A `composite_loops[]` entry owns an immediate cyclic SCC only when its `components` set exactly matches that SCC. Partial declarations and decorative loop declarations are invalid. Runtime external outputs from loop-owned components are staged until the loop finishes successfully; internal failure records loop error metrics and does not commit half-updated external outputs.

At runtime, a CompositeLoop compiled region owns its internal component scheduling. For `loop_policy.type: fixed_point`, the runtime executes internal components in deterministic compiled order up to `max_iterations` and emits loop iteration metrics. `loop_policy.convergence: single_pass` stops after the first iteration and records convergence. `loop_policy.type: solver_iteration` lets loop components report typed convergence with `GraphContext::report_loop_convergence()`, including an optional residual and reason. A `residual_threshold` can turn a reported residual into convergence. `budget_ms` stops the loop when elapsed loop time exceeds the budget and records a budget overrun. A requested cancellation is observed between loop iterations, records loop cancellation metrics, and stops without hard-preempting the component currently running. Publications from loop-internal components to components outside the loop are staged and committed only when the CompositeLoop region succeeds, or when an explicit `partial_success: commit_outputs` policy allows a non-converged solver stop to publish them; the default `solver_iteration` partial-success policy discards staged external outputs. Discarded staged async outputs decrement async in-flight accounting and are reported as cancelled async completions plus CompositeLoop/publication discard metrics.

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

## Runtime Trace Timeline

`RuntimeRunnerResult::trace` is an ordered timeline using trace schema version
`1`. Each `RuntimeTraceEvent` includes bounded phase and identity fields
(`component_id`, `channel_id`, `lane`, `worker_id`, `epoch_id`,
`transaction_id`, `correlation_id`, and `causation_id`) in addition to
event-specific attributes. Metrics/trace remain observer output only and do not
feed back into scheduling.

## Runtime Error Propagation

Runtime execution reports failures through `RuntimeRunnerResult::runtime_errors`.
Each `RuntimeError` entry records `phase`, `component_id`, `lane`, `message`,
`code`, `trace_id`, and `fatal`.

Lifecycle phases use `configure`, `activate`, optional start-epoch `restore`/`reset`, optional post-run `snapshot`, and `deactivate`. Component invocation failures use `execute`; runtime/compiler failures use `validate`, `dry_run`, or `runtime`. Restore/reset happen after activation and before the scheduler starts, so they never interleave with component execution. Snapshot capture happens after the scheduler run and before deactivation. Deactivate errors are recorded with `fatal: false` when they are cleanup follow-ons so they do not hide the original fatal error. The default runtime policy remains fail-fast; non-fail-fast policies are still future work and must not be silently emulated.

## Runtime Observers

`RuntimeRunnerOptions::observers` registers optional `RuntimeObserver` instances
for best-effort delivery of result, metric, trace, health-event, and structured
runtime-error records after a run result is assembled. Observer callbacks return
`Status`; failures increment `RuntimeRunnerResult::observer_failure_count`, add a
non-fatal `observer_failure` runtime error, and emit `runtime.observer.*`
metrics without changing graph scheduling, triggers, publication, or `ok`.

The default path has no observer. `NoopRuntimeObserver` is the explicit no-op
implementation, and `InMemoryRuntimeObserver` is a bounded, try-locking recorder
for tests and embedders. Custom observers should be non-blocking and bounded;
adapters should export from this surface instead of changing core runtime
semantics or adding OpenTelemetry/Prometheus/Perfetto dependencies to core.
