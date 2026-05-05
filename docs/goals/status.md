# Goal Status

Last updated: 2026-05-06

## Current Plan Source

- Completed architecture plan: `docs/plans/plan.md` (G0-G25 archived complete)
- Active architecture plan: `docs/plans/plan2.md` (G26+)
- Backlog: `docs/goals/backlog.md`
- Required repository gate: `scripts/agent_check.sh`

## Current Stage

G26 established the post-G25 release-candidate baseline, G27 completed the public API stability pass, and G28 added the runtime semantic contract.
The next unfinished P0/P1 goal is G66 Architecture Enforcement CI v2.

## Active / Recent Goals

| ID | Status | Evidence | Notes |
| --- | --- | --- | --- |
| G0-G25 | archived complete | Previous entries in git history through `b86a586d3a48d84bf4e03ccabde3d061e3073579`, release docs, golden/package/sanitizer evidence. | Do not treat the old board as active work unless a regression is found. |
| G26 | complete | `docs/current-baseline.md`, `docs/release-progression.md`, `docs/release-checklist.md`, `docs/goals/backlog.md`, `docs/goals/status.md`, `docs/plans/plan2.md`, and expanded `tests/golden/*` coverage. | Protects the post-G25 baseline, recommends `v0.2.0-alpha.0` as the next prerelease decision, and adds golden coverage for Chrome trace, schema dump, and doctor JSON. |
| G27 | complete | `docs/public-api.md`, `docs/api-change-checklist.md`, installed `include/topoexec/**` stability markers, `tests/cmake/runtime_smoke/main.cpp`, `docs/versioning.md`, and `CHANGELOG.md`. | Public API is classified as stable-v0.2/mixed/experimental; runtime-only downstream smoke covers GraphBuilder, ComponentRegistry, typed payloads, RuntimeRunner, and metrics/trace result consumption. |
| G28 | complete | `docs/semantic-contract.md`, `docs/versioning.md`, `docs/runtime-semantics.md`, `docs/schema-v1.md`, `docs/cli.md`, `include/topoexec/runtime/graph.hpp`, `tools/topoexec/main.cpp`, `schema/topoexec.schema.v1.json`, `tests/golden/doctor.json`, `tests/golden/schema_dump.json`, and `tests/schema/check_schema_contract.py`. | Runtime semantic contract version `0.2` is documented and exposed through doctor/schema dump without adding a new CLI command or changing graph behavior. |

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

grep -RInE '#include .*(yaml|rclcpp|opentelemetry|prometheus|Python|perfetto|tools/topoexec|src/)' include || true
# G27 passed: no YAML/CLI/adapter/private includes in installed headers
```

## Blockers

No active blockers.
