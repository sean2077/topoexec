# Composite Solver

Reference-app slice for an iterative solver owned by a declared CompositeLoop:

```text
source --immediate--> estimator --immediate--> controller --immediate--> sink
                         ^                         |
                         |-------immediate---------|
```

Run after building examples:

```bash
./build/topoexec_app_composite_solver
```

Stable output:

```text
converged_iteration_count=2
loop_converged_count=1
solver_residual=0.05
budget_overrun_count=1
```

The first run uses `loop_policy.type: solver_iteration`, component-reported
residuals, and `residual_threshold: 0.1` to stop before `max_iterations`. The
second run swaps in a slow estimator and a 1 ms loop budget to show
`runtime.loop.budget_overrun` evidence without adding threads or external solver
libraries.
