# Component Lifecycle

TopoExec makes component lifecycle and error behavior explicit so embedded apps
can start, execute, stop, and diagnose graphs predictably.

## Phases

1. **Construct**: the registry creates component instances from graph type names.
2. **Describe**: descriptors expose role and ports for validation.
3. **Configure**: component config and graph-level config snapshots are provided.
4. **Activate**: runtime starts components in compiled order.
5. **Restore / reset at start epoch**: optional `RuntimeRunnerOptions`
   restore snapshots are applied, then requested resets run before any
   component execution.
6. **Execute**: ready invocations run according to lane and trigger policy.
7. **Snapshot**: optional component state snapshots are captured after the
   scheduler run reaches an epoch boundary and before deactivation.
8. **Deactivate**: runtime tears down activated components in reverse order.

## Extended lifecycle hooks

G43 adds experimental stateful lifecycle hooks on `Component`:

- `reset(GraphContext&)` / `reset_status(...)`: clear component-local state at a
  controlled epoch boundary.
- `pause_status()` and `resume_status()`: no-op default hooks reserved for
  future runtime-driven pause/resume policy.
- `snapshot_state()`: return a `ComponentStateSnapshot` with component type,
  version, immutable payload, and optional byte-size metadata.
- `restore_state(...)`: validate and restore a prior snapshot before execution.

The default reset/pause/resume hooks are no-ops. The default snapshot is empty.
The default restore accepts empty snapshots and rejects non-empty snapshots so a
component does not silently pretend to support restore.

Runner options keep the first runtime integration small:

- `reset_component_ids`: component ids reset once after activation and before
  the scheduler starts.
- `restore_component_states`: per-component snapshots restored after activation
  and before reset/execution. A non-empty `component_type` must match the graph
  component type.
- `capture_component_state_snapshots`: captures snapshots after execution and
  stores them in `RuntimeRunnerResult::component_state_snapshots`.

This is not a hot live-control API yet. Resets/restores are intentionally driven
at run start or run end boundaries so they do not interleave with component
execution or violate transaction visibility.

## Config reload hooks

G44 adds experimental config reload hooks on `Component`:

- `validate_config(const ConfigView&)`: reject a pending component config before
  any component in the transaction is changed.
- `apply_config(GraphContext&, const ConfigView&)`: apply the new config at an
  epoch boundary. The default delegates to `configure_status()` so existing
  components keep their original behavior unless they opt into a live-reload
  implementation.

Runtime config reloads are transaction-scoped. `EventRuntime` validates all
pending component configs, applies them before component execution for the next
epoch, commits `ConfigSnapshotStore` only after every apply succeeds, and
rolls back pending updates on validation/apply failure. The API intentionally
does not mutate component config in the middle of an invocation.

## Error model

`RuntimeRunnerResult::runtime_errors` records structured phase, component, code,
fatal flag, and message data. Legacy string errors remain for compatibility, but
new code should inspect structured errors where possible.

Only `fail_fast` execution is implemented. Other parsed `execution.on_error`
values are rejected rather than silently falling back.

## Cleanup guarantees

- If configure or activate fails, already activated components are cleaned up.
- If restore or reset fails, already activated components are cleaned up and the
  failure is reported as `component_restore` or `component_reset`.
- Deactivation runs in reverse activation order.
- CompositeLoop internal failures stop the loop and suppress half-updated
  external commits.
- Async task failures become task completions with error metrics/trace instead
  of hidden background exceptions.

## Related references

- [Runtime semantics](runtime-semantics.md)
- [Scheduler](scheduler.md)
- [Async tasks](async-tasks.md)
- [Diagnostics](diagnostics.md)
