# Contributing

TopoExec is a small C++20 in-process semantic execution graph runtime.
Contributions should preserve the core boundary: `topoexec::runtime` is
embeddable and must not depend on YAML, CLI tooling, ROS, Python,
OpenTelemetry, Prometheus, editor/LSP SDKs, dynamic plugin loading, or other
adapters.

## Project philosophy

- Runtime semantics are explicit: edge visibility, trigger readiness, channel
  capacity, scheduler lanes, CompositeLoop ownership, metrics, trace, and
  diagnostics must stay explainable.
- Prefer tests, docs, and deletion over new abstractions.
- Keep public APIs small and embeddable; add optional preview targets outside the
  runtime boundary when integration pressure exists.
- Treat observability as evidence, not hidden control flow.
- Release claims must describe implemented behavior, not roadmap intent.

## Non-goals

Do not present these as implemented unless a future goal explicitly adds and
verifies them:

- distributed runtime orchestration;
- production ROS 2 packages or executor integration;
- production OpenTelemetry/Prometheus exporters;
- native Python bindings beyond the CLI-backed preview;
- stable C ABI beyond ABI-version-0 preview;
- sandboxed/stable plugin ecosystem or graph-driven package discovery;
- schema v2 loader or migration CLI;
- full editor extension or LSP server;
- hard real-time scheduling, hard preemption, or OS affinity guarantees.

## Contribution lanes

Use the smallest lane that fits the change:

| Lane | Use when | Required surfaces |
| --- | --- | --- |
| Bug fix | Existing behavior contradicts docs/tests. | Regression test, changelog when user-visible, focused gate. |
| Runtime semantic change | Edge, trigger, channel, scheduler, loop, lifecycle, or payload meaning changes. | Design proposal issue, semantic docs, runtime tests, golden/schema review when applicable. |
| Public API change | Installed headers, CMake targets, CLI JSON, metrics, trace, diagnostics, schema, or package metadata changes. | `docs/61-api/api-change-checklist.md`, `docs/61-api/public-api.md`, versioning note, downstream smoke when embedder-facing. |
| Component/example | Adds in-process examples or reusable component patterns. | Example README, docs catalog update, app/CLI smoke, no adapter dependency. |
| Observability metric/trace/diagnostic | Adds or changes machine-readable telemetry. | Descriptor/registry update, docs, bounded-label review, golden or focused smoke. |
| Schema field | Adds or changes graph YAML/JSON shape. | `docs/33-specs-rfcs/schema-v2-notes.md` classification, schema docs/JSON, schema contract tests, migration note if breaking. |
| Adapter/FFI/editor preview | Adds integration surface outside runtime. | Default-off or dependency-free boundary, package/policy smoke, explicit non-goals. |

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Agent-friendly shortcuts:

```bash
./scripts/goal_check.sh all
./scripts/goal_check.sh quick
./scripts/goal_check.sh format
./scripts/goal_check.sh tidy
./scripts/goal_check.sh package
./scripts/goal_check.sh fuzz
./scripts/goal_check.sh sanitizer
```

Local pre-commit hooks mirror the same quality scripts:

```bash
pre-commit install
pre-commit install --hook-type commit-msg
pre-commit run --all-files
```

Always run `./scripts/agent_check.sh` before marking a repository change ready.
Use focused gates as additional evidence, not a substitute for the full gate
unless a blocker is documented.

## Commit messages

Commit messages must be written in English and use a Conventional Commit subject:

```text
<type>(optional-scope): concise intent
```

Allowed types are `build`, `chore`, `ci`, `docs`, `feat`, `fix`, `perf`,
`refactor`, `revert`, `style`, and `test`. Use lowercase scopes such as
`runtime`, `schema`, `docs`, `api`, or `packaging` when they clarify the review
surface.

The first line should describe why the change exists in a short, reviewable
form. When decision context matters, keep the Lore trailers from `AGENTS.md`
after the body, especially `Constraint:`, `Rejected:`, `Directive:`, `Tested:`,
and `Not-tested:`.

## How to propose a semantic change

Open a design proposal issue before changing runtime meaning. Include:

1. The current contract section (`docs/21-architecture/runtime-semantics.md`,
   `docs/21-architecture/semantic-contract.md`, `docs/33-specs-rfcs/schema-v1.md`, or a feature page).
2. A minimal graph or C++ builder example showing the problem.
3. The old behavior, proposed behavior, and compatibility impact.
4. Whether the change is additive, behavior-changing, or breaking.
5. Required tests and golden/schema updates.
6. Migration or deprecation notes when existing users could be affected.

Breaking semantic changes require a schema/semantic-contract decision before
implementation. Do not silently reinterpret existing graph files.

## How to add a component or example

1. Prefer an example app under `examples/apps/<name>` when the behavior is useful
   to embedders.
2. Link only the minimum target, ideally `topoexec_runtime` for pure C++ examples.
3. Add an app README with graph shape, run command, expected evidence, semantic
   lesson, and contrast/non-goal.
4. Add a CTest smoke and update `docs/11-user-guide/examples.md` / `examples/README.md`.
5. Do not pull adapter SDKs, ROS, Python, OpenTelemetry, Prometheus, or plugin
   loading into runtime examples.

## How to add a metric

1. Start from the metric purpose and cardinality risk.
2. Add or update `RuntimeMetricDescriptor` metadata in the descriptor registry.
3. Use bounded labels only; never add correlation ids, file paths, raw component
   configs, payload values, or unbounded user input as default labels.
4. Update `docs/62-schemas-protocols/metrics.md`, tests, and goldens when CLI output changes.
5. State whether the metric is stable-v0.2 or experimental in API/versioning docs
   if it becomes part of an embedder/exporter contract.

## How to add a schema field

1. Classify the field in `docs/33-specs-rfcs/schema-v2-notes.md` before editing the schema.
2. Keep it in schema v1 only if it is optional, has a documented default, is
   adapter-independent, and preserves existing graph meaning.
3. Update `schema/topoexec.schema.v1.json`, `docs/33-specs-rfcs/schema-v1.md`, examples, and
   `tests/schema/check_schema_contract.py` when v1 shape changes.
4. Add semantic validation tests when JSON Schema cannot express the rule.
5. Use schema v2 planning for runtime nesting, adapter-specific transport fields,
   graph-driven plugin/package discovery, arbitrary expressions, or behavior
   selection by version.

## How to change public API or CLI JSON

Use `docs/61-api/api-change-checklist.md`. Public API includes installed headers,
CMake target names, CLI JSON field names, schema fields, metric/trace/diagnostic
schema fields, and package metadata. Prefer additive fields. Document removals,
renames, or behavior changes in `CHANGELOG.md`, `docs/43-ci-build-release-tools/versioning.md`, and the
owning reference page.

## Governance

### Release cadence

- `main` may carry unreleased architecture-stabilization work.
- Human maintainers choose public tags using `docs/43-ci-build-release-tools/release-runbook.md` and
  `scripts/release_prepare.sh`.
- Public tags are immutable; mistakes should use fix-forward prerelease tags.
- Release notes must copy explicit deferrals and avoid adapter/ecosystem claims
  unless those targets are implemented and verified.

### API change review

- Stable-v0.2 surfaces need a compatibility note and focused regression tests.
- Experimental surfaces may change faster but still need changelog notes when a
  user could depend on them.
- Schema/semantic changes require `docs/33-specs-rfcs/schema-v2-notes.md` and
  `docs/21-architecture/semantic-contract.md` review before implementation.

### Adapter acceptance policy

Adapter-like changes must stay outside `topoexec::runtime` unless they are
purely generic runtime abstractions. An adapter PR must state:

- external dependency and license;
- CMake target boundary and default-on/default-off behavior;
- how runtime remains usable without the adapter;
- package/export smoke evidence;
- whether the adapter is a dependency-free preview or production integration.

Production adapters, network exporters, editor extensions, and plugin discovery
need separate goals and review. A docs-only design is not implementation
readiness.

## Issue and PR structure

Use the templates under `.github/ISSUE_TEMPLATE/` for bug reports, semantic
mismatches, adapter requests, performance issues, component/example proposals,
metric changes, schema changes, and broader design proposals.

All participation is covered by the root `CODE_OF_CONDUCT.md`.

Every PR should include:

- goal source or issue link;
- scope matrix for API/schema/runtime/metrics/CLI/docs/package surfaces;
- behavior and compatibility notes;
- validation evidence;
- known limitations and follow-up goals.

Agent-generated PRs use the same PR template as human PRs. They must include the
full validation evidence they ran and must not hide deferred scope.

## Commit policy

Use English Conventional Commit subjects and the Lore commit protocol from
`AGENTS.md`. Include what was tested and known gaps. Keep commits grouped by
goal or subsystem so each commit can be reviewed or reverted independently.
