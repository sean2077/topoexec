# Graph Templates

G42 adds a small schema-v1 template surface for reusable YAML graph snippets.
Templates are expanded while loading the graph; the runtime, scheduler, channel
router, and validators only see the expanded flat `GraphSpec`.

## Contract

Templates are deliberately limited:

- no arbitrary code, expressions, file includes, loops, or conditionals;
- parameter substitution is the only dynamic behavior;
- placeholders use `{{name}}` and must match a declared parameter exactly;
- instance parameter maps must provide every declared parameter and no unknown
  parameters;
- expansion prefixes local component, edge, `depends_on`, and CompositeLoop ids
  with the `template_instances[].id` namespace;
- edges cannot be hidden because every template edge expands into a normal
  `GraphSpec::edges` entry before validation.

Use templates for small repeated shapes such as source-transform-sink,
request-worker-response, bounded async task paths, or delay feedback snippets.
Do not use them as a dynamic plugin system or runtime graph loader.

## YAML shape

```yaml
schema_version: 1
graph: {name: template_source_transform_sink, kind: internal_test}
lanes: {main: {type: event_loop}}
components: []
edges: []
templates:
  - id: source_transform_sink
    parameters: [source_descriptor, sink_descriptor]
    components:
      - id: source
        type: topoexec.boundary.Input
        boundary: {role: input, descriptor: "{{source_descriptor}}"}
        event_sources: [{type: manual}]
        trigger_policy: {type: manual}
        execution: {lane: main}
      - id: transform
        type: topoexec.transforms.Identity
        event_sources: [{type: message, inputs: [in]}]
        trigger_policy: {type: any_input, inputs: [in]}
        execution: {lane: main}
      - id: sink
        type: topoexec.boundary.Output
        boundary: {role: output, descriptor: "{{sink_descriptor}}"}
        event_sources: [{type: message, inputs: [in]}]
        trigger_policy: {type: any_input, inputs: [in]}
        execution: {lane: main}
    edges:
      - {id: source_transform, kind: immediate, from: source.out, to: transform.in}
      - {id: transform_sink, kind: immediate, from: transform.out, to: sink.in}
template_instances:
  - id: demo
    template: source_transform_sink
    parameters: {source_descriptor: stdin, sink_descriptor: stdout}
```

The instance expands into components `demo.source`, `demo.transform`, and
`demo.sink`; edges `demo.source_transform` and `demo.transform_sink`; and
endpoints such as `demo.source.out` and `demo.sink.in`. Plan JSON shows the
expanded namespace under `hierarchy[]`; runtime metrics and trace use the
expanded ids.

## Validation

Template expansion happens before normal graph validation, so all existing
semantic checks still apply:

- unsupported component types fail registry validation;
- immediate cycles still require exact CompositeLoop ownership;
- missing ports, invalid trigger inputs, and invalid channel policies fail the
  same way as handwritten expanded YAML;
- invalid placeholders, missing parameters, unknown parameters, and duplicate
  template ids fail during loading.

The CLI has no separate `template` command. Use existing graph commands:

```bash
./build/topoexec graph validate examples/template_source_transform_sink.yaml
./build/topoexec graph plan examples/template_source_transform_sink.yaml --format json
```

<!-- topoexec-doc-test: ${TOPOEXEC} graph validate ${SOURCE_DIR}/examples/template_source_transform_sink.yaml -->
