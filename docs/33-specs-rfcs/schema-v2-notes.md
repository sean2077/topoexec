# Schema v2 Notes

This is an exploration and compatibility-boundary artifact. The repository still
implements only `schema_version: 1`: the v1 loader remains strict, the bundled
JSON Schema remains `topoexec.schema.v1.json`, and no `topoexec schema migrate`
command or v2 loader is introduced by this note.

The purpose of schema v2 is to avoid growing schema v1 with fields that would
change existing graph meaning, security posture, or runtime lifecycle behavior.
Use this page before proposing new graph fields.

## Status

- API stability: design note only.
- Implemented loader: schema v1 only.
- Implemented machine-readable schema: Draft 2020-12 `schema_version: 1` only.
- Implemented migration tooling: none.
- Implemented v2 fixtures: none accepted by CI as runnable graphs.

Experimental v2 sketches may be added later as non-executable fixtures, but they
must be labeled as sketches and must not be used as evidence that schema v2 is
implemented. Existing v1 examples and the `schema_v1_contract_smoke` remain the
compatibility gate.

## Decision rules

Keep a change in schema v1 only when all of these are true:

1. The field is optional and has a documented default.
2. Existing valid v1 graphs still parse, validate, and execute with the same
   meaning.
3. Unknown fields remain rejected by the strict v1 loader.
4. The field is useful without a specific adapter, package registry, plugin
   ecosystem, or external service.
5. The runtime can explain the field with existing semantic-contract language or
   with a compatible additive semantic-contract note.

Use schema v2 when any of these are true:

- a previously valid graph would be rejected or would mean something different;
- a new required root/section field is needed;
- an enum value must be renamed or reinterpreted;
- graph files need to select runtime behavior by version rather than by an
  additive option with a compatible default;
- the field introduces runtime nesting, dynamic package/plugin discovery,
  adapter-specific transport configuration, arbitrary expressions, or new graph
  side effects;
- validation needs first-class graph constructs that cannot be represented by
  the current flat `GraphSpec` plus descriptor-backed checks.

A semantic-contract version bump may be enough when graph shape stays identical
but runtime meaning is tightened or clarified. A schema version bump is required
when graph shape or accepted-file meaning changes incompatibly.

## Candidate feature classification

| Candidate | Current v1 status | Keep additive v1 only if | Requires schema v2 when |
| --- | --- | --- | --- |
| Hierarchical subgraphs | `subgraphs[]` already expand at load time into flat v1 components, edges, `depends_on`, and CompositeLoops. | The feature remains deterministic compile-time namespace expansion with no nested runtime scheduler. | It adds runtime-nested graphs, `graph_ref`, encapsulated subgraph ports, independent clocks, or nested lifecycle/metrics scopes. |
| Typed ports | Descriptor-backed validation already checks payload schema/type, required inputs, multiplicity, and boundary roles without YAML `ports`. | New checks stay registry-backed or optional metadata that does not change YAML graph shape. | YAML-authored port declarations, endpoint signatures, package-level type schemas, or first-class multiplicity become graph input. |
| Semantic contract version | Runtime/tool metadata currently exposes semantic contract `0.2` through doctor and schema dump annotations. | A new field is purely informational and does not select behavior. | Graph files choose behavior by contract version, require minimum runtime semantics, or carry compatibility modes. |
| Richer scheduler lanes | v1 has event-loop, fixed-rate, and thread-pool lanes with documented implemented/advisory fields. | New fields are advisory or optional with no change to existing lane scheduling. | Existing lane defaults, priority meanings, clock behavior, isolation, or hard timing guarantees change. |
| Condition/watermark triggers | v1 includes declarative `condition`, `watermark`, `debounce`, and `rate_limit` preview policies. | New trigger options are enum-only/declarative and compatible with current readiness semantics. | Arbitrary expressions/scripts, external watermark coordination, wall-clock debounce timers, or trigger-language packages are introduced. |
| Health event sink config | v1 exposes observer-only health events through runtime options and edge `emit_health_events`. | Additions remain host-side runtime options or non-semantic documentation. | Graph files route health events, configure sinks/exporters, or let health events control runtime flow. |
| Adapter boundary descriptors | v1 has generic `boundary.role` and `boundary.descriptor`; adapter details live outside core schema. | Metadata is generic, adapter-independent, and useful for in-process validation. | ROS/OTel/Prometheus/service-specific fields, QoS schemas, transport endpoints, or external resource discovery enter graph files. |
| Config hot reload policy | Runtime/component hooks support staged config transactions outside schema fields. | Additions remain host-side runner options or descriptor metadata with identical graph execution meaning. | Graphs declare reload cadence, transaction isolation, failure policy, live mutation authority, or cross-component reload ordering. |
| Plugin/component package refs | The plugin loader loads trusted native plugins only from explicit host-provided paths. | Package metadata stays outside graph files or in host/plugin manifests. | Graphs name packages, versions, repositories, dynamic plugin paths, capability negotiation, or sandbox policy. |

## Breaking vs additive changes

Examples that can remain additive in schema v1:

- optional fields with stable defaults that preserve old graph behavior;
- new enum values whose absence preserves old behavior and whose presence is
  rejected or handled deterministically by older validators;
- descriptor-backed semantic validation that does not add YAML fields;
- metadata exposed in `topoexec doctor`, `schema dump`, metrics, trace, or docs
  without changing accepted graph meaning.

Examples that are breaking and should wait for schema v2:

- changing default edge visibility, trigger readiness, channel overflow, or loop
  ownership semantics;
- making any currently optional field required;
- renaming `kind`, `type`, `policy`, or enum values used by existing examples;
- accepting adapter-specific or plugin-discovery fields in core v1;
- replacing compile-time `subgraphs[]` or templates with runtime interpretation;
- adding arbitrary expression evaluation to graph files.

## Migration plan

1. Keep the v1 loader and `topoexec.schema.v1.json` intact. Existing examples
   and golden outputs remain the regression gate.
2. Design a separate v2 loader only after the v2 field set is reviewed. It
   should parse `schema_version: 2` into a reviewed intermediate form and then
   normalize to `GraphSpec` or a successor runtime model.
3. Keep v1 and v2 validation paths explicit. A v2 loader must not silently
   reinterpret a v1 graph as v2 or relax v1 unknown-field rejection.
4. Add v2 fixtures as experimental non-default tests first. They should prove the
   new parser rejects incomplete or ambiguous v2 sketches before any runtime
   execution claims are made.
5. Consider `topoexec schema migrate` only after the v2 model is stable enough to
   produce deterministic output. Until then, migration guidance belongs in docs,
   not a CLI command.

## Non-goals

- No schema v2 implementation.
- No migration CLI.
- No new adapter, package registry, plugin discovery, expression language, or
  runtime nesting.
- No relaxation of strict schema v1 validation and no change to accepted v1 examples.

## Validation

Use these checks for any future schema v2 design slice:

```bash
./scripts/goal_check.sh schema
./scripts/goal_check.sh quick
./scripts/goal_check.sh docs
./scripts/agent_check.sh
```

For this design slice, the expected evidence is that v1 examples still pass, the
schema v1 contract still rejects non-v1 `schema_version` values, and this page
is covered by the docs map smoke. No v2 graph should be accepted as runnable
until a reviewed v2 loader exists.
