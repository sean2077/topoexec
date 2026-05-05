# Goal Execution Protocol

TopoExec plan work is executed as small, reviewable goals. Use this directory with `docs/plan3.md` when the user asks for architecture-plan implementation and does not name a narrower goal.

## Selection Rule

1. Start from the earliest unfinished P0 goal in `docs/goals/backlog.md`.
2. If the user names a specific goal, follow that goal instead.
3. Prefer tests and docs that harden existing runtime behavior before adding new surface area.
4. Do not add adapters or new major CLI commands until the core/API/concurrency P0 goals are complete.

## Goal Record Format

Each goal entry should include:

- `ID`: stable goal id.
- `Priority`: P0, P1, P2, or P3.
- `Status`: not-started, in-progress, blocked, complete, or deferred.
- `Scope`: files or modules allowed for normal edits.
- `Acceptance`: concrete behavior, docs, tests, or evidence required.
- `Validation`: exact commands or checks to run.
- `Blocker protocol`: where to record decisions that cannot be made locally.

## Editing Rules

- Stay inside the goal scope unless a test exposes a bug in directly related runtime code.
- Keep runtime behavior and documentation aligned in the same goal.
- Add positive and negative tests for user-visible runtime semantics.
- Keep all queues, async paths, and worker paths bounded.
- Do not introduce new runtime dependencies without a documented reason.

## Blockers

If a goal requires a product or API decision, create:

```text
docs/goals/blockers/<goal-id>.md
```

The blocker note must include:

- decision needed;
- options considered;
- recommended option;
- impact on public API, runtime semantics, tests, and docs;
- safe independent goal to continue, if one exists.

## Completion

A goal is complete only after:

- all acceptance criteria are mapped to concrete evidence;
- the validation commands have passed, or exact blockers are documented;
- `docs/goals/status.md` is updated;
- `scripts/agent_check.sh` passes before declaring repository changes complete.
