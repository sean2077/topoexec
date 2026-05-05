# Memory and Buffer Pools

TopoExec payload memory is explicit and observable. Runtime channels do not silently deep-copy large payloads.

## Built-in ownership shapes

- `RuntimePayloadPtr` is `std::shared_ptr<const RuntimePayload>` and should be treated as immutable after publication.
- `BinaryBlobPayload` is a byte range over `SharedBuffer`.
- `FrameView` is a structured view over `SharedBuffer` with width, height, stride, and format metadata.
- `OpaquePayload` is a type-erased immutable object for application-defined schemas. It stores a shared pointer, byte-size hint, and debug summary.

Use `make_custom_payload<T>(shared_ptr<const T>, schema, summary)` when an application needs a custom schema without changing TopoExec core variants.

## BufferPool

`BufferPool` provides a small in-process loan/reuse helper for frame-like buffers:

1. `loan_frame(size, width, height, stride, format)` returns a move-only `LoanedFrame`.
2. Destroying or releasing the loan returns the buffer to the pool.
3. `LoanedFrame::detach()` transfers the `FrameView` out; after detach, the runtime payload keeps the shared buffer alive but the buffer is no longer automatically recycled by this pool.

`BufferPoolStats` exposes allocation, reuse, loan, release, available-buffer, and byte counters:

- `alloc_count`
- `reuse_count`
- `loan_count`
- `release_count`
- `bytes_allocated`
- `bytes_available`
- `available_count`

The pool is intentionally simple and in-process. It is not a cross-process shared-memory allocator.
