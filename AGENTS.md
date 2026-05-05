# TopoExec Agent Guide

This repository follows the top-level Codex and OMX instructions from the active session. This file records the repo-local defaults for future agents.

## Scope

Work in this repository should keep TopoExec small, embeddable, C++20-first, semantic, testable, and observable. Prefer hardening runtime semantics, tests, packaging, and public API clarity over adding more CLI commands.

## Development Rules

- Keep changes small, reviewable, and behavior-preserving unless the task explicitly asks for a feature.
- Do not add ROS, Python, OpenTelemetry, or Prometheus adapters before the core/runtime/API boundary is stable.
- Do not add new runtime dependencies without a documented reason.
- Prefer tests over new features.
- When implementing plan work from `docs/plans/plan2.md`, `docs/plans/plan.md`, or `docs/topoexec_plan.md`, start with the earliest unfinished P0/P1 goal in `docs/goals/backlog.md` unless the user narrows scope.
- Treat `docs/goals/status.md` as the current goal ledger. Update it when a goal starts, completes, is blocked, or is intentionally deferred.
- Each goal must have scope, allowed files, acceptance criteria, validation, and blocker handling before edits spread beyond documentation.
- If a goal needs a product/API decision, write a blocker note under `docs/goals/blockers/`, recommend one option, and continue only with a safe independent goal.
- Do not add adapters or new major CLI commands before the core/API/concurrency P0 goals are complete.
- For commits, use the Lore commit protocol required by the active agent instructions.

## Required Checks

Run `scripts/agent_check.sh` before declaring repo changes complete. If it cannot run in the environment, report the exact blocker and the closest checks that did run.
Use `scripts/goal_check.sh` for focused goal-specific checks, but do not treat it as a replacement for the required full gate unless a blocker is documented.
