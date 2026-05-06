# Goal Status

Last updated: 2026-05-06

## Current Plan Source

- Completed architecture plan: `docs/plans/plan.md` (G0-G25 archived complete)
- Active architecture plan: `docs/plans/plan2.md` (G26+)
- Backlog: `docs/goals/backlog.md`
- Required repository gate: `scripts/agent_check.sh`

## Current Stage

Phase A is complete: G26 established the post-G25 release-candidate baseline, G27 completed the public API stability pass, G28 added the runtime semantic contract, and G66 enforced architecture boundaries. Phase B is complete through G34; Phase C has G36, G37, G38, G39, and G40 complete; backlog-order lifecycle/config/observer goals G43, G44, and G46 are also complete.
The next unfinished P1 goal in backlog order is G47 Metrics v2: Cardinality and Schema Contract. Lower-priority G35, G41, G42, and G45 remain pending P2/P3 work and are deferred by the active ordering rule unless the plan order is explicitly reopened.

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
| G36 | complete | `include/topoexec/runtime/component.hpp`, `include/topoexec/runtime/channel.hpp`, `src/component.cpp`, `src/channel.cpp`, `src/trigger_policy.cpp`, `src/event_runtime.cpp`, `tests/test_runtime.cpp`, updated trace/metrics goldens, runtime/trace/channel/trigger/API docs, goal ledgers, and `CHANGELOG.md`. | Correlation/causation metadata now flows through publication, channel messages, trigger-created invocations, task completions, and CompositeLoop external commits; trace events include metadata attributes while metrics avoid default high-cardinality labels. |
| G37 | complete | `include/topoexec/runtime/health.hpp`, channel/event-runtime/runner/task-executor headers, `src/channel.cpp`, `src/event_runtime.cpp`, `src/runtime_runner.cpp`, `src/task_executor.cpp`, `tools/topoexec/main.cpp`, `tests/test_channel.cpp`, `tests/test_runtime.cpp`, updated metrics/doctor goldens, runtime/channel/metrics/trace/API docs, goal ledgers, and `CHANGELOG.md`. | Health is now observable through bounded observer-only events without recursive control flow; `emit_health_events` can disable event capture, `health_event_capacity` bounds retained records, high-watermark/overflow events are coalesced, and overflow events include edge id and policy. |
| G38 | complete | `tests/test_channel.cpp`, `tests/test_graph.cpp`, `src/graph.cpp`, `src/graph_io.cpp`, `tools/topoexec/main.cpp`, `CMakeLists.txt`, `examples/invalid_move_only_multireader.yaml`, updated plan golden, channel/payload/CLI/diagnostics docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Multi-reader queues now have focused slow-reader/drop and overflow-cursor coverage; `move_only` multi-reader misuse is visible through validation diagnostics and lint; plan/explain output exposes `readers`, `copy_policy`, and `slow_reader_drop_risk`; shared/loaned/move paths have no-copy/lifetime evidence while deeper loan-return callbacks and zero-copy pools remain G39 scope. |
| G39 | complete | `include/topoexec/runtime/buffer.hpp`, `src/buffer.cpp`, `include/topoexec/runtime/payload.hpp`, `src/payload.cpp`, `tools/topoexec/main.cpp`, `tests/test_channel.cpp`, `tests/test_runtime.cpp`, `CMakeLists.txt`, `examples/loaned_view_without_pool_owner.yaml`, memory/payload/API/CLI/example docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | `BufferPoolConfig` now supports fixed block sizing, bucket sizing, allocation-size alignment, max-byte exhaustion, active/owned/detached/exhausted/high-watermark stats, and outstanding-loan detection; payload schema summaries expose type/schema/summary/size; lint flags loaned views without producer ownership; no external SHM zero-copy is claimed. |
| G40 | complete | `include/topoexec/runtime/component.hpp`, `src/graph.cpp`, `src/diagnostics.cpp`, `tests/test_graph.cpp`, component/schema/diagnostics/public API docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Descriptor-backed typed port validation now checks endpoint existence, schema/payload-type compatibility, required vs optional input wiring, single-input fan-in, state-edge target type compatibility through the same edge contract, and boundary role compatibility while keeping schema v1 YAML unchanged. |
| G43 | complete | `include/topoexec/runtime/component.hpp`, `include/topoexec/runtime/runtime_runner.hpp`, `src/component.cpp`, `src/runtime_runner.cpp`, `tests/test_runtime.cpp`, lifecycle/API/metrics/semantic docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Components now have experimental reset/pause/resume/snapshot/restore hooks; `RuntimeRunnerOptions` can restore snapshots and reset selected components before scheduler execution and capture snapshots after execution; lifecycle metrics/trace and cleanup-on-reset/restore failure are covered while live pause/resume policy stays deferred. |
| G44 | complete | `include/topoexec/runtime/component.hpp`, `include/topoexec/runtime/state.hpp`, `src/component.cpp`, `src/event_runtime.cpp`, `src/runtime_runner.cpp`, `src/state.cpp`, `tests/test_state.cpp`, `tests/test_runtime.cpp`, state/lifecycle/runtime/metrics/trace/API/semantic docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | Component config hot reload now stages versioned transactions, validates all pending configs, applies at epoch boundaries before execution, commits only after all apply hooks succeed, and rolls back/fail-fast with the old committed config active on invalid config or apply failure. |
| G46 | complete | `include/topoexec/runtime/runtime_runner.hpp`, `src/runtime_runner.cpp`, `tests/test_runtime.cpp`, observer/API/runtime/metrics/adapter/baseline docs, `docs/plans/plan2.md`, goal ledgers, and `CHANGELOG.md`. | `RuntimeRunnerOptions::observers` now delivers best-effort result/metric/trace/health/error records through stable-v0.2 observer/sink callbacks; no-op and bounded in-memory observers are available; observer callback failures and bounded drops are non-fatal and observable. |

## Validation Evidence

Fresh checks in this working tree:

```bash
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
