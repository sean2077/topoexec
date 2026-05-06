# CLI

The `topoexec` CLI is a validation, inspection, and demo-execution tool. It is
not the required embedding surface; pure C++ apps can link `topoexec::runtime`
without CLI or YAML.

## Graph commands

```bash
./build/topoexec graph validate examples/minimal.yaml
./build/topoexec graph validate examples/template_source_transform_sink.yaml
./build/topoexec graph validate examples/minimal.yaml --schema-only --format json
./build/topoexec graph validate examples/minimal.yaml --semantic --format json
./build/topoexec graph validate examples/diagnostic_warnings.yaml --strict-diagnostics --format json
./build/topoexec graph validate examples/minimal.yaml --max-graph-input-bytes 65536 --format json
./build/topoexec graph plan examples/composite_loop.yaml --format json
./build/topoexec graph render examples/minimal.yaml --format mermaid
./build/topoexec graph run examples/minimal.yaml --steps 1
./build/topoexec graph metrics examples/minimal.yaml --steps 1 --format json
./build/topoexec graph trace examples/minimal.yaml --steps 1 --format json
./build/topoexec graph lint examples/control_feedback_delay.yaml
./build/topoexec graph explain examples/minimal.yaml
./build/topoexec graph diff-plan examples/minimal.yaml examples/control_feedback_delay.yaml
./build/topoexec graph bench examples/minimal.yaml --steps 1 --runs 2 --format json
```

`graph metrics --format json` includes `metric_schema_version` so scripts and
future exporter adapters can verify the descriptor/cardinality contract they are
mapping. `graph trace --format json` and `graph trace --format chrome` include
`trace_schema_version` so trace consumers can verify the timeline/causality
contract before mapping spans or tracks. `graph validate --format json` includes
`diagnostics_schema_version`, diagnostic `category`, and `suggested_fix`;
`--strict-diagnostics` fails warning diagnostics for stricter CI/editor workflows.
`graph bench --format json` includes benchmark schema v2 metadata such as
`graph_hash`, compiler, build type, CPU info, and commit for local baseline
tracking without global timing thresholds.
`graph plan --format json` includes `hierarchy[]` when schema-v1 `subgraphs[]`
compile-time namespace expansion is used; empty hierarchy is emitted as `[]` for
stable scripting.
There is intentionally no separate template CLI; validate/plan/run operate on
the expanded graph after schema-v1 `templates[]` / `template_instances[]`
substitution.
Graph-reading commands also accept per-invocation defensive parser overrides:
`--max-graph-input-bytes`, `--max-lanes`, `--max-components`, `--max-edges`,
`--max-composite-loops`, `--max-identifier-bytes`, `--max-config-depth`,
`--max-config-value-bytes`, and `--max-string-bytes`. Parse failures are reported
as normal validation JSON when `--format json` is selected.

<!-- topoexec-doc-test: ${TOPOEXEC} graph validate ${SOURCE_DIR}/examples/minimal.yaml --schema-only --format json -->
<!-- topoexec-doc-test: ${TOPOEXEC} graph validate ${SOURCE_DIR}/examples/template_source_transform_sink.yaml -->
<!-- topoexec-doc-test: ${TOPOEXEC} graph validate ${SOURCE_DIR}/examples/minimal.yaml --semantic --format json -->
<!-- topoexec-doc-test: ${TOPOEXEC} graph render ${SOURCE_DIR}/examples/minimal.yaml --format mermaid -->
<!-- topoexec-doc-test: ${TOPOEXEC} graph lint ${SOURCE_DIR}/examples/control_feedback_delay.yaml -->
<!-- topoexec-doc-test: ${TOPOEXEC} graph diff-plan ${SOURCE_DIR}/examples/minimal.yaml ${SOURCE_DIR}/examples/control_feedback_delay.yaml -->

## Schema and doctor commands

```bash
./build/topoexec schema dump --format json
./build/topoexec schema check examples/minimal.yaml --format json
./build/topoexec doctor --format json
```

`schema dump --format json` includes the schema annotation
`x-topoexec-semantic_contract_version`. `doctor --format json` reports the same
runtime contract as `semantic_contract_version` alongside the graph
`schema_version`, health-event observer defaults, and default
`graph_input_limits`.

<!-- topoexec-doc-test: ${TOPOEXEC} schema dump --format json -->
<!-- topoexec-doc-test: ${TOPOEXEC} schema check ${SOURCE_DIR}/examples/minimal.yaml --format json -->
<!-- topoexec-doc-test: ${TOPOEXEC} doctor --format json -->

## Output stability

- Text output is intended for humans and smoke tests.
- JSON output is intended for scripts and golden tests.
- Chrome trace output is compatible with Perfetto/Chrome trace viewers, carries
  `trace_schema_version`, and groups events by stable phase/lane/component/channel
  tracks without requiring an external Perfetto adapter.
- Bench output is for local regression comparison only; do not compare absolute
  performance across machines or enable thresholds outside a controlled
  per-machine baseline setup.

## Debugging sequence

1. `validate --schema-only` catches shape, parser-limit, UTF-8, and unknown-field issues.
2. `validate --semantic` adds compiler diagnostics and region order. Use `--strict-diagnostics` when warning diagnostics should fail CI.
3. `explain --format json` groups diagnostics by graph structure, scheduler, channel, payload, and trigger categories.
4. `plan --format json` shows what will execute.
5. `lint` flags risky or invalid edge policies such as large copies, slow-reader drop risk, `move_only` multi-reader misuse, and loaned views without an explicit producer/pool owner.
6. `run`, `metrics`, and `trace` confirm runtime behavior.
7. `diff-plan` explains semantic drift between two graph revisions.
