# Overload: Latest Versus Queue

Graph shape:

```text
fast_source --latest/overwrite--> slow_processor
fast_source --queue/drop_oldest(capacity=2)--> slow_processor
```

Run:

```bash
./build/topoexec_app_overload_latest_vs_queue
```

Expected output:

```text
latest_payloads=frame-3
latest_drop_count=2
latest_max_depth=1
queue_payloads=event-2,event-3
queue_drop_count=1
queue_max_depth=2
```

This app demonstrates bounded channel policy under overload. `latest` keeps only the newest sample and records overwritten samples as drops. `queue` preserves FIFO order up to its capacity, then applies the configured overflow policy.

Contrast case: `drop_newest` would reject the incoming payload instead of evicting the oldest queued payload, and `block` is unsuitable for the single-thread event-loop hot path unless a graph explicitly opts into blocking behavior.
