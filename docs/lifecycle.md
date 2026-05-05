# Component Lifecycle

TopoExec makes component lifecycle and error behavior explicit so embedded apps
can start, execute, stop, and diagnose graphs predictably.

## Phases

1. **Construct**: the registry creates component instances from graph type names.
2. **Describe**: descriptors expose role and ports for validation.
3. **Configure**: component config and graph-level config snapshots are provided.
4. **Activate**: runtime starts components in compiled order.
5. **Execute**: ready invocations run according to lane and trigger policy.
6. **Deactivate**: runtime tears down activated components in reverse order.

## Error model

`RuntimeRunnerResult::runtime_errors` records structured phase, component, code,
fatal flag, and message data. Legacy string errors remain for compatibility, but
new code should inspect structured errors where possible.

Only `fail_fast` execution is implemented. Other parsed `execution.on_error`
values are rejected rather than silently falling back.

## Cleanup guarantees

- If configure or activate fails, already activated components are cleaned up.
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
