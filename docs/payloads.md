# Payloads And Ownership

TopoExec payloads are small value wrappers around one of the built-in runtime payload categories:

- `TextPayload`: UTF-8/string-like control data.
- `BinaryBlobPayload`: immutable byte ranges backed by `SharedBuffer`.
- `FrameView`: structured frame/buffer views, including loaned buffers.

`RuntimePayload` stores the schema string beside the variant value. The schema is useful for graph contracts and diagnostics; the C++ variant is the type-safe access path.

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

## Copy Policy

Edge `policy.copy_policy` controls how published payloads enter runtime channels:

- `copy`: copies text payloads. Large payloads are rejected instead of silently copied.
- `shared_view`: stores the shared immutable payload pointer.
- `loaned_view`: preserves loaned frame/buffer identity without copying.
- `move_only`: allowed only with `readers: single`.

Copy metrics are exposed as `runtime.channel.payload_copy_count`. Large payload copy rejection records channel degradation details and returns a failed publication result.

## Lifetime Rules

- `RuntimePayloadPtr` is a `std::shared_ptr<const RuntimePayload>`.
- Shared and loaned views must point at immutable data for the duration of graph visibility.
- Producers should not mutate buffers after publishing them.
- Consumers should treat all payloads as read-only.
- Multi-reader edges cannot use `move_only`.
