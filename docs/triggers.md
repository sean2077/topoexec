# Trigger Semantics

Trigger readiness is owned by the runtime. Components receive `Invocation` objects only after the trigger engine has drained inputs, applied timing/admission policy, and selected the trigger kind.

## Trigger types

| Type | Runtime behavior |
| --- | --- |
| `manual` | Produces one manual invocation when the scheduler reaches the component. |
| `timer` / `on_tick` | Produces timer invocations from timer event sources. Current time progression is simulated by runner ticks. |
| `any_input` / `on_event` | Produces one invocation per pending input message unless coalescing or rate limiting applies. |
| `all_inputs` | Waits until every configured input has one pending message, then consumes one from each input. |
| `time_sync` | Waits for every input and, when comparable event timestamps exist, drops oldest out-of-slop samples until the front messages align. |
| `batch` | Flushes `batch_size` messages or, when `batch_window_ms` expires, flushes the available partial batch. |
| `request` | Produces request invocations with `EventKind::kRequest` and `TriggerKind::kRequest`. |
| `task_ready` / `future_ready` | Produces task/future-ready event invocations from async completion-style inputs. |

## Coalescing, rate limits, and timeouts

- `coalesce: true` keeps only the latest pending message per input for one invocation.
- `min_interval_ms` suppresses repeated invocations inside the interval.
- `max_latency_ms` is the current timeout guard for pending input messages: messages older than this limit are dropped by the trigger engine before readiness is evaluated.

Timeouts are cooperative and deterministic; they do not interrupt component code that is already executing.

## Request and future metadata

Every message-driven invocation carries:

- `channel_id`: the channel that supplied the first payload;
- `correlation_id`: a stable chain-level id, initially derived from the root channel message when no upstream correlation exists;
- `causation_id`: the direct channel message id (`<channel_id>#<message_sequence>`) that caused this invocation;
- `epoch_id`, `transaction_id`, source component/port, and trigger kind;
- `received_at` / `published_at` timestamps;
- `deadline_missed` and optional event timestamp metadata from the source message.

The correlation and causation ids are intended for trace/log diagnostics and response-routing plans, not default metric labels. A full service/future response API remains deferred; the task-executor preview only routes completed task payloads back through normal graph edges.

## Metrics

Trigger metrics are exported with component ids:

- `runtime.trigger.ready_count`
- `runtime.trigger.suppressed_count`
- `runtime.trigger.coalesced_count`
- `runtime.trigger.timeout_drop_count`
- `runtime.trigger.batch_flush_count`
- `runtime.trigger.time_sync_drop_count`

These metrics are stable enough for scripts and golden tests. They intentionally describe trigger-engine decisions rather than component behavior.
