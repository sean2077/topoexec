# Goal Status

Last updated: 2026-05-05

## Current Plan Source

- Architecture plan: `docs/plans/plan.md`
- Backlog: `docs/goals/backlog.md`
- Required repository gate: `scripts/agent_check.sh`

## Active / Recent Goals

| ID | Status | Evidence | Notes |
| --- | --- | --- | --- |
| G0 | complete | `tests/golden/check_cli_golden.py`, `tests/golden/*.json`, `tests/golden/render_minimal.mmd`, `cli_golden_outputs` CTest, `docs/current-baseline.md`. | Normalizes volatile duration/trace fields while preserving semantic plan/metrics/trace/render drift detection. |
| G1 | complete | `docs/public-api.md` and `tests/cmake/runtime_smoke/main.cpp`. | Installed downstream smoke links only `topoexec::runtime` and uses GraphBuilder, typed payload access, ComponentRegistry, and RuntimeRunner. |
| G5 | complete | `schema/topoexec.schema.v1.json`, `docs/schema-v1.md`, `cli_validate_schema_only_minimal`, `cli_validate_semantic_minimal`, and `schema_v1_contract_smoke`. | JSON Schema is a generation/documentation contract; semantic SCC/port/trigger rules remain enforced by C++ validation. |
| G2 | complete | `RuntimeRunnerResult::runtime_errors`, `ExecutionSpec::on_error`, runtime lifecycle tests, thread_pool execute-failure test, and `docs/runtime-semantics.md`. | Existing `errors` strings remain compatible; structured errors expose phase/component/code/fatal and only `fail_fast` is implemented. |
| G4 | partial | `GraphValidationResult::diagnostics`, `GraphCompileResult::diagnostics`, CLI validate JSON diagnostics, and graph tests. | Diagnostic code/severity/suggested-fix now exist; graph paths and involved ids need follow-up. |
| G22 | complete | `scripts/goal_check.sh`, `docs/agent-goals.md`, `.github/PULL_REQUEST_TEMPLATE.md`, `.github/ISSUE_TEMPLATE/*`, and `docs/contributing.md`. | Agents and reviewers have goal-specific validation, handoff, PR, issue, and contribution surfaces. |
| G23 | complete | `docs/architecture-guardrails.md`, `docs/public-api.md`, and runtime-only package smoke. | Module boundaries and dependency constraints are explicit and partially enforced by install smoke. |

## Current Stage

The repository has moved beyond the initial P0 baseline/API/schema lock. The next safe implementation stage is to finish the partial P0/P1 runtime hardening goals in this order:

1. G3 explicit invariant suite coverage;
2. G4 graph compiler diagnostic paths/involved ids and edge/trigger tables;
3. then G6/G8/G10/G11 runtime completeness work.

## Validation Evidence

Most recent targeted checks in this working tree:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure -R 'cli_validate_schema_only_minimal|cli_validate_semantic_minimal|schema_v1_contract_smoke|cli_golden_outputs'
ctest --test-dir build --output-on-failure -R 'schema_v1_contract_smoke|cli_golden_outputs|cmake_package_runtime_smoke'
```

Run `./scripts/agent_check.sh` before declaring a repo-changing stage complete.

## Blockers

No active blockers.
