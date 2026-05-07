# Examples and Showcase Workflow

G74 turns `examples/` and the root `README.md` into a short, runnable showcase
for TopoExec. The goal is to make the project easy to understand and try without
claiming production adoption or adding cosmetic runtime behavior.

## Scope and non-goals

In scope:

- curated examples under `examples/[0-9][0-9]-*/`;
- per-example `example.json` metadata and README pages;
- generated graph, metrics, trace, and README showcase assets under
  `docs/assets/generated/`;
- focused anti-rot gates for examples and README assets.

Out of scope for this workflow:

- a full GUI editor or production dashboard;
- production ROS, OpenTelemetry, Prometheus, Python, or Perfetto adapters;
- runtime semantics added solely to make a diagram look better;
- hand-maintained README screenshots that cannot be regenerated or checked;
- claims that the project is production-proven.

## Curated examples

Start at the repository-root `examples/README.md`. The generated learning
path covers:

1. minimal getting-started pipeline;
2. branching / fan-out / join;
3. trigger policies;
4. async worker with bounded inflight;
5. CompositeLoop fixed-point solver;
6. metrics, trace, Chrome trace, and live observe;
7. validation diagnostics and golden-friendly output;
8. local benchmark smoke;
9. a synthetic robot-cell-inspired mini scenario.

Legacy top-level YAML files and `examples/apps/` remain supported because tests
and deeper docs still use them. Public README links should prefer the curated
example directories unless a legacy fixture is the specific semantic reference.

## Metadata contract

Each curated example has a dependency-free `example.json` file. The schema lives
at repository-root `examples/metadata.schema.json`.
Metadata records:

- stable example id, category, title, summary, difficulty, and tags;
- primary graph file;
- commands to validate, render, run, observe, trace, or benchmark;
- expected command success/failure;
- generated asset requirements;
- focused CI smoke flags;
- related documentation links.

Each example README follows the template in
repository-root `examples/_templates/example-readme.md`:
what it demonstrates, graph structure, how to run, expected result, what to
inspect, and related docs.

## Generated assets

Generated assets live under:

```text
docs/assets/generated/
  examples/<example-id>/
    graph.mmd
    graph.svg
    summary.json
    metrics_summary.*    # when requested by metadata
    trace_summary.*      # when requested by metadata
  readme/
    hero.svg
    minimal-pipeline.svg
    trigger-semantics.svg
    observability.svg
    composite-loop.svg
    showcase.json
```

`docs/assets/static/` is reserved for rare hand-maintained brand/manual assets.
Do not add a hand-maintained README image when the same information can be
regenerated from example metadata and CLI output.

## Regeneration commands

After changing example metadata, graph YAML, or README showcase content, run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
python3 scripts/update_examples_index.py
python3 scripts/render_example_assets.py --topoexec build/topoexec
```

In review or CI, use check mode:

```bash
python3 scripts/update_examples_index.py --check
python3 scripts/render_example_assets.py --topoexec build/topoexec --check
```

## Focused gates

Use these goal-specific gates while working on examples or README assets:

```bash
./scripts/goal_check.sh examples
./scripts/goal_check.sh showcase
```

`examples` checks:

- generated examples index freshness;
- generated asset freshness;
- metadata shape and related-doc links;
- each metadata command with expected success/failure;
- validate/render/run smoke flags for curated examples.

`showcase` checks:

- generated README assets exist and are current;
- README status/quick-start phrases remain present;
- README and example Markdown links/assets resolve locally;
- quick-start validate/render/run commands still execute.

The required repository gate remains `./scripts/agent_check.sh` before declaring
repository changes complete.

## README status language

README should stay attractive but honest:

- say beta / pre-production;
- emphasize validation, deterministic execution, examples, trace/metrics, and
  testing support;
- do not imply large-scale production deployment;
- keep deferred adapters and production dashboard/editor scope explicit.
