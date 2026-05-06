# 2026-05 Process Ledger Cleanup

Date: 2026-05-06

This follow-up cleanup removed completed planning and process-ledger material
that competed with current architecture, API, testing, and release docs.

## Deleted Docs

| Deleted path | Evidence | Replacement |
| --- | --- | --- |
| `docs/31-planning-roadmap/plans/plan.md` | 1997-line completed G0-G25 task board. Its active facts are now represented by current runtime/API docs, release docs, tests, and git history. | `docs/31-planning-roadmap/goals/status.md`, `docs/31-planning-roadmap/goals/backlog.md`, architecture/API/testing docs. |
| `docs/31-planning-roadmap/plans/plan2.md` | 2704-line completed G26-G70 task board with self-references and historical command/process detail. All G26-G70 work is complete. | `docs/31-planning-roadmap/goals/status.md`, `docs/43-ci-build-release-tools/current-baseline.md`, `docs/43-ci-build-release-tools/beta-readiness-review.md`, current feature docs. |
| `docs/31-planning-roadmap/topoexec-next-stage-plan.md` | 902-line older next-stage plan superseded by the completed G26-G70 rollup and current deferral list. | `docs/31-planning-roadmap/goals/backlog.md`, `docs/43-ci-build-release-tools/release-progression.md`. |

## Compressed Docs

| Path | Cleanup |
| --- | --- |
| `docs/31-planning-roadmap/goals/status.md` | Replaced the verbose per-goal transcript ledger with a compact live status, evidence map, completed rollup, and deferral list. |
| `docs/31-planning-roadmap/goals/backlog.md` | Replaced the completed G26-G70 task table with active selection rules and deferred backlog items. |
| `docs/43-ci-build-release-tools/current-baseline.md` | Removed copied per-goal command logs and kept release baseline, environment, gate, and maintenance rules. |
| `docs/43-ci-build-release-tools/release-progression.md` | Removed direct dependence on deleted plan files and kept release-stage decision guidance. |
| `docs/43-ci-build-release-tools/release-checklist.md` | Removed historical per-goal local evidence blocks and kept current release gates, artifact smoke, and limitation checklist. |

## Navigation Updates

- Agent guidance now points to the current goal backlog and status ledger
  instead of deleted plan files.
- Release docs point to current baseline, release checklist/runbook, beta
  review, and goal ledgers.
- Docs validation treats this migration record as part of the required
  documentation map.

## Rule Going Forward

Do not reintroduce large generated task boards or full command transcripts as
primary docs. Keep active state in the compact goal ledger, and keep detailed
evidence in CI logs, release-prep artifacts, or git history.
