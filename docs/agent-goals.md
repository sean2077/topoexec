# Agent Goals

This file is the human-readable entry point for continuing `docs/plans/plan.md` work.

## Current queue

Use `docs/goals/backlog.md` as the ordered source of truth and `docs/goals/status.md` as the current ledger.

Current safe order:

1. G2 structured status/error propagation;
2. G3 runtime invariant suite;
3. G4 graph compiler diagnostics;
4. G6 scheduler lane completeness;
5. G8 channel/backpressure completeness;
6. G10 trigger engine completeness;
7. G11 CompositeLoop/region completeness;
8. remaining P1/P2 docs, packaging, examples, and adapter preview work.

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
- `./scripts/goal_check.sh package` — install/export downstream runtime-only smoke.
- `./scripts/goal_check.sh debug` — local Debug GCC build and CTest.

Always run `./scripts/goal_check.sh all` before declaring repository changes complete.
