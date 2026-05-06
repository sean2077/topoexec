# Goal Status

Last updated: 2026-05-06

## Current Plan Source

- Completed architecture plan: `docs/plans/plan.md` (G0-G25 archived complete)
- Active architecture plan: `docs/plans/plan2.md` (G26+)
- Backlog: `docs/goals/backlog.md`
- Required repository gate: `scripts/agent_check.sh`

## Current Stage

Phase A is complete: G26 established the post-G25 release-candidate baseline, G27 completed the public API stability pass, G28 added the runtime semantic contract, and G66 enforced architecture boundaries. Phase B is complete through G34; Phase C has G35, G36, G37, G38, G39, G40, G41, G42, G45, G46, G47, G48, G49, G50, G51, G52, G53, G54, G55, G56, G57, G58, G59, G67, G69, and G70 complete; backlog-order lifecycle/config goals G43 and G44 are also complete.
All P0/P1 plan2 goals are complete. For the active "finish all plan2 goals" objective, the next unfinished backlog goals are G60-G65 adapter/interface/ecosystem preview work and G68 community readiness. Concrete adapter implementations remain deferred unless that scope is explicitly opened.

## Active / Recent Goals

| ID | Status | Evidence | Notes |
| --- | --- | --- | --- |
| G0-G25 | archived complete | Previous entries in git history through `b86a586d3a48d84bf4e03ccabde3d061e3073579`, release docs, golden/package/sanitizer evidence. | Do not treat the old board as active work unless a regression is found. |
| G26 | complete | `docs/current-baseline.md`, `docs/release-progression.md`, `docs/release-checklist.md`, `docs/goals/backlog.md`, `docs/goals/status.md`, `docs/plans/plan2.md`, and expanded `tests/golden/*` coverage. | Protects the post-G25 baseline, recommends `v0.2.0-alpha.0` as the next prerelease decision, and adds golden coverage for Chrome trace, schema dump, and doctor JSON. |
| G27 | complete | `docs/public-api.md`, `docs/api-change-checklist.md`, installed `include/topoexec/**` stability markers, `tests/cmake/runtime_smoke/main.cpp`, `docs/versioning.md`, and `CHANGELOG.md`. | Public API is classified as stable-v0.2/mixed/experimental; runtime-only downstream smoke covers GraphBuilder, ComponentRegistry, typed payloads, RuntimeRunner, and metrics/trace result consumption. |
| G28 | complete | `docs/semantic-contract.md`, `docs/versioning.md`, `docs/runtime-semantics.md`, `docs/schema-v1.md`, `docs/cli.md`, `include/topoexec/runtime/graph.hpp`, `tools/topoexec/main.cpp`, `schema/topoexec.schema.v1.json`, `tests/golden/doctor.json`, `tests/golden/schema_dump.json`, and `tests/schema/check_schema_contract.py`. | Runtime semantic contract version `0.2` is documented and exposed through doctor/schema dump without adding a new CLI command or changing graph behavior. |
| G66 | complete | `tests/policy/check_no_adapter_deps.py`, `CMakeLists.txt`, `scripts/goal_check.sh`, `docs/architecture-guardrails.md`, `docs/goals/backlog.md`, `docs/goals/status.md`, and `CHANGELOG.md`. | Architecture policy now audits installed-header markers, common/runtime/YAML/CLI boundaries, private include leaks, adapter tokens, CMake target links, CLI semantic-bypass includes, and planted fake dependency violations. |
| G29 | complete | `docs/scheduler.md`, `docs/concurrency.md`, `docs/diagnostics.md`, `src/graph.cpp`, `src/graph_io.cpp`, `src/diagnostics.cpp`, `tests/test_graph.cpp`, `tests/golden/plan_composite_loop.json`, `docs/goals/backlog.md`, `docs/goals/status.md`, and `CHANGELOG.md`. | Scheduler plan JSON now exposes lane capability summaries; validation emits advisory diagnostics for parsed-but-not-enforced lane/execution fields without failing valid graphs. |
| G30 | complete | `src/event_runtime.cpp`, `src/graph.cpp`, `src/graph_io.cpp`, `tests/test_runtime.cpp`, `tests/test_graph.cpp`, `docs/scheduler.md`, `docs/concurrency.md`, `docs/schema-v1.md`, `docs/semantic-contract.md`, `docs/trace-events.md`, release docs, goal ledgers, and `CHANGELOG.md`. | `thread_pool` now uses run-scoped persistent worker pools with bounded FIFO admission, stop/drain behavior, worker-id trace attributes, updated lane capability summaries, and focused runtime/graph coverage. |
| G31 | complete | `include/topoexec/runtime/graph.hpp`, `include/topoexec/runtime/scheduler.hpp`, `schema/topoexec.schema.v1.json`, `src/event_runtime.cpp`, `src/graph.cpp`, `src/graph_io.cpp`, `src/runtime_runner.cpp`, `tests/test_runtime.cpp`, `tests/test_graph.cpp`, updated goldens, scheduler/concurrency/schema/trace docs, release docs, and `CHANGELOG.md`. | `fixed_rate` now keeps deterministic stepping by default and supports opt-in cooperative wall-clock cadence v1 with `overrun_policy`, tick/skipped/max-lateness/blocked metrics, and fixed-rate trace events without hard real-time claims. |
| G32 | complete | `include/topoexec/runtime/scheduler.hpp`, `schema/topoexec.schema.v1.json`, `src/event_runtime.cpp`, `src/graph.cpp`, `src/graph_io.cpp`, `src/runtime_runner.cpp`, `src/diagnostics.cpp`, `tests/test_runtime.cpp`, `tests/test_graph.cpp`, updated goldens, scheduler/concurrency/schema/metrics/diagnostics docs, release docs, goal ledgers, and `CHANGELOG.md`. | `execution.priority` now accepts `background`/`low`/`normal`/`high`, orders independent ready regions and worker-queue items deterministically, reports priority/rejection/starvation metrics, rejects unknown runtime priority classes, and keeps lane/OS priority fields advisory. |
| G33 | complete | `include/topoexec/runtime/cancellation.hpp`, `include/topoexec/runtime/component.hpp`, `include/topoexec/runtime/task_executor.hpp`, `src/event_runtime.cpp`, `src/runtime_runner.cpp`, `src/task_executor.cpp`, `tests/test_runtime.cpp`, updated goldens, scheduler/concurrency/runtime-semantics/metrics/trace/API docs, release docs, goal ledgers, and `CHANGELOG.md`. | Cooperative cancellation is now exposed through `CancellationToken`, `Invocation::cancel_requested()`, and `GraphContext::cancel_requested()`; component/task/CompositeLoop timeout and cancellation evidence is reported without hard preemption or forced thread termination. |
| G34 | complete | `include/topoexec/runtime/task_executor.hpp`, `include/topoexec/runtime/component.hpp`, `src/task_executor.cpp`, `src/component.cpp`, `tests/test_runtime.cpp`, `docs/async-tasks.md`, public API/semantic/concurrency/baseline docs, goal ledgers, and `CHANGELOG.md`. | `TaskExecutor` remains the deterministic compatibility helper, `ITaskExecutor` is the attachable interface, and `ThreadedTaskExecutor` is an opt-in bounded worker preview with smoke, cancel-pending, shutdown-drain, failure, and exactly-once publication coverage. |
| G35 | complete | `TriggerPolicySpec`, trigger schema v1, `TriggerPolicyEngine`, trigger metrics, runtime/graph tests, goldens, trigger/schema/runtime/metrics/API/versioning docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Trigger Engine v2 preview adds `watermark`, `condition`, `debounce`, and `rate_limit` without breaking existing trigger types; condition predicates are enum-only, watermark drops late timestamped samples, debounce/rate-limit reuse deterministic readiness, and metrics explain late drops or suppressions. |
| G36 | complete | `include/topoexec/runtime/component.hpp`, `include/topoexec/runtime/channel.hpp`, `src/component.cpp`, `src/channel.cpp`, `src/trigger_policy.cpp`, `src/event_runtime.cpp`, `tests/test_runtime.cpp`, updated trace/metrics goldens, runtime/trace/channel/trigger/API docs, goal ledgers, and `CHANGELOG.md`. | Correlation/causation metadata now flows through publication, channel messages, trigger-created invocations, task completions, and CompositeLoop external commits; trace events include metadata attributes while metrics avoid default high-cardinality labels. |
| G37 | complete | `include/topoexec/runtime/health.hpp`, channel/event-runtime/runner/task-executor headers, `src/channel.cpp`, `src/event_runtime.cpp`, `src/runtime_runner.cpp`, `src/task_executor.cpp`, `tools/topoexec/main.cpp`, `tests/test_channel.cpp`, `tests/test_runtime.cpp`, updated metrics/doctor goldens, runtime/channel/metrics/trace/API docs, goal ledgers, and `CHANGELOG.md`. | Health is now observable through bounded observer-only events without recursive control flow; `emit_health_events` can disable event capture, `health_event_capacity` bounds retained records, high-watermark/overflow events are coalesced, and overflow events include edge id and policy. |
| G38 | complete | `tests/test_channel.cpp`, `tests/test_graph.cpp`, `src/graph.cpp`, `src/graph_io.cpp`, `tools/topoexec/main.cpp`, `CMakeLists.txt`, `examples/invalid_move_only_multireader.yaml`, updated plan golden, channel/payload/CLI/diagnostics docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Multi-reader queues now have focused slow-reader/drop and overflow-cursor coverage; `move_only` multi-reader misuse is visible through validation diagnostics and lint; plan/explain output exposes `readers`, `copy_policy`, and `slow_reader_drop_risk`; shared/loaned/move paths have no-copy/lifetime evidence while deeper loan-return callbacks and zero-copy pools remain G39 scope. |
| G39 | complete | `include/topoexec/runtime/buffer.hpp`, `src/buffer.cpp`, `include/topoexec/runtime/payload.hpp`, `src/payload.cpp`, `tools/topoexec/main.cpp`, `tests/test_channel.cpp`, `tests/test_runtime.cpp`, `CMakeLists.txt`, `examples/loaned_view_without_pool_owner.yaml`, memory/payload/API/CLI/example docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | `BufferPoolConfig` now supports fixed block sizing, bucket sizing, allocation-size alignment, max-byte exhaustion, active/owned/detached/exhausted/high-watermark stats, and outstanding-loan detection; payload schema summaries expose type/schema/summary/size; lint flags loaned views without producer ownership; no external SHM zero-copy is claimed. |
| G40 | complete | `include/topoexec/runtime/component.hpp`, `src/graph.cpp`, `src/diagnostics.cpp`, `tests/test_graph.cpp`, component/schema/diagnostics/public API docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Descriptor-backed typed port validation now checks endpoint existence, schema/payload-type compatibility, required vs optional input wiring, single-input fan-in, state-edge target type compatibility through the same edge contract, and boundary role compatibility while keeping schema v1 YAML unchanged. |
| G41 | complete | `include/topoexec/runtime/graph.hpp`, `src/graph_io.cpp`, `src/graph.cpp`, `src/channel.cpp`, `tools/topoexec/main.cpp`, schema v1, graph/runtime tests, hierarchy/schema/runtime/API/versioning/baseline/release docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Hierarchical `subgraphs[]` now compile-time expand into namespaced flat components, edges, `depends_on`, and CompositeLoops; validation after expansion prevents hidden cycles, plan JSON/Mermaid expose hierarchy, and runtime metrics/trace use expanded component paths without runtime nesting or `graph_ref`. |
| G42 | complete | `src/graph_io.cpp`, `schema/topoexec.schema.v1.json`, `examples/template_source_transform_sink.yaml`, `CMakeLists.txt`, graph/schema/docs tests, graph-template/schema/runtime/baseline/release docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Graph templates now use schema-v1 `templates[]` plus `template_instances[]` for strict scalar `{{parameter}}` substitution, deterministic namespace expansion before validation/runtime, invalid-parameter rejection, and a runnable template example without a separate template CLI or runtime interpreter. |
| G43 | complete | `include/topoexec/runtime/component.hpp`, `include/topoexec/runtime/runtime_runner.hpp`, `src/component.cpp`, `src/runtime_runner.cpp`, `tests/test_runtime.cpp`, lifecycle/API/metrics/semantic docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Components now have experimental reset/pause/resume/snapshot/restore hooks; `RuntimeRunnerOptions` can restore snapshots and reset selected components before scheduler execution and capture snapshots after execution; lifecycle metrics/trace and cleanup-on-reset/restore failure are covered while live pause/resume policy stays deferred. |
| G44 | complete | `include/topoexec/runtime/component.hpp`, `include/topoexec/runtime/state.hpp`, `src/component.cpp`, `src/event_runtime.cpp`, `src/runtime_runner.cpp`, `src/state.cpp`, `tests/test_state.cpp`, `tests/test_runtime.cpp`, state/lifecycle/runtime/metrics/trace/API/semantic docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Component config hot reload now stages versioned transactions, validates all pending configs, applies at epoch boundaries before execution, commits only after all apply hooks succeed, and rolls back/fail-fast with the old committed config active on invalid config or apply failure. |
| G45 | complete | `include/topoexec/runtime/component.hpp`, `include/topoexec/runtime/graph.hpp`, `include/topoexec/runtime/scheduler.hpp`, `include/topoexec/runtime/runtime_runner.hpp`, `include/topoexec/runtime/channel.hpp`, `src/component.cpp`, `src/graph.cpp`, `src/graph_io.cpp`, `src/event_runtime.cpp`, `src/runtime_runner.cpp`, `src/channel.cpp`, schema/goldens, runtime/graph tests, composite-solver app/docs, metrics/trace/schema/API/runtime docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | `solver_iteration` now adds typed in-process convergence/residual reports, optional residual-threshold convergence, loop-local iteration context, stop-reason/residual metrics and trace, partial-success output discard/fail/commit policy, graph/schema validation, and composite-solver example coverage without external solver plugins or unsafe cycle bypasses. |
| G46 | complete | `include/topoexec/runtime/runtime_runner.hpp`, `src/runtime_runner.cpp`, `tests/test_runtime.cpp`, observer/API/runtime/metrics/adapter/baseline docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | `RuntimeRunnerOptions::observers` now delivers best-effort result/metric/trace/health/error records through stable-v0.2 observer/sink callbacks; no-op and bounded in-memory observers are available; observer callback failures and bounded drops are non-fatal and observable. |
| G47 | complete | `include/topoexec/runtime/metric_schema.hpp`, `src/metric_schema.cpp`, `tools/topoexec/main.cpp`, `tests/test_runtime.cpp`, `tests/golden/metrics_minimal.json`, metrics/API/CLI/versioning/semantic docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Runtime metric schema version 1 now records descriptor name/kind/unit/labels/cardinality/stability, validates exported runtime samples, forbids high-cardinality default tags like correlation ids, and exposes the schema version in metrics JSON. |
| G48 | complete | `include/topoexec/runtime/runtime_runner.hpp`, `src/runtime_runner.cpp`, `tools/topoexec/main.cpp`, `tests/test_runtime.cpp`, updated trace/metrics goldens, trace/API/CLI/runtime/semantic docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Trace schema version 1 now records ordered timeline events with explicit phase/component/channel/lane/worker/epoch/transaction/correlation/causation fields and Chrome trace phase tracks, while legacy `trace_events` remains a compatibility name list. |
| G49 | complete | `include/topoexec/runtime/diagnostics.hpp`, `include/topoexec/runtime/graph.hpp`, `src/diagnostics.cpp`, `src/graph.cpp`, `tools/topoexec/main.cpp`, `tests/test_graph.cpp`, `examples/diagnostic_warnings.yaml`, diagnostics/API/CLI/semantic docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Diagnostic schema version 1 now exposes stable severity/category/suggested-fix descriptors, warning diagnostics for backpressure/deep queues/large copies/never-ready triggers, grouped explain JSON/text output, and `--strict-diagnostics` warning failure mode. |
| G50 | complete | `include/topoexec/runtime/graph.hpp`, `src/graph_io.cpp`, `tools/topoexec/main.cpp`, `schema/topoexec.schema.v1.json`, `tests/test_graph.cpp`, `tests/cli/check_parser_limits.py`, `tests/fuzz/fuzz_graph_inputs.py`, updated schema/doctor goldens, defensive-input/API/CLI/schema/testing/semantic docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Graph input loading now exposes `GraphInputLimits`, reads files incrementally under byte caps, rejects invalid UTF-8 and overlong strings/counts/configs before runtime execution, reports parser-limit CLI failures as validation JSON, and expands deterministic fuzz smoke coverage while keeping coverage-guided fuzzing in G51. |
| G51 | complete | `CMakeLists.txt`, `tests/fuzz/fuzz_graph_inputs.cpp`, `tests/fuzz/corpus/graph_inputs/*`, `scripts/fuzz_smoke.sh`, `scripts/goal_check.sh`, `.github/workflows/ci.yml`, fuzzing/build/testing/defensive-input docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Coverage-guided graph input fuzzing is now optional through `TOPOEXEC_BUILD_FUZZERS` with libFuzzer on Clang and standalone corpus replay elsewhere; minimized crash regressions can be committed as corpus seeds without changing the default agent gate. |
| G52 | complete | `tests/test_stress.cpp`, `tests/stress/check_stress_workloads.py`, `scripts/stress_smoke.sh`, `CMakeLists.txt`, `scripts/goal_check.sh`, `docs/stress-testing.md`, testing/build/release/baseline docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Bounded stress smoke now covers generated scheduler/channel workloads, `thread_pool` overload, and `ThreadedTaskExecutor` overload with queue-depth/drop/reject assertions; opt-in soak mode is bounded by steps/duration/iterations and remains confidence evidence, not a performance claim. |
| G53 | complete | `benchmarks/*.yaml`, `benchmarks/task_executor.cpp`, `tests/bench/check_bench_contract.py`, `scripts/bench_baseline.*`, `CMakeLists.txt`, `scripts/goal_check.sh`, benchmark/testing/build/release docs, updated doctor golden, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Benchmark schema v2 now includes graph hashes plus compiler/build/CPU/commit metadata; expanded graph cases and a non-installed task-executor benchmark are output-contract checked, while local baseline comparison remains opt-in and per-machine. |
| G54 | complete | `cmake/topoexecConfig.cmake.in`, `CMakeLists.txt`, `tests/cmake/*_smoke`, `tests/package/check_package_drafts.py`, `packaging/vcpkg/*`, `packaging/conan/*`, `tools/topoexec/main.cpp`, packaging/build/release docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Installed CMake packages now expose version/schema/semantic metadata, discover YAML dependencies when the YAML component is requested, support runtime-only/YAML/imported-CLI downstream consumption, find installed schema from the CLI path, generate CPack TGZ archives, and keep package-manager recipes as reviewable drafts. |
| G55 | complete | `docs/README.md`, `docs/cookbook.md`, `docs/architecture-diagrams.md`, `docs/why-topoexec.md`, `docs/design-principles.md`, `tests/docs/check_docs.py`, testing docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Documentation now has a learning/reference/release map, executable cookbook recipes, architecture diagrams, why-not comparisons, design principles, and a recursive docs smoke that checks required G55 pages and sections. |
| G56 | complete | `examples/apps/low_latency_sensor_pipeline`, `examples/apps/control_loop_with_state`, `examples/apps/async_request_response`, `examples/apps/composite_solver`, `examples/apps/payload_pool_pipeline`, `CMakeLists.txt`, examples/testing/baseline docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Reference apps now exercise latest/drop semantics, fixed-rate state plus delay boundaries, deterministic task-completion response flow, CompositeLoop convergence and budget-overrun metrics, and BufferPool copy/shared/loaned metrics without adding external adapters; G41 covers hierarchy through parser/runtime tests rather than a separate runtime-nesting app. |
| G57 | complete | `include/topoexec/adapters/sdk.hpp`, `CMakeLists.txt`, `cmake/topoexecConfig.cmake.in`, `tests/test_adapter_sdk.cpp`, `tests/cmake/adapter_sdk_smoke`, package/runtime-only smokes, architecture policy, adapter/API/guardrail docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Adapter SDK v0 now exports a dependency-free `topoexec::adapter_sdk` interface target over public runtime types, observer/result-sink aliases, bounded `BoundaryBridge` contracts, and explicit `ComponentFactoryProvider`; runtime does not link/include the SDK and no concrete adapter is implemented. |
| G58 | complete | `include/topoexec/adapters/otel.hpp`, `CMakeLists.txt`, `cmake/topoexecConfig.cmake.in`, `tests/test_otel_adapter.cpp`, `tests/cmake/otel_adapter_smoke`, `tests/cmake/otel_adapter_options_smoke.cmake`, adapter policy checks, adapter/API/metrics/trace/package docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Default-off `topoexec_adapters::otel` maps runtime metric descriptors, trace events, health events, runtime errors, and result summaries into dependency-free in-memory OTel-shaped records; package and policy smokes prove `topoexec::runtime` stays free of adapter or telemetry SDK dependencies. |
| G59 | complete | `include/topoexec/adapters/prometheus.hpp`, `CMakeLists.txt`, `cmake/topoexecConfig.cmake.in`, `tests/test_prometheus_adapter.cpp`, `tests/cmake/prometheus_adapter_smoke`, `tests/cmake/prometheus_adapter_options_smoke.cmake`, adapter policy checks, adapter/API/metrics/package docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Default-off `topoexec_adapters::prometheus` renders descriptor-backed counters/gauges and custom histogram summaries as dependency-free text exposition with bounded labels only; no HTTP server, Prometheus library, or runtime exporter dependency is added. |
| G67 | complete | `scripts/release_prepare.sh`, `.github/workflows/release-dry-run.yml`, `tests/release/check_release_prepare.py`, `docs/release-runbook.md`, release/progression/versioning docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Release preparation is now reproducible from a clean candidate commit: the script checks tag/changelog/doc policy, runs gates unless skipped, drafts notes, generates source/CPack/schema artifacts plus checksums, writes a human-only annotated tag command, and never tags or publishes automatically. |
| G69 | complete | `examples/apps/robot_cell_pilot`, `docs/case-study-robot-cell.md`, `docs/examples.md`, `examples/README.md`, README/docs index updates, `CMakeLists.txt`, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | The robot-cell pilot composes event-loop/thread-pool/fixed-rate lanes, async frame overload/drop, state and delay feedback, BufferPool `FrameView` payloads, config transaction/snapshot evidence, runtime metrics/trace/observer evidence, and invalid-config rejection while linking only `topoexec_runtime` and adding no adapter dependency. |
| G70 | complete | `docs/beta-readiness-review.md`, `docs/public-api.md`, `docs/versioning.md`, `docs/runtime-invariants.md`, release docs, README/docs index updates, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | TopoExec can honestly enter a human-approved core-runtime beta candidate review after the required gates pass, but must not claim adapter/ecosystem beta readiness, signed/published package artifacts, hard real-time scheduling, or deferred G60-G65/G68 scope. |

## Validation Evidence

Fresh checks in this working tree:

```bash
./scripts/goal_check.sh adapters
# G59 passed: test_adapter_sdk, test_otel_adapter, test_prometheus_adapter, both adapter option smokes, and policy smokes, 7/7.

./scripts/goal_check.sh quick
# G59 passed: cli_golden_outputs and schema_v1_contract_smoke.

./scripts/goal_check.sh docs
# G59 passed: recursive docs command smoke after adapter docs updates.

cmake --build build --target topoexec_format_check
# G59 passed.

./scripts/goal_check.sh policy
# G59 passed: policy_no_core_adapter_deps and policy_architecture_self_test.

./scripts/agent_check.sh
# G59 passed: 74/74 CTest tests in the default RelWithDebInfo GCC build.

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G59 passed: 74/74 CTest tests in the ASAN+UBSAN Debug build.

git diff --check
# G59 passed.

ctest --test-dir build --output-on-failure -R 'test_runtime|test_graph|app_composite_solver_runs|schema_v1_contract_smoke|cli_golden_outputs|docs_command_smoke'
# G45 focused pass: solver_iteration convergence/residual/partial-output behavior, graph validation, composite-solver app, schema/golden, and docs smoke passed.

./scripts/goal_check.sh quick
# G45 passed: cli_golden_outputs and schema_v1_contract_smoke after loop-policy schema/golden updates.

./scripts/goal_check.sh docs
# G45 passed: recursive docs command smoke after CompositeLoop solver docs updates.

cmake --build build --target topoexec_format_check
# G45 passed.

./scripts/agent_check.sh
# G45 passed: 72/72 CTest tests in the default RelWithDebInfo GCC build.

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G45 passed: 72/72 CTest tests in the ASAN+UBSAN Debug build.

git diff --check
# G45 passed.

git diff --cached --check
# G45 passed with an empty index before staging.

ctest --test-dir build --output-on-failure -R 'test_graph|cli_validate_template_source_transform_sink|cli_run_template_source_transform_sink|schema_v1_contract_smoke|cli_golden_outputs|docs_command_smoke'
# G42 focused pass: deterministic template expansion, invalid parameter rejection, expanded graph validation, template CLI example, schema/golden, and docs smoke passed.

./scripts/goal_check.sh quick
# G42 passed: cli_golden_outputs and schema_v1_contract_smoke after template schema golden update.

./scripts/goal_check.sh docs
# G42 passed: recursive docs command smoke includes graph-templates.md and template example validation.

cmake --build build --target topoexec_format_check
# G42 passed.

./scripts/agent_check.sh
# G42 passed: 72/72 CTest tests in the default RelWithDebInfo GCC build.

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G42 passed: 72/72 CTest tests in the ASAN+UBSAN Debug build.

git diff --check
# G42 passed.

git diff --cached --check
# G42 passed with an empty index before staging.

ctest --test-dir build --output-on-failure -R 'test_graph|test_runtime|schema_v1_contract_smoke|cli_golden_outputs|docs_command_smoke'
# G41 focused pass: subgraph expansion, hidden-cycle rejection, CompositeLoop ownership, runtime path metrics, schema, golden, and docs smoke passed.

./scripts/goal_check.sh quick
# G41 passed: cli_golden_outputs and schema_v1_contract_smoke after hierarchy schema/plan JSON golden updates.

./scripts/goal_check.sh docs
# G41 passed: recursive docs command smoke includes hierarchical-graphs.md in the docs map.

cmake --build build --target topoexec_format_check
# G41 passed.

./scripts/agent_check.sh
# G41 passed: 70/70 CTest tests in the default RelWithDebInfo GCC build.

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G41 passed: 70/70 CTest tests in the ASAN+UBSAN Debug build.

git diff --check
# G41 passed.

git diff --cached --check
# G41 passed with an empty index before staging.

ctest --test-dir build --output-on-failure -R 'test_runtime|test_graph|schema_v1_contract_smoke'
# G35 focused pass: runtime trigger-v2 tests, graph validation, and schema contract passed.

python3 tests/golden/check_cli_golden.py --topoexec build/topoexec --source-dir . --golden-dir tests/golden --update
./scripts/goal_check.sh quick
# G35 passed: updated schema/metrics goldens then cli_golden_outputs and schema_v1_contract_smoke.

./scripts/goal_check.sh docs
# G35 passed: recursive docs command smoke after trigger-v2 docs updates.

cmake --build build --target topoexec_format_check
# G35 passed.

./scripts/agent_check.sh
# G35 passed: 70/70 CTest tests in the default RelWithDebInfo GCC build.

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G35 passed: 70/70 CTest tests in the ASAN+UBSAN Debug build.

git diff --check
# G35 passed.

./scripts/goal_check.sh docs
# G70 passed: recursive docs command smoke includes the beta-readiness review.

./scripts/goal_check.sh policy
# G70 passed: architecture policy checks (2/2).

./scripts/goal_check.sh quick
# G70 passed: cli_golden_outputs and schema_v1_contract_smoke.

./scripts/goal_check.sh release
# G70 passed: release_prepare_smoke.

./scripts/goal_check.sh fuzz
# G70 passed: deterministic fuzz smoke plus optional fuzzer-target corpus smoke.

./scripts/goal_check.sh stress
# G70 passed: test_stress plus generated stress graph smoke.

./scripts/goal_check.sh bench
# G70 passed: benchmark CTest smokes plus /tmp local baseline generation without thresholds.

./scripts/goal_check.sh package
# G70 passed: installed package, runtime-only option, CPack, and package-draft smokes.

cmake --build build --target topoexec_format_check
# G70 passed after robot-cell pilot formatting refresh.

./scripts/agent_check.sh
# G70 passed: 70/70 CTest tests in the default RelWithDebInfo GCC build.

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G70 passed: 70/70 CTest tests in the ASAN+UBSAN Debug build.

git diff --check
# G70 passed.

ctest --test-dir build --output-on-failure -R 'app_robot_cell_pilot_runs|docs_command_smoke'
# G69 focused pass: robot-cell pilot smoke and docs command marker both passed.

./scripts/goal_check.sh docs
# G69 passed: recursive docs command smoke including the robot-cell case-study marker.

./scripts/goal_check.sh quick
# G69 passed: cli_golden_outputs and schema_v1_contract_smoke.

cmake --build build --target topoexec_format_check
# G69 passed.

./scripts/agent_check.sh
# G69 passed: 70/70 CTest tests in the default RelWithDebInfo GCC build.

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G69 passed: 70/70 CTest tests in the ASAN+UBSAN Debug build.

git diff --check
# G69 passed.

bash -n scripts/release_prepare.sh
python3 -m py_compile tests/release/check_release_prepare.py tests/docs/check_docs.py
# G67 passed: release script and Python smoke helpers have valid syntax.

./scripts/goal_check.sh release
# G67 passed: release_prepare_smoke dry-runs the release prep script without tagging or creating artifacts.

./scripts/release_prepare.sh --version v0.2.0-alpha.0 --skip-gates --allow-dirty --artifacts-dir build/release-prepare-artifact-smoke --build-dir build-release-candidate-smoke --notes-out build/release-prepare-artifact-smoke/release-notes-v0.2.0-alpha.0.md
# G67 artifact rehearsal passed: release notes, source archive, CPack binary/source TGZ, schema artifact, SHA256SUMS, and human-only tag-command draft were generated without publishing.

./scripts/goal_check.sh docs
# G67 passed: recursive docs command smoke including the release runbook marker.

./scripts/goal_check.sh package
# G67 passed: installed package, runtime-only option, CPack, and package-draft smokes.

./scripts/goal_check.sh quick
# G67 passed: cli_golden_outputs and schema_v1_contract_smoke.

cmake --build build --target topoexec_format_check
# G67 passed.

./scripts/agent_check.sh
# G67 passed: 69/69 CTest tests in the default RelWithDebInfo GCC build.

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G67 passed: 69/69 CTest tests in the ASAN+UBSAN Debug build.

git diff --check
# G67 passed.

ctest --test-dir build --output-on-failure -R 'test_adapter_sdk|policy_no_core_adapter_deps|cmake_package_runtime_smoke|cmake_runtime_only_options_smoke'
# G57 focused pass: adapter SDK unit tests, adapter policy, installed package smoke, and runtime-only adapter SDK smoke (4/4).

./scripts/goal_check.sh quick
# G57 passed: cli_golden_outputs and schema_v1_contract_smoke.

cmake --build build --target topoexec_format_check
# G57 passed.

./scripts/agent_check.sh
# G57 passed: 68/68 CTest tests in the default RelWithDebInfo GCC build.

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G57 passed: 68/68 CTest tests in the ASAN+UBSAN Debug build.

./scripts/goal_check.sh docs
# G57 passed: recursive docs command smoke after adapter docs updates.

git diff --check
# G57 passed.

./scripts/goal_check.sh package
# G57 passed: package smoke now includes adapter SDK downstream consumption.

./scripts/goal_check.sh policy
# G57 passed: adapter SDK target boundary and no-core-adapter-deps checks.

ctest --test-dir build --output-on-failure -R 'app_|docs_command_smoke'
# G56 passed: docs command smoke plus all 11 example-app smokes, including five new reference apps (12/12).

./scripts/goal_check.sh docs
# G56 passed: recursive docs command smoke, including reference-app doc markers.

./scripts/goal_check.sh quick
# G56 passed: cli_golden_outputs and schema_v1_contract_smoke.

cmake --build build --target topoexec_format_check
# G56 passed.

./scripts/agent_check.sh
# G56 passed: 67/67 CTest tests in the default RelWithDebInfo GCC build.

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G56 passed: 67/67 CTest tests in the ASAN+UBSAN Debug build.

git diff --check
# G56 passed.

./scripts/goal_check.sh docs
# G55 passed: recursive docs command smoke plus required docs map/section contract

./scripts/goal_check.sh quick
# G55 passed: cli_golden_outputs and schema_v1_contract_smoke

cmake --build build --target topoexec_format_check
# G55 passed

./scripts/agent_check.sh
# G55 passed: 62/62 CTest tests with recursive docs smoke included

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G55 passed: 62/62 CTest tests in the ASAN+UBSAN Debug build

git diff --check
python3 -m py_compile tests/docs/check_docs.py
# G55 passed

cmake --build build -j
# G54 passed: package metadata, downstream smoke, and installed-schema CLI changes rebuilt successfully

cmake --build build --target topoexec_format_check
# G54 passed

ctest --test-dir build --output-on-failure -R 'cmake_package_runtime_smoke|cmake_runtime_only_options_smoke|cmake_cpack_smoke|package_draft_smoke|cli_golden_outputs|docs_command_smoke'
# G54 passed: package downstream smokes, CPack, package drafts, golden, and docs smoke

./scripts/goal_check.sh package
# G54 passed: runtime/YAML/CLI installed package smokes, runtime-only options, CPack TGZ, and package draft checks

./scripts/goal_check.sh quick
# G54 passed: cli_golden_outputs and schema_v1_contract_smoke

./scripts/agent_check.sh
# G54 passed: 62/62 CTest tests with package and CPack smokes included

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G54 passed: 62/62 CTest tests in the ASAN+UBSAN Debug build

git diff --check
python3 -m py_compile tests/package/check_package_drafts.py
# G54 passed

cmake --build build -j
# G53 passed: benchmark schema v2 CLI and task-executor targets rebuilt successfully

cmake --build build --target topoexec_format_check
# G53 passed

ctest --test-dir build --output-on-failure -R 'bench|cli_bench|cli_golden_outputs|schema_v1_contract_smoke|docs_command_smoke'
# G53 passed: benchmark output-contract smokes, golden/schema drift checks, and docs smoke

./scripts/goal_check.sh quick
# G53 passed: cli_golden_outputs and schema_v1_contract_smoke

./scripts/goal_check.sh bench
# G53 passed: CLI/task-executor benchmark smokes plus local /tmp baseline generation without thresholds

./scripts/agent_check.sh
# G53 passed: 60/60 CTest tests with benchmark contract smoke included

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G53 passed: 60/60 CTest tests in the ASAN+UBSAN Debug build

git diff --check
python3 -m py_compile scripts/bench_baseline.py tests/bench/check_bench_contract.py
# G53 passed

cmake --build build -j
# G52 passed: stress target and generated graph smoke wiring rebuilt successfully

cmake --build build --target topoexec_format_check
# G52 passed

ctest --test-dir build --output-on-failure -R 'test_stress|stress_graph_smoke|docs_command_smoke|cli_golden_outputs|schema_v1_contract_smoke'
# G52 passed: C++ overload stress, generated stress graph smoke, docs, golden, and schema drift checks

./scripts/goal_check.sh quick
# G52 passed: cli_golden_outputs and schema_v1_contract_smoke

./scripts/goal_check.sh stress
# G52 passed: test_stress plus stress_smoke high fan-out, high fan-in, long chain, mixed edge-kind, and bounded thread-pool workloads

TOPOEXEC_STRESS_PROFILE=soak TOPOEXEC_STRESS_SCALE=16 TOPOEXEC_STRESS_STEPS=8 TOPOEXEC_STRESS_DURATION_SECONDS=1 TOPOEXEC_STRESS_MAX_ITERATIONS=2 ./scripts/stress_smoke.sh
# G52 passed: opt-in soak wrapper completed two bounded iterations

./scripts/agent_check.sh
# G52 passed: 58/58 CTest tests with default stress smoke included

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G52 passed: 58/58 CTest tests in the ASAN+UBSAN Debug build

git diff --check
python3 -m py_compile tests/stress/check_stress_workloads.py
# G52 passed

cmake --build build -j
# G51 passed: optional fuzzer build wiring left default build unaffected

TOPOEXEC_FUZZER_ENGINE=STANDALONE ./scripts/fuzz_smoke.sh
# G51 passed: fuzz_graph_inputs standalone target built and replayed checked-in corpus through CTest

./scripts/goal_check.sh fuzz
# G51 passed: deterministic fuzz smoke plus standalone fuzzer corpus replay

cmake --build build --target topoexec_format_check
# G51 passed

cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure -R 'fuzz_graph_input_smoke|schema_v1_contract_smoke|docs_command_smoke'
# G51 passed: default build/schema/docs/fuzz smokes remained green

./scripts/agent_check.sh
# G51 passed: 56/56 CTest tests with fuzzers off by default

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G51 passed: 56/56 CTest tests in the ASAN+UBSAN Debug build

git diff --check
# G51 passed

cmake --build build -j
# G50 passed: defensive parser-limit code/tests rebuilt successfully

cmake --build build --target topoexec_format_check
# G50 passed

ctest --test-dir build --output-on-failure -R 'test_graph|cli_validate_input_limit_override_fails_safely|schema_v1_contract_smoke|fuzz_graph_input_smoke|cli_golden_outputs|cli_doctor_json|cli_schema_dump_json'
# G50 passed: parser limit API/tests, CLI JSON limit failure, schema/golden drift, doctor/schema JSON, and deterministic malformed-input fuzz smoke

./scripts/goal_check.sh quick
# G50 passed: cli_golden_outputs and schema_v1_contract_smoke

./scripts/goal_check.sh fuzz
# G50 passed: deterministic malformed/invalid-UTF-8/oversized parser fuzz smoke

./scripts/agent_check.sh
# G50 passed: 56/56 CTest tests after defensive input updates

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G50 passed: 56/56 CTest tests in the ASAN+UBSAN Debug build

git diff --check
# G50 passed

cmake --build build -j
# G49 passed: diagnostic schema/category/warning code rebuilt successfully

cmake --build build --target topoexec_format_check
# G49 passed

ctest --test-dir build --output-on-failure -R 'test_graph|cli_validate_warning_diagnostics_do_not_fail|cli_validate_strict_diagnostics_fail_warnings|cli_golden_outputs|schema_v1_contract_smoke'
# G49 passed: warning diagnostics remain non-failing by default, strict diagnostics fail warnings, grouped diagnostic fields/goldens/schema smoke

./scripts/goal_check.sh quick
# G49 passed: cli_golden_outputs and schema_v1_contract_smoke

./scripts/agent_check.sh
# G49 passed: 55/55 CTest tests after diagnostics v2 updates

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G49 passed: 55/55 CTest tests in the ASAN+UBSAN Debug build

git diff --check
# G49 passed

cmake --build build -j
# G48 passed: trace schema code/tests rebuilt successfully

cmake --build build --target topoexec_format_check
# G48 passed

ctest --test-dir build --output-on-failure -R 'test_runtime|cli_golden_outputs|schema_v1_contract_smoke'
# G48 passed: trace timeline ordering, legal durations, causality fields, updated trace/metrics goldens, schema smoke

./scripts/goal_check.sh quick
# G48 passed: cli_golden_outputs and schema_v1_contract_smoke

./scripts/agent_check.sh
# G48 passed: 53/53 CTest tests after trace schema updates

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G48 passed: 53/53 CTest tests in the ASAN+UBSAN Debug build

git diff --check
# G48 passed

cmake --build build -j
# G47 passed: metric schema descriptor code/tests rebuilt successfully

cmake --build build --target topoexec_format_check
# G47 passed

ctest --test-dir build --output-on-failure -R 'test_runtime|cli_golden_outputs|schema_v1_contract_smoke'
# G47 passed: descriptor uniqueness, exported metric validation, high-cardinality label rejection, metrics JSON schema-version golden, schema smoke

./scripts/goal_check.sh quick
# G47 passed: cli_golden_outputs and schema_v1_contract_smoke

./scripts/agent_check.sh
# G47 passed: 53/53 CTest tests after metric schema/cardinality updates

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G47 passed: 53/53 CTest tests in the ASAN+UBSAN Debug build

git diff --check
# G47 passed

cmake --build build -j
# G46 passed: observer API code/tests rebuilt successfully

cmake --build build --target topoexec_format_check
# G46 passed

ctest --test-dir build --output-on-failure -R 'test_runtime|cli_golden_outputs|schema_v1_contract_smoke'
# G46 passed: in-memory observer delivery, health events, non-fatal observer failure, bounded drops, golden/schema drift checks

./scripts/goal_check.sh quick
# G46 passed: cli_golden_outputs and schema_v1_contract_smoke

./scripts/agent_check.sh
# G46 passed: 53/53 CTest tests after RuntimeObserver v1 updates

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G46 passed: 53/53 CTest tests in the ASAN+UBSAN Debug build

git diff --check
# G46 passed

cmake --build build -j
# G44 passed: config transaction code/tests rebuilt successfully

cmake --build build --target topoexec_format_check
# G44 passed

ctest --test-dir build --output-on-failure -R 'test_state|test_runtime|cli_golden_outputs|schema_v1_contract_smoke'
# G44 passed: config store transaction metadata/rollback, valid reload, invalid reject, apply rollback, golden/schema drift checks

./scripts/goal_check.sh quick
# G44 passed: cli_golden_outputs and schema_v1_contract_smoke

./scripts/agent_check.sh
# G44 passed: 53/53 CTest tests after config hot-reload transaction updates

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G44 passed: 53/53 CTest tests in the ASAN+UBSAN Debug build

git diff --check
# G44 passed

ctest --test-dir build --output-on-failure -R 'test_runtime|cli_golden_outputs|schema_v1_contract_smoke'
# G43 passed: reset before execution, snapshot/restore handoff, incompatible restore rejection, reset-failure cleanup, golden/schema drift checks

cmake --build build --target topoexec_format_check
# G43 passed

./scripts/goal_check.sh quick
# G43 passed: cli_golden_outputs and schema_v1_contract_smoke

./scripts/agent_check.sh
# G43 passed: 53/53 CTest tests after lifecycle reset/snapshot/restore updates

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G43 passed: 53/53 CTest tests in the ASAN+UBSAN Debug build

git diff --check
# G43 passed

ctest --test-dir build --output-on-failure -R 'test_graph|test_runtime|cli_golden_outputs|schema_v1_contract_smoke'
# G40 passed: typed port mismatch, required/optional input, multiplicity, boundary role, runtime compatibility, golden/schema drift checks

cmake --build build --target topoexec_format_check
# G40 passed

./scripts/goal_check.sh quick
# G40 passed: cli_golden_outputs and schema_v1_contract_smoke

./scripts/agent_check.sh
# G40 passed: 53/53 CTest tests after typed port validation updates

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G40 passed: 53/53 CTest tests in the ASAN+UBSAN Debug build

git diff --check
# G40 passed

ctest --test-dir build --output-on-failure -R 'test_channel|test_runtime|cli_lint_loaned_view_without_pool_owner|cli_golden_outputs|schema_v1_contract_smoke'
# G39 passed: BufferPool bounded/exhaustion/loan accounting, copy/no-copy payload evidence, loaned-view lint, golden/schema drift checks

cmake --build build --target topoexec_format_check
# G39 passed

./scripts/goal_check.sh quick
# G39 passed: cli_golden_outputs and schema_v1_contract_smoke

./scripts/agent_check.sh
# G39 passed: 53/53 CTest tests after payload/memory hardening

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G39 passed: 53/53 CTest tests in the ASAN+UBSAN Debug build

git diff --check
# G39 passed

ctest --test-dir build --output-on-failure -R 'test_channel|test_graph|cli_lint_reject_move_only_multireader|cli_golden_outputs|schema_v1_contract_smoke'
# G38 passed: multi-reader cursor/drop, move-only diagnostics, plan/golden/schema drift, and CLI lint coverage

cmake --build build --target topoexec_format_check
# G38 passed

./scripts/goal_check.sh quick
# G38 passed: cli_golden_outputs and schema_v1_contract_smoke

./scripts/agent_check.sh
# G38 passed: 52/52 CTest tests after multi-reader/move-only hardening

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G38 passed: 52/52 CTest tests in the ASAN+UBSAN Debug build

git diff --check
# G38 passed

ctest --test-dir build --output-on-failure -R 'test_channel|test_runtime|cli_golden_outputs|schema_v1_contract_smoke'
# G37 passed: channel high-watermark/overflow/stale/deadline health events, bounded sink, runner config/capacity exposure, task/scheduler reject events, golden/schema drift checks

./scripts/goal_check.sh quick
# G37 passed: cli_golden_outputs and schema_v1_contract_smoke

cmake --build build --target topoexec_format_check
# G37 passed

./scripts/agent_check.sh
# G37 passed: 51/51 CTest tests after bounded health-event updates

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G37 passed: 51/51 CTest tests in the ASAN+UBSAN Debug build

git diff --check
# G37 passed

ctest --test-dir build --output-on-failure -R 'test_runtime|test_graph|cli_golden_outputs|schema_v1_contract_smoke'
# G36 passed: immediate-chain correlation, delay causation, async completion correlation, CompositeLoop causation, graph/golden/schema drift checks

./scripts/goal_check.sh quick
# G36 passed: cli_golden_outputs and schema_v1_contract_smoke

cmake --build build --target topoexec_format_check
# G36 passed

./scripts/agent_check.sh
# G36 passed: 51/51 CTest tests after invocation metadata updates

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G36 passed: 51/51 CTest tests in the ASAN+UBSAN Debug build

ctest --test-dir build --output-on-failure -R 'test_runtime|test_graph|cli_golden_outputs|schema_v1_contract_smoke'
# G34 passed: deterministic compatibility, threaded executor smoke/failure/cancel/shutdown/publication coverage, graph/golden/schema drift checks

./scripts/goal_check.sh quick
# G34 passed: cli_golden_outputs and schema_v1_contract_smoke

cmake --build build --target topoexec_format_check
# G34 passed

./scripts/agent_check.sh
# G34 passed: 51/51 CTest tests after threaded executor preview updates

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G34 passed: 51/51 CTest tests in the ASAN+UBSAN Debug build

ctest --test-dir build --output-on-failure -R 'test_runtime|test_graph|cli_golden_outputs|schema_v1_contract_smoke'
# G33 passed: cooperative component cancellation, ignored-cancel timeout reporting, CompositeLoop cancellation, TaskExecutor cancellation/budget coverage, graph/golden/schema drift checks

./scripts/goal_check.sh quick
# G33 passed: cli_golden_outputs and schema_v1_contract_smoke

cmake --build build --target topoexec_format_check
# G33 passed

./scripts/agent_check.sh
# G33 passed: 51/51 CTest tests after cooperative cancellation/timeout updates

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G33 passed: 51/51 CTest tests in the ASAN+UBSAN Debug build

./scripts/goal_check.sh quick
# passed: cli_golden_outputs and schema_v1_contract_smoke

./scripts/agent_check.sh
# passed: 51/51 CTest tests in the default RelWithDebInfo GCC build

cmake --build build --target topoexec_format_check
# passed

./scripts/goal_check.sh package
# passed: cmake_package_runtime_smoke and cmake_runtime_only_options_smoke

./scripts/goal_check.sh golden
# passed: cli_golden_outputs

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# passed: 50/50 CTest tests in the ASAN+UBSAN Debug build

./scripts/goal_check.sh package
# G27 passed: runtime-only downstream smoke covers result metrics/trace consumption

cmake --build build --target topoexec_format_check && ./scripts/goal_check.sh policy
# G27 passed: format target and architecture/dependency policy smoke

./scripts/agent_check.sh
# G27 passed: 50/50 CTest tests after API marker and runtime smoke updates

./scripts/goal_check.sh schema
# G28 passed: semantic contract schema annotation is covered by schema_v1_contract_smoke

./scripts/goal_check.sh golden
# G28 passed: doctor/schema dump goldens include semantic_contract_version 0.2

./scripts/agent_check.sh
# G28 passed: 50/50 CTest tests after semantic contract version output updates

cmake --build build --target topoexec_format_check
# G28 passed

./scripts/goal_check.sh policy
# G66 passed: policy_no_core_adapter_deps and policy_architecture_self_test

./scripts/agent_check.sh
# G66 passed: 51/51 CTest tests after adding the architecture self-test

cmake --build build --target topoexec_format_check
# G66 passed

./scripts/goal_check.sh quick
# G29 passed: updated scheduler plan JSON golden and schema smoke

ctest --test-dir build --output-on-failure -R test_graph
# G29 passed: advisory scheduler diagnostics and lane capability summary unit tests

./scripts/agent_check.sh
# G29 passed: 51/51 CTest tests after scheduler v2 contract updates

cmake --build build --target topoexec_format_check
# G29 passed

ctest --test-dir build --output-on-failure -R 'test_runtime|test_graph|cli_golden_outputs'
# G30 passed: persistent worker-pool runtime coverage, capability summary unit coverage, and CLI golden drift check

./scripts/goal_check.sh quick
# G30 passed: cli_golden_outputs and schema_v1_contract_smoke

./scripts/agent_check.sh
# G30 passed: 51/51 CTest tests after persistent worker-pool updates

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G30 passed: 51/51 CTest tests in the ASAN+UBSAN Debug build

ctest --test-dir build --output-on-failure -R 'test_runtime|test_graph|cli_golden_outputs|schema_v1_contract_smoke'
# G31 passed: fixed-rate runtime coverage, overrun policy parsing, schema/golden drift checks

./scripts/goal_check.sh quick
# G31 passed: cli_golden_outputs and schema_v1_contract_smoke

cmake --build build --target topoexec_format_check
# G31 passed

./scripts/agent_check.sh
# G31 passed: 51/51 CTest tests after fixed-rate wall-clock updates

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G31 passed: 51/51 CTest tests in the ASAN+UBSAN Debug build

ctest --test-dir build --output-on-failure -R 'test_runtime|test_graph|cli_golden_outputs|schema_v1_contract_smoke'
# G32 passed: runtime priority ordering, low-priority rejection metrics, schema/golden drift checks

./scripts/goal_check.sh quick
# G32 passed: cli_golden_outputs and schema_v1_contract_smoke

cmake --build build --target topoexec_format_check
# G32 passed

./scripts/agent_check.sh
# G32 passed: 51/51 CTest tests after scheduler priority/admission updates

TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
# G32 passed: 51/51 CTest tests in the ASAN+UBSAN Debug build

grep -RInE '#include .*(yaml|rclcpp|opentelemetry|prometheus|Python|perfetto|tools/topoexec|src/)' include || true
# G27 passed: no YAML/CLI/adapter/private includes in installed headers
```

## Blockers

No active blockers.
