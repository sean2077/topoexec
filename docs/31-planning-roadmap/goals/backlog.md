# Goal Backlog

Last updated: 2026-05-08

This is the current roadmap entry point. It lists how to choose the next goal,
whether any work is active, which historical ranges are complete, and which
future tracks remain deferred.

## Selection Rule

1. Start with the earliest unfinished P0 or P1 item below unless the user names
   a narrower goal.
2. Prefer runtime semantics, API clarity, tests, packaging, release evidence,
   and documentation accuracy over new CLI surface.
3. Keep production adapters, schema v2 implementation, editor/LSP work, and
   registry publication deferred until explicitly opened.
4. Each new goal must state scope, allowed files, acceptance criteria,
   validation commands, and blocker handling before edits spread.

## Active Backlog

No active implementation goal is open.

## Recently Completed

### G100 Runtime contract bugfix and boundedness stabilization

State: complete.

Scope:
- Align schema/runtime trigger behavior for mixed timer/message sources,
  unsupported action sources, and reserved debounce window semantics.
- Make channel fan-out and batch publication reject paths atomic where the
  runtime can preflight the failure before commit.
- Harden bounded drain, scheduler metric, executor completion, health emission,
  and stop-responsiveness behavior with focused tests.
- Keep semantic docs, schema/goldens, CHANGELOG, and preview boundary wording
  synchronized with the implementation.

Allowed files:
- Runtime implementation: `src/trigger_policy.cpp`, `src/graph.cpp`,
  `src/graph_io.cpp`, `src/channel.cpp`, `src/task_executor.cpp`,
  `src/scheduler.cpp`, `src/event_runtime.cpp`, and matching public runtime
  headers when required.
- Tests: `tests/test_runtime.cpp`, `tests/test_channel.cpp`,
  `tests/test_graph.cpp`, `tests/test_stress.cpp`, and focused CLI/schema/golden
  checks for changed contracts.
- Docs and generated contracts: runtime semantic docs, schema files, generated
  goldens, this roadmap ledger, and CHANGELOG entries directly tied to G100.

Acceptance criteria:
- Unsupported schema-accepted runtime semantics are rejected or explicitly
  reserved with diagnostics; supported semantics have implementation evidence.
- Fan-out and batch publication preflightable failures are all-or-nothing.
- Bounded batch/capacity/queue/metric behavior is covered by regression tests.
- No new adapter, schema-v2 implementation, native Python binding, production
  exporter, hard-real-time, or runtime dashboard scope is added.

Validation:
- Required final gate: `scripts/agent_check.sh`.
- Focused gates as applicable: `scripts/goal_check.sh docs`, `golden`,
  `schema`, `stress`, `fuzz`, `sanitizer`, `bench`, and `live`.
- Targeted CTest filters should run before broader gates for graph, runtime,
  channel, stress, scheduler, and executor regressions.

Current evidence:
- `scripts/agent_check.sh` passed format, clang-tidy, build, and 87/87 CTest
  cases.
- `release_prepare_smoke` now passes without deleting or rewriting an existing
  local `v0.2.0-alpha.0` tag.
- Sanitizer CTest passed 86/86 after excluding only the release smoke.
- Focused docs, schema, golden, package, stress, fuzz, bench, live, and
  live-perf gates passed.

## Completed Historical Sweeps

| Range | Status | Current evidence |
| --- | --- | --- |
| G0-G25 | Complete | Git history through the post-G25 baseline, release docs, runtime/API docs, and current tests. |
| G26-G70 | Complete | Current architecture, API, testing, release, integration, schema, and user-guide docs. |
| G71 | Complete | Post-alpha semantic hardening, benchmark expansion, optional Doxygen API docs, Pages workflow, and release alignment summarized in `status.md`. |
| G73 | Complete | Live runtime validation workbench, `graph observe`, live assertions, record/replay artifacts, dashboard, and live gates summarized in `status.md`. |
| G74 | Complete | Examples/showcase/README refresh with generated visual assets and examples/showcase anti-rot gates summarized in `status.md`. |
| G75-G85 | Complete | Release/adoption readiness, dogfood, compatibility, package, reliability, adoption, ecosystem deferral, beta-readiness, and v1 deferral summarized in `status.md`. |
| G86-G95 | Complete | Trust-trial maintainability sweeps over runtime, CLI/schema/YAML, performance, coverage, package/API, reliability, public consistency, and release-candidate surfaces summarized in `status.md`. |
| G99 | Complete | Final validation and ledger reconciliation for G75-G95 summarized in `status.md`. |
| G100 | Complete | Runtime contract bugfix and boundedness stabilization summarized in `status.md`. |

The old detailed plan files were deleted as completed process artifacts during
the 2026-05 documentation cleanup. Current facts live in this backlog, the
status ledger, stable docs, and git history.

## Deferred Backlog

| Topic | Status | Open only when |
| --- | --- | --- |
| Schema v2 implementation and migration CLI | Deferred | A reviewed v2 loader/migration design is accepted. |
| Full editor extension or LSP server | Deferred | The current JSON Schema and diagnostic JSON workflow no longer covers editor UX needs. |
| Production OpenTelemetry or Prometheus exporters | Deferred | A concrete exporter dependency and transport boundary is approved. |
| Real ROS 2 package/client-library adapter | Deferred | The core/runtime/API boundary remains stable and a ROS dependency decision is approved. |
| Stable C ABI beyond ABI version 0 | Deferred | FFI ownership, error, and versioning rules are ready for compatibility guarantees. |
| Native Python bindings | Deferred | CLI-backed automation is insufficient and native binding ownership/performance goals are explicit. |
| Sandboxed or stable plugin ecosystem | Deferred | The trusted-native loader preview is not enough and sandbox/unload/API policy is settled. |
| Package registry publication | Deferred | A human release owner approves exact artifacts, tags, and registry targets. |
| Hard real-time scheduling, affinity, or preemption | Deferred | OS policy, timing guarantees, and test strategy are explicitly scoped. |
| v1.0 readiness | Deferred | Post-beta adoption evidence, stable-surface decisions, package/adoption maturity, and human release-owner acceptance exist. |

## Blockers

Active blockers live under `docs/31-planning-roadmap/goals/blockers/` for
G81/G82/G83 ecosystem, integration, package-publication, and schema-v2
decisions. v1.0 readiness is also deferred until post-beta adoption evidence,
stable-surface decisions, package/adoption maturity, and a human release-owner
decision exist.

If a new product/API decision blocks a future goal, write:

```text
docs/31-planning-roadmap/goals/blockers/<goal-id>.md
```

The blocker note should include the decision needed, options, recommendation,
API/runtime/test/doc impact, and any safe independent goal that can continue.

## Ledger Policy

This backlog stays status-oriented. Keep detailed command output in CI logs,
release-prep artifacts, or git history, and keep local runtime evidence paths out
of this primary roadmap entry point.
