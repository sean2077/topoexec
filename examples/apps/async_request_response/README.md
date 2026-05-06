# Async Request/Response

Reference-app slice for an in-process request boundary, validator, task executor,
and response boundary:

```text
request_boundary --queue--> validator --task completion--> response_boundary
```

Run after building examples:

```bash
./build/topoexec_app_async_request_response
```

Stable output:

```text
request_payload=req-1
response_payload=accepted:req-1
executor_completed_count=1
```

The validator uses `GraphContext::submit_task()` with the deterministic
`TaskExecutor`. The completion callback publishes through a normal TopoExec
channel; it does not call the response boundary directly and does not require an
external service adapter.
