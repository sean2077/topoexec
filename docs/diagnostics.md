# Diagnostics

TopoExec graph diagnostics are the machine-readable counterpart to legacy `errors[]` strings. New tooling should prefer `diagnostics[]` while preserving `errors[]` for human-readable compatibility.

Each diagnostic has:

- `code`: stable machine-readable id;
- `severity`: `error` for validation/compile failures, or advisory/warning levels for accepted-but-not-enforced fields;
- `message`: concrete human-readable failure;
- `graph_path`: best-effort schema path such as `components.source` or `edges.source_sink`;
- `involved_components` / `involved_edges`: ids useful for editor highlights;
- `suggested_fix`: stable remediation text from the diagnostic registry.

## Stable registry

The C++ API exposes the registry through `topoexec/runtime/diagnostics.hpp`:

```cpp
const auto descriptor = topoexec::graph_diagnostic_descriptor("multi_state_writer");
```

Current stable codes:

| Code | Meaning |
| --- | --- |
| `unknown_component` | An edge, dependency, or CompositeLoop references a missing component. |
| `unknown_port` | An edge endpoint references a port not exposed by the descriptor. |
| `duplicate_id` | A section contains duplicate ids. |
| `immediate_cycle_without_loop` | Immediate edges form a cycle without an exact CompositeLoop owner. |
| `partial_composite_loop` | A CompositeLoop partially matches an immediate SCC. |
| `decorative_composite_loop` | A CompositeLoop does not own an immediate cycle. |
| `multi_state_writer` | Multiple state edges write the same target snapshot. |
| `invalid_move_only_multireader` | `move_only` was combined with multi-reader delivery. |
| `invalid_channel_policy` | Channel policy fields are unsupported or inconsistent. |
| `unsupported_lane_type` | Scheduler lane type is unsupported. |
| `trigger_missing_input` | Trigger input is missing or lacks an incoming edge. |
| `incompatible_trigger_edge_mode` | Trigger/event-source declarations do not match incoming edge shape. |
| `unsupported_error_policy` | Non-`fail_fast` execution error policy requested. |
| `advisory_lane_field_ignored` | A scheduler lane field is parsed and preserved but not enforced, or only applied best-effort, by the current runtime. |
| `advisory_execution_field_ignored` | A component execution field is parsed and preserved but not enforced by the current runtime. |
| `graph_validation_error` | Generic fallback validation error. |

Add new codes rather than changing existing meanings. If a code meaning must change, update `docs/versioning.md` and the changelog.
