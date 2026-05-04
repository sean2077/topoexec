# TopoExec Next-Stage Plan

Version: `plan-2026-05-04-refresh-2`  
Target repository: `sean2077/topoexec`  
Reviewed branch: `main`  
Latest visible commit on GitHub: `9877209 Tighten trigger policy cleanup boundaries`  
Review basis: public GitHub files visible from the browser on 2026-05-04. I could not clone/build the repository from this environment, so all build/test statements below are based on repository evidence and should be independently reproduced by CI or a local checkout.

---

## 1. Executive Summary

TopoExec has already completed much of the earlier “runtime first, apps second, tools third” plan. The current repository is no longer just a prototype schema or CLI wrapper. It already contains:

- C++20 single-process stateful dataflow runtime positioning.
- Runtime semantics documentation for epoch, transaction, commit, edge visibility, trigger readiness, and CompositeLoop ownership.
- Strict schema v1 with `immediate`, `delay`, `state`, and `async` edge kinds.
- Split CMake targets: `topoexec::core`, `topoexec::runtime`, and `topoexec::yaml`.
- Optional YAML layer and CLI tooling.
- Runtime modules for graph, channel, trigger policy, event runtime, scheduler, runner, metrics, trace, component registry, payload, and buffer handling.
- Runnable examples for minimal pipeline, latest-vs-queue overload, delay feedback, CompositeLoop fixed-point, async worker, and pure C++ graph builder usage.
- CI workflow, `scripts/agent_check.sh`, `.clang-format`, `.editorconfig`, `AGENTS.md`, versioning docs, release checklist, metrics docs, and trace docs.
- An implementation audit claiming 22/22 CTest tests passed for the current runtime/spec gap-closing phase.

Therefore the next phase should **not** focus on adding more CLI commands or more demo graphs. The next phase should focus on:

```text
API stability -> runtime invariants -> scheduler/concurrency -> async admission -> release hardening -> docs/onboarding -> adapters later
```

The strategic direction should be:

```text
small + embeddable + C++20-first + semantic + testable + observable
```

TopoExec’s core differentiator remains:

```text
explicit semantics for edge visibility, feedback, state, trigger readiness,
bounded channels, CompositeLoop ownership, and runtime observability inside
one ordinary process.
```

---

## 2. Current Completion Matrix Compared With The Earlier Plan

| Earlier planned area                            |                 Current status | Next action                                                                                |
| ----------------------------------------------- | -----------------------------: | ------------------------------------------------------------------------------------------ |
| Runtime semantics doc                           |          Done enough for alpha | Keep stable; add cross-references and invariant examples                                   |
| Schema v1 edge kinds                            |                    Implemented | Preserve compatibility; add schema regression tests                                        |
| Graph compiler / SCC / CompositeLoop validation | Implemented according to audit | Add property/randomized graph tests                                                        |
| Event runtime / compiled plan execution         |           Implemented baseline | Harden stop/error/lifecycle behavior                                                       |
| Publication staging                             |                    Implemented | Strengthen transaction-boundary tests                                                      |
| Delay/state/async visibility                    |           Implemented baseline | Add mixed-edge and multi-epoch stress tests                                                |
| Trigger engine                                  |            Broadly implemented | Harden cleanup boundaries, time-sync, batch-window, coalesce, min-interval edge cases      |
| Channel modes / overflow / copy policy          |            Broadly implemented | Add stress tests, metrics consistency tests, and ownership docs                            |
| CompositeLoop runtime                           |           Implemented baseline | Replace stringly convergence policy with typed or pluggable convergence hooks              |
| Runnable apps                                   |                    Implemented | Keep as tutorials; avoid demo sprawl                                                       |
| Metrics and trace                               |           Implemented baseline | Add component duration/latency metrics and golden output tests                             |
| CLI run/metrics/trace/lint/explain/diff/bench   |              Implemented early | Freeze command set; improve output stability only                                          |
| CMake target split                              |           Implemented baseline | Verify downstream package consumption and dependency isolation                             |
| CI baseline                                     |           Implemented baseline | Add sanitizer CI and package install smoke in CI if not already covered by workflow output |
| Public API                                      |                        Partial | Define stable vs internal API; improve error/status model                                  |
| Worker-pool scheduling                          |                Not implemented | Main next runtime feature after invariants are locked                                      |
| Async max-inflight                              |                       Deferred | Implement as explicit admission control, not just channel overflow                         |
| ROS/OTel/Prometheus/Python adapters             |                       Deferred | Keep deferred until beta core stabilizes                                                   |

---

## 3. Highest-Value Gaps To Fix Now

### Gap 1: Public C++ API needs a stability boundary

Current docs identify `topoexec::runtime` and `topoexec::yaml` as user-facing targets, and `docs/api-overview.md` explains the basic Component/Registry/GraphSpec usage. However, the public API surface is still in a pre-1.0 state. The current `Component` lifecycle hooks are still `void`-style, and `execute()` is also `void`; runtime error reporting mostly depends on exceptions or accumulated runner errors.

#### Why this matters

If users start embedding TopoExec, the most important questions will be:

- Which headers/classes are stable?
- Which classes are internal runtime implementation details?
- How does a component report failure without throwing?
- How does lifecycle failure propagate to `RuntimeRunnerResult`?
- What is the stable shape of `Invocation` and `GraphContext`?
- How should users access typed payloads safely?

#### Recommended direction

Add an API stabilization track:

1. Create `docs/public-api.md` or expand `docs/api-overview.md`.
2. Categorize public API by stability:
   - stable in `0.1.x`;
   - experimental before beta;
   - internal/implementation detail.
3. Add `Status` or `Result<T>` incrementally, not as a sweeping rewrite.
4. Start with optional status-returning extension methods or adapter helpers:

```cpp
virtual Status configure2(GraphContext&, const ConfigView&);
virtual Status execute2(const Invocation&, GraphContext&);
```

or a cleaner breaking-change candidate before beta:

```cpp
virtual Status configure(GraphContext&, const ConfigView&) = 0;
virtual Status execute(const Invocation&, GraphContext&) = 0;
```

5. Add tests for:
   - configure failure;
   - activate failure;
   - execute failure;
   - deactivate/cleanup after partial startup;
   - runtime result error messages.

#### Acceptance criteria

- `docs/api-overview.md` states what is stable and what is experimental.
- Public API examples compile against `topoexec::runtime` only.
- Component failure is observable without requiring exceptions.
- Existing apps and tests still pass.

---

### Gap 2: Scheduler lane semantics are schema-visible but not yet product-grade runtime behavior

The schema supports lane types such as `event_loop`, `fixed_rate`, and `thread_pool`, and scheduler structs expose fields like `max_threads`, priority, affinity, RT policy, and metrics. Current release notes still list threaded worker-pool scheduling as not implemented. The runtime should not overclaim lane behavior until it is real.

#### Why this matters

Scheduler semantics are central to TopoExec’s value. Users need to know:

- Does `thread_pool` actually run components concurrently?
- What does `reentrant: false` enforce?
- Can two invocations of the same component overlap?
- Is queue capacity bounded?
- What happens on stop/cancel while worker tasks are running?
- Are priority/affinity/RT fields advisory, ignored, or enforced?

#### Recommended direction

Create a scheduler hardening milestone before beta:

1. Add `docs/scheduler.md`.
2. Explicitly define current lane semantics:
   - `event_loop`: deterministic single-thread execution;
   - `fixed_rate`: simulated or runtime tick semantics;
   - `thread_pool`: currently unsupported or experimental until implemented.
3. Implement bounded `thread_pool` MVP only after invariant tests pass.
4. Enforce component reentrancy:
   - non-reentrant component: at most one in-flight invocation;
   - reentrant component: concurrent invocations allowed only within lane bounds.
5. Add deterministic shutdown:
   - stop token requested;
   - no new tasks accepted;
   - pending tasks drained/cancelled according to policy;
   - timeout recorded if cleanup exceeds limit.
6. Add TSAN CI after real concurrency lands.

#### Acceptance criteria

- `thread_pool` behavior is either documented as unsupported or implemented with tests.
- `reentrant: false` is enforced by runtime, not merely stored in schema.
- Worker queue capacity is bounded.
- Stop/cancel behavior is deterministic.
- Metrics expose queue depth, active workers, in-flight count, rejected count, and completed count.

---

### Gap 3: Async max-inflight needs explicit admission control

Current docs say richer async worker-pool `max_inflight` policy is future scope, and async backpressure is currently enforced through bounded channel capacity and overflow policy. That is acceptable for alpha, but not enough for beta if TopoExec wants to be credible for async pipelines.

#### Why this matters

For real workloads, async behavior needs to answer:

- How many tasks may be in flight?
- What happens when a new task arrives while old tasks are still running?
- Does the runtime drop old pending work, reject new work, or coalesce?
- How are completed tasks delivered back into the graph?
- What happens during shutdown?

#### Recommended direction

Add an async admission controller separate from channel overflow:

```yaml
execution:
  lane: async_pool
  reentrant: true

trigger_policy:
  type: task_ready

async_policy:
  max_inflight: 2
  queue_capacity: 4
  overflow: drop_oldest | drop_newest | reject | block
  completion_event: task_ready
```

Implementation shape:

```text
input event -> admission controller -> async worker/future -> completion event -> async/state/delay edge -> downstream trigger
```

#### Acceptance criteria

- `max_inflight` is enforced independently of channel capacity.
- Completion is delivered as a later event, never same-call-stack recursion.
- Metrics include accepted, rejected, dropped, in-flight, completed, cancelled.
- Stop token cancels or drains pending tasks deterministically.
- Async worker app demonstrates overload behavior.

---

### Gap 4: Runtime invariant tests should become the safety net for all future work

The implementation audit already lists many named tests, but the next value is invariant testing rather than more happy paths.

#### Required invariants

Add or strengthen tests for:

1. `GraphContext::publish()` never directly calls downstream `execute()`.
2. `immediate` is the only edge kind participating in immediate SCC rejection.
3. `delay`, `state`, and `async` are not visible in the publishing transaction.
4. CompositeLoop components have exactly one scheduling owner.
5. CompositeLoop external outputs commit only at region boundary.
6. Multiple state writers to one target remain invalid until merge policy exists.
7. `move_only` remains invalid for multi-reader edges.
8. All runtime channels are bounded.
9. Drop/overwrite/fail/block behavior increments metrics deterministically.
10. Lifecycle cleanup happens after stop-token shutdown and component errors.
11. `run_until_idle` stops only after a full iteration executes no components.
12. Branching immediate DAG region order is deterministic.

#### Property/randomized tests

Add a lightweight property-test style suite without adding new dependencies if possible:

- generate random DAGs and verify deterministic compiled region order;
- generate random immediate cycles and verify rejection unless exactly declared;
- generate random non-immediate feedback and verify no immediate SCC rejection;
- generate overlapping cycles and verify they collapse into one SCC/CompositeLoop owner.

#### Acceptance criteria

- Each invariant has a focused test.
- Tests assert behavior and metrics/error text where possible.
- Tests run under both Debug and RelWithDebInfo CI.

---

### Gap 5: Observability exists, but needs component-level latency and golden contracts

Metrics and trace docs exist and already define runtime, channel, publication, loop, and trace output. However, component execution latency and trigger/scheduler timing should be made more useful for real debugging.

#### Recommended additions

Metrics:

```text
runtime.component.execution_count
runtime.component.error_count
runtime.component.last_duration_ns
runtime.component.max_duration_ns
runtime.component.budget_overrun_count
runtime.trigger.ready_count
runtime.trigger.suppressed_count
runtime.trigger.coalesced_count
runtime.scheduler.queue_depth
runtime.scheduler.active_count
runtime.scheduler.in_flight_count
```

Trace:

- component execute span with real `duration_ns`;
- scheduler iteration span;
- composite loop span;
- async admission and completion events;
- channel commit attributes with edge kind.

Golden outputs:

- `examples/minimal.yaml` metrics JSON;
- delay feedback trace;
- CompositeLoop trace;
- async worker metrics.

#### Acceptance criteria

- Metrics names are documented and stable.
- Trace JSON has golden tests.
- Chrome Trace output opens in common trace viewers.
- Component duration and budget overrun are visible without reading logs.

---

### Gap 6: Payload ownership and typed access need better ergonomics

The current runtime has copy policies: `copy`, `shared_view`, `loaned_view`, and `move_only`. It also rejects large payload copy and preserves loaned buffer evidence according to the audit. This is a strong foundation, but users still need ergonomic and safe payload APIs.

#### Recommended direction

1. Define payload categories:
   - text/string;
   - binary/blob;
   - frame/buffer;
   - typed object;
   - external immutable view;
   - loaned buffer.
2. Add typed helpers:

```cpp
auto text = invocation.payload_as<TextPayload>();
auto frame = invocation.payload_as<FramePayload>();
auto maybe = invocation.try_payload_as<MyType>();
```

3. Document ownership rules:
   - when copy occurs;
   - when shared view is safe;
   - when loaned view is valid;
   - who owns buffer lifetime;
   - when move-only is legal.
4. Add tests for invalid type access, lifetime safety, and copy counters.

#### Acceptance criteria

- Users can write component code without manual casting boilerplate.
- Large payload paths do not accidentally copy.
- Bad type access produces clear error/status.
- Copy metrics match expected payload policy.

---

### Gap 7: Schema docs are good, but scheduler/API docs are still missing

`docs/schema-v1.md`, `docs/runtime-semantics.md`, `docs/metrics.md`, `docs/trace-events.md`, `docs/versioning.md`, and `docs/release-checklist.md` exist. `docs/api-overview.md` also exists. However, `docs/scheduler.md` and `docs/faq.md` appear to be missing.

#### Recommended docs

Add:

```text
docs/scheduler.md
docs/public-api.md or expanded docs/api-overview.md
docs/payloads.md
docs/faq.md
docs/concurrency.md, if worker_pool lands
docs/adapters.md, if adapter design starts
```

README should stay concise and link to deeper docs:

```text
Quickstart
What TopoExec is
What TopoExec is not
Core concepts
C++ builder example
YAML example
Runtime semantics
Examples/tutorials
Known limitations
Release status
```

#### Acceptance criteria

- A new user can build and run the C++ builder app without reading source.
- A new user can choose edge kinds correctly after reading docs.
- Known limitations are explicit and match changelog/release notes.

---

### Gap 8: Release readiness is close, but not complete

The repository has `CHANGELOG.md`, `docs/versioning.md`, and `docs/release-checklist.md`, and the target is `v0.1.0-alpha`. Release checklist items still need to be executed against the final commit to be tagged.

#### Recommended release path

For `v0.1.0-alpha`:

1. Do not add major new features.
2. Finish documentation cleanup.
3. Run CI matrix.
4. Run package smoke test.
5. Run local artifact smoke build from clean checkout.
6. Confirm known limitations in changelog:
   - no threaded worker-pool scheduling;
   - no async max-inflight admission controller;
   - sanitizer CI planned but not required;
   - adapters deferred.
7. Tag only after release checklist is complete.

For `v0.1.0-beta`:

1. Add sanitizer CI.
2. Add scheduler semantics doc.
3. Add worker-pool MVP or explicitly mark lane as unsupported.
4. Add API stability docs.
5. Add performance baselines.

#### Acceptance criteria

- Alpha can be installed and consumed by a downstream project.
- Docs match implementation and limitations.
- No public release tag is moved or retagged.

---

### Gap 9: Performance evidence should move from CLI bench to useful baselines

The CLI has a `bench` command, but users will ask what the runtime overhead actually is.

#### Benchmark suite

Add optional benchmarks for:

1. single component invocation overhead;
2. immediate chain length N;
3. fan-out/fan-in graph;
4. latest overwrite throughput;
5. bounded queue throughput;
6. delay edge epoch overhead;
7. shared/loaned large payload no-copy path;
8. CompositeLoop iteration overhead;
9. trigger policies: any/all/batch/time-sync;
10. worker-pool throughput once implemented.

Avoid mandatory heavy benchmark dependencies in core. Either use a small internal benchmark harness or keep benchmark dependencies optional.

#### Acceptance criteria

- Bench output is machine-readable.
- README includes representative results with environment info.
- Benchmarks are not required in normal unit-test CI.

---

### Gap 10: Adapter design should stay deferred, but define extension boundaries now

Adapters should remain deferred until beta core stabilizes, but it is useful to define extension boundaries now so core does not accidentally absorb adapter assumptions.

Candidate adapters after beta:

- ROS 2 adapter:
  - ROS topic -> TopoExec input boundary;
  - TopoExec output boundary -> ROS topic;
  - parameter/lifecycle mapping;
  - no ROS dependency in core/runtime.
- OpenTelemetry/Prometheus:
  - metrics and spans export;
  - no OTEL/Prometheus dependency in core.
- Python binding:
  - configuration/testing/scripting first;
  - not high-performance data path initially.
- Perfetto adapter:
  - richer trace export and metadata.

Acceptance criteria:

- Adapter interfaces use stable runtime metrics/trace/channel APIs.
- Core/runtime does not include adapter-specific fields unless useful generically.

---

## 4. Recommended Milestones

## Milestone 0 — Freeze Feature Sprawl And Confirm Baseline

Goal: treat current implementation as the alpha candidate baseline.

Tasks:

1. Record current commit, CI status, local build/test commands, and known limitations in `docs/current-baseline.md`.
2. Ensure `scripts/agent_check.sh` runs in a clean checkout.
3. Verify CI matrix actually runs and passes for GCC/Clang and Debug/RelWithDebInfo.
4. Do not add new CLI commands.
5. Do not add adapters.

Acceptance:

- `docs/current-baseline.md` exists.
- CI is green.
- Local or CI evidence records build/test result.
- All known limitations match `CHANGELOG.md` and `release-checklist.md`.

Suggested branch:

```text
agent/current-baseline
```

---

## Milestone 1 — Public API Stability

Goal: make TopoExec understandable and safe to embed.

Tasks:

1. Expand `docs/api-overview.md` into public/internal API reference.
2. Add `docs/payloads.md`.
3. Audit lifecycle and `execute()` error propagation.
4. Decide whether to add `Status` return types before beta.
5. Add typed payload helpers.
6. Add tests for bad payload access and component failure status.

Acceptance:

- Public API surface is documented.
- Examples use public API only.
- Component errors are observable in `RuntimeRunnerResult`.
- No YAML dependency is required for pure C++ usage.

Suggested branches:

```text
agent/public-api-overview
agent/payload-api-helpers
agent/component-status-propagation
```

---

## Milestone 2 — Runtime Invariant Hardening

Goal: make runtime semantics trustworthy before concurrency work.

Tasks:

1. Add invariant test suite for publish/epoch/edge visibility.
2. Add CompositeLoop ownership and boundary-commit tests.
3. Add lifecycle cleanup tests.
4. Add graph SCC randomized/property tests.
5. Add metrics consistency tests.

Acceptance:

- Each core semantic has positive and negative tests.
- Error messages identify the graph entity when possible.
- Tests are deterministic.

Suggested branches:

```text
agent/runtime-invariant-tests
agent/graph-property-tests
agent/metrics-consistency-tests
```

---

## Milestone 3 — Scheduler And Async Runtime Maturity

Goal: implement real concurrency deliberately.

Tasks:

1. Add `docs/scheduler.md`.
2. Clarify unsupported vs supported lane fields.
3. Implement bounded worker-pool lane MVP.
4. Enforce `reentrant` rules.
5. Implement async `max_inflight` admission controller.
6. Add cancellation and pending task cleanup.
7. Add TSAN CI after worker pool lands.

Acceptance:

- Worker pool has bounded queue and deterministic shutdown.
- Non-reentrant components cannot overlap.
- Async completion is a later event.
- Metrics expose queue/in-flight/rejection state.

Suggested branches:

```text
agent/scheduler-docs
agent/worker-pool-mvp
agent/async-max-inflight
agent/tsan-ci
```

---

## Milestone 4 — Observability Productization

Goal: make runtime behavior debug-friendly.

Tasks:

1. Add component execution metrics.
2. Add trigger suppression/coalesce metrics.
3. Add real duration spans for trace events.
4. Add golden tests for JSON metrics and trace.
5. Keep CLI output stable.

Acceptance:

- Component durations are visible.
- Budget overruns are visible.
- Trace output is stable and documented.
- Golden tests prevent accidental JSON contract breaks.

Suggested branches:

```text
agent/component-metrics
agent/trace-duration-spans
agent/metrics-golden-tests
```

---

## Milestone 5 — Documentation And Onboarding

Goal: make the project usable without reading source.

Tasks:

1. Rewrite README into a concise product/onboarding page.
2. Add `docs/faq.md`.
3. Add/expand `docs/scheduler.md`, `docs/payloads.md`, and `docs/public-api.md`.
4. Ensure every runnable app README has:
   - graph shape;
   - run command;
   - expected output;
   - semantic lesson;
   - contrast/failure case.
5. Add architecture diagram, preferably Mermaid.

Acceptance:

- New user can understand the project in 15 minutes.
- New user can run examples in under 10 minutes after dependencies are installed.
- Known limitations are easy to find.

Suggested branches:

```text
agent/readme-onboarding
agent/faq-docs
agent/scheduler-payload-docs
```

---

## Milestone 6 — Alpha Release

Goal: tag first useful open-source prerelease.

Tasks:

1. Complete `docs/release-checklist.md`.
2. Update `CHANGELOG.md` with final alpha notes.
3. Verify `docs/versioning.md` version/tag policy.
4. Run clean checkout artifact smoke build.
5. Run install/package smoke test.
6. Tag `v0.1.0-alpha` with an annotated tag.

Acceptance:

- Release checklist is fully checked.
- CI is green.
- Package smoke test passes.
- Known limitations are explicit.

Suggested branch:

```text
agent/v0.1.0-alpha-release
```

---

## Milestone 7 — Beta Runtime Maturity

Goal: prepare for wider users.

Tasks:

1. Land worker-pool MVP or explicitly mark it unsupported in schema/docs.
2. Land async max-inflight or explicitly defer to `v0.2`.
3. Add sanitizer CI.
4. Add benchmark baselines.
5. Stabilize API docs and examples.
6. Decide if `Status` API migration is required before beta.

Acceptance:

- Beta users can embed runtime with clear API.
- Concurrency semantics are either real or not exposed as production behavior.
- Performance baselines exist.
- Docs and code limitations match.

Suggested branch:

```text
agent/v0.1.0-beta-hardening
```

---

## 5. Prioritized Next 12 Pull Requests

1. `baseline: record current repo baseline and CI evidence`
2. `docs: clarify public API stability and internal surfaces`
3. `payload: add typed payload helpers and ownership docs`
4. `tests: add runtime invariant suite for edge visibility and publish staging`
5. `tests: add graph SCC randomized/property coverage`
6. `tests: add metrics consistency golden checks`
7. `docs: add scheduler lane semantics`
8. `runtime: enforce reentrant ownership rules in scheduler`
9. `runtime: implement bounded worker-pool lane MVP`
10. `runtime: implement async max-inflight admission controller`
11. `observability: add component duration metrics and trace spans`
12. `release: complete v0.1.0-alpha checklist and tag`

---

## 6. Rules For Future AI-Agent Iteration

1. Prefer tests and invariant hardening over new features.
2. Do not add new CLI commands unless they support an existing runtime contract.
3. Do not add ROS, Python, OTEL, Prometheus, or other adapters before alpha/beta core stabilizes.
4. Do not introduce new runtime dependencies without an explicit documented reason.
5. Keep YAML/CLI optional; `topoexec::runtime` must remain embeddable.
6. If a task touches public API, write a short API migration note.
7. If a task touches concurrency, add deterministic tests and consider TSAN.
8. If ambiguity affects public API, stop and write a blocker.
9. If ambiguity is reversible, choose the smallest implementation and document the assumption.
10. Every PR must state:
    - files changed;
    - semantic risk;
    - tests run;
    - known limitations;
    - follow-up tasks.

---

## 7. Definition Of Done For The Next Stage

The next stage is complete when:

- Current baseline is reproducible in CI.
- Public API surface is documented.
- Core runtime can be used without YAML/CLI.
- Runtime invariants are test-backed.
- Scheduler semantics are documented.
- Worker-pool and async max-inflight are either implemented with tests or explicitly deferred.
- Metrics and trace outputs have golden tests.
- Runnable examples remain tutorials, not just test fixtures.
- Release checklist is complete.
- `v0.1.0-alpha` is tagged, or there is a clear blocker document explaining why not.

---

## 8. What Not To Do Next

Do not spend the next development cycle on:

- more CLI commands;
- ROS 2 adapter;
- Python binding;
- distributed runtime;
- GUI/low-code editor;
- more example apps beyond the current semantic set;
- broad rewrites of graph/runtime just for aesthetics;
- worker-pool implementation without first documenting scheduler semantics.

---

## 9. Strategic Closing

TopoExec has already become much more than “topological execution.” The next stage should make it credible as an embeddable library. The most important work is not feature count; it is trust:

```text
Can users trust the edge semantics?
Can users trust feedback loops will not recurse accidentally?
Can users trust overload behavior is bounded and observable?
Can users embed the runtime without unwanted dependencies?
Can users understand the public API without reading source?
Can users debug timing, drops, and loop behavior from metrics/trace?
```

If the answer becomes yes, TopoExec has a clear niche: a compact C++20 semantic execution graph runtime for stateful, feedback-aware, observable in-process systems.
