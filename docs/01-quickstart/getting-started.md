# Getting Started

This tutorial path builds TopoExec, runs one pure C++ embedded graph, then uses
the CLI to validate, inspect, run, and debug a YAML graph.

## 1. Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
```

The required agent gate is:

```bash
./scripts/agent_check.sh
```

## 2. Run the pure C++ builder example

```bash
./build/topoexec_app_cpp_builder_minimal
```

<!-- topoexec-doc-test: ${BUILD_DIR}/topoexec_app_cpp_builder_minimal -->

Expected stable lines:

```text
builder_sink=hello:built
builder_order=source,transform,sink
builder_committed=2
```

This app links only the runtime target and constructs the graph through
`topoexec/runtime/graph_builder.hpp`.

## 3. Validate a YAML graph

```bash
./build/topoexec graph validate examples/minimal.yaml
```

<!-- topoexec-doc-test: ${TOPOEXEC} graph validate ${SOURCE_DIR}/examples/minimal.yaml -->

Expected output:

```text
ok
```

## 4. Inspect the compiled plan

```bash
./build/topoexec graph plan examples/minimal.yaml --format json
```

<!-- topoexec-doc-test: ${TOPOEXEC} graph plan ${SOURCE_DIR}/examples/minimal.yaml --format json -->

Use plan output to review region order, edge kinds, trigger policies, and
CompositeLoop ownership before runtime execution.

## 5. Run the graph

```bash
./build/topoexec graph run examples/minimal.yaml --steps 1
```

<!-- topoexec-doc-test: ${TOPOEXEC} graph run ${SOURCE_DIR}/examples/minimal.yaml --steps 1 -->

Expected stable lines:

```text
order: source,transform,sink
runtime_publication_committed: 2
```

## 6. Debug with metrics and trace

```bash
./build/topoexec graph metrics examples/minimal.yaml --steps 1 --format json
./build/topoexec graph trace examples/minimal.yaml --steps 1 --format json
./build/topoexec graph explain examples/minimal.yaml
```

<!-- topoexec-doc-test: ${TOPOEXEC} graph metrics ${SOURCE_DIR}/examples/minimal.yaml --steps 1 --format json -->
<!-- topoexec-doc-test: ${TOPOEXEC} graph trace ${SOURCE_DIR}/examples/minimal.yaml --steps 1 --format json -->
<!-- topoexec-doc-test: ${TOPOEXEC} graph explain ${SOURCE_DIR}/examples/minimal.yaml -->

The metrics view answers "what happened," the trace view answers "when and in
what order," and explain/lint output maps compiler/runtime semantics back to the
graph authoring model.

## 7. Next pages

- Learn the vocabulary in [Concepts](../10-user-overview/concepts.md).
- Write a component with [Components](../11-user-guide/components.md).
- Author YAML or C++ graphs with [Graph spec](../11-user-guide/graph-spec.md).
- Explore more use cases in [Examples](../11-user-guide/examples.md).
