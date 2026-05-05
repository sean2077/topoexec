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

## Execution rules

- Components should be deterministic with respect to their invocation payload,
  config snapshot, and explicit state inputs.
- Use `GraphContext::publish()` instead of touching downstream components.
- Use `GraphContext::submit_task()` only for the deterministic async task surface
  documented in [Async tasks](async-tasks.md).
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
