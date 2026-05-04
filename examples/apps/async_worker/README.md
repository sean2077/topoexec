# Async Worker

Graph shape:

```text
source --immediate--> worker --async(queue capacity=1, drop_oldest)--> join
```

Run:

```bash
./build/topoexec_app_async_worker
```

Expected output:

```text
task_ready_epoch=2
async_publication_count=4
async_drop_count=1
channel_drop_count=2
```

This app demonstrates the current async edge contract. Worker completions are staged for a later epoch, so the join component runs on a `task_ready` event in epoch 2. The async channel is bounded and drops stale completions according to `drop_oldest`.

Contrast case: this is not a threaded worker pool yet. It proves deferred async visibility and bounded queue behavior; real threaded scheduling and max-inflight policy are future scope.
