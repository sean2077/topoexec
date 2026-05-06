# Documentation System

The canonical documentation root is `docs/`. This project uses numbered zone directories because it has multiple reader audiences, active maintenance workflows, architecture history, testing/release evidence, and preview integration boundaries.

## Zone Rules

- `0x`: first-contact and quickstart paths.
- `1x`: user, graph-author, and integrator guidance.
- `2x`: developer, architecture, codebase, and test strategy.
- `3x`: planning, roadmap, goal ledgers, and specs/RFC-like notes.
- `4x`: tools, CI/build/release tooling, coding standards, and documentation standards.
- `6x`: reference surfaces such as API and output schemas.
- `9x`: documentation-system records, migrations, archives, and deprecated material.

Root `README.md` and `docs/README.md` must stay navigation-focused. Detailed setup, architecture, API, release, and planning material belongs in the relevant zone.

## Link Policy

Prefer relative links inside the docs tree. Update `tests/docs/check_docs.py` when a required navigation page moves. Keep preview/deferred wording explicit on adapter, Python, plugin, C API, schema v2, and editor pages.

## Maintenance Checks

- `./scripts/goal_check.sh docs` validates docs command markers and required docs map entries.
- `./scripts/agent_check.sh` is the required full gate before declaring repository changes complete.
- `git diff --check` catches whitespace issues in docs and code.

## Migration Record

The 2026-05 reorganization map is recorded in [2026 documentation reorganization](../94-doc-migrations/2026-05-doc-reorganization.md). A follow-up cleanup deleted completed process artifacts and compressed long ledgers; see [2026 process-ledger cleanup](../94-doc-migrations/2026-05-process-ledger-cleanup.md).
