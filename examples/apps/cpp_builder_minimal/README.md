# C++ Builder Minimal

Graph shape:

```text
source --immediate--> transform --immediate--> sink
```

Run:

```bash
./build/topoexec_app_cpp_builder_minimal
```

Expected output:

```text
builder_sink=hello:built
builder_order=source,transform,sink
builder_committed=2
```

This app demonstrates building a graph directly in C++ with `topoexec/runtime/graph_builder.hpp`. It links against `topoexec_runtime` and does not require YAML loading.

Contrast case: the YAML examples exercise the optional `topoexec_yaml` target. This app is the embeddability path for applications that want a pure C++ graph description.
