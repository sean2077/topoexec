# CompositeLoop Fixed Point

Graph shape:

```text
source --immediate--> estimator --immediate--> controller --immediate--> sink
                         ^                         |
                         |---------immediate-------|
```

The immediate feedback SCC is declared as:

```yaml
composite_loops:
  - id: estimator_controller_loop
    components: [estimator, controller]
    loop_policy: {type: fixed_point, max_iterations: 2}
```

Run:

```bash
./build/topoexec_app_composite_loop_fixed_point
```

Expected output:

```text
undeclared_loop_rejected=true
loop_iteration_count=2
loop_max_iteration_hit_count=1
```

This app demonstrates CompositeLoop ownership. The same graph is first validated without the loop declaration to prove the immediate cycle is rejected. With the declaration restored, the runtime executes the loop region through the loop owner and reports loop metrics.

Contrast case: a partial loop declaration, or a loop declaration that does not exactly match the immediate SCC, is invalid.
