# Documentation System

The canonical documentation root is `docs/`. This project uses numbered zone directories because it has multiple reader audiences, active maintenance workflows, architecture history, testing/release evidence, and preview integration boundaries.

## Zone Rules

- `0x`: first-contact and quickstart paths.
- `1x`: user, graph-author, and integrator guidance.
- `2x`: developer, architecture, codebase, and test strategy.
- `3x`: planning, roadmap, goal ledgers, and specs/RFC-like notes.
- `4x`: tools, CI/build/release tooling, coding standards, and documentation standards.
- `6x`: reference surfaces such as API and output schemas.
- `9x`: reserved for durable archives or deprecated material only when a real
  reader need exists. Do not add migration logs or temporary execution records as
  primary docs.

Root `README.md` and `docs/README.md` must stay navigation-focused. Detailed setup, architecture, API, release, and planning material belongs in the relevant zone.

## Link Policy

Prefer relative links inside the docs tree. Update `tests/docs/check_docs.py` when a required navigation page moves. Keep preview/deferred wording explicit on adapter, Python, plugin, C API, schema v2, and editor pages.

## Ledger Policy

Planning ledgers under `docs/31-planning-roadmap/goals/` are current-state
surfaces. They may record scope, outcome, blocker locations, and validation gate
names, but must not carry milestone command tables, generated task boards, full
command transcripts, or local OMX evidence paths. Keep detailed evidence in CI
artifacts, release-prep bundles, or git history.

Migration records and old-to-new move logs should not be exposed as a docs zone.
When documentation is reorganized, update live links and tests, keep any durable
rule here, and rely on git history for deleted-path archaeology.

## Maintenance Checks

- `./scripts/goal_check.sh docs` validates docs command markers and required docs map entries.
- `./scripts/agent_check.sh` is the required full gate before declaring repository changes complete.
- `git diff --check` catches whitespace issues in docs and code.
