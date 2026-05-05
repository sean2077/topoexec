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

## Stable Names

Scheduler:

- `runtime.scheduler.completed_count`: completed component invocations for a lane.
- `runtime.scheduler.tick_overrun_count`: lane tick overruns observed by the scheduler.
- `runtime.scheduler.queue_depth`: maximum queued scheduler tasks observed for a lane. This is `0` for the single-thread event loop.
- `runtime.scheduler.queue_capacity`: configured/effective pending queue capacity for the lane.
- `runtime.scheduler.worker_count`: configured/effective active worker width for the lane.
- `runtime.scheduler.last_callback_duration_ms`: latest scheduler iteration duration observed for the lane.
- `runtime.scheduler.tick_jitter_ms`: positive simulated overrun amount above the fixed-rate period or tick budget.
- `runtime.scheduler.active_count`: maximum active workers observed for a lane. This is `0` for the single-thread event loop.
- `runtime.scheduler.in_flight_count`: maximum in-flight scheduler tasks observed for a lane. This is `0` for the single-thread event loop.
- `runtime.scheduler.rejected_count`: scheduler admission rejections, skipped ready invocations, or dropped ready invocations for a lane.

Components:

- `runtime.component.execution_count`: completed `Component::execute_status()` invocations for a component.
- `runtime.component.error_count`: failed status-returning or exception-wrapped component invocations.
- `runtime.component.last_duration_ns`: most recent component execution duration in nanoseconds.
- `runtime.component.max_duration_ns`: maximum component execution duration observed in the run.
- `runtime.component.budget_overrun_count`: component invocations whose duration exceeded `execution.budget_ms`.
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

Publication router:

- `runtime.publication.staged`: total staged publications observed by `GraphContext::publish()`.
- `runtime.publication.committed`: staged publications committed to runtime channels.
- `runtime.publication.delayed`: publications deferred by `delay` edges.
- `runtime.publication.state`: publications deferred by `state` edges.
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

Trace:

- `runtime.trace.event_count`: structured trace events emitted during the run.

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
- `async_publication_count`
- `failed_publication_commit_count`
- `loop_iteration_count`
- `loop_converged_count`
- `loop_budget_overrun_count`
- `loop_max_iteration_hit_count`

These fields summarize the sample array for quick CLI and test assertions; the sample array remains the extensible product surface.


## Channel health metrics

See [channels.md](channels.md) for the channel/backpressure contract. Deadline misses, stale drops, rejects, overwrites, and aggregate health events are exported as stable `runtime.channel.*` metrics.
