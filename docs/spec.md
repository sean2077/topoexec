# TopoExec AI-Agent Development Spec & Iteration Plan

Version: draft-0.2  
Date: 2026-05-02  
Priority model: **Core runtime first, applications second, tooling third**  
Project: `sean2077/topoexec`

---

## 0. Purpose

This document is intended to guide AI coding agents that iterate on TopoExec. The agent should treat TopoExec as a **general-purpose, single-process, in-process semantic graph runtime**, not as a ROS-only framework.

The main goal is to make TopoExec useful as a small embeddable C++20 runtime for composing stateful components with explicit semantics for:

- graph structure
- component lifecycle
- event and timer triggering
- internal channel policy
- feedback and cycles
- runtime scheduling
- overload/backpressure
- metrics and tracing

The immediate development priority is **not** to build a large CLI or lint platform first. Validation, lint, explain, and render are useful, but they should serve a runtime that already has clear and executable semantics.

---

## 1. Current Project Baseline

From the current repository shape, TopoExec already has these foundations:

- C++20 single-process stateful dataflow runtime positioning.
- Existing schema version 1 with `lanes`, `components`, `edges`, `composite_loops`, and required edge kinds: `immediate`, `delay`, `state`, `async`.
- Runtime-facing data structures for lane specs, event sources, trigger policies, execution specs, edge policies, edge kinds, composite loops, compiled regions, compiled graph plans, and dry-run results.
- Component API with `configure`, `activate`, `deactivate`, `execute`, `GraphContext`, `InputView`, `Invocation`, `EventKind`, and `TriggerKind`.
- Runtime channels with `latest`, queue-like, latched, previous-tick, barrier-like channel types; drop policies; copy policies; channel metrics; and wait-for-update support.
- Trigger engine with manual, timer, any-input, all-input, batch, request/task-ready style semantics partially represented.
- Runtime runner modes: validate, dry-run, run.
- CLI for validate, plan, and render.
- Tests for graph, channel, common utilities, and runtime.

Important interpretation: the project already has the **right vocabulary**. The next phase should make those semantics real, testable, and application-demonstrable.

---

## 2. Product Positioning

### 2.1 One-sentence positioning

**TopoExec is a lightweight C++20 in-process semantic execution graph runtime for composing stateful components with explicit trigger, channel, feedback, scheduling, and observability contracts.**

### 2.2 What TopoExec should be

TopoExec should be:

- a C++20 library first;
- embeddable inside a normal application process;
- domain-agnostic;
- usable without ROS, DDS, network middleware, or distributed orchestration;
- able to host small stateful components with clear input/output ports;
- able to compile and execute a semantic graph contract;
- able to detect or explicitly model feedback loops;
- able to make overload behavior explicit instead of hidden;
- able to expose useful runtime metrics.

### 2.3 What TopoExec should not be initially

TopoExec should not initially try to be:

- a distributed middleware;
- a workflow engine for long-running business jobs;
- a low-code GUI platform;
- a hard-real-time framework with formal timing guarantees;
- a replacement for oneTBB, GStreamer, Dora, GXF, ROS 2, or Drake;
- a multi-language process orchestration platform;
- a ROS-specific framework.

ROS can be supported later through an adapter layer. The core must remain generic.

---

## 3. Architecture Target

### 3.1 Module structure

Target structure:

```text
topoexec_core/
  graph model
  component interface
  port and schema descriptors
  edge kind and channel policy types
  trigger policy types
  compiled plan model
  status/error types

topoexec_runtime/
  runtime engine
  channel bus
  transaction/epoch engine
  trigger engine
  scheduler lanes
  event queue
  composite loop execution
  metrics and trace sink interfaces

topoexec_apps/
  small runnable applications and examples
  no external domain dependency by default

topoexec_tools/
  CLI wrapper around core/runtime
  validate, run, plan, render, later lint/explain

topoexec_adapters/
  optional adapters only
  ros2, OpenTelemetry, Prometheus, Perfetto, Python bindings later
```

The repo does not have to be physically split this way immediately, but AI agents should avoid coupling the core runtime to CLI, YAML, ROS, or any domain-specific dependency.

### 3.2 Dependency direction

Allowed:

```text
tools -> runtime -> core
apps  -> runtime -> core
adapters -> runtime -> core
```

Forbidden:

```text
core -> tools
core -> yaml-cpp
core -> ROS
core -> OpenTelemetry
runtime -> CLI
runtime -> app examples
```

`yaml-cpp`, `CLI11`, and serialization libraries should stay outside the core model when possible.

---

## 4. Core Semantics

### 4.1 Graph

A graph is a declarative contract containing:

- components;
- ports;
- edges;
- lanes;
- event sources;
- trigger policies;
- execution policies;
- optional composite loops;
- optional clock/time policy.

The graph should be compiled before runtime execution.

### 4.2 Component

A component is a stateful execution unit. It should not be assumed to be a process or ROS node.

Recommended interface:

```cpp
class Component {
public:
  virtual ~Component() = default;

  virtual ComponentDescriptor describe() const = 0;
  virtual Status configure(GraphContext& ctx, const ConfigView& config) = 0;
  virtual Status activate(GraphContext& ctx);
  virtual Status deactivate(GraphContext& ctx);
  virtual Status cleanup(GraphContext& ctx);

  virtual Status execute(const Invocation& invocation, GraphContext& ctx) = 0;
};
```

If the current API still uses `void` lifecycle methods, do not rewrite everything at once. Introduce `Status` only when a migration path and tests are ready.

### 4.3 Invocation

An invocation represents one scheduled execution of a component. It should contain:

- event kind;
- trigger kind;
- ready input ports;
- payloads by port;
- batch payloads if relevant;
- scheduled time;
- started time;
- event timestamp;
- sequence number;
- budget;
- lane;
- priority;
- cancellation/stop token.

A component should not decide whether it is ready. Readiness belongs to the trigger engine.

### 4.4 EdgeKind

Edge kind is not decorative. It must affect runtime behavior.

#### immediate

The output is available in the current transaction/epoch and participates in immediate dependency/SCC analysis.

Use for:

- normal feed-forward dependencies;
- algorithm-internal immediate dependency;
- explicitly declared composite loops.

#### delay

The output becomes visible only at the next epoch, next tick, or next transaction boundary.

Use for:

- feedback from previous frame;
- previous-tick state;
- sample-and-hold behavior;
- breaking feedback loops safely.

#### state

The output writes staged state. Readers see the current committed state until the next commit boundary.

Use for:

- configuration snapshots;
- latched state;
- blackboard-like shared state;
- slow-to-fast rate crossing.

#### async

The output produces an event that is delivered asynchronously and does not cause direct recursive execution.

Use for:

- worker completion;
- future-ready events;
- diagnostics;
- replan triggers;
- low-priority notifications.

### 4.5 ChannelPolicy

Internal channels must be bounded. No unbounded queue should be accepted in runtime-critical paths.

Minimum policies:

```text
mode:
  latest
  queue
  latched
  previous_tick
  barrier

overflow:
  overwrite
  drop_oldest
  drop_newest
  block
  fail_fast

copy_policy:
  copy
  shared_view
  loaned_view
  move_only
```

Rules:

- `latest + overwrite + capacity=1` is the default for low-latency sensor-like streams.
- `queue + bounded capacity` is used for events or commands that must preserve order.
- `block` must never be used inside a single-threaded hot path unless explicitly allowed.
- large payloads should reject `copy` unless the user explicitly opts in.
- all drop/overwrite/fail events should increment metrics.

### 4.6 TriggerPolicy

Do not add many virtual callbacks such as `on_tick`, `on_message`, `on_batch`, `on_sync`, etc. Use one `execute()` and express readiness through `TriggerPolicy`.

Minimum trigger types:

```text
manual
on_tick
time_periodic
any_input
all_inputs
batch
time_sync
request
task_ready
```

Initial runtime priority:

1. manual
2. timer/tick
3. any_input
4. all_inputs
5. batch
6. min_interval/coalesce
7. time_sync
8. request/task_ready

`time_sync` can be delayed until the channel and timestamp model are reliable.

### 4.7 Epoch and transaction model

TopoExec needs a clear time/commit model.

Definitions:

```text
Epoch:
  a bounded runtime step, often one scheduler iteration or one periodic tick.

Transaction:
  the set of component executions and staged publications processed under a scheduler decision boundary.

Commit:
  the moment staged publications become visible according to edge kind.
```

Runtime visibility:

```text
immediate:
  may become visible within the same epoch/transaction according to compiled region order.

delay:
  visible in the next epoch/transaction.

state:
  writes are staged and committed at a state boundary; readers see stable snapshots.

async:
  creates an event for later scheduling, never recursive direct execution.
```

A component's `ctx.publish()` must not directly call downstream component code. The scheduler owns all execution.

---

## 5. Graph Compiler Requirements

The compiler is still important, but its first job is to support runtime execution, not to become a fancy CLI.

### 5.1 Required compiler passes

Passes:

1. parse/load graph spec;
2. validate component ids are unique;
3. validate lane ids are unique;
4. validate edge ids are unique;
5. validate endpoints use `component.port` format;
6. validate referenced components exist;
7. validate component descriptors expose referenced ports when registry is available;
8. build immediate dependency graph using only `immediate` edges;
9. compute SCCs of the immediate graph;
10. reject nontrivial immediate SCCs unless exactly declared as `composite_loop`;
11. condense components/composite loops into compiled regions;
12. topologically sort regions;
13. produce `CompiledGraphPlan`.

### 5.2 Compiler output

`CompiledGraphPlan` should include:

```text
regions:
  id
  kind: component | composite_loop
  components
  incoming_regions
  outgoing_regions
  loop policy if composite

region_order:
  topological order

component_region:
  component id -> region id

immediate_sccs:
  raw SCC list for debugging and runtime planning
```

### 5.3 Validation severity

Use three levels:

```text
error:
  runtime semantics would be invalid or unsafe.

warning:
  runtime can proceed but behavior may be inefficient or surprising.

info:
  useful diagnostics.
```

Initial implementation may keep only errors. Add warning/info after runtime matures.

---

## 6. Runtime Engine Requirements

### 6.1 Runtime engine responsibilities

The runtime engine should:

- instantiate components from registry;
- configure components;
- activate components;
- create channels from edges;
- create scheduler lanes;
- create event sources;
- run the compiled graph plan;
- route publications through channels;
- enforce edge visibility semantics;
- execute composite loops;
- stop cleanly;
- collect metrics;
- deactivate and cleanup components in reverse lifecycle order.

### 6.2 Runtime execution modes

Keep these modes:

```text
validate:
  compile and validate only.

dry_run:
  instantiate/configure/activate/deactivate, optionally simulate fixed ticks.

run:
  execute actual runtime semantics.
```

Add later:

```text
run_until_idle:
  useful for tests and deterministic examples.

run_for:
  bounded duration.

run_steps:
  bounded epochs/iterations.
```

### 6.3 Event loop model

Initial runtime should support a deterministic single-thread event loop:

```text
while not stopped:
  collect due timers
  collect channel updates
  compute ready invocations
  execute regions in compiled order
  commit staged publications according to edge kind
  update metrics
```

Use worker pools only after deterministic single-thread behavior is correct. Current runtime support is a bounded worker-lane MVP that preserves compiled-region barriers.

### 6.4 Scheduler lanes

Initial lane types:

```text
event_loop:
  deterministic single-thread lane.

periodic:
  fixed tick lane, can be simulated in tests.

worker_pool:
  bounded worker lane for ready invocations; reentrant components may overlap up to max_threads.
```

Rules:

- Non-reentrant components must not execute concurrently.
- A component has exactly one scheduling owner.
- Composite loops execute as one region owner.
- Runtime should expose stop token checks to long-running components.

### 6.5 CompositeLoop runtime

A composite loop is not just a display grouping. It must change execution behavior.

For `loop_policy.type: fixed_point`, runtime should:

1. create a local region transaction;
2. execute internal components in deterministic order;
3. repeat until convergence or `max_iterations`;
4. stop if `budget_ms` is exceeded;
5. commit external outputs only at region boundary;
6. report loop metrics.

Minimum loop metrics:

```text
loop_iteration_count
loop_converged_count
loop_budget_overrun_count
loop_max_iteration_hit_count
```

### 6.6 Publication staging

Use a publication stage instead of direct recursive propagation.

Suggested behavior:

```text
Component publishes output
  -> GraphContext stages publication
  -> Runtime commit phase routes to channels
  -> Trigger engine observes channel updates
  -> Scheduler decides next invocations
```

For minimal compatibility, direct channel publish may remain internally, but the long-term runtime should centralize publication staging.

---

## 7. Application Priority

After the runtime can execute meaningful graphs, build small applications before investing heavily in lint/explain.

### 7.1 Required example applications

#### app_minimal_pipeline

Purpose: prove source -> transform -> sink routing.

Graph:

```text
source -> transform -> sink
```

Acceptance:

- payload reaches sink;
- runtime metrics show publish and delivery count;
- deterministic order is stable.

#### app_overload_latest_vs_queue

Purpose: prove channel policy matters.

Graph:

```text
fast_source -> slow_processor -> sink
```

Scenarios:

- `latest + overwrite + capacity=1` drops/overwrites old payloads;
- `queue + capacity=N + drop_oldest` preserves bounded history;
- metrics expose drop count and max depth.

#### app_control_feedback_delay

Purpose: prove feedback can be safely modeled without immediate recursion.

Graph:

```text
sensor -> estimator -> controller -> actuator
controller -> estimator correction through delay/state edge
```

Acceptance:

- no immediate cycle unless composite loop declared;
- delay edge makes feedback visible in next epoch;
- actuator receives command.

#### app_composite_loop_fixed_point

Purpose: prove immediate feedback can be explicitly owned by a CompositeLoop.

Graph:

```text
estimator <-> controller
```

Acceptance:

- illegal if loop not declared;
- legal if declared;
- runtime executes up to max iterations;
- loop metrics are emitted.

#### app_async_worker

Purpose: prove async edges and task-ready events.

Graph:

```text
source -> async_worker -> join/sink
```

Acceptance:

- worker completion does not recursively execute downstream;
- task_ready event triggers downstream later;
- max_inflight/drop policy works.

---

## 8. Tooling Priority

Tooling should support runtime development and applications. It should not dominate the next phase.

### 8.1 Keep now

Keep:

```text
graph validate
graph plan
graph render
```

These are already useful for development and debugging.

### 8.2 Add after runtime examples work

Add:

```text
graph run examples/app.yaml --steps N
graph run examples/app.yaml --duration-ms N
graph metrics examples/app.yaml --steps N --format json
```

### 8.3 Add later

Add after core runtime semantics are stable:

```text
graph lint
graph explain
graph diff-plan
graph trace
graph bench
```

`lint` and `explain` are high-value features, but they should be derived from real runtime semantics rather than invented before runtime behavior is final.

---

## 9. Iteration Plan for AI Agents

## Milestone A — Stabilize Runtime Contract

Goal: make the existing vocabulary precise and test-backed.

### A1. Write runtime semantics doc

Files:

```text
docs/runtime-semantics.md
docs/schema-v1.md
README.md
```

Tasks:

- Define `epoch`, `transaction`, `commit`, `immediate`, `delay`, `state`, `async`.
- Define channel visibility rules.
- Define trigger readiness rules.
- Define composite loop ownership.
- State that `publish()` never directly executes downstream components.

Acceptance:

- docs describe at least three graphs: DAG, delay feedback, composite loop;
- docs match current schema names;
- no runtime code required for this task unless tests must be updated.

### A2. Add focused tests for graph compile semantics

Files:

```text
tests/test_graph.cpp
src/graph.cpp
include/topoexec/runtime/graph.hpp
```

Test cases:

- acyclic immediate graph compiles;
- immediate cycle without composite loop is rejected;
- declared composite loop is accepted;
- partial composite loop declaration is rejected;
- overlapping simple cycles are handled as one SCC;
- delay/state/async edges do not create immediate SCCs;
- compiled region order is deterministic.

Acceptance:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
```

---

## Milestone B — Make Runtime Execution Semantics Real

Goal: move from “iterate all components in plan order” toward a real event/trigger/channel runtime.

### B1. Introduce RuntimeEngine or upgrade EventRuntime

Files:

```text
include/topoexec/runtime/event_runtime.hpp
src/event_runtime.cpp
include/topoexec/runtime/runtime_runner.hpp
src/runtime_runner.cpp
```

Requirements:

- runtime uses `CompiledGraphPlan` as the execution source of truth;
- runtime can run for bounded steps/epochs;
- runtime can run until idle for tests;
- runtime can stop by stop token;
- runtime produces deterministic results in single-thread mode.

Acceptance tests:

- source -> echo -> sink works in run mode;
- component order matches compiled region order;
- stop token stops execution cleanly;
- error in component stops runtime and deactivates started components.

### B2. Implement publication staging

Files:

```text
include/topoexec/runtime/channel.hpp
src/channel.cpp
include/topoexec/runtime/component.hpp
src/component.cpp
src/event_runtime.cpp
```

Requirements:

- `GraphContext::publish()` stages publication or routes through a runtime-owned publication stage;
- runtime commits staged publications at defined boundaries;
- no component execute call is made directly by publish;
- metrics count publish, delivery, drop, copy.

Acceptance tests:

- publish inside source does not directly call transform;
- after commit, transform can become ready;
- multiple publications in one component execution are committed deterministically.

### B3. Implement delay/state/async visibility

Requirements:

- `immediate` edges visible according to current transaction rules;
- `delay` edges visible only in next epoch;
- `state` edges visible as stable snapshots and update on commit boundary;
- `async` edges enqueue an event and do not trigger same-call-stack recursion.

Acceptance tests:

- delay feedback appears one epoch later;
- state reader sees old value during current epoch and new value after commit;
- async edge triggers downstream only after event loop observes it;
- metrics show staged/committed counts.

---

## Milestone C — Trigger Engine Completeness

Goal: make trigger policies robust enough for useful applications.

### C1. Any-input and all-input correctness

Requirements:

- `any_input` creates invocations for available input updates;
- `all_inputs` waits until every required input has at least one message;
- ready inputs and payloads_by_port are correctly populated;
- no stale pending data is consumed silently.

Acceptance tests:

- any-input invokes when one input arrives;
- all-input does not invoke until all ports have data;
- all-input consumes one message from each input;
- ready_inputs are deterministic.

### C2. Batch trigger

Requirements:

- batch fires only when batch size is reached;
- batch window can be implemented later, but must be documented if not supported;
- batch_payloads preserve order.

Acceptance tests:

- batch size 3 fires after 3 payloads;
- batch size 3 does not fire after 2 payloads;
- overflow policy still applies.

### C3. Timer trigger

Requirements:

- timer components execute according to period in bounded-step simulation;
- timer invocation uses `EventKind::kTimer` and `TriggerKind::kOnTick`;
- periodic lane metrics record jitter/overrun once scheduler supports it.

Acceptance tests:

- timer component executes expected number of times in simulated steps;
- timer and message triggers can coexist in one graph.

### C4. Coalesce and min_interval

Requirements:

- coalescing merges multiple pending updates into one invocation when configured;
- min_interval suppresses invocations until interval passes;
- suppressed invocations are counted in metrics.

Acceptance tests:

- coalesced component runs once for multiple queued updates;
- non-coalesced component runs multiple times;
- min_interval blocks repeated invocation in same period.

---

## Milestone D — Channel Runtime Maturity

Goal: make channel policy a reliable runtime feature, not just schema decoration.

### D1. Channel modes

Implement and test:

```text
latest
queue
latched
previous_tick
barrier
```

Acceptance tests:

- latest only exposes newest payload;
- queue drains FIFO within capacity;
- latched keeps last value for late reader;
- previous_tick does not expose value until next epoch;
- barrier waits for required ports.

### D2. Overflow policies

Implement and test:

```text
overwrite
drop_oldest
drop_newest
block -> returns would-block unless blocking runtime mode explicitly enabled
fail_fast
```

Acceptance tests:

- each policy has deterministic behavior;
- metrics update correctly;
- block never deadlocks tests;
- fail_fast returns an error path visible to runtime.

### D3. Copy policy and large payload

Requirements:

- `copy` copies small payloads;
- `shared_view` shares immutable payload pointer;
- `loaned_view` may alias `shared_view` until a real loaned buffer API exists;
- `move_only` must enforce single-reader or explicit ownership transfer;
- copying large payloads should reject or emit degradation reason.

Acceptance tests:

- large payload + copy fails or warns as specified;
- shared_view does not increment copy count;
- copy increments copy count.

---

## Milestone E — CompositeLoop Runtime

Goal: make feedback loops useful and safe.

### E1. Region execution owner

Requirements:

- composite loop region is scheduled as one region;
- internal components are not independently scheduled outside the loop owner;
- loop region controls iteration and commit.

Acceptance tests:

- component inside composite loop does not run twice due to both region and raw component scheduling;
- external incoming edges feed loop inputs;
- external outgoing edges publish only at region boundary.

### E2. Fixed-point policy

Requirements:

- execute internal components in deterministic order;
- repeat up to `max_iterations`;
- stop early if convergence hook says converged;
- obey `budget_ms`.

Acceptance tests:

- max_iterations respected;
- convergence stops early;
- budget overrun increments metric and stops or reports according to policy.

### E3. Loop diagnostics

Metrics:

```text
loop.iterations
loop.converged
loop.max_iterations_hit
loop.budget_overrun
```

Acceptance:

- runtime result exposes loop metrics;
- CLI metrics command can print them later.

---

## Milestone F — Runnable Applications

Goal: demonstrate that TopoExec is useful outside tests.

### F1. Add app_minimal_pipeline

Files:

```text
examples/apps/minimal_pipeline/
```

Acceptance:

- builds with CMake;
- runs from command line;
- prints final sink output;
- uses real RuntimeRunner run mode.

### F2. Add app_overload_latest_vs_queue

Acceptance:

- shows drop/overwrite metrics;
- compares latest and queue mode;
- does not require external dependencies.

### F3. Add app_control_feedback_delay

Acceptance:

- demonstrates delay edge feedback;
- logs epoch number and delayed correction value;
- passes deterministic expected-output test.

### F4. Add app_composite_loop_fixed_point

Acceptance:

- demonstrates declared immediate feedback;
- shows loop iteration count;
- validates illegal version fails when composite loop declaration is removed.

---

## Milestone G — Observability

Goal: make runtime behavior visible.

### G1. Metrics model

Metrics should cover:

```text
component execution count
component duration
component error count
edge publish count
edge delivery count
edge drop count
edge max depth
edge copy count
scheduler completed count
scheduler overrun count
loop iteration count
```

Acceptance:

- metrics snapshot can be obtained from RuntimeRunnerResult;
- examples print metrics in a stable text or JSON format.

### G2. Trace events

Add simple trace events:

```text
component_execute_begin
component_execute_end
channel_publish
channel_commit
scheduler_iteration_begin
scheduler_iteration_end
loop_iteration_begin
loop_iteration_end
```

Acceptance:

- trace sink can record events in memory;
- optional JSON export is available later.

---

## Milestone H — CLI After Runtime

Goal: make tools useful without making them the center of development.

### H1. Runtime CLI

Add:

```bash
topoexec graph run examples/...yaml --steps 10
topoexec graph run examples/...yaml --duration-ms 1000
topoexec graph metrics examples/...yaml --steps 10 --format json
```

Acceptance:

- CLI uses RuntimeRunner;
- errors are human-readable;
- JSON output is machine-parseable.

### H2. Lint and Explain

Add after runtime is stable:

```bash
topoexec graph lint graph.yaml
topoexec graph explain graph.yaml
```

Initial lint rules:

- immediate cycle without composite loop;
- unbounded or suspicious channel capacity;
- large payload with copy policy;
- blocking overflow on event_loop lane;
- component budget greater than lane period;
- async edge without max_inflight when admission needs a bound before channel capacity;
- state edge with multiple writers;
- delay edge with ambiguous epoch boundary.

---

## 10. AI Agent Operating Rules

Every AI agent working on this repo must follow these rules.

### 10.1 Before coding

1. Inspect existing files before modifying.
2. Identify the smallest milestone task to implement.
3. Preserve public names unless the task explicitly requires migration.
4. Prefer additive changes over broad rewrites.
5. Do not introduce new third-party dependencies without explicit approval.
6. Keep core independent of YAML, CLI, ROS, and app examples.

### 10.2 During coding

1. Add tests first or in the same commit.
2. Keep runtime semantics deterministic in tests.
3. Avoid hidden recursive downstream execution.
4. Avoid unbounded queues.
5. Make every error path observable through returned status or runtime result.
6. Use clear, stable error messages.

### 10.3 After coding

Run:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
```

If tests cannot be run in the environment, the agent must state that clearly and still provide the expected tests.

### 10.4 PR summary template

Each change should include:

```text
Title:
  [runtime] implement delay edge visibility

Why:
  Delay edges existed in schema but did not yet affect runtime visibility.

What changed:
  - Added epoch staging for delay edges.
  - Added tests for next-epoch visibility.
  - Added metrics for staged delay commits.

Tests:
  - test_channel DelayEdgeVisibleNextEpoch
  - test_runtime DelayFeedbackDoesNotRecurse
  - ctest --test-dir build --output-on-failure

Known limitations:
  State edge still uses latest snapshot; full transaction state commit is next task.
```

---

## 11. Definition of Done

A milestone is done only when:

- relevant code compiles;
- unit tests cover positive and negative cases;
- integration test or example proves real runtime behavior;
- docs or comments describe semantics if they are user-visible;
- runtime metrics expose important overload or execution outcomes;
- no new dependency is added without justification;
- behavior is deterministic under test;
- failure modes are explicit.

---

## 12. Suggested Test Matrix

### Graph compiler

- acyclic immediate graph;
- illegal immediate cycle;
- declared composite loop;
- partial composite loop;
- overlapping cycles as one SCC;
- delay/state/async do not create immediate cycle;
- deterministic region order.

### Channel

- latest overwrite;
- queue FIFO;
- drop_oldest;
- drop_newest;
- fail_fast;
- block returns would-block;
- latched late reader;
- previous_tick boundary;
- copy vs shared_view metrics;
- large payload copy rejection.

### Trigger

- manual;
- timer;
- any_input;
- all_inputs;
- batch;
- min_interval;
- coalesce;
- time_sync later.

### Runtime

- source -> transform -> sink;
- delay feedback;
- state snapshot;
- async event;
- stop token;
- duration bound;
- component exception cleanup;
- reverse lifecycle deactivation.

### CompositeLoop

- undeclared loop rejected;
- declared loop accepted;
- max_iterations;
- convergence;
- budget overrun;
- external output committed at boundary.

### Applications

- minimal pipeline;
- overload latest vs queue;
- delay feedback;
- composite loop;
- async worker.

---

## 13. Near-Term Priority Order

Implement in this order:

1. Runtime semantics documentation and compiler tests.
2. Deterministic event runtime based on compiled plan.
3. Publication staging and commit boundaries.
4. Delay/state/async edge runtime behavior.
5. Any/all/batch/timer trigger correctness.
6. Channel mode/drop/copy policy completeness.
7. CompositeLoop execution owner and fixed-point policy.
8. Runnable applications demonstrating runtime value.
9. Runtime metrics and trace output.
10. CLI run/metrics.
11. Lint/explain/render improvements.
12. Optional ROS or other adapters.

The principle is:

```text
runtime behavior -> applications -> metrics -> tools -> adapters
```

---

## 14. Recommended First Three Agent Tasks

### Task 1: Runtime semantics doc + tests for immediate-cycle compile behavior

Why first:

- It protects the core concept.
- It gives agents a stable target.
- It does not require a large runtime rewrite.

Deliverables:

- `docs/runtime-semantics.md`
- tests for legal/illegal immediate SCCs
- test for delay edge breaking cycle

### Task 2: Delay edge next-epoch runtime behavior

Why second:

- It is the simplest feature that proves edge kind affects runtime.
- It demonstrates TopoExec is not just a DAG runner.

Deliverables:

- runtime epoch staging for delay edges
- test: feedback arrives next epoch
- example: control feedback delay

### Task 3: Minimal application using real run mode

Why third:

- It proves the runtime is usable outside tests.
- It gives future agents a reference integration target.

Deliverables:

- `examples/apps/minimal_pipeline`
- command-line runnable example
- runtime metrics printout

---

## 15. Long-Term Direction

Once the runtime and applications are credible, TopoExec can grow into:

- a robust C++ semantic graph runtime;
- a graph contract compiler;
- a runtime metrics and trace framework;
- a small app-embedding library;
- optional adapters for ROS 2, OpenTelemetry, Prometheus, Python, and other ecosystems.

The long-term value is not “topological execution.” The value is:

```text
explicit semantics for time, feedback, state, channels, scheduling, and overload
inside one ordinary C++ process.
```
