# Control Loop with State

Reference-app slice for a deterministic control loop:

```text
sensor --immediate--> estimator --immediate--> controller --immediate--> actuator
estimator --state--> controller
controller --delay--> estimator
```

Run after building examples:

```bash
./build/topoexec_app_control_loop_with_state
```

Stable output:

```text
state_snapshot_epoch=2
delayed_feedback_epoch=2
fixed_rate_trace=true
```

The graph uses a deterministic `fixed_rate` lane, a `state` edge for the
controller snapshot, and a `delay` feedback edge for estimator correction. Both
deferred paths become visible only in epoch 2.
