# Goal Status

Last updated: 2026-05-05

## Current Plan Source

- Architecture plan: `docs/plan3.md`
- Backlog: `docs/goals/backlog.md`
- Required repository gate: `scripts/agent_check.sh`

## Active / Recent Goals

| ID | Status | Evidence | Notes |
| --- | --- | --- | --- |
| A1 | complete | `docs/current-baseline.md` records `b4886c2`, CTest 29/29, CI run `25355571811`, and tag relationship. | Current README, changelog, baseline, and release checklist agree on thread_pool, async max_inflight, sanitizer CI, and deferred adapters. |
| A2 | complete | `AGENTS.md`, `docs/goals/README.md`, this status file, and `docs/goals/backlog.md`. | Goal format, blocker protocol, and adapter/CLI gates are documented. |
| A3 | complete | CI already covers GCC/Clang Debug/RelWithDebInfo, package smoke, and non-blocking clang TSAN; `cmake --build build --target topoexec_format_check` passed locally. | Local quality commands are documented in README and the release checklist. |
| 002 | complete | `docs/public-api.md` maps stable, mixed, experimental, internal, schema bump, and CLI JSON boundaries; runtime public headers carry API-category comments. | `examples/apps/overload_latest_vs_queue` is explicitly marked as the advanced low-level channel-policy tutorial. |
| 003 | complete | `tests/test_runtime.cpp` covers immediate same-epoch feed-forward, delay/state/async next-epoch visibility, and exact staged/committed publication metric agreement. | Targeted runtime tests passed for the new and updated edge-visibility cases. |
| 004 | complete | `tests/test_runtime.cpp` covers configure, activate, execute, and deactivate failures, including partial startup cleanup and reverse deactivation order. | Targeted lifecycle tests passed for configure/execute plus new activate/deactivate cases. |
| 005 | complete | `tests/test_graph.cpp` has fixed-seed random DAG determinism and fixed-seed immediate-cycle rejection paired with exact CompositeLoop acceptance. | Targeted graph compiler tests passed for DAG, cycle, partial loop, overlapping SCC, and non-immediate feedback cases. |
| 006 | complete | `docs/scheduler.md` and `docs/concurrency.md` define event-loop, fixed-rate, and thread-pool behavior, enforced vs advisory fields, barriers, stop/drain, metrics, and deferred worker-pool work. | Targeted thread-pool tests passed, including reentrant overlap within `max_threads` and non-reentrant serialization. |
| 007 | complete | `tests/test_runtime.cpp` covers async max-inflight acceptance, `drop_oldest`, `drop_newest`, `reject`, `fail_fast`, and `block` behavior with matching async metrics. | Targeted async tests passed and docs already distinguish async edge admission from task execution. |
| 008 | complete | `docs/payloads.md`, `tests/test_channel.cpp`, and `tests/test_graph.cpp` cover typed wrong-type/null/missing-port/batch access plus copy/shared/loaned/move-only/large-copy semantics. | Targeted payload, channel, and move-only graph tests passed. |

## Next Goal

After Goal 008 passes validation, all P0 backlog goals from `docs/plan3.md` are complete. Continue to P1 Goal 009 only when the next work should move beyond beta-entry hardening.

## Blockers

No active blockers.
