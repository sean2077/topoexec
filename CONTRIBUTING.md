# Contributing to TopoExec

Start with the full contributor guide in [`docs/44-coding-standards/contributing.md`](docs/44-coding-standards/contributing.md).
This root file exists so GitHub can surface the same rules for human and
agent-generated pull requests.

Minimum expectations:

1. Keep `topoexec::runtime` small and dependency-free.
2. Use the issue and design-proposal templates for semantic/API/schema changes.
3. Update docs, tests, goal ledgers, and `CHANGELOG.md` for user-visible changes.
4. Write commit messages in English with Conventional Commit subjects.
5. Run `./scripts/agent_check.sh` before marking a PR ready, plus focused gates
   for touched surfaces.
6. Fill out `.github/PULL_REQUEST_TEMPLATE.md`; agent-generated PRs use the same
   structure as human PRs.
