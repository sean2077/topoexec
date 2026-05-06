# Runtime Metrics

TopoExec runtime metrics are exposed through `RuntimeRunnerResult::runtime_metrics` and through the CLI:

```bash
topoexec graph metrics examples/minimal.yaml --steps 1 --format json
```

The JSON output is a `RuntimeRunnerResult` object. Its `metrics` field is an array of metric samples with this shape:

```json
{
  "name": "runtime.channel.publish_count",
  "value": 1.0,
  "component_id": "",
  "lane": "",
  "channel_id": "source_transform",
  "tags": []
}
```

Metric names are part of the public observability contract. Prefer adding new names over changing the meaning of existing names.

`MetricRegistry::histogram(name)` exports lightweight in-process histogram summaries without external dependencies. Snapshot suffixes are `name.count`, `name.min`, `name.max`, `name.avg`, `name.p50`, `name.p95`, and `name.p99`; percentile values use deterministic linear interpolation over the observed sample set.

## Stable Names

Scheduler:

- `runtime.scheduler.tick_count`: fixed-rate ticks executed by the lane. Non-`fixed_rate` lanes report `0`.
- `runtime.scheduler.completed_count`: completed component invocations for a lane.
- `runtime.scheduler.tick_overrun_count`: lane tick overruns observed by the scheduler.
- `runtime.scheduler.skipped_tick_count`: fixed-rate ticks intentionally skipped by an overrun policy. This remains `0` for deterministic stepping unless a wall-clock overrun policy skips ticks.
- `runtime.scheduler.max_lateness_ms`: maximum observed fixed-rate lateness or simulated overrun amount.
- `runtime.scheduler.queue_depth`: maximum queued scheduler tasks observed for a lane. This is `0` for the single-thread event loop.
- `runtime.scheduler.queue_capacity`: configured/effective pending queue capacity for the lane.
- `runtime.scheduler.worker_count`: configured/effective worker count for the lane; for `thread_pool`, this is the persistent worker count.
- `runtime.scheduler.last_callback_duration_ms`: latest scheduler iteration duration observed for the lane.
- `runtime.scheduler.blocked_duration_ms`: maximum wall-clock wait duration inserted before a fixed-rate tick.
- `runtime.scheduler.tick_jitter_ms`: positive simulated overrun amount above the fixed-rate period or tick budget.
- `runtime.scheduler.active_count`: maximum active workers observed for a lane. This is `0` for the single-thread event loop.
- `runtime.scheduler.in_flight_count`: maximum in-flight scheduler tasks observed for a lane. This is `0` for the single-thread event loop.
- `runtime.scheduler.rejected_count`: scheduler admission rejections, skipped ready invocations, or dropped ready invocations for a lane.
- `runtime.scheduler.priority_high_count`: completed invocations with runtime `execution.priority: high`.
- `runtime.scheduler.priority_normal_count`: completed invocations with default or explicit runtime `execution.priority: normal`.
- `runtime.scheduler.priority_low_count`: completed invocations with runtime `execution.priority: low`.
- `runtime.scheduler.priority_background_count`: completed invocations with runtime `execution.priority: background`.
- `runtime.scheduler.low_priority_rejected_count`: low/background ready invocations rejected or dropped by lane admission overflow.
- `runtime.scheduler.starvation_guard_count`: explicit runtime starvation-guard interventions; v1 normally reports `0` because no aging intervention is implemented.

Components:

- `runtime.component.execution_count`: completed `Component::execute_status()` invocations for a component.
- `runtime.component.error_count`: failed status-returning or exception-wrapped component invocations.
- `runtime.component.last_duration_ns`: most recent component execution duration in nanoseconds.
- `runtime.component.max_duration_ns`: maximum component execution duration observed in the run.
- `runtime.component.budget_overrun_count`: component invocations whose duration exceeded `execution.budget_ms`.
- `runtime.component.cancellation_requested_count`: component invocations that completed after their cooperative cancellation token was requested.
- `runtime.component.cancellation_observed_count`: component invocations that called `Invocation::cancel_requested()`, `GraphContext::cancel_requested()`, or legacy `stop_requested()` after cancellation was requested.
- `runtime.component.timeout_budget_exceeded_count`: component invocations whose cooperative timeout budget was exceeded. This is reported after the invocation returns; it is not hard preemption.
- `runtime.component.max_in_flight_count`: maximum concurrent in-flight invocations observed for the component. It is `1` or less for non-reentrant components on the current event-loop runtime.

Triggers:

- `runtime.trigger.ready_count`: invocations admitted by trigger readiness for a component.
- `runtime.trigger.suppressed_count`: event-driven checks that did not admit an invocation.
- `runtime.trigger.coalesced_count`: invocations admitted through a coalescing trigger policy.
- `runtime.trigger.timeout_drop_count`: pending messages dropped by trigger `max_latency_ms`.
- `runtime.trigger.batch_flush_count`: batch invocations flushed by size or window.
- `runtime.trigger.time_sync_drop_count`: oldest out-of-slop samples dropped by `time_sync`.

Channels:

- `runtime.channel.publish_count`: accepted publications for a channel.
- `runtime.channel.delivery_count`: delivered messages for a channel.
- `runtime.channel.drop_count`: dropped, overwritten, rejected, or failed-fast channel publications.
- `runtime.channel.deadline_miss_count`: delivered messages older than `policy.deadline_ms`.
- `runtime.channel.stale_drop_count`: messages dropped before delivery because `policy.lifespan_ms` expired.
- `runtime.channel.reject_count`: publications rejected or would-blocked by capacity/backpressure policy.
- `runtime.channel.overwrite_count`: stored messages overwritten or dropped-oldest to accept newer work.
- `runtime.channel.health_event_count`: aggregate channel degradation events from overwrite, reject, stale, or deadline paths.
- `runtime.channel.max_depth`: maximum observed channel depth.
- `runtime.channel.message_age_ms`: latest observed age at delivery time for that channel.
- `runtime.channel.payload_copy_count`: payload copies forced by channel copy policy.

Health events:

- `runtime.health.event_count`: bounded health events retained in `RuntimeRunnerResult::health_events`.
- `runtime.health.dropped_count`: health events dropped because the sink capacity was full or the sink could not accept the event without waiting.
- `runtime.health.coalesced_count`: repeated health events merged into an existing retained event with the same coalescing key.

Health event metrics summarize the observer sink only. Channel-specific `runtime.channel.health_event_count` remains the per-channel degradation counter and is not a substitute for the bounded event list.

Publication router:

- `runtime.publication.staged`: total staged publications observed by `GraphContext::publish()`.
- `runtime.publication.committed`: staged publications committed to runtime channels.
- `runtime.publication.delayed`: publications deferred by `delay` edges.
- `runtime.publication.state`: publications deferred by `state` edges.
- `runtime.publication.state_committed`: state-edge publications committed at epoch boundaries.
- `runtime.publication.async`: publications deferred by `async` edges.
- `runtime.publication.failed_commit`: failed publication commit batches.

Async admission:

- `runtime.async.accepted_count`: async completions accepted by edge-level admission.
- `runtime.async.rejected_count`: async completions rejected by `policy.max_inflight` admission.
- `runtime.async.dropped_count`: pending or newest async completions dropped by admission overflow.
- `runtime.async.in_flight_count`: async completions still pending deferred delivery at the end of the run.
- `runtime.async.max_in_flight_count`: maximum pending async completions observed during the run.
- `runtime.async.completed_count`: async completions committed to runtime channels at epoch boundaries.
- `runtime.async.cancelled_count`: async completions cancelled by shutdown. This is `0` in the current cooperative MVP.

Composite loops:

- `runtime.loop.iterations`: loop iterations executed for a CompositeLoop region. `component_id` carries the loop id.
- `runtime.loop.converged`: convergence stops for a CompositeLoop region. `component_id` carries the loop id.
- `runtime.loop.budget_overrun`: budget stops for a CompositeLoop region. `component_id` carries the loop id.
- `runtime.loop.max_iterations_hit`: max-iteration stops for a CompositeLoop region. `component_id` carries the loop id.
- `runtime.loop.error`: internal CompositeLoop component failures. `component_id` carries the loop id.
- `runtime.loop.cancellation_requested`: cancellation requests observed by a CompositeLoop between iterations. `component_id` carries the loop id.
- `runtime.loop.cancellation_observed`: CompositeLoop cancellation stops performed between iterations. `component_id` carries the loop id.

State/config snapshots:

- `runtime.state.staged_write_count`: blackboard writes staged for an epoch-boundary commit.
- `runtime.state.committed_write_count`: blackboard writes applied at epoch boundaries.
- `runtime.state.rejected_write_count`: blackboard writes rejected by empty namespace/key/writer, null payload, or single-writer policy.
- `runtime.state.snapshot_read_count`: blackboard snapshots/read calls.
- `runtime.state.current_value_count`: current committed blackboard values.
- `runtime.config.version`: committed config transaction version. Initial graph/component config load starts at version 0.
- `runtime.config.last_transaction_id`: most recently committed config transaction id.
- `runtime.config.staged_update_count`: component config snapshots staged for an epoch-boundary update.
- `runtime.config.committed_update_count`: component config snapshots applied at epoch boundaries.
- `runtime.config.immediate_update_count`: explicitly immediate component config updates.
- `runtime.config.rejected_update_count`: rejected config updates.
- `runtime.config.rolled_back_update_count`: pending component config updates discarded by failed validation/apply rollback.
- `runtime.config.snapshot_read_count`: graph/component config snapshot reads.

Component lifecycle:

- `runtime.lifecycle.reset_count`: component reset hooks completed at the start epoch boundary.
- `runtime.lifecycle.reset_failure_count`: reset hooks rejected or threw before scheduler execution.
- `runtime.lifecycle.restore_count`: component state snapshots restored before execution.
- `runtime.lifecycle.restore_failure_count`: restore requests rejected by component id, type, version, or payload validation.
- `runtime.lifecycle.snapshot_count`: component state snapshots captured after scheduler execution.
- `runtime.lifecycle.snapshot_failure_count`: snapshot capture failures reported after execution.
- `runtime.lifecycle.snapshot_size_bytes`: total byte size reported or estimated for captured component snapshots.

Trace:

- `runtime.trace.event_count`: structured trace events emitted during the run.

Runtime observer:

- `runtime.observer.failure_count`: best-effort observer callback/status failures recorded without changing runtime `ok`.
- `runtime.observer.dropped_event_count`: events dropped by bounded observers such as `InMemoryRuntimeObserver`.

Custom histograms:

- `<name>.count`, `<name>.min`, `<name>.max`, `<name>.avg`, `<name>.p50`, `<name>.p95`, `<name>.p99`: summaries emitted by `MetricRegistry::histogram(name)`.

## Aggregate Counters

The top-level JSON result also carries aggregate counters for common dashboards:

- `channel_publish_count`
- `channel_delivery_count`
- `channel_drop_count`
- `payload_copy_count`
- `staged_publication_count`
- `committed_publication_count`
- `delayed_publication_count`
- `state_publication_count`
- `state_commit_count`
- `async_publication_count`
- `failed_publication_commit_count`
- `loop_iteration_count`
- `loop_converged_count`
- `loop_budget_overrun_count`
- `loop_max_iteration_hit_count`
- `loop_cancellation_requested_count`
- `loop_cancellation_observed_count`

These fields summarize the sample array for quick CLI and test assertions; the sample array remains the extensible product surface. Correlation and causation metadata are emitted on trace events and invocation/channel records, but they are not default metric labels to avoid cardinality explosion.


## Channel health metrics

See [channels.md](channels.md) for the channel/backpressure contract. Deadline misses, stale drops, rejects, overwrites, and aggregate health counters are exported as stable `runtime.channel.*` metrics. Optional bounded health events are observer records in `RuntimeRunnerResult::health_events` and trace output, not default metric labels.
