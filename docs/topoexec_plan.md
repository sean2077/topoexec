# TopoExec Next-Stage Plan

Version: `plan-2026-05-04`

Repository reviewed: `sean2077/topoexec`, `main` branch snapshot visible on GitHub on 2026-05-04.

Scope: compare the current implementation against the earlier AI-Agent development plan and define the next useful development phase.

> Note: this review is based on the public GitHub files available through the browser. I could not clone/build the repository in this environment, so the first task below is to reproduce the repository's own build/test evidence locally or in CI.

---

## 1. Executive Summary

TopoExec has advanced much faster than the original staged plan. The current repository already has:

- a C++20 single-process stateful dataflow runtime positioning;
- `docs/runtime-semantics.md` defining epoch, transaction, commit, edge visibility, trigger readiness, and CompositeLoop ownership;
- schema v1 with required edge kinds: `immediate`, `delay`, `state`, `async`;
- runtime components for graph, channel, trigger policy, event runtime, scheduler, runner, metrics, and trace;
- CLI commands for validate, plan, render, run, metrics, trace, lint, explain, diff-plan, and bench;
- runnable apps for minimal pipeline, overload latest-vs-queue, delay feedback, CompositeLoop fixed-point, and async worker;
- an implementation audit claiming 22/22 CTest tests passed.

This means the next phase should not continue adding more CLI commands or decorative features. The next phase should focus on:

1. **making the library embeddable and maintainable**;
2. **hardening runtime semantics through stronger tests and invariants**;
3. **splitting core/runtime from YAML/CLI dependencies**;
4. **making scheduler, async, and worker-pool semantics real rather than aspirational**;
5. **stabilizing public API, docs, packaging, and release quality**.

The principle for the next stage:

```text
freeze feature sprawl -> harden core -> improve embeddability -> add real concurrency -> release beta
```

---

## 2. Current Completion Matrix Against Earlier Plan

| Area from earlier plan | Current status | Next action |
|---|---:|---|
| Runtime semantics doc | Mostly complete | Expand into structured reference; add examples and invariants |
| Schema v1 edge kinds | Implemented | Document defaults/enums fully; add schema compatibility tests |
| Graph compiler / SCC / CompositeLoop validation | Implemented according to audit | Add randomized/property validation tests |
| Event runtime / compiled plan execution | Implemented according to audit | Harden lifecycle, error, stop, and deterministic behavior |
| Publication staging | Implemented according to audit | Add invariant tests and clearer transaction boundaries |
| Delay/state/async visibility | Implemented according to audit | Add edge-case tests, especially mixed edge kinds and multi-epoch cases |
| Trigger engine | Broadly implemented | Harden time-sync, batch-window, min-interval, coalesce edge cases |
| Channel modes and overflow | Broadly implemented | Add stress tests and policy docs; verify metrics correctness |
| Copy/shared/loaned payload policy | Implemented baseline | Clarify ownership model; add API ergonomics for real users |
| CompositeLoop runtime | Implemented baseline | Replace stringly convergence policy with typed/explicit extension point |
| Runnable apps | Implemented | Turn apps into tutorials with expected output and design lessons |
| Metrics and trace | Implemented baseline | Stabilize schema and add trace export format |
| CLI lint/explain/diff/bench | Implemented earlier than planned | Freeze feature growth; use only to support runtime quality |
| Worker pool / real threaded scheduling | Deferred / optional | Design and implement after deterministic single-thread invariants are locked |
| Async max-inflight / rich async policy | Deferred / optional | Add as a focused milestone |
| Adapters: ROS2, OTEL, Prometheus, Python | Deferred | Keep deferred until beta core stabilizes |
| Build/package/release ergonomics | Partial | Improve CMake package, CI matrix, docs, formatting |

---

## 3. Important Gaps To Fix Before Adding More Features

### Gap 1: Core library currently appears coupled to YAML/JSON dependencies

The earlier plan wanted dependency direction like this:

```text
tools -> runtime -> core
apps  -> runtime -> core
adapters -> runtime -> core
```

and wanted to keep YAML/CLI parsing outside the core model.

Current `CMakeLists.txt` builds a single `topoexec` library and links `yaml-cpp` and `nlohmann_json` publicly. This is not fatal, but it weakens TopoExec as a small embeddable C++ runtime.

Target direction:

```text
topoexec_core      # graph model, component interface, policies, status, no YAML
topoexec_runtime   # channel, scheduler, event runtime, runner, metrics, trace
topoexec_yaml      # YAML schema loader/serializer
topoexec_tools     # CLI
topoexec_examples  # apps
```

Acceptance:

- A user can link `topoexec_core` + `topoexec_runtime` without `yaml-cpp` or `CLI11`.
- YAML loader is opt-in.
- CLI remains a thin wrapper.
- Existing CLI/tests still pass.

---

### Gap 2: Source formatting and reviewability need attention

Several public files are currently visible as one-line or highly compressed raw files. Even if this is partly caused by generation or formatting tooling, it makes code review, blame, diffs, and AI-agent edits much less reliable.

Target direction:

- Add `.clang-format`.
- Add `.editorconfig`.
- Normalize newlines and formatting for CMake, headers, source, docs.
- Add CI check for formatting or at least `git diff --check`.

Acceptance:

- Raw source files are readable.
- Diffs are line-based and reviewable.
- `scripts/agent_check.sh` includes `git diff --check`.

---

### Gap 3: Current audit claims completion; the next step is independent reproducibility

`docs/spec-implementation-audit.md` claims 22/22 CTest tests passed. Treat this as useful internal evidence, not a release-quality guarantee.

Target direction:

- Add GitHub Actions CI.
- Reproduce tests on at least Ubuntu + GCC + Clang.
- Add Debug and RelWithDebInfo builds.
- Add sanitizer jobs later.

Acceptance:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
git diff --check
```

runs in CI and is green.

---

### Gap 4: Runtime semantics are implemented, but invariants need stronger tests

The current tests appear to cover many named scenarios, but the next value is not more happy-path examples. The next value is invariant testing.

Key invariants:

- `publish()` never directly calls downstream `execute()`.
- `immediate` edges are the only edges participating in immediate SCC rejection.
- `delay/state/async` publications are not visible in the publishing transaction.
- CompositeLoop components have exactly one scheduling owner.
- A component cannot execute concurrently unless explicitly reentrant.
- All runtime queues are bounded.
- Drop/overwrite/fail/block behavior increments metrics deterministically.
- Lifecycle cleanup happens after component error and stop-token shutdown.

Acceptance:

- Add tests that intentionally try to violate each invariant.
- Tests assert both behavior and metrics/error messages.
- Add randomized graph compiler tests for SCC/region-order invariants.

---

### Gap 5: Scheduler lanes need bounded runtime support

Post-alpha status: persistent `thread_pool` worker-pool v1, runtime priority/admission ordering v1, opt-in `fixed_rate` wall-clock cadence v1, and async edge `policy.max_inflight` admission now exist with tests. Remaining work is OS-level scheduling policy, advanced starvation aging, timeout preemption, and TSAN CI.

This remains the largest runtime maturity area.

Do not implement it casually. The recommended path is:

1. lock deterministic single-thread semantics;
2. define scheduler ownership and reentrancy rules;
3. add worker-pool lane with bounded queue;
4. add async max-inflight policy;
5. add cancellation and pending-task cleanup;
6. add thread-safety tests and TSAN CI.

Acceptance:

- Non-reentrant component never runs concurrently.
- Reentrant component can run concurrently only when explicitly declared.
- Worker pool has bounded queue and explicit rejection/drop policy.
- Async task completion becomes a later event, never direct recursion.
- Runtime stops and cleans pending tasks deterministically.

---

### Gap 6: Public component API needs release-level stability

Before beta, define what is public API and what is internal.

Questions to settle:

- Should lifecycle hooks return `Status` instead of `void`?
- Should `execute()` return `Status`?
- What is the stable shape of `Invocation`?
- How should typed inputs be accessed?
- How does a component declare descriptors: runtime registry, static C++ builder, YAML, or all of them?
- How are errors propagated from component to runtime result?

Target direction:

- Do not rewrite everything at once.
- Add status-returning APIs only with compatibility strategy.
- Mark internal classes as internal or put them under an internal namespace if needed.
- Add one pure C++ typed builder example that does not require YAML.

Acceptance:

- `examples/apps/minimal_pipeline` can be expressed through pure C++ builder API.
- YAML remains available but is not required for library use.
- Public API is documented in `docs/api-overview.md`.

---

### Gap 7: Schema docs are too terse for external users

`docs/schema-v1.md` currently captures the critical high-level rules but should become a full schema reference.

Add:

- all root fields;
- all enums;
- defaults;
- required vs optional fields;
- edge examples;
- trigger examples;
- CompositeLoop examples;
- invalid examples;
- migration/versioning policy.

Acceptance:

- A user can write a valid graph from the schema doc alone.
- Invalid examples are tested with CLI validation.
- Schema v1 compatibility is preserved unless version is bumped.

---

### Gap 8: Observability exists, but its output contract is not yet a product surface

Metrics and trace are already present, but they need stable schemas.

Add:

- `docs/metrics.md`;
- `docs/trace-events.md`;
- stable metric names;
- JSON examples;
- trace export to Chrome Trace or Perfetto JSON;
- optional Prometheus/OpenTelemetry adapter later.

Acceptance:

- CLI metrics output has a documented JSON schema.
- Trace events include timestamp, event name, component/edge/loop id where relevant.
- Golden tests validate metrics/trace output for minimal graphs.

---

### Gap 9: Runnable apps exist, but should become onboarding tutorials

Current apps are useful acceptance fixtures. Turn them into product explanations.

For each app, add:

- short README;
- graph diagram;
- command to build/run;
- expected output;
- what semantic feature it demonstrates;
- failure mode or contrast case.

Priority apps:

1. `minimal_pipeline` — source -> transform -> sink.
2. `overload_latest_vs_queue` — low latency vs bounded FIFO.
3. `control_feedback_delay` — feedback without recursion.
4. `composite_loop_fixed_point` — explicit immediate feedback owner.
5. `async_worker` — async completion and bounded delivery.

Acceptance:

- New user can run all examples in under 10 minutes.
- README links to examples.
- Each example is also a test target.

---

### Gap 10: CLI feature growth should pause until runtime/API hardening is done

The CLI already has validate, plan, render, run, metrics, trace, lint, explain, diff-plan, and bench. More CLI commands are not the next highest-value work.

Target direction:

- Freeze CLI command set for now.
- Improve correctness, output stability, and docs.
- Ensure CLI is a thin layer over library APIs.
- Do not let CLI-specific logic leak into core/runtime.

Acceptance:

- CLI JSON outputs have golden tests.
- CLI errors include graph file path and failing entity id when possible.
- CLI does not duplicate validation logic already in runtime/core.

---

### Gap 11: Packaging and release readiness need work

Current CMake has install/export basics, but beta users need a clean package.

Add:

- generated `topoexecConfig.cmake` and version file;
- clear target names such as `topoexec::core`, `topoexec::runtime`, `topoexec::yaml`, `topoexec::topoexec` if kept;
- package smoke test using `find_package(topoexec CONFIG REQUIRED)`;
- installation test in CI;
- versioning policy;
- changelog.

Acceptance:

- A downstream sample project can build with installed TopoExec.
- Package exports do not force unnecessary dependencies on users who do not use YAML/CLI.

---

### Gap 12: Performance evidence is needed

TopoExec's value depends on explicit runtime semantics, but users will still ask:

- how much overhead does a graph step add?
- how many messages/second can channels handle?
- what is p50/p95/p99 latency?
- how many copies happen under copy/shared/loaned policies?
- what happens under overload?

Add microbenchmarks:

- single component invocation overhead;
- chain length N immediate pipeline;
- latest vs queue throughput;
- delay edge epoch overhead;
- shared/loaned large payload path;
- CompositeLoop max-iteration overhead;
- worker-pool throughput for bounded `thread_pool` lanes.

Acceptance:

- Bench output is machine-readable.
- Benchmarks run optionally, not as mandatory unit tests.
- README includes representative results with environment info.

---

## 4. Recommended Milestones

## Milestone 0 — Baseline Protection

Goal: make the current fast implementation safe to continue iterating.

Tasks:

1. Add `AGENTS.md` with autonomy, safety, and validation rules.
2. Add `scripts/agent_check.sh`.
3. Add `.github/workflows/ci.yml`.
4. Add `.clang-format` and `.editorconfig`.
5. Normalize formatting/newlines.
6. Re-run and record current tests.

Acceptance:

- CI passes.
- `scripts/agent_check.sh` passes locally.
- No feature change required.

Suggested branch:

```text
agent/baseline-protection
```

---

## Milestone 1 — Library Boundary and Embeddability

Goal: make TopoExec feel like a reusable C++ library, not only a CLI/demo project.

Tasks:

1. Split targets or at least isolate dependencies:
   - `topoexec_core`
   - `topoexec_runtime`
   - `topoexec_yaml`
   - `topoexec_cli`
2. Move YAML parsing out of core/runtime public dependency path.
3. Add pure C++ graph builder example.
4. Add install/package smoke test.
5. Document public vs internal API.

Acceptance:

- A downstream app can link runtime without YAML/CLI.
- Existing CLI still works.
- Existing tests pass.

Suggested branches:

```text
agent/split-core-runtime-yaml
agent/pure-cpp-builder-example
agent/cmake-package-smoke-test
```

---

## Milestone 2 — Runtime Invariant Hardening

Goal: prove the semantic runtime behaves correctly beyond happy paths.

Tasks:

1. Add invariant tests for non-recursive publish.
2. Add multi-epoch tests for delay/state/async visibility.
3. Add randomized graph compile tests for SCC and region ordering.
4. Add lifecycle failure and reverse cleanup tests.
5. Add deterministic ordering tests for branches and multiple publications.
6. Add metric consistency tests.

Acceptance:

- Each runtime semantic claim has at least one positive and one negative test.
- Failure messages are clear and stable.

Suggested branches:

```text
agent/runtime-invariant-tests
agent/graph-scc-property-tests
agent/lifecycle-error-cleanup-tests
```

---

## Milestone 3 — Scheduler and Async Runtime Maturity

Goal: implement real concurrency carefully.

Tasks:

1. Define scheduler lane semantics in `docs/scheduler.md`.
2. Implement bounded worker-pool lane.
3. Add component reentrancy enforcement.
4. Add async max-inflight policy.
5. Add cancellation and pending task cleanup.
6. Add TSAN CI job after concurrency lands.

Acceptance:

- Worker pool behavior is bounded and deterministic enough for tests.
- Non-reentrant components are protected.
- Async completion produces later events.
- Stop token can drain/cancel tasks without hanging.

Suggested branches:

```text
agent/scheduler-docs
agent/worker-pool-lane
agent/async-max-inflight
agent/tsan-concurrency-tests
```

---

## Milestone 4 — Public API and Typed Usability

Goal: make TopoExec pleasant for real application developers.

Tasks:

1. Audit lifecycle and `execute()` return types.
2. Add `Status` migration path if needed.
3. Add typed payload helpers.
4. Add typed component descriptor/builder APIs.
5. Add `docs/api-overview.md`.
6. Add examples using only library API, no YAML.

Acceptance:

- A user can build a graph in pure C++.
- Common payload access patterns are type-safe and concise.
- Error propagation is explicit.

Suggested branches:

```text
agent/status-api-audit
agent/typed-payload-helpers
agent/cpp-builder-api
```

---

## Milestone 5 — Observability Productization

Goal: make metrics/trace useful for real debugging.

Tasks:

1. Add `docs/metrics.md`.
2. Add `docs/trace-events.md`.
3. Add JSON schema or stable examples for metrics output.
4. Add Chrome Trace / Perfetto-compatible export.
5. Add golden tests for metrics and trace CLI output.
6. Add optional runtime metrics API independent of CLI.

Acceptance:

- Metrics names are stable.
- Trace output can be opened by common tooling.
- Examples explain how to inspect overload and feedback behavior.

Suggested branches:

```text
agent/metrics-schema-docs
agent/trace-export-json
agent/metrics-golden-tests
```

---

## Milestone 6 — Documentation and Tutorials

Goal: make the project understandable without reading source code.

Tasks:

1. Rewrite README with:
   - what TopoExec is;
   - what it is not;
   - why edge kinds matter;
   - quickstart;
   - pure C++ example;
   - YAML example;
   - links to runtime semantics and schema docs.
2. Expand `docs/schema-v1.md` into full reference.
3. Add example READMEs.
4. Add architecture diagram.
5. Add `docs/faq.md`.

Acceptance:

- A new developer can understand the project in 15 minutes.
- Each key semantic has one runnable example.

Suggested branches:

```text
agent/readme-product-docs
agent/schema-v1-reference
agent/example-tutorials
```

---

## Milestone 7 — Release Candidate

Goal: prepare for first useful open-source release.

Tasks:

1. Add versioning policy.
2. Add changelog.
3. Add release checklist.
4. Add CI matrix: GCC/Clang, Debug/RelWithDebInfo.
5. Add sanitizer builds.
6. Add install/package smoke tests.
7. Add benchmark suite.
8. Tag `v0.1.0-alpha` or `v0.1.0-beta` after API/docs settle.

Acceptance:

- Release artifacts are buildable.
- Docs match implementation.
- Known limitations are explicit.

Suggested branch:

```text
agent/release-readiness
```

---

## Milestone 8 — Optional Adapters After Beta

Goal: grow ecosystem reach without contaminating core.

Do after core/runtime/API are stable.

Candidate adapters:

1. ROS 2 adapter:
   - ROS topic -> TopoExec input channel;
   - TopoExec output channel -> ROS topic;
   - lifecycle and parameters mapping.
2. OpenTelemetry / Prometheus adapter:
   - export metrics and spans.
3. Python binding:
   - configuration, tests, scripting;
   - not high-performance core path initially.
4. Perfetto adapter:
   - trace visualization.

Acceptance:

- Each adapter is optional.
- Core and runtime do not depend on adapter libraries.

---

## 5. Concrete Task Board for Codex / AI Agents

Use these as small autonomous tasks. Each task should run `scripts/agent_check.sh` before completion.

### P0 Tasks

#### T0.1 Add baseline automation

Files:

```text
AGENTS.md
scripts/agent_check.sh
.github/workflows/ci.yml
```

Acceptance:

- CI runs configure/build/test/diff-check.
- Agent rules are explicit.

#### T0.2 Normalize source formatting

Files:

```text
.clang-format
.editorconfig
CMakeLists.txt
include/**
src/**
tests/**
examples/**
```

Acceptance:

- Files are reviewable line-by-line.
- No semantic changes unless needed to compile.

#### T0.3 Reproduce current audit

Files:

```text
docs/current-baseline.md
```

Acceptance:

- Records commit hash.
- Records build/test commands.
- Records pass/fail status.
- Records known environment.

---

### P1 Tasks

#### T1.1 Split dependency targets

Goal: separate embeddable runtime from YAML/CLI dependencies.

Acceptance:

- `topoexec_runtime` can be linked without YAML/CLI.
- CLI still works.
- Tests pass.

#### T1.2 Add C++ GraphBuilder example

Goal: prove YAML is optional.

Acceptance:

- A pure C++ app builds and runs a minimal graph.
- It uses public API only.

#### T1.3 Add package smoke test

Goal: prove downstream install works.

Acceptance:

- `cmake --install` works.
- A downstream fixture uses `find_package(topoexec CONFIG REQUIRED)`.

---

### P2 Tasks

#### T2.1 Runtime invariant test suite

Acceptance:

- Tests for no-recursive publish.
- Tests for exact epoch visibility.
- Tests for CompositeLoop ownership.
- Tests for lifecycle cleanup.

#### T2.2 Graph property tests

Acceptance:

- Random DAGs compile deterministically.
- Random immediate cycles are rejected unless declared.
- Delay/state/async feedback does not create immediate SCC.

#### T2.3 Metrics consistency tests

Acceptance:

- Publish/deliver/drop/copy counts match expected event flows.
- Loop metrics match loop execution count.

---

### P3 Tasks

#### T3.1 Scheduler semantics doc

Acceptance:

- Documents lane types, ownership, reentrancy, priority, stop behavior.

#### T3.2 Worker pool lane MVP

Acceptance:

- Bounded queue.
- Max workers.
- Reentrancy enforcement.
- Deterministic shutdown.

#### T3.3 Async max-inflight

Acceptance:

- Configurable max inflight.
- Explicit overflow policy.
- Task completion maps to future/task-ready event.

---

### P4 Tasks

#### T4.1 Public API audit

Acceptance:

- List public headers/classes.
- Identify internal classes.
- Decide status-returning lifecycle migration.

#### T4.2 Typed payload helpers

Acceptance:

- Safer accessors for text/frame/blob/custom payload.
- Tests for bad type access.

#### T4.3 API overview docs

Acceptance:

- A user can implement a component and run a graph from docs.

---

### P5 Tasks

#### T5.1 Metrics schema docs

Acceptance:

- Stable metric names.
- JSON examples.
- Golden tests.

#### T5.2 Trace export

Acceptance:

- Chrome Trace or Perfetto-compatible JSON.
- Minimal example produces trace file.

---

### P6 Tasks

#### T6.1 README rewrite

Acceptance:

- Explains positioning, non-goals, quickstart, and examples.

#### T6.2 Schema v1 reference

Acceptance:

- Full enum/default/reference coverage.

#### T6.3 Example tutorials

Acceptance:

- Each runnable app has README and expected output.

---

## 6. Prioritized Next 10 Pull Requests

1. `baseline: add AGENTS, agent_check, and CI`
2. `format: normalize source formatting and add clang-format`
3. `build: split runtime/core from yaml and cli dependencies`
4. `examples: add pure cpp builder minimal pipeline`
5. `tests: add runtime invariant suite for publish/epoch/loop ownership`
6. `tests: add graph SCC property tests`
7. `docs: expand schema v1 into full reference`
8. `runtime: document and implement scheduler lane contract`
9. `runtime: add bounded worker pool lane MVP`
10. `observability: stabilize metrics schema and add golden tests`

---

## 7. Rules for AI-Agent Iteration

1. Do not add new CLI commands until Milestones 0-2 are done.
2. Do not add ROS, Python, or OpenTelemetry adapters before beta core stabilizes.
3. Do not introduce new runtime dependencies without justification.
4. Prefer tests over new features.
5. Prefer small branches over broad rewrites.
6. Every task must state:
   - files changed;
   - semantic risk;
   - tests run;
   - known limitations.
7. If ambiguity affects public API, stop and write a blocker.
8. If ambiguity is reversible, choose the smallest implementation and document the assumption.

---

## 8. Definition of Done for Next Stage

The next stage is complete when:

- CI passes on at least GCC and Clang.
- Source is formatted and reviewable.
- Core/runtime can be linked without YAML/CLI.
- Runtime invariants are test-backed.
- Current runnable apps are documented as tutorials.
- Metrics and trace schemas are documented.
- Public API is clearly separated from internal implementation.
- Worker-pool / async max-inflight exists with tests or is explicitly deferred.
- First alpha/beta release checklist is ready.

---

## 9. Strategic Direction

TopoExec should not compete by being the biggest orchestration platform. Its best chance is to be:

```text
small + embeddable + C++20-first + semantic + testable + observable
```

The differentiator remains:

```text
explicit semantics for edge visibility, feedback, state, trigger readiness,
bounded channels, CompositeLoop ownership, and runtime observability
inside one ordinary process.
```

The next phase should therefore make those semantics easy to trust, easy to embed, easy to package, and easy to explain.
