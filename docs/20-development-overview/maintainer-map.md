# Maintainer Map

Use this page when changing TopoExec rather than only consuming it.

## Local Development Flow

1. Read [Codebase map](../22-codebase/codebase-map.md) for source ownership.
2. Check the relevant semantic or API page before editing runtime behavior.
3. Add or update focused tests for changed behavior.
4. Run the focused goal check when available.
5. Run `scripts/agent_check.sh` before declaring repository changes complete.

## Common Change Routes

- Runtime semantics: [Runtime semantics](../21-architecture/runtime-semantics.md), [Semantic contract](../21-architecture/semantic-contract.md), [Runtime invariant coverage](../21-architecture/runtime-invariants.md).
- Public API: [API overview](../61-api/api-overview.md), [Public API stability](../61-api/public-api.md), [API change checklist](../61-api/api-change-checklist.md).
- Graph schema: [Schema v1](../33-specs-rfcs/schema-v1.md), [Schema v2 notes](../33-specs-rfcs/schema-v2-notes.md).
- CLI or editor tooling: [CLI](../41-development-tools/cli.md), [Editor and schema UX](../41-development-tools/editor-schema.md).
- Testing and release: [Testing strategy](../24-testing/testing-strategy.md), [Build and package](../43-ci-build-release-tools/build-and-package.md), [Release runbook](../43-ci-build-release-tools/release-runbook.md).
- Planning history: [Goal backlog](../31-planning-roadmap/goals/backlog.md), [Goal status](../31-planning-roadmap/goals/status.md).

## Required Boundary

Keep the runtime target small and embeddable. Adapter previews, C API, Python automation, plugin loading, YAML loading, and CLI tooling must remain optional surfaces outside the core runtime dependency boundary.
