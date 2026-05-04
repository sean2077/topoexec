# Minimal Pipeline

Graph shape:

```text
source --immediate--> transform --immediate--> sink
```

Run:

```bash
./build/topoexec_app_minimal_pipeline
```

Expected output:

```text
sink=hello:transformed
order=source,transform,sink
channel_publish_count=2
channel_delivery_count=2
runtime_publication_committed=2
```

This app demonstrates the basic single-epoch immediate pipeline. `publish()` stages each output, the runtime commits it after the component returns, and the compiled region order lets the downstream component observe it in the same bounded run step.

Contrast case: changing either edge to `delay`, `state`, or `async` would defer downstream visibility to a later epoch instead of producing the full source-transform-sink order in one step.
