# Feedback and Triage Workflow

Status: G80 external feedback loop for early adopters and maintainers. This is a
lightweight docs-and-template process, not a support SLA, not a production deployment claim,
and not package-registry publication.

## First-user path

Ask a first-time evaluator to attach where they stopped in this path:

```text
README -> clone/download -> build -> install -> find_package -> run curated example -> inspect docs -> report issue
```

The local maintainer reproduction path is:

```bash
./scripts/goal_check.sh package
./scripts/goal_check.sh examples
./scripts/goal_check.sh showcase
./scripts/goal_check.sh adoption
```

## Debug pack

For bug, packaging, compatibility, performance, and semantic mismatch reports,
ask for the smallest useful debug pack:

```text
- TopoExec commit or tag
- OS, compiler, CMake, and build type
- exact command line
- minimal graph or C++ snippet
- stdout/stderr or JSON output
- focused gate tried, if any
- whether the issue affects runtime-only, YAML/CLI, examples, package, or docs
```

Prefer command output from existing local gates over screenshots. For live
observe, attach NDJSON or record bundles. For benchmarks, attach JSON output and
machine context; do not compare timings across unrelated machines.

## Label taxonomy

Use a small label set so issues can be triaged without guessing ownership:

| Label | Meaning | First response |
| --- | --- | --- |
| `bug` | Runtime, CLI, packaging, docs, or test defect. | Ask for debug pack and reproduction command. |
| `adoption` | First-user onboarding, install, example, README, or docs friction. | Map failure to first-user path stage. |
| `compatibility` | Stable-v0.2 API/schema/CLI JSON/package compatibility concern. | Ask whether `./scripts/goal_check.sh compat` reproduces. |
| `performance` | Benchmark, latency, throughput, allocation, or soak concern. | Ask for benchmark JSON and machine context. |
| `semantics` | Runtime behavior differs from documented graph semantics. | Ask for minimal graph and contract reference. |
| `schema` | Schema v1/v2 field or validation change. | Apply schema-v2 notes classification. |
| `adapter` | ROS/OTel/Prometheus/Python/plugin/editor boundary request. | Confirm it remains outside `topoexec::runtime`. |
| `release` | Tag, artifact, docs site, or package release question. | Require human release-owner authority. |

## Triage states

```text
new -> needs-repro -> confirmed -> scoped -> in-progress -> fixed/declined/deferred
```

Deferral is acceptable when a request needs production adapters, native Python,
package registry publication, schema v2, editor/LSP, hard real-time, or other
explicitly deferred scope. Record the blocker or decision note instead of
silently expanding scope.

## Response checklist

- [ ] Identify the first-user path stage or core contract affected.
- [ ] Confirm whether the report is stable-v0.2, preview, docs-only, or deferred scope.
- [ ] Request the debug pack only if missing.
- [ ] Reproduce with the narrowest focused gate.
- [ ] Link the relevant docs page or roadmap blocker.
- [ ] Avoid promising production support, registry publication, or hard real-time behavior.
