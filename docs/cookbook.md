# Cookbook

These recipes are small, executable patterns for choosing TopoExec semantics.
They use checked-in examples and CLI commands so agents can verify the recipe
still matches the runtime.

## Low-latency latest pipeline

Use `latest` when the newest state sample should replace stale samples and the
consumer should not drain a backlog.

```bash
./build/topoexec graph metrics benchmarks/latest_vs_queue.yaml --steps 2 --format json
```

<!-- topoexec-doc-test: ${TOPOEXEC} graph metrics ${SOURCE_DIR}/benchmarks/latest_vs_queue.yaml --steps 2 --format json -->

Checklist:

- Prefer `mode: latest` for sensor/state snapshots.
- Record drop/stale behavior through metrics or health events.
- Do not use `latest` for commands that must be processed in order.

## Bounded queue command stream

Use a bounded `queue` with explicit `capacity` and `overflow` for command/event
streams where order matters but unbounded backlog is not acceptable.

```bash
./build/topoexec graph run examples/boundary_adapter_pattern.yaml --steps 1
```

<!-- topoexec-doc-test: ${TOPOEXEC} graph run ${SOURCE_DIR}/examples/boundary_adapter_pattern.yaml --steps 1 -->

Checklist:

- Set `capacity` explicitly.
- Pick `drop_oldest`, `drop_newest`, `reject`, or `fail_fast` intentionally.
- Watch queue-depth and rejection metrics before increasing capacity.

## Delay feedback control

Use `delay` when feedback must be visible in the next epoch rather than through
hidden recursion.

```bash
./build/topoexec graph run examples/control_feedback_delay.yaml --steps 2
```

<!-- topoexec-doc-test: ${TOPOEXEC} graph run ${SOURCE_DIR}/examples/control_feedback_delay.yaml --steps 2 -->

Checklist:

- Keep the feed-forward path immediate.
- Put feedback on `delay` unless an exact bounded CompositeLoop owns the SCC.
- Verify order with `graph trace` when changing loop boundaries.

## CompositeLoop solver

Use `composite_loops` only when an immediate feedback SCC is intentional and the
iteration policy is bounded.

```bash
./build/topoexec graph plan examples/composite_loop.yaml --format json
```

<!-- topoexec-doc-test: ${TOPOEXEC} graph plan ${SOURCE_DIR}/examples/composite_loop.yaml --format json -->

Checklist:

- The loop component set must exactly match one immediate SCC.
- Set `max_iterations` and budget fields before treating it as solver-like work.
- Keep external publication committed after the loop, not recursively inside it.

## Async request/response

Use async/task-ready flow for request/response work that should be deferred and
bounded rather than executed recursively.

```bash
./build/topoexec graph run examples/service_pipeline.yaml --steps 2
```

<!-- topoexec-doc-test: ${TOPOEXEC} graph run ${SOURCE_DIR}/examples/service_pipeline.yaml --steps 2 -->

Checklist:

- Model readiness with `task_ready` event sources/triggers.
- Bound `max_inflight` and queue capacity.
- Treat failures and drops as observable runtime events.

## State/config snapshot

Use `state` edges and config transactions for committed snapshots, not mutable
globals shared during an epoch.

```bash
./build/topoexec graph run examples/state_config_snapshot.yaml --steps 2
```

<!-- topoexec-doc-test: ${TOPOEXEC} graph run ${SOURCE_DIR}/examples/state_config_snapshot.yaml --steps 2 -->

Checklist:

- Expect state visibility after an epoch boundary.
- Keep config validation/apply hooks deterministic and rollback-safe.
- Avoid multiple writers until a merge policy is explicitly implemented.

## Large payload ownership

Use copy/shared/loaned policy deliberately. Large in-process payloads can be made
observable, but external shared-memory zero-copy is still out of scope.

```bash
./build/topoexec graph lint examples/large_payload_copy.yaml
```

<!-- topoexec-doc-test: ${TOPOEXEC} graph lint ${SOURCE_DIR}/examples/large_payload_copy.yaml -->

Checklist:

- Copy small payloads freely; lint large-copy paths.
- Use `shared_view` when readers share immutable in-process data.
- Use `loaned_view` only with explicit producer/pool ownership.
