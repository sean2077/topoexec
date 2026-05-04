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

Channels:

- `runtime.channel.publish_count`: accepted publications for a channel.
- `runtime.channel.delivery_count`: delivered messages for a channel.
- `runtime.channel.drop_count`: dropped, overwritten, rejected, or failed-fast channel publications.
- `runtime.channel.max_depth`: maximum observed channel depth.
- `runtime.channel.payload_copy_count`: payload copies forced by channel copy policy.

Publication router:

- `runtime.publication.staged`: total staged publications observed by `GraphContext::publish()`.
- `runtime.publication.committed`: staged publications committed to runtime channels.
- `runtime.publication.delayed`: publications deferred by `delay` edges.
- `runtime.publication.state`: publications deferred by `state` edges.
- `runtime.publication.async`: publications deferred by `async` edges.
- `runtime.publication.failed_commit`: failed publication commit batches.

Composite loops:

- `runtime.loop.iterations`: loop iterations executed for a CompositeLoop region. `component_id` carries the loop id.
- `runtime.loop.converged`: convergence stops for a CompositeLoop region. `component_id` carries the loop id.
- `runtime.loop.budget_overrun`: budget stops for a CompositeLoop region. `component_id` carries the loop id.
- `runtime.loop.max_iterations_hit`: max-iteration stops for a CompositeLoop region. `component_id` carries the loop id.

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
