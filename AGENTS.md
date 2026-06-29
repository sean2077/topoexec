# TopoExec Agent Guide

This repository follows the top-level Codex and OMX instructions from the active session. This file records the repo-local defaults for future agents.

## Scope

Work in this repository should keep TopoExec small, embeddable, C++20-first, semantic, testable, and observable. Prefer hardening runtime semantics, tests, packaging, and public API clarity over adding more CLI commands.

## Development Rules

- Keep changes small, reviewable, and behavior-preserving unless the task explicitly asks for a feature.
- Do not add ROS, Python, OpenTelemetry, or Prometheus adapters before the core/runtime/API boundary is stable.
- Do not add new runtime dependencies without a documented reason.
- Prefer tests over new features.
- When implementing roadmap work, start with the earliest unfinished P0/P1 goal in `docs/31-planning-roadmap/goals/backlog.md` unless the user narrows scope.
- Treat `docs/31-planning-roadmap/goals/status.md` as the current goal ledger. Update it when a goal starts, completes, is blocked, or is intentionally deferred.
- Each goal must have scope, allowed files, acceptance criteria, validation, and blocker handling before edits spread beyond documentation.
- If a goal needs a product/API decision, write a blocker note under `docs/31-planning-roadmap/goals/blockers/`, recommend one option, and continue only with a safe independent goal.
- Do not add adapters or new major CLI commands before the core/API/concurrency P0 goals are complete.
- For commits, use English Conventional Commit subjects and the Lore commit protocol required by the active agent instructions.

## Required Checks

Run `scripts/agent_check.sh` before declaring repo changes complete. If it cannot run in the environment, report the exact blocker and the closest checks that did run.
Use `scripts/goal_check.sh` for focused goal-specific checks, but do not treat it as a replacement for the required full gate unless a blocker is documented.

<!-- agent-scaffold:start — managed by the agent-scaffold skill. Edit project prose OUTSIDE these markers; `agent-scaffold upgrade` refreshes this block. -->
## Agent Harness (Claude Code + Codex)

This repo carries a vendored, dual-host agent harness. `.agents/` is the single
source of truth (SSOT); `.claude/` and `.codex/` are wired to the **same**
implementations under `tools/agent/`.

### Worktree-per-change (hard rule)

**Never edit trunk (`main`) directly** — every change, however small ("just docs"
is NOT an exception), starts in its own worktree cut from the trunk tip:

```bash
tools/agent/worktree.sh new <name>   # edit inside .worktrees/<name>/  (branch feat|fix|docs|chore/<name>)
tools/agent/worktree.sh done         # merge back to local trunk (--no-ff) + clean up + ff-only push
```

`tools/agent/hooks/trunk_edit_guard.sh` (PreToolUse) mechanically blocks edits to
tracked files while on trunk. Escape hatch — only when the user explicitly
authorizes a trunk edit: `touch .claude/allow-trunk-edit` (auto-expires in 2 h)
or `WORKTREE_ALLOW_TRUNK_EDIT=1`.

### Authority docs

`AGENTS.md` (root + every subdirectory; `CLAUDE.md` is a symlink) is an **entry
point**, not a detail dump. `tools/agent/hooks/authority_doc_budget.sh`
(PostToolUse) advises when a contract exceeds its line budget (root 320 / nested
120; override with `AUTHORITY_DOC_MAX_ROOT|NESTED`). Subdirectory `AGENTS.md`
files carry `<!-- Parent: ../AGENTS.md -->` and stay subordinate to the root.

### SSOT layout

| Path | Role | Commit? |
|---|---|---|
| `.agents/skills/<name>/SKILL.md` | project skill source | ✅ |
| `.agents/subagents/<name>/{metadata.json,instructions.md}` | subagent source | ✅ |
| `.claude/skills/<name>` | symlink → `.agents/skills/<name>` (CC discovery; Codex reads `.agents/` directly) | ✅ |
| `.claude/agents/*.md`, `.codex/agents/*.toml` | **generated** subagent projections — do NOT hand-edit | ✅ |
| `tools/agent/hooks/` | shared hook impls (trunk guard / doc budget / format) | ✅ |
| `tools/agent/worktree.sh` | worktree lifecycle | ✅ |
| `.claude/allow-trunk-edit`, `.claude/settings.local.json` | escape hatch / personal overrides | ❌ ignored |

- **Add a skill**: edit `.agents/skills/` → run `./.agents/relink-skills.sh` → commit source + symlink.
- **Add a subagent** (needs Node): edit `.agents/subagents/` → run `node tools/agent/generate-subagents.mjs` → commit source + generated. A pre-commit `--check` guards the two sides from drifting.
- **Third-party skills** install separately via `npx skills`; they land as real dirs in `.claude/skills/` and the relinker leaves them untouched.

**Codex trust**: project-level `.codex/` (config + hooks + agents) only loads for a
**trusted** project; until trusted it is silently skipped. Trust once: run `codex`
here and accept, or add `[projects."<repo abs path>"] trust_level = "trusted"` to
`~/.codex/config.toml`.
<!-- agent-scaffold:end -->
