# Components

Components are application-owned C++ objects. TopoExec supplies the lifecycle,
execution context, graph validation, channel routing, metrics, and trace events;
your app supplies component factories and payload semantics.

## Minimal component shape

A component describes its type, ports, and role, then publishes through the
runtime context:

```cpp
class SourceComponent final : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "my.Source";
    descriptor.name = descriptor.type;
    descriptor.role = topoexec::ComponentRole::kInputBoundary;
    descriptor.outputs = {{"out", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void execute(const topoexec::Invocation&, topoexec::GraphContext& context) override {
    auto result = context.publish("out", topoexec::make_text_payload("hello"));
    if (!result.accepted) {
      throw std::runtime_error(result.reason);
    }
  }
};
```

The compileable tutorial version lives in `examples/apps/cpp_builder_minimal` and
is covered by `app_cpp_builder_minimal_runs`.

## Descriptor rules

- `type` is the key used by `ComponentRegistry`.
- `inputs` and `outputs` are validated against graph edges at runtime compile
  time when descriptors are available.
- `role` distinguishes input boundary, output boundary, and processing nodes.
- Port schemas should match payload types; use custom `OpaquePayload` schemas
  when built-ins are not enough.

## Port contracts

`PortDescriptor` is intentionally lightweight and descriptor-owned. YAML schema
v1 still stores only endpoint strings such as `source.out` and `sink.in`; when a
registry is provided, the graph validator uses component descriptors to enforce
the semantic port contract before runtime:

```cpp
topoexec::PortDescriptor required_text{"in", topoexec::kTextPayloadSchema};
required_text.payload_type = "TextPayload";
required_text.required = true;

topoexec::PortDescriptor optional_side{"side", topoexec::kTextPayloadSchema};
optional_side.required = false;
```

Port fields:

- `name`: endpoint suffix used by graph edges and trigger inputs.
- `schema`: stable payload schema id such as `topoexec::kTextPayloadSchema`.
- `payload_type`: optional embedder-owned type name. Empty means "schema-only".
- `multiplicity`: `kSingle` by default; use `kMultiple` when several incoming
  edges are valid for one input.
- `required`: `false` by default to preserve existing examples; set `true` for
  inputs that must be wired.

Registry-backed validation rejects unknown endpoints, incompatible non-empty
schemas or payload types, missing required inputs, single inputs with multiple
incoming edges, and graph boundary roles that contradict the descriptor role.
Unconnected optional inputs emit the advisory diagnostic
`optional_input_unconnected` without failing validation.

## Execution rules

- Components should be deterministic with respect to their invocation payload,
  config snapshot, and explicit state inputs.
- Use `GraphContext::publish()` instead of touching downstream components.
- Use `GraphContext::submit_task()` only with an attached bounded `ITaskExecutor`; deterministic `TaskExecutor` remains the default test-friendly helper, and opt-in `ThreadedTaskExecutor` behavior is documented in [Async tasks](async-tasks.md).
- Return status or throw for failures; the runtime records structured errors.
- Do not implement hidden global readiness logic; use trigger policies.

## Registration

```cpp
topoexec::ComponentRegistry registry;
registry.register_component({"my.Source"}, [] {
  return std::make_unique<SourceComponent>();
});
```

App-defined factory registration is the current plugin-like extension point.
Dynamic plugin loading and package discovery are future adapter/plugin work.
