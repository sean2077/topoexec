# Schema Version 1

Required root fields:

- `schema_version: 1`
- `graph`
- `lanes`
- `components`
- `edges`

Important semantics:

- `edges[].kind` is required and must be `immediate`, `delay`, `state`, or `async`.
- Immediate cycles are invalid unless they exactly match a `composite_loops[]` component set.
- `boundary.role` is generic and may be `processing`, `input`, `output`, or `input_output`.
- Unknown root and section fields are rejected.
