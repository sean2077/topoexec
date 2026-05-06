# Design Principles

These principles guide runtime/API changes and should be checked during reviews.

## Bounded everything

Queues, input sizes, health buffers, task admission, loop iterations, fuzz runs,
stress runs, and benchmark baselines must have explicit bounds. If a bound is
missing, add one before treating the feature as release-candidate quality.

## Explicit feedback

Feedback is either delayed to an epoch boundary or declared as a bounded
CompositeLoop. Hidden recursion through `publish()` is not allowed because it
makes ordering, diagnostics, and failure behavior hard to reason about.

## No hidden recursion

Components publish data; the runtime routes it after the component returns.
Downstream components are never called directly from `GraphContext::publish()`.
This keeps stack depth, ordering, and error attribution observable.

## Observation is not control

Metrics, trace, diagnostics, health events, observers, and benchmark baselines are
for evidence. They must not introduce recursive control flow or silently change
runtime behavior unless a future product decision explicitly adds such a policy.
