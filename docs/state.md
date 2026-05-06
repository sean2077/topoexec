# State And Config Snapshots

TopoExec state is explicit, snapshot-based, and committed on epoch boundaries. It is not a hidden mutable global.

## State edges

`state` edges are feedback-safe edge kinds:

- the publisher stages a next snapshot during the current epoch;
- readers continue to observe the currently committed snapshot during that epoch;
- the staged value becomes visible at the next epoch boundary;
- multiple state edges writing the same target endpoint are rejected by validation until an explicit merge policy exists.

This makes slow-to-fast crossings and retained controller/estimator state deterministic: downstream components cannot observe a half-updated state edge in the same transaction that produced it.

## RuntimeStateStore

`topoexec/runtime/state.hpp` provides an optional in-process blackboard:

```cpp
topoexec::RuntimeStateStore store;
store.stage_write("robot", "pose", "estimator.state",
                  topoexec::make_shared_payload(topoexec::make_text_payload("p1")));

auto before = store.snapshot("robot"); // no pending value yet
store.commit_epoch_boundary();
auto after = store.snapshot("robot");  // pose is now visible
```

Contract:

- namespaces and keys must be non-empty;
- writers must be explicit strings, conventionally `component.port`;
- snapshots copy immutable `RuntimePayloadPtr` handles and are not mutated by later commits;
- the v1 merge policy is single-writer per namespace/key;
- rejected writes increment state metrics instead of silently overwriting another writer.

`RuntimeRunner` wires a store into `GraphContext::state_store` for components that need this lower-level surface.

## ConfigSnapshotStore

`graph.config` is parsed into `GraphSpec::config` and loaded into `ConfigSnapshotStore` by `RuntimeRunner`. Each component's YAML `config` is also copied into that store.

Components may stage component config updates:

```cpp
topoexec::ConfigView next;
next.values["gain"] = "2";
ctx.config_store->stage_component_config_update("controller", next);
```

By default, staged config updates form one transaction and apply at the next
epoch boundary. Components that read the config snapshot during the publishing
epoch still see the previously committed values. At the boundary, `EventRuntime`
validates every pending component config with `Component::validate_config()`,
applies each one with `Component::apply_config()`, and only then commits the
store version. If validation fails, or an apply hook fails and rollback is
attempted, the pending transaction is discarded and the previous committed
config remains active.

`ConfigSnapshotStore::last_transaction()` records the applied transaction id,
version, epoch, timestamp, and component ids. Explicit immediate updates are
still exposed for setup/test code, but runtime hot-reload code should prefer the
epoch-boundary path so component execution never observes a mid-iteration
mutation.

## Metrics

State/config snapshot work is observable through:

- `runtime.publication.state` and `runtime.publication.state_committed` for state-edge staging/commit;
- `runtime.state.*` for blackboard staged/committed/rejected/snapshot-read counts;
- `runtime.config.*` for config version, transaction id, staged/committed/rolled-back/rejected/snapshot-read counts.
