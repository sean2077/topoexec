# Channels and Backpressure

TopoExec channels are bounded runtime contracts for every edge. They define what is stored, when a reader can observe it, what happens under overload, and which metrics explain degradation.

## Modes

| Mode | Behavior | Typical use |
| --- | --- | --- |
| `latest` | Keeps only the newest message. A newer publication overwrites the prior value and increments drop/overwrite health metrics. | Low-latency sensor/frame path where stale work is worse than lost work. |
| `queue` / `ring_buffer` | Keeps messages in FIFO order up to `capacity`; `drop_oldest`/`overwrite` discard oldest work when full. | Commands or event streams that should preserve order within a bound. |
| `latched` | Keeps the last message and lets late readers see it once. | Configuration or snapshot-like boundary value. |
| `previous_tick` | Publishes into a pending slot and exposes it only after `advance_epoch()`. | Feedback that must be delayed by one epoch. |
| `barrier` | Delivers only after `capacity` messages are queued. | Small synchronization batches. |

All modes have finite `capacity`. `capacity <= 0` is invalid.

## Overflow and health accounting

Overflow policy is explicit:

- `overwrite` / `drop_oldest`: accept the new publication and drop older stored work where needed.
- `drop_newest`: reject the new publication and preserve existing queued work.
- `reject` / `fail_fast`: reject capacity overflow; `fail_fast` uses the channel-capacity error path.
- `block`: reports `would block producer` in the current non-blocking runtime. It does not block an event-loop thread.

Channel metrics distinguish common causes:

- `runtime.channel.drop_count`: all dropped or failed-overflow messages.
- `runtime.channel.overwrite_count`: stored work overwritten or oldest queued work discarded to accept newer work.
- `runtime.channel.reject_count`: publications rejected or would-blocked by capacity policy.
- `runtime.channel.stale_drop_count`: messages dropped because `lifespan_ms` expired before delivery.
- `runtime.channel.deadline_miss_count`: delivered messages older than `deadline_ms`.
- `runtime.channel.health_event_count`: aggregate health/degradation events from overwrite, reject, stale, or deadline paths.

`RuntimeChannelMetrics::degradation_reason` stores the latest human-readable reason for a degradation path.

## Read semantics

Low-level readers expose the same semantics used by the runtime trigger engine:

- `peek_latest_for_component_port()` returns the latest visible value without marking it delivered.
- `snapshot_for_component_port()` copies the currently visible latest/queued messages without consuming them.
- `drain_for_component_port(..., max_batch)` consumes a bounded batch and preserves remaining queued messages for later drains.
- `consume_for_component()` drains all visible inputs for the component.
- `drain_for_reader(channel_id, reader_id, max_batch)` is the explicit per-reader queue API.

Runtime channel messages carry invocation metadata (`correlation_id`, `causation_id`, `epoch_id`, `transaction_id`, source endpoint, and trigger kind) alongside payload and timing fields. This metadata flows through immediate, delay/state/async, task-completion, and CompositeLoop external commits; metrics avoid using it as default labels.

`readers: single` is the default. Queue-style single-reader drains remove delivered messages from the channel. `readers: multi` / `readers: multiple` keeps bounded queue storage and tracks a per-reader sequence cursor so each explicit reader can observe each retained queued message once. Because retained history is still bounded by channel `capacity`, a slow reader can miss messages dropped by overflow.

`move_only` payloads require `readers: single`; use `shared_view` or `copy` for multi-reader paths.

## Lifespan and deadline

- `lifespan_ms` drops stale messages before delivery or snapshot and increments `stale_drop_count` plus `health_event_count`.
- `deadline_ms` still delivers the message, marks `RuntimeChannelMessage::deadline_missed`, increments `deadline_miss_count`, and records message age metrics.

The message `received_at` timestamp remains the publication/enqueue timestamp. Trigger policies such as batch windows depend on that invariant; delivery latency is reported separately through metrics.
