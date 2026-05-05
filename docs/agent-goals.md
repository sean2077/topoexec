# Agent Goals

This file is the human-readable entry point for continuing `docs/plans/plan.md` work.

## Current queue

Use `docs/goals/backlog.md` as the ordered source of truth and `docs/goals/status.md` as the current ledger.

Current safe order:

All goals currently tracked in `docs/goals/backlog.md` are complete. Start a new
plan, explicit release-tagging task, or user-scoped follow-up before changing
runtime or adapter behavior.

## Goal handoff template

```md
Goal ID:
Title:

Scope:
- Allowed files:
- Do not modify:

Acceptance criteria:
- ...

Validation:
- ./scripts/goal_check.sh all
- optional focused check: ./scripts/goal_check.sh <mode>

Blocker protocol:
- If a product/API decision is required, write docs/goals/blockers/<goal-id>.md with options, recommendation, and safe next task.
```

## Validation shortcuts

- `./scripts/goal_check.sh quick` — fast build plus golden/schema drift checks.
- `./scripts/goal_check.sh golden` — normalized CLI plan/metrics/trace/render golden checks.
- `./scripts/goal_check.sh schema` — schema v1 contract and CLI schema/semantic validation split.
- `./scripts/goal_check.sh package` — install/export downstream runtime-only smoke plus runtime-only option smoke.
- `./scripts/goal_check.sh docs` — executable docs command smoke.
- `./scripts/goal_check.sh fuzz` — deterministic parser/compiler fuzz smoke.
- `./scripts/goal_check.sh policy` — architecture/dependency policy smokes.
- `./scripts/goal_check.sh sanitizer` — ASAN+UBSAN Debug build plus full CTest.
- `./scripts/goal_check.sh debug` — local Debug GCC build and CTest.

Always run `./scripts/goal_check.sh all` before declaring repository changes complete.
