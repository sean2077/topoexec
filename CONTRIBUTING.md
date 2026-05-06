# Contributing to TopoExec

Start with the full contributor guide in [`docs/contributing.md`](docs/contributing.md).
This root file exists so GitHub can surface the same rules for human and
agent-generated pull requests.

Minimum expectations:

1. Keep `topoexec::runtime` small and dependency-free.
2. Use the issue and design-proposal templates for semantic/API/schema changes.
3. Update docs, tests, goal ledgers, and `CHANGELOG.md` for user-visible changes.
4. Run `./scripts/agent_check.sh` before marking a PR ready, plus focused gates
   for touched surfaces.
5. Fill out `.github/PULL_REQUEST_TEMPLATE.md`; agent-generated PRs use the same
   structure as human PRs.
