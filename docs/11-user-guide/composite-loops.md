# CompositeLoop Regions

A `composite_loops[]` entry is an explicit owner for one immediate cyclic strongly connected component (SCC). It is not a decorator: validation rejects partial loop declarations, decorative loop declarations, duplicate ownership, and immediate cycles without an exact loop owner.

## Region model

The graph compiler condenses an accepted loop SCC into one CompositeLoop region. The runtime executes components inside that region in compiled order for a bounded number of loop-local iterations.

Current loop policies are intentionally small:

- `max_iterations`: hard bound; omitted or non-positive means one iteration at runtime.
- `convergence`: `single_pass`, `after_first_iteration`, or `always` stops after one iteration and records convergence; `stable_state` remains an accepted advisory value for existing examples.
- `solver_iteration`: a bounded policy for iterative components. Components can
  call `GraphContext::report_loop_convergence({converged, residual, reason})`
  during execution. The runtime exposes the loop id, zero-based iteration index,
  one-based iteration number, policy type, and max iteration count through
  `GraphContext::loop_iteration`.
- `residual_threshold`: optional non-negative threshold. A reported residual at
  or below this threshold marks a `solver_iteration` loop converged even when the
  report did not set `converged`.
- `partial_success`: `commit_outputs`, `discard_outputs`, or `fail_run`.
  `solver_iteration` defaults to `discard_outputs` when it stops without
  convergence; existing fixed-point/transaction-style policies default to
  `commit_outputs` for compatibility.
- `budget_ms`: stops the loop when the cumulative loop duration exceeds the budget.
- cooperative cancellation: a requested cancellation is observed between loop
  iterations, never by interrupting the component currently executing.

No dynamic solver plugins or external optimization libraries are involved; the
typed report API is in-process component code.

## Transaction and visibility

Publications inside the loop still go through `GraphContext::publish()` and the runtime publication router. Outputs from loop-owned components to components outside the loop are staged as CompositeLoop external outputs and committed only after the loop finishes successfully, or after an explicit `partial_success: commit_outputs` policy allows a non-converged solver stop to publish them.

If an internal component fails, the loop stops with `SchedulerStopReason::kError`, records `runtime.loop.error`, emits a `loop_error` trace event, and does not commit staged external loop outputs. This prevents downstream components from observing half-updated loop state.

## Metrics and trace

CompositeLoop metrics use the loop id as `component_id` in `RuntimeMetricSample`:

- `runtime.loop.iterations`
- `runtime.loop.converged`
- `runtime.loop.budget_overrun`
- `runtime.loop.max_iterations_hit`
- `runtime.loop.error`
- `runtime.loop.cancellation_requested`
- `runtime.loop.cancellation_observed`
- `runtime.loop.output_discarded`
- `runtime.loop.residual`

Trace events:

- `loop_iteration_begin`
- `loop_iteration`
- `loop_iteration_end`
- `loop_converged`
- `loop_budget_overrun`
- `loop_max_iterations_hit`
- `loop_output_discarded`
- `loop_error`
- `loop_cancellation_requested`
- `loop_cancellation_observed`

`loop_iteration*` and `loop_converged` events may include `policy`, `residual`,
`residual_threshold`, and `reason` attributes. These events are sufficient to
explain bounded iteration, convergence, partial-output discard, budget stop,
cooperative cancellation, and internal failure paths without source-level
debugging.
