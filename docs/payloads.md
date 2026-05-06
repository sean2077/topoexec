# Payloads And Ownership

TopoExec payloads are small value wrappers around one of the built-in runtime payload categories:

- `TextPayload`: UTF-8/string-like control data.
- `BinaryBlobPayload`: immutable byte ranges backed by `SharedBuffer`.
- `FrameView`: structured frame/buffer views, including loaned buffers.
- `OpaquePayload`: type-erased immutable application payloads with schema, byte-size hint, and debug summary.

`RuntimePayload` stores the schema string beside the variant value. The schema is useful for graph contracts and diagnostics; the C++ variant is the type-safe access path. Custom application schemas can use `make_custom_payload<T>()` / `OpaquePayload` without adding new core dependencies.

## Typed Access

Use typed helpers in component code:

```cpp
const auto& text = invocation.payload_as<topoexec::TextPayload>();
const auto* maybe_frame = invocation.try_payload_as<topoexec::FrameView>();
```

For direct payload values:

```cpp
if (topoexec::payload_is<topoexec::BinaryBlobPayload>(payload)) {
  const auto& blob = topoexec::payload_as<topoexec::BinaryBlobPayload>(payload);
}
```

Bad access throws `std::runtime_error` with context. Components that prefer non-exception reporting can catch that error and return `Status::error(...)` from `execute_status()`.

If an `Invocation` has no primary payload, `try_payload_as<T>()` returns `nullptr` and `payload_as<T>(context)` throws with the supplied context string. Use that context to name the component port, for example `payload_as<TextPayload>("consumer.in")`.

Input lookup by port is nullable:

- `context.inputs().peek_latest("port")` returns `nullptr` when no payload is visible or the port is unknown.
- `context.inputs().read_latest_update("port")` returns `nullptr` when no unread payload exists.
- `context.inputs().drain("port")` returns an empty vector when no queued payloads exist or the port is unknown.

Batch triggers expose ordered `Invocation::batch_payloads`; use the same typed helpers on each non-null payload pointer.

## Copy Policy

Edge `policy.copy_policy` controls how published payloads enter runtime channels:

- `copy`: copies text payloads. Large payloads are rejected instead of silently copied.
- `shared_view`: stores the shared immutable `RuntimePayloadPtr`; use it for multi-reader immutable values.
- `loaned_view`: preserves loaned frame/buffer identity without copying; use it for in-process frame/buffer handoff when all consumers treat the view as immutable.
- `move_only`: avoids payload copies and is allowed only with `readers: single`.

Copy metrics are exposed as `runtime.channel.payload_copy_count`. Large payload copy rejection records channel degradation details and returns a failed publication result. Buffer reuse metrics are available through `BufferPoolStats`; see [memory.md](memory.md).

## Lifetime Rules

- `RuntimePayloadPtr` is a `std::shared_ptr<const RuntimePayload>`.
- Shared and loaned views must point at immutable data for the duration of graph visibility.
- Producers should not mutate buffers after publishing them.
- Consumers should treat all payloads as read-only.
- Multi-reader edges cannot use `move_only`.
- `loaned_view` currently preserves in-process `FrameView` / `SharedBuffer` identity; it is not an external shared-memory middleware.
- `LoanedFrame::detach()` transfers the frame view out of `BufferPool` automatic return accounting. A detached frame published through `loaned_view` keeps the buffer alive without a copy, but explicit release callbacks or zero-copy pool return from channel readers are deferred to the payload/memory v2 work.

Ownership flow:

```text
copy         producer value -> runtime-owned copied text payload -> one or more readers
shared_view  producer/runtime shared_ptr<const RuntimePayload> ---> retained channel view ---> readers
loaned_view  detached FrameView/SharedBuffer --------------------> retained channel view ---> readers
move_only    producer payload -----------------------------------> single-reader channel ---> one reader
```

Plan/explain output includes the selected copy policy and reader policy per edge. Lint flags large payloads with `copy` and invalid `move_only` multi-reader combinations so ownership mistakes are visible before runtime.
