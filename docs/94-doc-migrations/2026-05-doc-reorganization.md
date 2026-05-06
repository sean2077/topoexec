# 2026 Documentation Reorganization

Date: 2026-05-06

## Reason

The project had accumulated many flat `docs/*.md` files after runtime, packaging, adapter-preview, testing, release, and goal-ledger work. Readers needed a stable architecture/lifecycle map, and maintainers needed planning, tooling, release, and reference material separated by lifecycle.

## Result

The documentation root remains `docs/`. Flat pages were moved into numbered zones:

- `0x`: first-contact and quickstart.
- `1x`: user guide and integration preview guidance.
- `2x`: development, architecture, codebase, and testing.
- `3x`: planning and specs.
- `4x`: development tools, CI/build/release tools, coding standards, and docs standards.
- `6x`: API and output-schema reference.
- `9x`: documentation system migrations.

## Deleted Content

No documentation content was deleted. The old `docs/adapters/` directory became empty after moving its files under `docs/12-integrations/adapters/` and was removed as an empty directory only.

A follow-up cleanup later deleted completed process-ledger artifacts; see
[`2026-05-process-ledger-cleanup.md`](2026-05-process-ledger-cleanup.md).

## Notable Moves

- `docs/getting-started.md` -> `docs/01-quickstart/getting-started.md`
- `docs/concepts.md` -> `docs/10-user-overview/concepts.md`
- `docs/examples.md` -> `docs/11-user-guide/examples.md`
- `docs/adapters.md` -> `docs/12-integrations/adapter-boundaries.md`
- `docs/runtime-semantics.md` -> `docs/21-architecture/runtime-semantics.md`
- `docs/testing-strategy.md` -> `docs/24-testing/testing-strategy.md`
- `docs/plans/` -> `docs/31-planning-roadmap/plans/`
- `docs/goals/` -> `docs/31-planning-roadmap/goals/`
- `docs/schema-v1.md` -> `docs/33-specs-rfcs/schema-v1.md`
- `docs/cli.md` -> `docs/41-development-tools/cli.md`
- `docs/release-runbook.md` -> `docs/43-ci-build-release-tools/release-runbook.md`
- `docs/contributing.md` -> `docs/44-coding-standards/contributing.md`
- `docs/api-overview.md` -> `docs/61-api/api-overview.md`
- `docs/metrics.md` -> `docs/62-schemas-protocols/metrics.md`

## Updated Surfaces

The root README, docs index, repo agent guide, GitHub templates, release script, docs smoke, and community-readiness smoke were updated to point at the new paths.
