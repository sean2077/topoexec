# TopoExec Agent Guide

This repository follows the top-level Codex and OMX instructions from the active session. This file records the repo-local defaults for future agents.

## Scope

Work in this repository should keep TopoExec small, embeddable, C++20-first, semantic, testable, and observable. Prefer hardening runtime semantics, tests, packaging, and public API clarity over adding more CLI commands.

## Development Rules

- Keep changes small, reviewable, and behavior-preserving unless the task explicitly asks for a feature.
- Do not add ROS, Python, OpenTelemetry, or Prometheus adapters before the core/runtime/API boundary is stable.
- Do not add new runtime dependencies without a documented reason.
- Prefer tests over new features.
- When implementing plan work from `docs/topoexec_plan.md`, start with the earliest unfinished milestone unless the user narrows scope.
- For commits, use the Lore commit protocol required by the active agent instructions.

## Required Checks

Run `scripts/agent_check.sh` before declaring repo changes complete. If it cannot run in the environment, report the exact blocker and the closest checks that did run.
