# Goal Status

Last updated: 2026-05-06

## Current Plan Source

- Completed architecture plan: `docs/plans/plan.md` (G0-G25 archived complete)
- Active architecture plan: `docs/plans/plan2.md` (G26+)
- Backlog: `docs/goals/backlog.md`
- Required repository gate: `scripts/agent_check.sh`

## Current Stage

G26 establishes the post-G25 release-candidate baseline for the new plan2 board.
The next unfinished P0 goal is G27 Public API Stability Pass v2.

## Active / Recent Goals

| ID | Status | Evidence | Notes |
| --- | --- | --- | --- |
| G0-G25 | archived complete | Previous entries in git history through `b86a586d3a48d84bf4e03ccabde3d061e3073579`, release docs, golden/package/sanitizer evidence. | Do not treat the old board as active work unless a regression is found. |
| G26 | complete | `docs/current-baseline.md`, `docs/release-progression.md`, `docs/release-checklist.md`, `docs/goals/backlog.md`, `docs/goals/status.md`, `docs/plans/plan2.md`, and expanded `tests/golden/*` coverage. | Protects the post-G25 baseline, recommends `v0.2.0-alpha.0` as the next prerelease decision, and adds golden coverage for Chrome trace, schema dump, and doctor JSON. |

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
```

## Blockers

No active blockers.
