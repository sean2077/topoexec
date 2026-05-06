# Agent Goals

This file is the human-readable entry point for continuing `docs/plans/plan2.md` work.

## Current queue

Use `docs/goals/backlog.md` as the ordered source of truth and `docs/goals/status.md` as the current ledger.

Current safe order after the completed P0/P1 queue plus G35/G41/G42:

1. G45 CompositeLoop Solver-Style Policies.
2. G58-G65 adapter/interface/ecosystem preview goals only after the core/API
   boundary remains clean for the chosen slice.
3. G68 Community and Contribution Readiness.

Continue by backlog order in `docs/goals/backlog.md`, keeping concrete adapter
implementations deferred unless the user explicitly opens that scope.

## Goal handoff template

```md
Goal ID:
Title:

Plan source:
- docs/plans/plan2.md
- docs/goals/backlog.md
- docs/goals/status.md

Scope:
- Allowed files:
- Do not modify:

Acceptance criteria:
- ...

Validation:
- ./scripts/agent_check.sh
- optional focused check: ./scripts/goal_check.sh <mode>

Blocker protocol:
- If a product/API decision is required, write docs/goals/blockers/<goal-id>.md with options, recommendation, and safe next task.
```

## Validation shortcuts

- `./scripts/goal_check.sh quick` — fast build plus golden/schema drift checks.
- `./scripts/goal_check.sh golden` — normalized CLI plan/metrics/trace/Chrome-trace/render/schema/doctor golden checks.
- `./scripts/goal_check.sh schema` — schema v1 contract and CLI schema/semantic validation split.
- `./scripts/goal_check.sh package` — install/export downstream runtime-only smoke plus runtime-only option smoke.
- `./scripts/goal_check.sh docs` — executable docs command smoke.
- `./scripts/goal_check.sh fuzz` — deterministic parser/compiler fuzz smoke.
- `./scripts/goal_check.sh policy` — architecture/dependency policy smokes.
- `./scripts/goal_check.sh sanitizer` — ASAN+UBSAN Debug build plus full CTest.
- `./scripts/goal_check.sh format` — clang-format target.
- `./scripts/goal_check.sh debug` — local Debug GCC build and CTest.

Always run `./scripts/agent_check.sh` before declaring repository changes complete. Use focused goal checks as additional evidence, not as a replacement for the full gate unless a blocker is documented.
