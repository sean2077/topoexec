# Memory and Buffer Pools

TopoExec payload memory is explicit and observable. Runtime channels do not silently deep-copy large payloads.

## Built-in ownership shapes

- `RuntimePayloadPtr` is `std::shared_ptr<const RuntimePayload>` and should be treated as immutable after publication.
- `BinaryBlobPayload` is a byte range over `SharedBuffer`.
- `FrameView` is a structured view over `SharedBuffer` with width, height, stride, and format metadata.
- `OpaquePayload` is a type-erased immutable object for application-defined schemas. It stores a shared pointer, byte-size hint, and debug summary.

Use `make_custom_payload<T>(shared_ptr<const T>, schema, summary)` when an application needs a custom schema without changing TopoExec core variants.
Use `describe_payload_schema(payload)` when tooling needs a stable type name, schema id, summary string, size estimate, and large-payload flag without inspecting the variant directly.

## BufferPool

`BufferPool` provides a small in-process loan/reuse helper for frame-like buffers:

1. `loan_frame(size, width, height, stride, format)` returns a move-only `LoanedFrame`.
2. Destroying or releasing the loan returns the buffer to the pool.
3. `LoanedFrame::detach()` transfers the `FrameView` out; after detach, the runtime payload keeps the shared buffer alive but the buffer is no longer automatically recycled by this pool.

`BufferPoolConfig` can bound and shape allocations without adding an external allocator:

- `fixed_block_size`: round allocations up to a fixed in-pool block size when buckets are not configured.
- `bucket_sizes`: choose the smallest configured bucket that can hold the requested frame.
- `alignment`: round allocation sizes up to an alignment multiple. This is size rounding, not a hard OS/page-alignment claim.
- `max_bytes`: reject new allocations once pool-owned bytes would exceed the bound; reuse of already-owned buffers still works.

`BufferPoolStats` exposes allocation, reuse, loan, release, active, detached, exhausted, available-buffer, and byte counters:

- `alloc_count`
- `reuse_count`
- `loan_count`
- `release_count`
- `detached_count`
- `exhausted_count`
- `bytes_allocated`
- `bytes_owned`
- `active_bytes`
- `high_watermark_bytes`
- `bytes_available`
- `active_count`
- `available_count`
- `max_bytes`

`active_count` / `active_bytes` are the leak-detection surface: they remain non-zero while a loan is outstanding. `has_outstanding_loans()` is a convenience check for the same condition. Detached buffers decrement active and owned pool accounting because their lifetime moved to the returned `FrameView` / `RuntimePayloadPtr`.

The pool is intentionally simple and in-process. It is not a cross-process shared-memory allocator, and `loaned_view` currently means "preserve in-process buffer identity without copying," not external SHM zero-copy.
