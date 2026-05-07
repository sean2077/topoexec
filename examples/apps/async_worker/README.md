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
async_overwrite_count=2
channel_drop_count=0
channel_overwrite_count=1
```

This app demonstrates the async edge contract. Worker completions are staged for a later epoch, so the join component runs on a `task_ready` event in epoch 2. The async admission path is bounded and overwrites older pending completions according to `drop_oldest`.

Contrast case: this app uses async edge delivery, not a `thread_pool` lane. Use `policy.max_inflight` on an `async` edge when admission must be limited before channel capacity.
