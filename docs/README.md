# TopoExec Documentation

This directory is the canonical documentation root. The project is complex enough to use numbered zones: first contact and user learning paths come first, developer and architecture material live in `2x`, planning and decision history in `3x`, tools and standards in `4x`, and reference material in `6x`.

## Reader Paths

- New user: [project map](00-start-here/project-map.md) -> [getting started](01-quickstart/getting-started.md) -> [examples guide](11-user-guide/examples.md) -> [cookbook](11-user-guide/cookbook.md).
- Embedder: [API overview](61-api/api-overview.md) -> [public API stability](61-api/public-api.md) -> [runtime architecture](21-architecture/runtime-architecture.md).
- Maintainer: [maintainer map](20-development-overview/maintainer-map.md) -> [codebase map](22-codebase/codebase-map.md) -> [testing strategy](24-testing/testing-strategy.md) -> [live observe performance](24-testing/live-observe-performance.md).
- Planner or release owner: [goal backlog](31-planning-roadmap/goals/backlog.md) -> [goal status](31-planning-roadmap/goals/status.md) -> [release and adoption readiness](43-ci-build-release-tools/adoption-readiness.md) -> [release runbook](43-ci-build-release-tools/release-runbook.md) -> [beta readiness review](43-ci-build-release-tools/beta-readiness-review.md) -> [v1 readiness program](43-ci-build-release-tools/v1-readiness-program.md).

## Zones

| Zone | Purpose | Start here |
| --- | --- | --- |
| `00-start-here` | First-contact map and orientation. | [Project map](00-start-here/project-map.md) |
| `01-quickstart` | Fastest safe local build and first execution. | [Getting started](01-quickstart/getting-started.md) |
| `10-user-overview` | Product vocabulary, FAQ, and comparison context. | [Concepts](10-user-overview/concepts.md) |
| `11-user-guide` | Stable usage guides for graph authors and example readers. | [Cookbook](11-user-guide/cookbook.md) |
| `12-integrations` | Adapter, FFI, Python, and plugin preview boundaries. | [Adapter boundaries](12-integrations/adapter-boundaries.md) |
| `20-development-overview` | Maintainer entry point and development map. | [Maintainer map](20-development-overview/maintainer-map.md) |
| `21-architecture` | Stable architecture, runtime semantics, and boundaries. | [Runtime architecture](21-architecture/runtime-architecture.md) |
| `22-codebase` | Source tree and ownership map. | [Codebase map](22-codebase/codebase-map.md) |
| `24-testing` | Test strategy, fuzz/stress/bench evidence, and defensive input checks. | [Testing strategy](24-testing/testing-strategy.md) / [Reliability program](24-testing/reliability-program.md) |
| `31-planning-roadmap` | Current backlog, goal status, active blockers, and roadmap records. | [Goal backlog](31-planning-roadmap/goals/backlog.md) / [Ecosystem decision gate](31-planning-roadmap/ecosystem-decision-gate.md) / [Competitive landscape](31-planning-roadmap/competitive-landscape.md) / [Conditional tracks ledger](31-planning-roadmap/conditional-tracks-ledger.md) |
| `33-specs-rfcs` | Schema and proposal-like design notes. | [Schema v1](33-specs-rfcs/schema-v1.md) |
| `41-development-tools` | CLI, quality gates, editor/schema tooling, live validation, examples/showcase workflow, and agent helpers. | [Quality gates](41-development-tools/quality-gates.md) / [CLI](41-development-tools/cli.md) / [Examples and showcase](41-development-tools/examples-and-showcase.md) / [Live runtime validation](41-development-tools/live-runtime-validation.md) |
| `43-ci-build-release-tools` | Build, package, release, versioning, adoption readiness, and baseline evidence. | [Build and package](43-ci-build-release-tools/build-and-package.md) / [Package matrix](43-ci-build-release-tools/package-matrix.md) / [Release and adoption readiness](43-ci-build-release-tools/adoption-readiness.md) / [Core runtime beta candidate](43-ci-build-release-tools/core-runtime-beta-candidate.md) / [v1 readiness program](43-ci-build-release-tools/v1-readiness-program.md) |
| `44-coding-standards` | Contribution and coding process standards. | [Contributing](44-coding-standards/contributing.md) / [Feedback and triage](44-coding-standards/feedback-and-triage.md) |
| `45-doc-standards` | Documentation conventions, Pages publishing, and maintenance rules. | [Documentation system](45-doc-standards/documentation-system.md) / [GitHub Pages](45-doc-standards/github-pages.md) |
| `61-api` | Public C++/C API, compatibility, and generated reference entry points. | [API overview](61-api/api-overview.md) / [Doxygen](61-api/doxygen.md) |
| `62-schemas-protocols` | Metrics, trace, diagnostic, and live observe output schemas. | [Metrics](62-schemas-protocols/metrics.md) / [Live observe events](62-schemas-protocols/live-observe-events.md) |

## Stability Notes

Stable guidance describes current runtime, API, schema, test, and release behavior. Planning docs under `31-planning-roadmap` are compact status surfaces and should not duplicate implementation docs, CI logs, or release artifacts.

Adapter, C API, Python, editor, plugin, live dashboard, and README showcase docs are explicit
preview, local-tooling, or boundary surfaces unless their page says otherwise. Live observe is
an observability/test-validation path, not a runtime control path. Do not infer
production ROS 2, production OpenTelemetry/Prometheus, native Python bindings,
stable ABI, schema v2 implementation, migration CLI, remote dashboard access,
pause/resume/step/fault injection, or sandbox support from preview docs.

## Docs Validation

The `docs_command_smoke` CTest runs selected commands embedded as `topoexec-doc-test` markers across this tree. It also checks that required navigation pages and section contracts remain present after moves.

The optional Pages build uses MkDocs plus Doxygen through
`scripts/docs_build_site.sh`. It publishes the Markdown site and copies
generated API HTML under `/api/`; normal runtime builds do not require docs
tooling.
