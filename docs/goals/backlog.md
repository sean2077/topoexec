# Goal Backlog

This backlog is derived from `docs/plans/plan.md` and is ordered for agents that are asked to continue the long-running plan without a narrower goal. Status is evidence-based; do not mark a goal complete unless its acceptance criteria have code/docs/tests evidence and `./scripts/agent_check.sh` passes.

## Active ordering rule

1. Finish the earliest `partial` P0/P1 goal before starting a new subsystem.
2. Prefer tests/docs that harden runtime semantics over new adapters or broad CLI surface.
3. Keep adapter implementation deferred until core/runtime/API/concurrency goals are complete.

## Goal board

| ID | Priority | Status | Scope | Acceptance / evidence gate | Validation |
| --- | --- | --- | --- | --- | --- |
| G0 | P0 | complete | `docs/current-baseline.md`, `tests/golden/`, `CMakeLists.txt` | Baseline records current local evidence; normalized golden tests cover plan JSON, metrics JSON, trace JSON, and Mermaid render. | `./scripts/agent_check.sh` |
| G1 | P0 | complete | `docs/public-api.md`, public headers, `tests/cmake/runtime_smoke` | Public API matrix exists; installed runtime-only smoke uses `GraphBuilder`, typed payload helpers, `ComponentRegistry`, and `RuntimeRunner` with only `topoexec::runtime`. | `./scripts/agent_check.sh` |
| G2 | P0 | complete | runtime lifecycle/error model | `RuntimeRunnerResult::runtime_errors` carries structured phase/component/code/fatal data; configure/activate/execute/deactivate and thread_pool execute failures are tested; non-fail-fast `execution.on_error` values parse but are rejected. | `./scripts/agent_check.sh` |
| G3 | P0 | complete | `docs/runtime-invariants.md`, runtime/graph/channel tests, golden tests | All 20 plan invariants are mapped to concrete CI tests or golden checks; maintenance rule requires updating coverage with semantic changes. | `./scripts/agent_check.sh` |
| G4 | P0/P1 | complete | graph compiler diagnostics/plan | Structured plan JSON, compiled regions/SCCs, `GraphDiagnostic` code/severity/path/involved ids/suggested-fix, and CLI validate JSON diagnostics are present. | `./scripts/agent_check.sh` |
| G5 | P1 | complete | `docs/schema-v1.md`, `schema/topoexec.schema.v1.json`, CLI validation tests | Schema reference, strict machine-readable schema, `--schema-only`, `--semantic`, schema contract smoke, valid fixtures, and invalid fixture rejection are present. | `./scripts/agent_check.sh` |
| G6 | P0/P1 | partial | scheduler docs/runtime/tests | Event-loop/thread-pool tests exist and simulated fixed_rate overrun metrics are tested; persistent worker pool and wall-clock cadence remain deferred. | `./scripts/agent_check.sh` |
| G7 | P1 | not-started | async task runtime | Async edge admission exists, but optional `TaskExecutor`/future runtime is not implemented. | `./scripts/agent_check.sh` |
| G8 | P0/P1 | partial | channel/backpressure docs/runtime/tests | Bounded policies, lifespan stale drops, deadline miss flags/metrics, and channel health metrics exist; fuller multi-reader/backpressure event semantics remain. | `./scripts/agent_check.sh` |
| G9 | P1 | partial | payload/memory/buffer pool | Built-in payloads and prototype `BufferPool` exist; custom registration, pool metrics, and no-copy examples need completion. | `./scripts/agent_check.sh` |
| G10 | P1 | partial | trigger engine docs/runtime/tests | Trigger engine supports basic any/all/time-sync/batch paths; request/future/watermark-style completeness remains. | `./scripts/agent_check.sh` |
| G11 | P1 | partial | CompositeLoop/region runtime | Exact SCC ownership and loop metrics exist; typed convergence/budget/error policy is still incomplete. | `./scripts/agent_check.sh` |
| G12 | P1/P2 | not-started | state/config snapshot docs/runtime/tests | State edge visibility exists; blackboard/config snapshot API is not implemented. | `./scripts/agent_check.sh` |
| G13 | P1 | partial | metrics/trace/diagnostics | Metrics/trace JSON and Chrome trace exist and have golden coverage; histograms and stable diagnostics registry remain. | `./scripts/agent_check.sh` |
| G14 | P1/P2 | partial | benchmark suite | CLI bench JSON exists; broader deterministic benchmark cases and docs remain. | `./scripts/agent_check.sh` |
| G15 | P2 | partial | CLI/tooling | Existing CLI commands are covered by smokes/goldens; doctor/schema dump/expanded lint/explain are not all complete. | `./scripts/agent_check.sh` |
| G16 | P1/P2 | partial | examples/apps | Core examples build/run; additional state, batch/time-sync, large payload, service, registry, and boundary-pattern examples remain. | `./scripts/agent_check.sh` |
| G17 | P1/P2 | partial | documentation system | Reference docs exist; tutorial path and snippet/doc tests need completion. | `./scripts/agent_check.sh` |
| G18 | P0/P1 | partial | testing strategy | Unit/semantic/package/golden tests exist; fuzz and sanitizer gates are not complete. | `./scripts/agent_check.sh` |
| G19 | P1/P2 | partial | build/package/distribution | Install/export/runtime smoke exists; optional target switches and package-manager drafts remain. | `./scripts/agent_check.sh` |
| G20 | P2 | not-started | adapter architecture preview docs/stubs | `docs/adapters.md` exists but needs the full contract/stub layout from plan. No adapter dependency should enter core. | docs review plus `./scripts/agent_check.sh` |
| G21 | P2/P3 | not-started | ROS 2 adapter plan | Deferred design doc only; no core ROS dependency. | docs review plus `./scripts/agent_check.sh` |
| G22 | P1 | complete | `AGENTS.md`, `docs/agent-goals.md`, `scripts/goal_check.sh`, `.github/*`, `docs/contributing.md` | Goal queue, focused validation dispatcher, PR template, issue templates, and contribution policy are present. | `./scripts/agent_check.sh` |
| G23 | P0/P1 | complete | `docs/architecture-guardrails.md`, CMake package smoke, public API docs | Module ownership, dependency rules, enforced package smoke, and review guardrails are documented. | `./scripts/agent_check.sh` |
| G24 | P2 | not-started | defensive input handling | Strict unknown-field parsing exists; parser limits/fuzz/path guards remain. | `./scripts/agent_check.sh` |
| G25 | P1/P2 | partial | release progression | Release checklist/versioning exist; progression docs should track goal completion. | `./scripts/agent_check.sh` |

## Next goal

Continue with **G6/G8/G10/G11 runtime completeness** unless the user explicitly asks to prioritize P1 productization. Do not start adapter code before the partial P0/P1 runtime goals above are complete.

## Blockers

No active blockers. Use `docs/goals/blockers/<goal-id>.md` if a product/API decision is required before implementation.
