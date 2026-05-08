# Changelog

TopoExec follows the versioning policy in [docs/43-ci-build-release-tools/versioning.md](docs/43-ci-build-release-tools/versioning.md).

## Unreleased

### Added

- Added aggregate channel overwrite/reject/stale/deadline fields beside
  `channel_drop_count` so CLI output and dashboards can distinguish real drops
  from intentional latest-value overwrite.

- Added release/adoption readiness for the `v0.2.0-alpha.0` candidate line:
  Apache-2.0 public metadata, `0.2.0` CMake/package draft version metadata,
  adoption guidance, prerelease notes, install/downstream onboarding, and
  local release gates without tagging or publishing.
- Added a synthetic dogfood pilot under `examples/90-dogfood-pilot/` with graph
  metadata, live assertion YAML, generated topology/metrics/trace assets,
  benchmark coverage, and case-study docs that keep hardware, ROS 2, ML runtime,
  production telemetry, and hard-real-time claims out of scope.
- Added a stable-v0.2 compatibility harness covering installed-header API
  inventory, versioning/deprecation docs, doctor/schema/metrics/trace/live
  observe JSON fields, golden files, schema smoke, live observe smoke, and
  runtime-only package consumption.
- Added distribution/package hardening with a package matrix, package metadata
  alignment checks, package-draft version/license docs, CPack/package-draft
  smokes, runtime-only smokes, downstream install smokes, and deferred registry
  publication.
- Added bounded reliability and adoption-readiness docs: soak-lite smoke,
  reliability policy checks, fuzz corpus ownership, perf regression policy,
  failure artifact conventions, non-blocking TSAN policy, debug-pack guidance,
  issue-template prompts, and first-user triage checks.
- Added ecosystem and release decision records that keep production telemetry
  exporters, native Python bindings, real ROS 2 adapters, editor/LSP,
  package-registry publication, schema v2/migration, beta readiness, and v1.0
  readiness blocked on adoption evidence and human owner decisions.
- Added final validation coverage for the prerelease candidate state, including
  the full repository gate plus focused release/adoption/dogfood/reliability/
  compatibility/live/performance/fuzz/stress/sanitizer checks.
- Added examples/showcase refresh with a generated README hero and visual
  showcase, 9 curated example directories with metadata/README pages, generated
  graph/metrics/trace assets under `docs/assets/generated`,
  `scripts/update_examples_index.py`, `scripts/render_example_assets.py`,
  `scripts/examples_smoke.py`, `scripts/check_readme_assets.sh`, and focused
  `./scripts/goal_check.sh examples` / `showcase` anti-rot gates.
- Added low-overhead live runtime validation with compile-time optional
  runtime live-observe transport, default-off `RuntimeRunnerOptions`
  activation, `topoexec graph observe` NDJSON/json-summary output,
  tooling-layer live assertions, replayable record artifacts, a local
  observe-only SSE dashboard, live/live-perf focused gates, and benchmark cases
  for live-observe overhead checks.
- Added post-alpha hardening coverage for previous-tick wake behavior,
  bounded trigger pending queues, condition timestamp head handling, async
  in-flight accounting, CompositeLoop output visibility, benchmark expansion,
  optional Doxygen API reference generation, and GitHub Pages site wiring.
- Added `runtime.trigger.pending_drop_count` to expose drops from bounded
  internal trigger pending queues.
- Added benchmark cases for combined channel modes and trigger-v2 policies, and
  expanded the benchmark contract/baseline scripts to include them.
- Added optional `TOPOEXEC_BUILD_DOCS` / `topoexec_doxygen` CMake support,
  `docs/61-api/doxygen.md`, `mkdocs.yml`, `scripts/docs_build_site.sh`, and a
  GitHub Pages workflow that publishes Markdown docs plus Doxygen HTML under
  `/api/`.
- Added CI preview-option smoke coverage for adapter, FFI, Python preview, and
  plugin-loader surfaces without making them runtime dependencies.
- Added release-candidate baseline docs for the architecture-stabilization stage;
  completed process details are summarized in the current goal ledgers.
- Added normalized golden coverage for Chrome trace shape, schema dump JSON, and doctor JSON.
- Added explicit stable-v0.2/mixed/experimental public API markers, an API change checklist, and stronger runtime-only downstream smoke coverage for result metrics/trace consumption.
- Added `docs/21-architecture/semantic-contract.md` and exposed `semantic_contract_version` through doctor/schema dump outputs.
- Added architecture policy checks for installed-header markers, runtime/YAML/CLI target boundaries, private include leaks, semantic-bypass CLI includes, and planted-violation self-tests.
- Added scheduler v2 capability summaries in plan JSON and advisory diagnostics for parsed-but-not-enforced lane/execution fields.
- Added persistent `thread_pool` worker-pool v1 with bounded queue admission, worker-id trace attributes, and stop/drain coverage for queued work.
- Added opt-in `fixed_rate` wall-clock cadence v1 with `overrun_policy`, tick/skipped/max-lateness metrics, and fixed-rate trace events while keeping deterministic stepping as the default.
- Added runtime-level scheduler priority/admission v1 with `execution.priority` classes, priority queue ordering, priority metrics, low-priority rejection metrics, schema validation, and starvation smoke coverage.
- Added cooperative cancellation/timeout semantics v1 with `CancellationToken`, `GraphContext::cancel_requested()`, `Invocation::cancel_requested()`, component/loop/task cancellation metrics, and post-return timeout-budget reporting without hard preemption.
- Added task-executor v2 preview with `ITaskExecutor`, explicit `DeterministicTaskExecutor`, opt-in bounded `ThreadedTaskExecutor`, queued-task metrics, shutdown policy, and threaded completion-routing tests.
- Added Trigger Engine v2 preview policies: `watermark`, `condition`,
  `debounce`, and `rate_limit`, with declarative schema fields, late/drop and
  suppression metrics, and runtime/graph coverage without arbitrary scripting.
- Added invocation metadata v1 with correlation, causation, epoch, transaction, source endpoint, and trigger-kind propagation through publish/channel/trigger/invocation/task/composite-loop paths plus trace attributes.
- Added bounded runtime health events for channel overflow/stale/deadline/high-watermark, task reject, and scheduler reject paths, exposed through `RuntimeRunnerResult`, CLI JSON, doctor feature metadata, and trace events without adding health-triggered control flow.
- Added edge reader/copy-policy explainability in plan/explain output, including `slow_reader_drop_risk`, plus lint surfacing for slow multi-reader drop risk and invalid `move_only` multi-reader edges.
- Added `BufferPoolConfig`, bounded pool allocation stats, payload schema summaries, and loaned-view pool-owner lint coverage for in-process large-payload memory planning.
- Added `OpaquePayload`/`make_custom_payload` and BufferPool loan/release/byte metrics with memory docs.
- Added descriptor-backed typed port validation for schema/payload-type compatibility, required and optional inputs, input multiplicity, and boundary role mismatches without adding schema v1 port fields.
- Added hierarchical `subgraphs[]` as schema-v1 compile-time namespace
  expansion, with expanded `GraphHierarchyEntry` plan metadata, last-dot
  endpoint parsing for namespaced component ids, CompositeLoop ownership after
  expansion, and graph/runtime tests proving hierarchy does not hide cycles or
  metric paths.
- Added graph templates with schema-v1 `templates[]` and `template_instances[]`
  for strict scalar `{{parameter}}` substitution, deterministic namespace
  expansion before validation/runtime, invalid-parameter tests, and a runnable
  source-transform-sink template example without a runtime template interpreter.
- Added CompositeLoop `solver_iteration` preview with loop-local iteration
  context, typed convergence/residual reports, residual-threshold convergence,
  partial-success output discard/fail/commit policy, runtime residual/discard
  metrics, and trace evidence while keeping cycles exact-owned and in-process.
- Added experimental component reset, snapshot, and restore lifecycle hooks with start-epoch runner options, post-run snapshot capture, lifecycle metrics, trace events, and cleanup-on-failure coverage.
- Added experimental config hot-reload transactions with component validate/apply hooks, epoch-boundary commit, transaction/version metrics, and rollback coverage for invalid or failed applies.
- Added RuntimeObserver v1 with result/metric/trace/health/error callbacks, runner option registration, no-op and bounded in-memory observers, and non-fatal observer failure/drop metrics.
- Added runtime metric schema version 1 with descriptor metadata, cardinality validation, forbidden high-cardinality default-label checks, and CLI metrics JSON schema-version output.
- Added runtime trace schema version 1 with ordered timeline fields, explicit phase/component/channel/lane/worker/epoch/transaction/correlation/causation identifiers, and Chrome trace phase tracks.
- Added graph diagnostic schema version 1 with stable severity/category fields, warning diagnostics for backpressure/deep queues/large copies/never-ready triggers, grouped explain output, and CLI strict-diagnostics failure mode.
- Added `GraphInputLimits`, bounded incremental graph file reads, UTF-8 input validation, non-config string limits, CLI parser-limit overrides, schema string-limit checks, and stronger deterministic malformed-input fuzz coverage.
- Added optional `TOPOEXEC_BUILD_FUZZERS` support with libFuzzer/standalone `fuzz_graph_inputs`, checked-in seed corpus, local fuzz smoke script, and optional Clang CI fuzz smoke.
- Added bounded stress and soak testing with `test_stress`, generated scheduler/channel graph workloads, `scripts/stress_smoke.sh`, release-candidate stress documentation, and queue-depth/drop/reject assertions for scheduler, channel, thread-pool, and task-executor surfaces.
- Added graph-level config parsing plus epoch-boundary state/config snapshot stores and state commit metrics.
- Added the stable graph diagnostics registry and histogram p50/p95/p99 snapshot samples.
- Added benchmark schema v2 with expanded graph cases, graph hashes, compiler/build/CPU/commit metadata, a non-installed task-executor benchmark, output-contract CTest coverage, and optional local baseline generation without global timing thresholds.
- Added packaging v2 smoke coverage with installed CMake package metadata, runtime-only/YAML/imported-CLI downstream consumers, installed-schema CLI lookup, CPack TGZ generation, and reviewable vcpkg/Conan draft files.
- Added documentation system v2 with a reorganized docs map, executable cookbook recipes, architecture diagrams, why-not comparisons, design principles, and recursive docs smoke coverage for required pages/sections.
- Added example applications v2 with dependency-free reference apps for low-latency latest/drop, fixed-rate state feedback, request/validator/task completion, CompositeLoop solver convergence/budget overrun, and BufferPool copy/shared/loaned payload metrics.
- Added Adapter SDK v0 with `topoexec::adapter_sdk`, `topoexec/adapters/sdk.hpp`, observer/result-sink aliases, bounded `BoundaryBridge` contracts, explicit `ComponentFactoryProvider`, adapter package smoke tests, and architecture policy coverage that keeps runtime dependency-free.
- Added the default-off `topoexec_adapters::otel` exporter preview with
  `TOPOEXEC_BUILD_OTEL_ADAPTER`, dependency-free metric/trace/health/error
  mapping records over the observer API, installed package metadata, downstream
  CMake smoke coverage, and policy checks proving runtime has no telemetry SDK
  dependency.
- Added the default-off `topoexec_adapters::prometheus` exporter preview with
  `TOPOEXEC_BUILD_PROMETHEUS_ADAPTER`, dependency-free Prometheus text
  exposition for descriptor-backed counters/gauges and custom histogram
  summaries, installed package metadata, downstream CMake smoke coverage, and
  policy checks proving runtime has no HTTP server or Prometheus library
  dependency.
- Added the default-off `topoexec_adapters::ros2` adapter preview with
  `TOPOEXEC_BUILD_ROS2_ADAPTER`, dependency-free topic/service/action endpoint
  descriptors, adapter-side QoS mapping, fake boundary bridge tests, installed
  package metadata, downstream CMake smoke coverage, and policy checks proving
  runtime has no ROS package dependency.
- Added the default-off `topoexec::c_api` FFI preview with
  `TOPOEXEC_BUILD_C_API`, `topoexec/c_api/topoexec.h`, opaque runtime/graph/result
  handles, create/run/destroy, borrowed error strings, metric iteration,
  downstream C smoke coverage, and ABI version `0` to avoid accidental ABI
  freeze.
- Added the default-off `topoexec_preview` Python automation preview with
  `TOPOEXEC_BUILD_PYTHON_PREVIEW`, stdlib-only CLI-backed validate/plan/run,
  metrics, and trace helpers, source/installed Python smokes, and disabled
  runtime-only C++ option coverage proving Python is not a required runtime
  dependency.
- Added the default-off `topoexec::plugin_loader` trusted-native dynamic plugin
  loader preview with `TOPOEXEC_BUILD_PLUGIN_LOADER`, manifest/plugin-API/schema
  validation, descriptor mismatch errors, sample native plugin smokes, installed
  package metadata, no default unload, and policy checks proving runtime has no
  dynamic-loader dependency.
- Added release automation with `scripts/release_prepare.sh`, `./scripts/goal_check.sh release`, manual CI dry-run artifact upload, release notes draft generation, annotated-tag/no-retag guardrails, source/CPack/schema artifacts, and checksum output without publishing or tagging automatically.
- Added developer quality gates with blocking `clang-format`, `clang-tidy`, local pre-commit hooks, CMake presets, and shared CI/release-prep wiring.
- Added the robot-cell pilot app and case study, composing multiple lanes, async overload drops, state/delay feedback, BufferPool frames, config transactions/snapshots, metrics/trace evidence, and invalid-config rejection without adapter dependencies.
- Added the beta readiness review, beta-candidate gate checklist, explicit
  deferred-scope ledger, and pre-1.0 deprecation policy for stable-v0.2,
  experimental, schema, and CLI JSON surfaces.
- Added `docs/33-specs-rfcs/schema-v2-notes.md` as the schema-v2 decision boundary, with
  candidate feature classification, additive-v1 vs breaking-v2 rules, migration
  guidance, docs-map coverage, and schema-contract coverage proving v1 remains
  strict and v2 sketches are not accepted by the v1 checker.
- Added `docs/41-development-tools/editor-schema.md` as the editor/schema UX guide, including
  schema discovery, VS Code/YAML Language Server association examples, inline
  modeline guidance, editor diagnostic JSON expectations, installed-schema
  package smoke coverage, and `editor_schema_ux_smoke` without adding a runtime
  dependency or LSP implementation.
- Added community readiness surfaces: root `CONTRIBUTING.md`, expanded
  contributor/governance guidance, root `CODE_OF_CONDUCT.md`, richer issue and
  PR templates for semantic/API/schema/component/metric changes, and
  `community_readiness_smoke` so human and agent-generated PRs share the same
  structure.
- Added `topoexec schema dump`, `topoexec schema check`, and `topoexec doctor` JSON/text tooling.
- Added state/config, batch/time-sync, service-style async, and boundary-adapter-pattern YAML examples plus an examples catalog with CLI smokes.
- Added a documentation index, tutorial/reference pages, and doc-command smoke coverage for the getting-started and CLI paths.
- Added deterministic graph-input fuzz smoke coverage plus explicit ASAN/UBSAN/TSAN sanitizer build gates.
- Added optional YAML/CLI/example CMake build switches, runtime-only option smoke coverage, and package-manager draft notes.
- Added adapter-boundary preview contracts, dependency-free adapter stub notes, and a policy smoke for accidental core adapter SDK dependencies.
- Added a ROS 2 adapter preview design covering boundary mapping, QoS separation, executor interaction, lifecycle, diagnostics, and fake-boundary-first tests.
- Added release progression docs that map completed goals to prerelease stages and refresh the release checklist evidence.
- Added optional deterministic task execution, `GraphContext::submit_task`, bounded task admission metrics, cancellation, and failure completions.
- Added CompositeLoop internal failure accounting, `runtime.loop.error`, `loop_error` trace events, and docs for external-output commit isolation.
- Added trigger timeout-drop, batch-flush, and time-sync-drop metrics plus local message correlation ids on `Invocation`.
- Added channel snapshot, bounded-drain, explicit per-reader queue drain, multi-reader cursor semantics, and channel health metrics for stale/drop/reject/overwrite/deadline paths.
- Added explicit scheduler lane admission fields (`queue_capacity`, `overflow`, `period_ms`, `tick_budget_ms`, `wall_clock_enabled`) with thread-pool queue admission metrics and `thread_pool_batch` trace spans.
- Added bounded `thread_pool` runtime execution for ready invocations, with `max_threads` worker width and non-reentrant serialization.
- Added async edge `policy.max_inflight` admission control before channel capacity, with async accepted/rejected/dropped/in-flight/completed metrics.
- Added a non-blocking GitHub Actions ThreadSanitizer job for the new concurrency surface.
- Added concurrency docs and runtime tests for reentrant worker overlap, non-reentrant serialization, and async admission drops.

### Fixed

- Rejected unsupported trigger contract shapes before runtime execution:
  timer/input-driven event source mixes, schema-v1 action goal/cancel event
  sources, and non-zero reserved `debounce_window_ms` values.
- Made fan-out and batch publication reject paths preflight before payloads
  become visible, while preserving reject/drop health and metric evidence on the
  rejecting channel.
- Bounded `ThreadedTaskExecutor` completed-record backlog by counting undrained
  completions toward admission and exposing `completed_backlog_depth`.
- Moved channel health-event sink calls outside the channel bus mutex while
  preserving in-lock metric and degradation accounting.
- Made `reject_new` lane overflow explicitly follow the reject-newest admission
  path with scheduler health-event policy coverage.
- Made opt-in fixed-rate wall-clock sleeps poll stop tokens instead of sleeping
  through the full period after a stop request.
- Made the release-prepare smoke idempotent when a local candidate tag already
  exists, without weakening the real release-prep existing-tag guard.
- Fixed `topoexec doctor --format json` graph inventory drift by discovering
  schema-versioned YAML examples and benchmark graphs recursively while
  excluding live-assertion YAML files from graph example lists.
- Removed internal goal-number wording from user-facing example and dogfood
  documentation so public guidance describes the examples by behavior rather
  than maintenance-program history.

### Changed

- Moved drained channel queue messages into component input batches instead
  of copying them, preserving delivery order while reducing hot-path payload
  churn.
- Replaced `InMemoryRuntimeObserver` front-vector erasure with bounded deque
  storage while keeping the public vector snapshot API unchanged.
- Expanded the focused schema gate to include CLI parser-limit and
  diagnostics error-path regression coverage.
- Clarified `overflow: block` as alpha would-block rejection rather than true
  producer blocking; true blocking remains deferred until cancellation/deadline
  semantics are designed.
- Clarified trigger-v2 `rate_limit`, `debounce`, and `condition` suppression as
  bounded and metric-visible; `condition: event_timestamp_present` drops
  missing-timestamp head items so later timestamped inputs are not permanently
  blocked.
- Optimized EventRuntime/RuntimeRunner hot paths by reusing run-scoped component,
  instance, spec, and region indexes instead of repeated vector scans while
  preserving deterministic output order.
- Reorganized documentation into numbered `docs/` zones and added architecture/codebase maps.
- Cleaned up completed planning docs by deleting obsolete long-form plan files and compressing goal/release ledgers.
- Runtime docs now describe `thread_pool` and async max-inflight as implemented MVP behavior instead of alpha limitations.
- Runtime docs now describe `thread_pool` as an experimental persistent worker-pool v1 while keeping OS priority/affinity/RT policy and hard timeout preemption deferred.
- Runtime docs now distinguish deterministic fixed-rate stepping from opt-in cooperative wall-clock cadence without claiming hard real-time scheduling or independent lane threads.
- Runtime docs now distinguish component/invocation runtime priority from advisory lane/OS priority fields.
- Runtime docs now describe cooperative cancellation and timeout-budget observation while keeping hard preemption deferred.
- Runtime docs now distinguish async-edge admission from optional deterministic/threaded task executor helpers.
- Runtime docs now describe bounded-cardinality correlation/causation metadata on invocations, channel messages, and trace events while keeping metrics labels stable by default.
- Runtime docs now distinguish channel health counters from optional bounded health events and document that health events are observer-only unless future graph-boundary wiring is explicitly added.
- Release docs now distinguish a conditional core-runtime beta candidate review
  from adapter/ecosystem beta readiness, package-registry publication, signed
  artifact release, sandboxed/stable plugin ecosystems, or hard real-time
  guarantees.
- Hardened multi-reader channel cursor/drop tests and shared/loaned/move payload lifetime/no-copy evidence while keeping deeper zero-copy pool-return APIs deferred.
- Hardened BufferPool tests for bounded allocation, exhaustion, detach accounting, outstanding-loan detection, release-on-drop, and copy/no-copy payload policy evidence.
- Current baseline and release checklist now record the post-alpha `main` commit, local CTest count, CI run, tag relationship, and remaining limitations.
- Added an optional CMake `topoexec_format_check` target for local clang-format validation.
- Expanded the public API map with stable, mixed, experimental, internal, schema, and CLI JSON compatibility boundaries.
- Hardened runtime edge-visibility invariant coverage for immediate feed-forward, delayed/state/async epoch boundaries, and staged/committed publication metrics.
- Hardened lifecycle invariant coverage for activate failure, deactivate failure, partial startup cleanup, and reverse deactivation order.
- Added fixed-seed graph compiler property coverage for randomized immediate-cycle rejection and exact CompositeLoop acceptance.
- Expanded scheduler and concurrency docs for event-loop, fixed-rate, and thread-pool lane enforcement boundaries, advisory policy fields, and worker-pool metrics.
- Hardened async `policy.max_inflight` tests for accept-within-limit, `drop_oldest`, `drop_newest`, `reject`, `fail_fast`, and `block` admission behavior.
- Hardened payload docs and tests for missing-port lookup, null invocation payload errors, ordered batch payload access, no-copy shared/loaned views, move-only validation, and large-copy rejection.

## v0.1.0-alpha - 2026-05-05

### Added

- Added examples/showcase refresh with a generated README hero and visual
  showcase, 9 curated example directories with metadata/README pages, generated
  graph/metrics/trace assets under `docs/assets/generated`,
  `scripts/update_examples_index.py`, `scripts/render_example_assets.py`,
  `scripts/examples_smoke.py`, `scripts/check_readme_assets.sh`, and focused
  `./scripts/goal_check.sh examples` / `showcase` anti-rot gates.
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
- CLI trace JSON now uses structured `trace` events directly and omits the legacy `trace_events` name list.
- Runtime runner failures now use structured `runtime_errors[]` only; the legacy
  runner `errors[]` string list was removed from runtime JSON and C++ API.
- `runtime.channel.drop_count` / `channel_drop_count` now count real drops only;
  latest, previous-tick, queue `drop_oldest`, and async admission overwrite
  paths increment overwrite counters instead.
- `thread_pool` lanes now execute through the experimental bounded worker-pool runtime instead of falling back to event-loop behavior.

### Known Limitations

- `thread_pool` scheduling is experimental and in-process only; OS priority/affinity, hard timeout preemption, and hard real-time guarantees remain deferred.
- Async max-inflight policy is represented by bounded async channel capacity and overflow policy, not a dedicated worker-pool admission controller.
- Production OpenTelemetry, Prometheus, ROS 2, native Python bindings, and Perfetto adapters are deferred until after beta core stabilization; current adapter/Python preview targets are dependency-free or CLI-backed previews only.
- ThreadSanitizer remains non-blocking policy evidence; ASAN+UBSAN is the blocking sanitizer gate.
