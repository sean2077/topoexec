# Changelog

TopoExec follows the versioning policy in [docs/versioning.md](docs/versioning.md).

## Unreleased

### Added

- Added trigger timeout-drop, batch-flush, and time-sync-drop metrics plus local message correlation ids on `Invocation`.
- Added channel snapshot, bounded-drain, explicit per-reader queue drain, multi-reader cursor semantics, and channel health metrics for stale/drop/reject/overwrite/deadline paths.
- Added explicit scheduler lane admission fields (`queue_capacity`, `overflow`, `period_ms`, `tick_budget_ms`, `wall_clock_enabled`) with thread-pool queue admission metrics and `thread_pool_batch` trace spans.
- Added bounded `thread_pool` runtime execution for ready invocations, with `max_threads` worker width and non-reentrant serialization.
- Added async edge `policy.max_inflight` admission control before channel capacity, with async accepted/rejected/dropped/in-flight/completed metrics.
- Added a non-blocking GitHub Actions ThreadSanitizer job for the new concurrency surface.
- Added concurrency docs and runtime tests for reentrant worker overlap, non-reentrant serialization, and async admission drops.

### Changed

- Runtime docs now describe `thread_pool` and async max-inflight as implemented MVP behavior instead of alpha limitations.
- Current baseline and release checklist now record the post-alpha `main` commit, local CTest count, CI run, tag relationship, and remaining limitations.
- Added an optional CMake `topoexec_format_check` target for local clang-format validation.
- Expanded the public API map with stable, mixed, experimental, internal, schema, and CLI JSON compatibility boundaries.
- Hardened runtime edge-visibility invariant coverage for immediate feed-forward, delayed/state/async epoch boundaries, and staged/committed publication metrics.
- Hardened lifecycle invariant coverage for activate failure, deactivate failure, partial startup cleanup, and reverse deactivation order.
- Added fixed-seed graph compiler property coverage for randomized immediate-cycle rejection and exact CompositeLoop acceptance.
- Expanded scheduler and concurrency docs for event-loop, fixed-rate, and thread-pool lane enforcement boundaries, advisory policy fields, and worker-batch metrics.
- Hardened async `policy.max_inflight` tests for accept-within-limit, `drop_oldest`, `drop_newest`, `reject`, `fail_fast`, and `block` admission behavior.
- Hardened payload docs and tests for missing-port lookup, null invocation payload errors, ordered batch payload access, no-copy shared/loaned views, move-only validation, and large-copy rejection.

## v0.1.0-alpha - 2026-05-05

### Added

- Split CMake package targets into `topoexec::core`, `topoexec::runtime`, and optional `topoexec::yaml`.
- Added a pure C++ graph builder and `examples/apps/cpp_builder_minimal`.
- Added package installation smoke coverage for downstream `find_package(topoexec)`.
- Added runtime invariant tests for non-recursive `publish()` staging and fixed-seed immediate-DAG compilation.
- Added structured runtime trace events and Chrome Trace / Perfetto-compatible CLI export.
- Added metrics and trace contract docs.
- Expanded schema v1 reference docs and invalid-schema CLI fixtures.
- Added per-app README tutorials with expected output.
- Added public API, scheduler, payload, FAQ, and adapter-boundary docs.
- Added status-returning component lifecycle and execute hooks for non-exception failure reporting.
- Added typed payload helper accessors for `RuntimePayload` and `Invocation`.
- Added component execution, trigger, and scheduler metric samples.
- Added real-duration component/scheduler/loop trace spans and edge-kind trace attributes.
- Added tests for status failure propagation, typed payload access, CompositeLoop ownership, thread-pool runtime rejection, and observability contracts.

### Changed

- CI now runs GCC and Clang across Debug and RelWithDebInfo builds.
- CLI trace JSON keeps the legacy `trace_events` name list and adds structured `trace` events.
- `RuntimeRunner` now rejects `thread_pool` lanes in `run` mode instead of silently executing them as event-loop work.

### Known Limitations

- Threaded worker-pool scheduling is not implemented; use `event_loop` for runnable alpha graphs.
- Async max-inflight policy is represented by bounded async channel capacity and overflow policy, not a dedicated worker-pool admission controller.
- OpenTelemetry, Prometheus, ROS 2, Python, and Perfetto adapters are deferred until after beta core stabilization.
- Sanitizer CI is planned but not yet wired.
