# Editor and Schema UX

TopoExec graph authoring is supported through the existing Draft 2020-12 JSON
Schema and machine-readable diagnostics. G65 does not add an editor extension,
LSP server, schema v2 loader, runtime dependency, or new CLI command.

## Status

- Implemented schema: `schema/topoexec.schema.v1.json`.
- Installed schema path: `share/topoexec/schema/topoexec.schema.v1.json` under
  the install prefix.
- Discovery commands: `topoexec doctor --format json` and
  `topoexec schema dump --format json`.
- Editor integration: configure an existing YAML language server or editor to use
  the bundled schema.
- LSP implementation: deferred design topic only.

## Schema discovery

From a source checkout, the schema lives at:

```text
schema/topoexec.schema.v1.json
```

From an installed package, the CLI can discover the installed schema relative to
its executable:

```bash
topoexec doctor --format json
```

Look for:

```json
{
  "schema_found": true,
  "schema_path": ".../share/topoexec/schema/topoexec.schema.v1.json"
}
```

`topoexec schema dump --format json` prints the same bundled schema, including
its `$id` and `x-topoexec-semantic_contract_version` annotation. Set
`TOPOEXEC_SCHEMA_PATH` only when testing a local schema override.

## VS Code workspace settings

VS Code users can install the Red Hat YAML extension / YAML Language Server and
associate graph YAML files with the local schema through workspace settings. The
upstream YAML Language Server documents both `yaml.schemas` associations and
`# yaml-language-server: $schema=...` modelines.

For a repository checkout, commit a project-relative `.vscode/settings.json` if
the whole team wants the same graph patterns:

```json
{
  "yaml.schemas": {
    "./schema/topoexec.schema.v1.json": [
      "examples/*.yaml",
      "graphs/**/*.yaml",
      "graphs/**/*.topoexec.yaml"
    ]
  },
  "yaml.validate": true,
  "yaml.completion": true
}
```

For an installed package, do not commit a user-specific absolute path. Instead,
run `topoexec doctor --format json`, copy `schema_path` into your user or
workspace settings, and keep the path local to that machine.

## Inline modeline

For a single file, add a modeline that points to the schema. A project-relative
path keeps the file portable inside this repository:

```yaml
# yaml-language-server: $schema=../schema/topoexec.schema.v1.json
schema_version: 1
graph: {name: minimal, kind: runnable}
lanes: {main: {type: event_loop}}
components: []
edges: []
```

Use the relative path that is correct for the file location. Avoid absolute paths
in checked-in examples.

## Diagnostics for editor integrations

JSON Schema validation catches strict shape issues: required fields, unknown
fields, scalar limits, enum values, and basic object/array structure.

`topoexec graph validate --format json` adds semantic diagnostics for editor or
CI integrations. Diagnostic records include stable fields suitable for editor
problem lists:

- `code`
- `severity`
- `category`
- `graph_path`
- `message`
- `suggested_fix`
- `involved_components`
- `involved_edges`

Use `--schema-only` when an editor task should only check the strict loader
contract. Use semantic validation when path-aware TopoExec diagnostics are
wanted.

## LSP design boundary

A future LSP or editor extension should be a thin adapter around existing
surfaces:

1. Discover the schema with `topoexec doctor --format json` or package metadata.
2. Use the bundled JSON Schema for completion and structural validation.
3. Run `topoexec graph validate --format json` for semantic diagnostics.
4. Translate `graph_path` into editor ranges using the YAML document parser owned
   by the editor/LSP adapter.
5. Keep runtime execution out of editor validation by default.

Do not add runtime dependencies, editor SDK dependencies, file watchers, or a
long-lived language server to `topoexec::runtime` for this preview.

## Validation

The G65 gate checks:

- source and installed schema discovery;
- stable `schema dump` output;
- editor-oriented diagnostic JSON fields (`code`, `graph_path`, and
  `suggested_fix`);
- docs-map coverage for this page;
- the required full `scripts/agent_check.sh` gate.
