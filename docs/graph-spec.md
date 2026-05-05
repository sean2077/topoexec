# Graph Spec

A graph can be authored directly in C++ or loaded from schema-versioned YAML. The
runtime target does not depend on YAML; YAML loading is provided by the optional
`topoexec::yaml` target and the CLI.

## Minimal YAML shape

```yaml
schema_version: 1
graph:
  name: minimal
  kind: internal_test
lanes:
  main: {type: event_loop}
components:
  - id: source
    type: topoexec.boundary.Input
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: sink
    type: topoexec.boundary.Output
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: main}
edges:
  - id: source_sink
    kind: immediate
    from: source.out
    to: sink.in
    policy: {mode: latest, copy_policy: shared_view}
```

Validate it with:

```bash
./build/topoexec graph validate examples/minimal.yaml
```

<!-- topoexec-doc-test: ${TOPOEXEC} graph validate ${SOURCE_DIR}/examples/minimal.yaml -->

## Required fields

- `schema_version`: currently `1`.
- `graph`: name, kind, and optional graph-level `config` snapshot.
- `lanes`: named execution lanes such as `event_loop` or bounded `thread_pool`.
- `components`: ids, type names, event sources, trigger policies, execution
  options, optional config, and optional boundary metadata.
- `edges`: id, `kind`, `from`, `to`, and channel policy.
- `composite_loops`: optional exact immediate-SCC ownership declarations.

See [Schema v1](schema-v1.md) for the strict field contract and
[Runtime semantics](runtime-semantics.md) for visibility rules.

## C++ builder path

Use `topoexec/runtime/graph_builder.hpp` when an application wants to avoid YAML
at runtime. The compileable example is `examples/apps/cpp_builder_minimal`, and
the installed package smoke under `tests/cmake/runtime_smoke` verifies that this
path links only `topoexec::runtime`.

## Authoring checklist

1. Give every component and edge a stable id.
2. Keep feedback safe: use `delay`, `state`, `async`, or an exact CompositeLoop.
3. Make channel capacity and overflow policy explicit for overload-prone paths.
4. Choose trigger policies that match readiness semantics, not implementation
   convenience.
5. Run `validate`, `plan --format json`, and an example run/metrics/trace smoke
   before treating the graph as stable.
