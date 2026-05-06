# Goal Backlog

Last updated: 2026-05-06

This is the current roadmap entry point. It replaces the completed long-form
plan boards with a compact status-oriented backlog.

## Selection Rule

1. Start with the earliest unfinished P0 or P1 item below unless the user names
   a narrower goal.
2. Prefer runtime semantics, API clarity, tests, packaging, release evidence,
   and documentation accuracy over new CLI surface.
3. Keep production adapters, schema v2 implementation, editor/LSP work, and
   registry publication deferred until explicitly opened.
4. Each new goal must state scope, allowed files, acceptance criteria,
   validation commands, and blocker handling before edits spread.

## Completed Historical Sweeps

| Range | Status | Current evidence |
| --- | --- | --- |
| G0-G25 | complete | Git history through the post-G25 baseline commit, release docs, runtime/API docs, and current tests. |
| G26-G70 | complete | Current architecture, API, testing, release, and integration docs plus `docs/31-planning-roadmap/goals/status.md`. |
| G71 | complete | Post-alpha runtime semantic hardening, benchmark expansion, optional Doxygen API docs, Pages workflow, release docs, and final local validation in `docs/31-planning-roadmap/goals/status.md`. |

The old detailed plan files were deleted as completed process artifacts during
the 2026-05 documentation cleanup. See
[`docs/94-doc-migrations/2026-05-process-ledger-cleanup.md`](../../94-doc-migrations/2026-05-process-ledger-cleanup.md)
for deletion evidence and replacement surfaces.

## Active Backlog

| ID | Priority | Status | Scope | Acceptance | Validation | Blocker handling |
| --- | --- | --- | --- | --- | --- | --- |
| G71-post-alpha-hardening-docs-pages | P0/P1 mixed | complete | `src/channel.cpp`, `src/trigger_policy.cpp`, `src/event_runtime.cpp`, related runtime headers/tests/docs, `CMakeLists.txt`, docs-site/Doxygen/Pages files, release and goal ledgers. | Baseline evidence recorded; P0 runtime semantic regressions covered; intentional alpha limitations documented; benchmark baseline evidence captured; optional Doxygen target added; Pages docs workflow added; README/CHANGELOG/release docs aligned. | `./scripts/agent_check.sh`, sanitizer/stress/fuzz/bench/docs-site/Doxygen checks passed locally on 2026-05-06; see `status.md`. | No product/API blockers. Repository owner still must enable GitHub Pages source as GitHub Actions before a public URL is advertised. |

## Deferred Backlog

| Topic | Status | Open only when |
| --- | --- | --- |
| Schema v2 implementation and migration CLI | deferred | A reviewed v2 loader/migration design is accepted. |
| Full editor extension or LSP server | deferred | The current JSON Schema and diagnostic JSON workflow no longer cover editor UX needs. |
| Production OpenTelemetry or Prometheus exporters | deferred | A concrete exporter dependency and transport boundary is approved. |
| Real ROS 2 package/client-library adapter | deferred | The core/runtime/API boundary remains stable and a ROS dependency decision is approved. |
| Stable C ABI beyond ABI version 0 | deferred | FFI ownership, error, and versioning rules are ready for compatibility guarantees. |
| Native Python bindings | deferred | CLI-backed automation is insufficient and native binding ownership/performance goals are explicit. |
| Sandboxed or stable plugin ecosystem | deferred | The trusted-native loader preview is not enough and sandbox/unload/API policy is settled. |
| Package registry publication | deferred | A human release owner approves exact artifacts, tags, and registry targets. |
| Hard real-time scheduling, affinity, or preemption | deferred | OS policy, timing guarantees, and test strategy are explicitly scoped. |

## Blockers

No active blockers. If a product/API decision blocks a future goal, write:

```text
docs/31-planning-roadmap/goals/blockers/<goal-id>.md
```

The blocker note should include the decision needed, options, recommendation,
API/runtime/test/doc impact, and any safe independent goal that can continue.
