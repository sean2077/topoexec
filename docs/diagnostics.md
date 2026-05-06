# Diagnostics

TopoExec graph diagnostics are the machine-readable counterpart to legacy `errors[]` strings. New tooling should prefer `diagnostics[]` while preserving `errors[]` for human-readable compatibility. Diagnostics JSON carries `diagnostics_schema_version: "1"`.

Each diagnostic has:

- `code`: stable machine-readable id;
- `severity`: `error`, `warning`, `info`, or `advisory`; only `error` fails validation by default;
- `category`: one of `graph_structure`, `scheduler`, `channel`, `payload`, or `trigger`;
- `message`: concrete human-readable failure;
- `graph_path`: best-effort schema path such as `components.source` or `edges.source_sink`;
- `involved_components` / `involved_edges`: ids useful for editor highlights;
- `suggested_fix`: stable remediation text from the diagnostic registry.

## Stable registry

The C++ API exposes the registry through `topoexec/runtime/diagnostics.hpp`:

```cpp
const auto descriptor = topoexec::graph_diagnostic_descriptor("multi_state_writer");
const auto category = topoexec::graph_diagnostic_category("large_payload_copy");
```

Current stable codes:

| Code | Severity | Category | Meaning |
| --- | --- | --- | --- |
| `unknown_component` | error | graph_structure | An edge, dependency, or CompositeLoop references a missing component. |
| `unknown_port` | error | graph_structure | An edge endpoint references a port not exposed by the descriptor. |
| `payload_type_mismatch` | error | payload | An edge connects descriptor ports with incompatible non-empty schema or payload type metadata. |
| `missing_required_input` | error | graph_structure | A descriptor-declared required input has no incoming edge. |
| `optional_input_unconnected` | advisory | graph_structure | A descriptor-declared optional input has no incoming edge; validation continues. |
| `boundary_role_mismatch` | error | graph_structure | A graph boundary role is incompatible with the component descriptor role. |
| `port_multiplicity_mismatch` | error | graph_structure | A single-input descriptor port has multiple incoming edges. |
| `duplicate_id` | error | graph_structure | A section contains duplicate ids. |
| `immediate_cycle_without_loop` | error | graph_structure | Immediate edges form a cycle without an exact CompositeLoop owner. |
| `partial_composite_loop` | error | graph_structure | A CompositeLoop partially matches an immediate SCC. |
| `decorative_composite_loop` | error | graph_structure | A CompositeLoop does not own an immediate cycle. |
| `multi_state_writer` | error | channel | Multiple state edges write the same target snapshot. |
| `invalid_move_only_multireader` | error | payload | `move_only` was combined with multi-reader delivery. |
| `invalid_channel_policy` | error | channel | Channel policy fields are unsupported or inconsistent. |
| `unsupported_lane_type` | error | scheduler | Scheduler lane type is unsupported. |
| `trigger_missing_input` | error | trigger | Trigger input is missing or lacks an incoming edge. |
| `incompatible_trigger_edge_mode` | error | trigger | Trigger/event-source declarations do not match incoming edge shape. |
| `unsupported_error_policy` | error | scheduler | Non-`fail_fast` execution error policy requested. |
| `advisory_lane_field_ignored` | advisory | scheduler | A scheduler lane field is parsed and preserved but not enforced, or only applied best-effort. |
| `advisory_execution_field_ignored` | advisory | scheduler | A component execution field is parsed and preserved but not enforced. |
| `backpressure_risk` | warning | channel | A channel can stall upstream execution or drop under slow-reader pressure. |
| `large_payload_copy` | warning | payload | A large frame/blob payload edge uses copy semantics. |
| `high_queue_depth_latency_risk` | warning | channel | A deep queue can hide latency and grow tail delays. |
| `trigger_never_ready` | warning | trigger | A trigger configuration is accepted but unlikely to become ready at runtime. |
| `subgraph_hidden_cycle` | warning | graph_structure | Reserved for future subgraph boundaries that hide immediate cycles. |
| `diagnostic_note` | info | graph_structure | Reserved informational diagnostic note. |
| `graph_validation_error` | error | graph_structure | Generic fallback validation error. |

Add new codes rather than changing existing meanings. If a code meaning must change, update `docs/versioning.md` and the changelog. `topoexec graph explain --format json` groups diagnostics by `category` under `diagnostic_groups`. `topoexec graph validate --strict-diagnostics` promotes `warning` diagnostics to validation failure; advisories and info remain non-failing guidance.

Note: `execution.priority` is implemented runtime behavior in the G32 scheduler
priority pass. Supported values do not emit `advisory_execution_field_ignored`;
unknown priority values fail validation.
