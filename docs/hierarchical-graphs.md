# Hierarchical Graphs

G41 adds a phase-1 hierarchy contract for organizing larger in-process graphs
without introducing nested runtime schedulers.

## Phase-1 model

`subgraphs[]` are **compile-time namespace expansions**:

- each subgraph has an `id`;
- local `components[]`, `edges[]`, and optional `composite_loops[]` are parsed
  with the same schema-v1 rules as top-level graph entries;
- component, edge, and CompositeLoop ids expand to `<subgraph>.<local_id>`;
- edge endpoints expand by prefixing the component part only, so
  `source.out` becomes `cell.source.out`;
- `depends_on` component references and subgraph-local CompositeLoop component
  references expand the same way;
- lanes remain top-level and runtime execution sees one flat `GraphSpec`.

This is intentionally not a CompositeComponent runtime, nested executor, dynamic
template loader, or adapter boundary. Future `components[].graph_ref` or schema
v2 template work must be a separate compatibility decision.

## Example

```yaml
schema_version: 1
graph: {name: hierarchical_preview, kind: internal_test}
lanes: {main: {type: event_loop}}
components: []
edges: []
subgraphs:
  - id: cell
    components:
      - id: source
        type: topoexec.test.Source
        event_sources: [{type: manual}]
        trigger_policy: {type: manual}
        execution: {lane: main}
      - id: sink
        type: topoexec.test.Sink
        event_sources: [{type: message, inputs: [in]}]
        trigger_policy: {type: any_input, inputs: [in]}
        execution: {lane: main}
        depends_on: [source]
    edges:
      - id: source_sink
        kind: immediate
        from: source.out
        to: sink.in
        policy: {mode: latest, copy_policy: shared_view}
```

The loader expands this to components `cell.source` and `cell.sink`, edge
`cell.source_sink`, endpoints `cell.source.out` and `cell.sink.in`, and a
hierarchy entry in plan JSON.

## Validation and tooling

Hierarchy never hides graph semantics:

- immediate-cycle validation runs after expansion, so a feedback loop inside or
  across a subgraph is rejected unless a matching expanded CompositeLoop owns the
  SCC;
- descriptor-backed port validation uses the last `.` as the endpoint separator,
  allowing namespaced component ids while preserving `component.port` behavior;
- runtime metrics, trace events, ticked components, and channel ids use expanded
  ids such as `cell.sink` and `cell.source_sink`;
- plan JSON includes a `hierarchy[]` section with expanded component, edge, and
  CompositeLoop ids;
- Mermaid rendering groups expanded components under `Subgraph: <id>` while
  still showing expanded component-level edges.

Phase 1 keeps hierarchy observable and semantic, but deliberately avoids runtime
nesting or hidden feedback-loop policy.
