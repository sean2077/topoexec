# Changelog

TopoExec follows the versioning policy in [docs/versioning.md](docs/versioning.md).

## Unreleased

No changes yet.

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
