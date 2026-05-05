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
| G6 | P0/P1 | complete | scheduler docs/runtime/tests | Scheduler lane spec parses admission/timing fields; thread_pool bounded-batch v1 covers reentrant overlap, non-reentrant serialization, queue admission overflow, rejected metrics, batch trace spans, stop cleanup, and fixed_rate simulated overrun/jitter metrics. Persistent named workers and wall-clock sleep cadence are explicitly deferred. | `./scripts/agent_check.sh` |
| G7 | P1 | complete | async task runtime | Optional deterministic `TaskExecutor`, bounded task admission, cancellation, failure completions, metrics, and `GraphContext::submit_task()` completion publication are implemented and tested. Threaded task pools remain a future extension. | `./scripts/agent_check.sh` |
| G8 | P0/P1 | complete | channel/backpressure docs/runtime/tests | Bounded policies, overflow/reject/overwrite/stale/deadline health metrics, peek/snapshot/bounded drain/read APIs, per-reader queue cursors, and channel docs are implemented and tested. Backpressure remains metric/health-event based, not recursive execution. | `./scripts/agent_check.sh` |
| G9 | P1 | complete | payload/memory/buffer pool | Built-in payloads, type-erased `OpaquePayload` custom schemas, BufferPool loan/release/byte metrics, copy-policy tests, no-copy loaned-frame tests, and memory docs are implemented. External shared-memory/zero-copy middleware remains out of scope. | `./scripts/agent_check.sh` |
| G10 | P1 | complete | trigger engine docs/runtime/tests | Runtime-owned trigger readiness covers any/all/time-sync/batch/request/task/future-ready paths, coalescing/min-interval/max-latency timeout, local correlation ids, and trigger metrics for timeout drops, batch flushes, and time-sync drops. Watermark/condition triggers remain future extensions. | `./scripts/agent_check.sh` |
| G11 | P1 | complete | CompositeLoop/region runtime | Exact SCC ownership, bounded iterations, convergence, budget overrun, internal failure accounting, external-output commit isolation, loop metrics, loop trace events, and CompositeLoop docs are implemented and tested. Solver-style/typed convergence callbacks remain future extensions. | `./scripts/agent_check.sh` |
| G12 | P1/P2 | complete | state/config snapshot docs/runtime/tests | State edges preserve committed snapshot isolation until the next epoch, state commit metrics are exposed, graph-level config parses, and optional epoch-boundary `RuntimeStateStore` / `ConfigSnapshotStore` APIs are implemented and tested. | `./scripts/agent_check.sh` |
| G13 | P1 | complete | metrics/trace/diagnostics | Metrics/trace JSON and Chrome trace have golden coverage; histograms export count/min/max/avg/p50/p95/p99; stable diagnostic descriptors are exposed through `topoexec/runtime/diagnostics.hpp` and `docs/diagnostics.md`. | `./scripts/agent_check.sh` |
| G14 | P1/P2 | complete | benchmark suite | `benchmarks/` deterministic cases cover single component, immediate chain, latest/queue, deferred edges, and thread_pool paths; bench JSON includes case/params/per-run latency percentiles/throughput/environment and docs avoid performance claims. | `./scripts/agent_check.sh` |
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

Continue with **G15+ productization/runtime hardening**, unless the user explicitly asks otherwise. Do not start adapter code before the partial P0/P1 runtime goals above are complete.

## Blockers

No active blockers. Use `docs/goals/blockers/<goal-id>.md` if a product/API decision is required before implementation.
