# Control Feedback Delay

Graph shape:

```text
sensor --immediate--> estimator --immediate--> controller --immediate--> actuator
controller --delay--> estimator
```

Run:

```bash
./build/topoexec_app_control_feedback_delay
```

Expected output:

```text
delayed_correction_epoch=2
channel_delivery_count=7
runtime_publication_delayed=2
```

This app demonstrates delay feedback. The controller publishes a correction back to the estimator, but the `delay` edge keeps that feedback out of the current transaction. The estimator observes the correction in epoch 2, not recursively during epoch 1.

Contrast case: making the feedback edge `immediate` would create an immediate cycle and require a matching `composite_loops[]` declaration.
