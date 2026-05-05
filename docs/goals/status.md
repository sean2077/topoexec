# Goal Status

Last updated: 2026-05-06

## Current Plan Source

- Completed architecture plan: `docs/plans/plan.md` (G0-G25 archived complete)
- Active architecture plan: `docs/plans/plan2.md` (G26+)
- Backlog: `docs/goals/backlog.md`
- Required repository gate: `scripts/agent_check.sh`

## Current Stage

Phase A is complete: G26 established the post-G25 release-candidate baseline, G27 completed the public API stability pass, G28 added the runtime semantic contract, and G66 enforced architecture boundaries. Phase B has started with G29 complete.
The next unfinished P1 goal is G30 Persistent Worker Pool v1.

## Active / Recent Goals

| ID | Status | Evidence | Notes |
| --- | --- | --- | --- |
| G0-G25 | archived complete | Previous entries in git history through `b86a586d3a48d84bf4e03ccabde3d061e3073579`, release docs, golden/package/sanitizer evidence. | Do not treat the old board as active work unless a regression is found. |
| G26 | complete | `docs/current-baseline.md`, `docs/release-progression.md`, `docs/release-checklist.md`, `docs/goals/backlog.md`, `docs/goals/status.md`, `docs/plans/plan2.md`, and expanded `tests/golden/*` coverage. | Protects the post-G25 baseline, recommends `v0.2.0-alpha.0` as the next prerelease decision, and adds golden coverage for Chrome trace, schema dump, and doctor JSON. |
| G27 | complete | `docs/public-api.md`, `docs/api-change-checklist.md`, installed `include/topoexec/**` stability markers, `tests/cmake/runtime_smoke/main.cpp`, `docs/versioning.md`, and `CHANGELOG.md`. | Public API is classified as stable-v0.2/mixed/experimental; runtime-only downstream smoke covers GraphBuilder, ComponentRegistry, typed payloads, RuntimeRunner, and metrics/trace result consumption. |
| G28 | complete | `docs/semantic-contract.md`, `docs/versioning.md`, `docs/runtime-semantics.md`, `docs/schema-v1.md`, `docs/cli.md`, `include/topoexec/runtime/graph.hpp`, `tools/topoexec/main.cpp`, `schema/topoexec.schema.v1.json`, `tests/golden/doctor.json`, `tests/golden/schema_dump.json`, and `tests/schema/check_schema_contract.py`. | Runtime semantic contract version `0.2` is documented and exposed through doctor/schema dump without adding a new CLI command or changing graph behavior. |
| G66 | complete | `tests/policy/check_no_adapter_deps.py`, `CMakeLists.txt`, `scripts/goal_check.sh`, `docs/architecture-guardrails.md`, `docs/goals/backlog.md`, `docs/goals/status.md`, and `CHANGELOG.md`. | Architecture policy now audits installed-header markers, common/runtime/YAML/CLI boundaries, private include leaks, adapter tokens, CMake target links, CLI semantic-bypass includes, and planted fake dependency violations. |
| G29 | complete | `docs/scheduler.md`, `docs/concurrency.md`, `docs/diagnostics.md`, `src/graph.cpp`, `src/graph_io.cpp`, `src/diagnostics.cpp`, `tests/test_graph.cpp`, `tests/golden/plan_composite_loop.json`, `docs/goals/backlog.md`, `docs/goals/status.md`, and `CHANGELOG.md`. | Scheduler plan JSON now exposes lane capability summaries; validation emits advisory diagnostics for parsed-but-not-enforced lane/execution fields without failing valid graphs. |

## Validation Evidence

Fresh G26 checks in this working tree:

```bash
./scripts/goal_check.sh quick
# passed: cli_golden_outputs and schema_v1_contract_smoke

./scripts/agent_check.sh
# passed: 50/50 CTest tests in the default RelWithDebInfo GCC build

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

grep -RInE '#include .*(yaml|rclcpp|opentelemetry|prometheus|Python|perfetto|tools/topoexec|src/)' include || true
# G27 passed: no YAML/CLI/adapter/private includes in installed headers
```

## Blockers

No active blockers.
