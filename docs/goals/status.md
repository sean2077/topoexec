# Goal Status

Last updated: 2026-05-06

## Current Plan Source

- Completed architecture plan: `docs/plans/plan.md` (G0-G25 archived complete)
- Active architecture plan: `docs/plans/plan2.md` (G26+)
- Backlog: `docs/goals/backlog.md`
- Required repository gate: `scripts/agent_check.sh`

## Current Stage

Phase A is complete: G26 established the post-G25 release-candidate baseline, G27 completed the public API stability pass, G28 added the runtime semantic contract, and G66 enforced architecture boundaries. Phase B has started with G29, G30, G31, G32, G33, and G34 complete.
The next unfinished P1 goal is G36 Correlation, Causality, and Invocation Metadata; G35 remains pending P2 and is deferred by the active ordering rule.

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

## Validation Evidence

Fresh checks in this working tree:

```bash
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
