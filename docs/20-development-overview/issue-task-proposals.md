# Issue Task Proposals (May 10, 2026)

## 1) Typo fix task
- **Issue found:** The Python package docstring calls itself an "automation preview," while nearby docs consistently use "Python preview client" terminology; this wording mismatch reads like a naming typo in package-level text and is easy to misread as a different feature surface.
- **Proposed task:** Normalize the package docstring wording in `python/topoexec_preview/__init__.py` to match "Python preview client" terminology used elsewhere.
- **Why now:** Small, low-risk clarity win that reduces terminology drift.

## 2) Bug fix task
- **Issue found:** `TopoExecClient._json_command(..., check=True)` only raises on non-zero process exit. If the CLI returns exit code 0 with JSON `{ "ok": false }`, callers silently get a non-exception path even though `CommandResult.ok` is false.
- **Proposed task:** Treat `check=True` as "fail on transport OR semantic failure" by raising `TopoExecCommandError` when either `returncode != 0` **or** parsed payload includes `ok: false`.
- **Why now:** Prevents false-success automation in CI scripts that rely on exception flow.

## 3) Comment/documentation discrepancy task
- **Issue found:** `materialized_graph()` accepts any non-empty suffix and only auto-appends `.yaml` when no suffix is present, while `GraphDocument.from_text()` implies YAML-focused behavior by default naming and package messaging. This can confuse users into thinking only YAML suffixes are valid/expected end-to-end.
- **Proposed task:** Clarify docs/comments to explicitly state accepted behavior: path-like inputs are passed through and only suffixless temporary names are normalized to `.yaml`.
- **Why now:** Prevents ambiguity for automation users who materialize temporary graph files.

## 4) Test improvement task
- **Issue found:** There is no focused unit test covering the semantic-failure path where CLI exits 0 but returns JSON `{ "ok": false }`.
- **Proposed task:** Add a Python preview unit test that mocks/subprocess-stubs this scenario and asserts that `check=True` raises once bug task #2 is implemented, while `check=False` still returns `CommandResult`.
- **Why now:** Locks in intended behavior and prevents regression in client-side automation contracts.
