# Goal Status

Last updated: 2026-05-06

This is the current goal ledger. It records live state and high-signal release
facts only; detailed per-goal command transcripts belong in CI artifacts,
release-prep evidence, or git history.

## Current State

| Area | State |
| --- | --- |
| Historical goal sweeps | G0-G25 and G26-G70 are complete. |
| Active implementation goal | None. |
| Required repository gate | `scripts/agent_check.sh` before declaring repo changes complete. |
| Focused docs gate | `scripts/goal_check.sh docs`. |
| Blockers | None active. |

## Canonical Evidence Surfaces

| Need | Canonical doc or check |
| --- | --- |
| Architecture and runtime behavior | `docs/21-architecture/runtime-architecture.md`, `docs/21-architecture/runtime-semantics.md`, `docs/21-architecture/runtime-invariants.md`, `docs/21-architecture/semantic-contract.md` |
| Codebase ownership | `docs/22-codebase/codebase-map.md` |
| Public API stability | `docs/61-api/public-api.md`, `docs/61-api/api-change-checklist.md`, `docs/61-api/c-api.md` |
| Schema and wire contracts | `docs/33-specs-rfcs/schema-v1.md`, `docs/33-specs-rfcs/schema-v2-notes.md`, `docs/62-schemas-protocols/metrics.md`, `docs/62-schemas-protocols/trace-events.md`, `docs/62-schemas-protocols/diagnostics.md` |
| Testing and release gates | `docs/24-testing/testing-strategy.md`, `docs/43-ci-build-release-tools/current-baseline.md`, `docs/43-ci-build-release-tools/release-checklist.md`, `docs/43-ci-build-release-tools/release-runbook.md` |
| Beta readiness and deferrals | `docs/43-ci-build-release-tools/beta-readiness-review.md`, `docs/43-ci-build-release-tools/release-progression.md` |
| Current backlog | `docs/31-planning-roadmap/goals/backlog.md` |
| Deleted planning/process artifacts | `docs/94-doc-migrations/2026-05-process-ledger-cleanup.md` |

## Completed Rollup

| Range | Outcome | Replacement evidence |
| --- | --- | --- |
| G0-G25 | Completed the earlier runtime/API stabilization board. | Current runtime/API docs, release docs, tests, and git history. |
| G26-G34 | Completed public API, semantic contract, scheduler, worker-pool, fixed-rate, priority, cancellation, and task-executor stabilization. | API, semantic, scheduler, concurrency, async, metrics, trace, and runtime docs plus tests. |
| G35-G45 | Completed trigger, metadata, channel, payload, graph compiler, hierarchy, templates, lifecycle, config, and CompositeLoop work. | User guides, architecture docs, schema docs, runtime tests, graph tests, and goldens. |
| G46-G54 | Completed observer, metric, trace, diagnostic, defensive-input, fuzz, stress, benchmark, and packaging surfaces. | Schema/protocol docs, testing docs, package docs, release docs, and focused goal checks. |
| G55-G70 | Completed docs system, examples, adapter/FFI/Python/plugin previews, schema-v2 boundary, editor/schema UX, architecture policy, release automation, community readiness, robot-cell pilot, and beta-readiness review. | Docs tree, examples, integration docs, release docs, policy checks, and beta review. |

## Deferred Scope

The following remain deferred unless the user opens a goal with explicit scope:

- schema v2 implementation and migration tooling;
- full editor extension or LSP server;
- production OpenTelemetry/Prometheus exporters;
- real ROS 2 package/client-library adapter;
- stable C ABI beyond ABI version 0;
- native Python bindings;
- sandboxed/stable plugin ecosystem and graph-driven plugin discovery;
- package-registry publication;
- hard real-time scheduling, CPU affinity guarantees, or hard preemption.

## Update Rule

When a future goal starts, completes, blocks, or is intentionally deferred,
update this page and `backlog.md` with the new state. Do not reintroduce large
generated task boards or paste full command transcripts into this ledger.
