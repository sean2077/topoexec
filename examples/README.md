# TopoExec Examples

This directory contains the public example gallery for TopoExec. The G74
showcase path is generated from curated `example.json` metadata, while the
legacy top-level YAML files remain in place as compatibility fixtures for
existing tests, docs, and golden outputs.

## Recommended learning path

1. [`00-getting-started/`](00-getting-started/) — Small source-transform-sink graph for the first validate, plan, render, and run commands.
2. [`10-basic-dataflow/`](10-basic-dataflow/) — A non-linear source -> preprocess -> branch_a/branch_b -> merge -> sink graph.
3. [`20-triggers/`](20-triggers/) — One graph showing any_input, all_inputs, time_sync, batch, rate_limit, debounce, and condition trigger policies.
4. [`30-async/`](30-async/) — Request validation followed by an async worker edge with max_inflight=2 and bounded queue capacity.
5. [`40-composite-loop/`](40-composite-loop/) — A declared estimator/controller feedback SCC with fixed-point policy, convergence label, and iteration budget.
6. [`50-observability/`](50-observability/) — Minimal graph run that produces metrics JSON, structured trace JSON, Chrome trace, and live observe NDJSON.
7. [`60-testing-validation/`](60-testing-validation/) — A deterministic valid graph plus an invalid fixture for showing clear diagnostics and CI-friendly golden checks.
8. [`70-performance/`](70-performance/) — Small graph bench command for local performance awareness without cross-machine timing claims.
9. [`80-realistic-mini-scenario/`](80-realistic-mini-scenario/) — A small robot-cell-inspired event/data pipeline with time sync, detector stage, async planner, and output boundary.
10. [`90-dogfood-pilot/`](90-dogfood-pilot/) — A deterministic robot-cell-inspired pilot that composes time sync, async bounded inflight, delay feedback, state audit, metrics, trace, and live assertions.

## Curated example matrix

| Directory | Metadata id | Category | Purpose |
| --- | --- | --- | --- |
| [`00-getting-started/`](00-getting-started/) | `minimal-pipeline` | Getting Started | Small source-transform-sink graph for the first validate, plan, render, and run commands. |
| [`10-basic-dataflow/`](10-basic-dataflow/) | `branching-fanout-join` | Basic Dataflow | A non-linear source -> preprocess -> branch_a/branch_b -> merge -> sink graph. |
| [`20-triggers/`](20-triggers/) | `trigger-semantics` | Execution Semantics | One graph showing any_input, all_inputs, time_sync, batch, rate_limit, debounce, and condition trigger policies. |
| [`30-async/`](30-async/) | `async-bounded-inflight` | Execution Semantics | Request validation followed by an async worker edge with max_inflight=2 and bounded queue capacity. |
| [`40-composite-loop/`](40-composite-loop/) | `composite-loop-solver` | Advanced Semantics | A declared estimator/controller feedback SCC with fixed-point policy, convergence label, and iteration budget. |
| [`50-observability/`](50-observability/) | `metrics-trace-observe` | Observability | Minimal graph run that produces metrics JSON, structured trace JSON, Chrome trace, and live observe NDJSON. |
| [`60-testing-validation/`](60-testing-validation/) | `validation-and-golden` | Testing and Validation | A deterministic valid graph plus an invalid fixture for showing clear diagnostics and CI-friendly golden checks. |
| [`70-performance/`](70-performance/) | `minimal-benchmark` | Performance | Small graph bench command for local performance awareness without cross-machine timing claims. |
| [`80-realistic-mini-scenario/`](80-realistic-mini-scenario/) | `sensor-fusion-mini` | Realistic Scenario | A small robot-cell-inspired event/data pipeline with time sync, detector stage, async planner, and output boundary. |
| [`90-dogfood-pilot/`](90-dogfood-pilot/) | `dogfood-robot-cell` | Dogfood Pilot | A deterministic robot-cell-inspired pilot that composes time sync, async bounded inflight, delay feedback, state audit, metrics, trace, and live assertions. |

## Metadata convention

Each curated example directory uses a dependency-free `example.json` file that
is validated by `examples/metadata.schema.json` and consumed by showcase tooling.
The metadata declares:

- stable example id, category, title, summary, difficulty, and tags;
- the primary graph file;
- commands to validate, render, run, observe, or benchmark the example;
- generated asset requirements;
- focused CI smoke expectations;
- related documentation links.

A reusable README template lives at `_templates/example-readme.md`. Individual
example READMEs follow these sections: What this example demonstrates, Graph
structure, How to run, Expected result, What to inspect, and Related docs.

## Regenerate index and assets

```bash
python3 scripts/update_examples_index.py
python3 scripts/render_example_assets.py --topoexec build/topoexec
```

Use `--check` on either script in CI to fail when generated content is stale.

## Legacy YAML and C++ apps

The existing top-level YAML graphs and `apps/` C++ examples are still supported
and continue to back tests and deeper docs. G74 showcase pages should link to
the curated directories first, then to legacy fixtures where they explain an
advanced or compatibility-specific contract.

## Quick smoke

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure -R 'app_|cli_run_|cli_validate_'
```
