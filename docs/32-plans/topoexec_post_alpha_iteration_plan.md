# TopoExec post-alpha hardening / documentation / GitHub Pages iteration plan

Date: 2026-05-06
Target repository: `sean2077/topoexec`
Prepared for: Codex `/goal` execution

## 0. Scope, assumptions, and limits

This plan is based on a source-and-documentation review of the public GitHub repository as of 2026-05-06. I could not perform a local clone/build in this container because DNS resolution for `github.com` failed, so any build or test claims below are **planned validation steps**, not locally verified outcomes. Codex should treat the first milestone as mandatory baseline verification before changing code.

The goal is not to expand TopoExec into a production adapter ecosystem. The repository already states that core runtime beta-readiness is separate from ROS 2, OpenTelemetry/Prometheus, native Python bindings, stable C ABI, package registry publication, sandboxed plugin systems, hard real-time scheduling, and schema v2 migration tooling. This plan preserves that boundary.

The intended output is a set of small, reviewable changes that:

1. harden runtime semantics and tests;
2. fix or explicitly document likely bugs and ambiguity;
3. create a repeatable performance baseline;
4. introduce API-reference documentation with Doxygen;
5. publish a static documentation site through GitHub Pages;
6. update roadmap/status docs and release evidence.

## 1. Repository snapshot

### 1.1 Product boundary

TopoExec is a compact C++20 runtime for deterministic, observable, stateful dataflow execution inside one process. The runtime model is built around `GraphSpec`, validation/compilation into regions, `RuntimeRunner`, `EventRuntime`, `TriggerPolicyEngine`, `RuntimePublicationRouter`, bounded channels, metrics, traces, diagnostics, and optional YAML/CLI tooling.

It is explicitly not a distributed runtime, production ROS adapter, production telemetry exporter, Python framework, GUI editor, or sandboxed plugin ecosystem.

### 1.2 Main directories

| Area | Role |
|---|---|
| `include/topoexec/runtime` | Public and low-level runtime headers: graph spec, components, channels, event runtime, scheduler, state, payload, trigger policies, task executor, diagnostics. |
| `include/topoexec/common` | Logging, metrics, trace common surfaces. |
| `include/topoexec/adapters`, `c_api`, `plugins` | Preview/boundary surfaces. Keep dependency-light and explicitly unstable where documented. |
| `src` | Runtime, graph validation/compilation, YAML IO, channel/publication router, event loop, scheduler, state, metrics/trace, plugin loader, C API. |
| `tools/topoexec` | CLI command surface around graph validate/render/plan/run/metrics/trace/lint/explain/diff-plan/bench/schema/doctor. |
| `tests` | Unit, CLI, schema, docs, policy, fuzz, stress, benchmark, packaging, plugin, release, Python preview tests. |
| `docs` | Canonical documentation root, reorganized into numbered zones. |
| `.github/workflows` | Current CI and release dry-run workflows. No Pages workflow yet. |
| `scripts` | Agent/release/sanitizer/fuzz/stress/benchmark gates. |
| `schema` | JSON schema artifacts. |
| `examples` | Minimal and semantic graph examples. |
| `packaging` | Packaging support. |

### 1.3 Existing goal protocol

The repository already has `docs/31-planning-roadmap/goals/README.md` defining goal execution rules. Current backlog says no active implementation goal is open. Therefore Codex should add a new post-alpha hardening goal entry rather than editing historical G0-G70 records as if they were still open.

## 2. Architecture and logic map

### 2.1 Build target model

The project uses CMake 3.20+, C++20, `topoexec` project version `0.1.0`, schema version `1`, and semantic contract version `0.2`. Build options include core testing, YAML loader, CLI, examples, fuzzers, C API, Python preview, plugin loader, OpenTelemetry/Prometheus/ROS2 preview adapters, and ASAN/UBSAN/TSAN toggles.

Recommended dependency rule for all future changes:

- `topoexec::runtime` must remain dependency-light and must not depend on YAML, CLI, adapter, ROS, telemetry, Python, or plugin loading.
- YAML and CLI remain optional layers.
- Preview adapter/C/Python/plugin surfaces remain documented as preview unless a separate stabilization goal is opened.

### 2.2 Data model

`GraphSpec` contains:

- `schema_version`, `name`, `kind`, `clock`, graph config;
- lanes (`LaneSpec`): fixed-rate/thread-pool metadata, budgets, priority/advisory scheduling properties;
- components (`ComponentNodeSpec`): id, type, execution settings, event sources, trigger policy, dependencies, boundaries, config;
- edges (`EdgeSpec`): `from`, `to`, `EdgeKind`, `EdgePolicySpec`;
- composite loops (`CompositeLoopSpec`) with `LoopPolicySpec`.

`EdgePolicySpec` captures channel mode, capacity, overflow behavior, lifespan/deadline, async `max_inflight`, ordering/drop semantics, timestamp domain, copy policy, owner, and readers.

`CompiledGraphPlan` contains compiled regions, region order, component-to-region mapping, and immediate-edge SCC information.

### 2.3 Validation and compilation flow

The graph compiler/validator performs these conceptual passes:

1. Basic graph schema/name/kind/clock validation.
2. Lane validation: ids, type, numeric budgets/capacity, overflow/overrun policy.
3. Component validation: unique ids, known registered type, valid execution lane, priority, event source and trigger policy syntax.
4. Registry-backed descriptor validation: component metadata, ports, config schema, role/boundary compatibility.
5. Lifecycle dependency order validation: missing dependencies, duplicates, cycles.
6. Edge validation: ids, endpoints, kind, channel policy, payload/port compatibility, async policy values.
7. Immediate-edge SCC detection via Tarjan algorithm.
8. CompositeLoop validation: every immediate feedback cycle must match exactly one composite loop; invalid composite loop ownership is rejected.
9. Region DAG condensation and topological ordering.
10. Dry-run and plan render surfaces for CLI/docs/testing.

Key invariant to preserve: immediate cycles are illegal unless the exact cycle is owned by a `CompositeLoop`; delay/state/async edges are temporal boundaries and do not create immediate SCCs.

### 2.4 Runtime orchestration

`RuntimeRunner` is the high-level embedded execution entry point:

1. validates the graph against the registry;
2. handles validate/dry-run/run modes;
3. creates runtime infrastructure: `RuntimeChannelBus`, `RuntimePublicationRouter`, `TraceCollector`, `MetricRegistry`, `RuntimeStateStore`, `ConfigSnapshotStore`, health event sink, structured logger;
4. instantiates components from `ComponentRegistry`;
5. configures and activates components;
6. restores/resets state if requested;
7. hands `EventRuntime` the compiled plan and runtime handles;
8. aggregates scheduler metrics, component metrics, trigger metrics, channel metrics, trace events, health events, runtime errors, lifecycle counts, loop state, and optional snapshots.

### 2.5 Event loop and scheduler semantics

`EventRuntime`:

- creates fixed-rate timing state;
- creates persistent worker pools for thread-pool lanes;
- builds a runtime region order from `CompiledGraphPlan`;
- applies config transactions and state epoch commits at epoch boundaries;
- calls `RuntimePublicationRouter::begin_epoch()` to make deferred publications visible;
- executes non-loop regions in compiled order;
- executes `CompositeLoop` regions for bounded loop iterations, convergence, cancellation, budget, and partial-success policies;
- asks `TriggerPolicyEngine` for ready invocations;
- executes invocations sequentially or through thread-pool lanes;
- commits immediate publications after each sequential invocation or after a worker batch depending on reentrancy/worker mode;
- records metrics, trace spans, health events, errors, and stop reason.

Scheduler semantics are cooperative. Timeouts/cancellation are observable and counted; they are not hard preemption.

### 2.6 Channel and publication semantics

`RuntimeChannelBus` owns bounded channel state. Channel modes include:

- latest/overwrite-style channel;
- every-message queue;
- latched snapshot;
- previous-tick channel;
- barrier.

Overflow policies include overwrite/drop-oldest/drop-newest/block/fail-fast, though `block` currently behaves as nonblocking admission rejection in full-queue handling and must be clarified or changed deliberately.

`RuntimePublicationRouter` stages publications across visibility boundaries:

- immediate: visible in current epoch/region order;
- delay/state: visible across epoch boundaries;
- async: admitted/deferred/completed according to async semantics;
- CompositeLoop: internal outputs are staged until loop commit or discarded on non-convergence/cancellation depending on policy.

### 2.7 Trigger semantics

`TriggerPolicyEngine` converts timer/message/request/task readiness into `Invocation` records. Current trigger families include:

- timer/manual/default invocations;
- any input;
- all inputs;
- time synchronization;
- batch;
- watermark;
- condition;
- debounce/coalescing;
- rate-limit/min interval.

Important implementation detail: for message sources, the engine drains channel inputs into internal pending queues, prunes timed-out messages, then checks rate-limit and specific collection policies. This creates correctness/performance risk if suppressed pending queues are not bounded.

### 2.8 Observability and docs/tests

The runtime emits:

- metrics;
- trace events/spans;
- health events;
- structured runtime errors;
- validation diagnostics;
- CLI JSON and plan outputs.

The docs tree is canonical and already split into reader-oriented zones. The docs smoke test checks command markers and required navigation/section contracts. This should be extended for a generated docs site without weakening existing docs validation.

## 3. Bug-fix and semantic-hardening plan

The following items are ordered by risk and dependency. Each item should be implemented with regression tests first.

### P0-1. Establish reproducible baseline before edits

**Scope**

- No code changes except, if necessary, documenting baseline failures.

**Commands**

```bash
git status --short
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
cmake --build build --target topoexec_format_check
./scripts/agent_check.sh
./scripts/sanitizer_check.sh   # ASAN+UBSAN mode where supported
./scripts/stress_smoke.sh
./scripts/fuzz_smoke.sh        # smoke only; may be nonblocking if matching existing CI policy
./scripts/bench_baseline.sh    # or documented benchmark command if script arguments differ
```

**Acceptance**

- Baseline pass/fail matrix recorded in `docs/31-planning-roadmap/goals/status.md` or a new goal file.
- Any pre-existing failure is recorded as a blocker with exact command and output snippet.
- No runtime behavior changed before baseline is known.

### P0-2. Previous-tick channel wake/update invariant

**Suspected issue**

In `RuntimeChannelBus::publish_to_state`, the previous-tick branch writes `pending_previous_tick` and returns accepted before the common accepted-publish path increments `update_sequence_` and calls `notify_all`. `wait_for_update()` waits on `update_sequence_` changes. If there is no separate notification during previous-tick visibility advancement, consumers waiting for updates may not wake as expected.

**Codex verification**

- Search `src/channel.cpp` for all `update_sequence_`, `pending_previous_tick`, and `advance_epoch` logic.
- Determine whether `advance_epoch()` increments/notifies for previous-tick visibility. If it does, assert that behavior with tests. If it does not, fix it.

**Desired invariant**

Every accepted publish or visibility-state transition that can make a reader observe a new message increments `update_sequence_` exactly once and wakes waiters.

**Tests**

Add focused tests in `tests/test_channel.cpp` or a new channel policy test:

1. publishing to a previous-tick edge changes observable update state or wakes a waiting thread according to documented semantics;
2. advancing an epoch makes previous-tick pending data visible and notifies waiters;
3. latest/every/latched behavior remains unchanged;
4. no double-notify/double-sequence increment for the same visibility event.

**Implementation options**

- Preferred: centralize accepted-publish finalization in a helper so all channel types use the same metric/update/notify path unless explicitly deferred.
- If previous-tick visibility should only notify on epoch advance, document that and assert it in tests.

### P0-3. Clarify or implement `overflow: block`

**Observed ambiguity**

The policy mapper accepts `overflow: block` and maps it to `DropPolicy::kBlockProducer`, but full-queue handling currently returns `false` with reason `would block producer` instead of actually blocking.

**Decision**

For the current alpha/core-runtime boundary, prefer **explicit nonblocking admission rejection** over introducing blocking in the channel hot path. Blocking producers can deadlock a single-process graph unless deadlines, cancellation, and scheduler ownership are specified.

**Actions**

- Rename documentation wording to “block is a nonblocking would-block rejection in v0.2-alpha” or similar.
- If YAML schema currently implies true blocking, adjust docs/schema comments/examples.
- Add tests that `overflow: block` returns rejected/would-block and emits metrics/health consistently.
- Add a deferred backlog item for true bounded blocking with cancellation/deadline semantics, if desired.

**Acceptance**

- No user-facing doc claims true blocking unless implemented.
- CLI lint/diagnostics explain the alpha semantics.
- Tests cover the exact result and metrics.

### P0-4. Bound trigger-policy pending queues under rate-limit/debounce/condition suppression

**Risk**

For message sources, the trigger engine drains channel inputs into internal pending queues, then applies rate-limit or condition suppression. If suppressed repeatedly, pending queues can grow independently of channel capacity. This weakens the “all queues bounded” goal and can produce latency spikes.

**Actions**

- Add a per-component pending bound derived from input channel capacities plus a small policy-defined cushion, or introduce explicit trigger pending capacity.
- For `rate_limit`, decide whether to keep newest, drop oldest, or coalesce. Recommended default: if `coalesce` or `debounce`, keep newest per input; otherwise drop oldest beyond derived bound and count `timeout_drop_count` or a new `pending_drop_count`.
- Emit health/metrics when trigger pending drops occur.
- Update docs for `rate_limit`, `debounce`, and `condition` suppression semantics.

**Tests**

- Continuous upstream messages with `min_interval_ms` cannot grow pending memory without bound.
- Drop/coalesce behavior is deterministic.
- Metrics expose suppressed and dropped counts.

### P0-5. Condition trigger head-of-line blocking

**Risk**

`condition: event_timestamp_present` inspects the front message for each input; if a front message lacks timestamp, the policy suppresses invocation and leaves the message in place. A later timestamped message can remain blocked behind the bad head item.

**Decision needed**

Choose one:

1. strict mode: missing timestamp stays at head and suppresses until max latency prunes it;
2. drop mode: missing timestamp is dropped with diagnostics/metrics;
3. schema/lint mode: reject graphs using `event_timestamp_present` on inputs whose edges do not provide timestamps.

**Recommendation**

Use option 2 for runtime resilience, plus option 3 as a lint warning where static metadata can identify the problem. Keep option 1 only if explicitly documented.

**Tests**

- Missing-timestamp head item does not permanently block timestamped items unless strict mode is documented.
- Late/missing timestamp counts are observable.

### P0-6. Async `policy.max_inflight` invariants

**Risk**

Async admission has several queues/stages: deferred ready, next epoch, composite external stage, inflight counts, cancellation/completion counters, drop-oldest/drop-newest/overwrite behavior. Count drift is easy.

**Actions**

- Add invariant checks in debug/test builds:
  - `async_inflight <= max_inflight` for every async channel with a positive limit;
  - every accepted async publication eventually decrements inflight on completion/cancel/drop;
  - drop-oldest removes from exactly one pending structure;
  - composite external staged async outputs are counted consistently.
- Add tests for `drop_oldest`, `drop_newest`, `overwrite`, cancellation, completion, composite loop discard/commit.

**Acceptance**

- Async metrics are monotonic where they should be and balanced where they should be.
- No negative/underflow-style counters.
- Stress smoke includes async-admission coverage.

### P0-7. CompositeLoop commit/discard semantics

**Risk**

CompositeLoop is core to immediate feedback safety. Outputs can be internal/immediate, external, delayed, state, or async; partial-success policy changes commit/discard behavior.

**Actions**

- Add table-driven tests for:
  - exact SCC ownership;
  - loop converged by component report;
  - residual threshold convergence;
  - max-iteration stop;
  - budget stop;
  - cancellation stop;
  - partial-success allowed/disallowed;
  - external immediate outputs commit only after loop success;
  - deferred/state/async outputs have documented visibility.
- Ensure trace and loop metrics reflect stop reason.

**Acceptance**

- Runtime semantics docs and tests agree on loop output visibility.
- Non-convergent loops cannot leak immediate external outputs unless policy explicitly allows partial success.

### P1-1. Graph validation diagnostics stability

**Actions**

- Convert critical validation/runtime errors from free-form only to structured diagnostic fields: phase, component, lane, edge, code/category, message.
- Keep current strings for compatibility but add stable codes for CLI JSON.
- Add golden tests for diagnostics JSON.

**Acceptance**

- CLI outputs remain human-readable.
- Machine-readable diagnostics have stable keys.
- Docs define code stability level pre-1.0.

### P1-2. Optional preview targets build matrix

**Actions**

- Ensure preview options can be turned on in at least one CI job without making runtime depend on them:
  - `TOPOEXEC_BUILD_C_API=ON`
  - `TOPOEXEC_BUILD_PYTHON_PREVIEW=ON`
  - `TOPOEXEC_BUILD_PLUGIN_LOADER=ON`
  - adapter preview flags as applicable.
- Keep failures scoped to preview job if dependencies are optional.

**Acceptance**

- Runtime-only builds remain clean.
- Preview docs explicitly say unstable/experimental.

## 4. Performance optimization plan

Performance work should not begin until semantic regression tests are in place. Prioritize measurement and low-risk allocation/contention reductions.

### 4.1 Baseline benchmark matrix

Create or update benchmarks for:

| Benchmark | Dimensions |
|---|---|
| Graph compile | 10, 100, 1k, 10k nodes; sparse/dense edges; with/without CompositeLoop. |
| Channel publish/read | latest, every, latched, previous_tick, barrier; capacities 1/16/1024; single/multiple readers. |
| Publication router | immediate, delay, state, async; composite loop staging. |
| Trigger policies | any_input, all_inputs, time_sync, batch, watermark, condition, rate_limit/debounce. |
| Event runtime | sequential lane, thread_pool lane, reentrant/non-reentrant, run_until_idle. |
| CLI | validate, plan, render, run, metrics, trace on representative examples. |

Store a versioned baseline artifact, e.g. `benchmarks/baselines/<date>-post-alpha.json`, or use the existing `scripts/bench_baseline.*` pattern if already established.

### 4.2 Hot-path indexing

Current runtime code frequently uses string-keyed `std::map` and `std::find_if` on component/region vectors. Low-risk optimization:

- Build stable runtime indexes once in `RuntimeRunner`/`EventRuntime`:
  - component id -> index;
  - component id -> spec/context/component pointer;
  - lane id -> lane config/pool;
  - region id -> region pointer/index;
  - input port -> channel id list;
  - edge source/target endpoint indexes.
- Use ordered maps only for deterministic serialization/output, not hot execution.
- Preserve deterministic result ordering by sorting at output aggregation boundaries.

### 4.3 Reduce message copies in `TriggerPolicyEngine`

Several collection paths copy `RuntimeChannelMessage` from deques before moving. Replace with move-from-front/back patterns where safe:

```cpp
auto message = std::move(queue.front());
queue.pop_front();
```

For coalescing, move the back message rather than copying it, then clear. For batch, move each front item.

Add tests ensuring payload ownership/correlation metadata remains correct.

### 4.4 Channel lock contention

Review channel critical sections:

- minimize work under mutex;
- move `notify_all()` outside lock after state mutation where safe;
- avoid expensive string construction in the hot path unless event/metric is emitted;
- pre-reserve queues and vectors where capacity is known;
- avoid repeatedly recomputing policy mappings.

Do not weaken thread-safety. Any lock refactor needs TSAN and stress smoke.

### 4.5 Payload ownership and copy policy

- Keep `copy` as the simple safe default for small payloads.
- Ensure `shared_view`, `loaned_view`, and `move_only` are documented with lifetime constraints.
- Add lint warnings for large payloads using `copy` if not already present.
- Add benchmarks for payload sizes: 1 KiB, 64 KiB, 1 MiB.

### 4.6 Acceptance thresholds

Initial post-alpha goal should avoid hard absolute latency claims. Use relative thresholds:

- no benchmark regression >10% on representative medians unless justified;
- no new unbounded memory path in stress tests;
- lower allocation count or runtime for trigger/channel hot paths after message move/index changes;
- benchmark JSON generated and committed only if repository convention allows committed baselines; otherwise attach as CI artifact and document location.

## 5. Documentation update plan

### 5.1 Normalize docs as source of truth

Keep `docs/README.md` as canonical map. Ensure the root README links to current numbered-zone docs. Add or verify link checks for:

- old path migrations;
- docs navigation anchors;
- examples referenced in docs;
- CLI command examples with `topoexec-doc-test` markers;
- schema references.

### 5.2 Documentation deltas by topic

| Topic | Required update |
|---|---|
| Channels | Exact semantics for latest/every/latched/previous_tick/barrier, `overflow: block`, health/metrics, multi-reader behavior. |
| Trigger policies | Bounded pending semantics, rate_limit/debounce behavior, condition head-of-line behavior, watermark lateness, batch window. |
| Async | `max_inflight` as async edge admission limit, not task executor capacity. Add metrics interpretation. |
| CompositeLoop | Output visibility and commit/discard matrix. |
| Scheduler | Cooperative cancellation/timeout, thread_pool bounded admission, no hard preemption/RT guarantees. |
| Public API | Stable/mixed/experimental surfaces, generated API reference link once Doxygen is added. |
| Release | Post-alpha hardening evidence, deferrals, Pages docs URL, Doxygen artifact. |
| CI/tools | How to build docs site locally and in CI. |

### 5.3 Doxygen recommendation

Recommendation: **introduce Doxygen**, but only as generated API reference. Do not move semantic docs, tutorials, architecture, schema docs, or roadmap content into Doxygen.

Rationale:

- TopoExec is a C++ library with public headers and preview/stability boundaries; embedders need searchable API reference.
- Existing Markdown docs are better for semantics, examples, architecture, and release evidence.
- Doxygen can be optional and dependency-free for normal runtime builds.

Initial implementation:

1. Add `docs/61-api/doxygen.md` explaining how API reference is generated and what it does not guarantee.
2. Add `Doxyfile.in` or `docs/Doxyfile.in`.
3. Add CMake option `TOPOEXEC_BUILD_DOCS=OFF` by default.
4. Add target `topoexec_doxygen` only if `Doxygen` is found.
5. Inputs:
   - `include/topoexec/runtime`
   - `include/topoexec/common`
   - `include/topoexec/adapters`
   - `include/topoexec/c_api`
   - `include/topoexec/plugins`
6. Exclude or mark internal/private implementation details in `src` unless explicitly needed.
7. Initial Doxygen settings:
   - `EXTRACT_ALL=YES` for first generated coverage, or `NO` if public comments are already complete;
   - `WARN_IF_UNDOCUMENTED=NO` initially to avoid mass churn;
   - later enable warnings for stable public headers only;
   - `GENERATE_HTML=YES`;
   - optional `GENERATE_XML=YES` only if future Sphinx/Breathe integration is chosen.
8. Add groups:
   - Runtime API;
   - Graph spec and compiler;
   - Components and registry;
   - Channels and publication routing;
   - Trigger policy;
   - Scheduler and cancellation;
   - Payload and ownership;
   - State/config;
   - Metrics/trace/diagnostics;
   - C API preview;
   - Adapter SDK preview;
   - Plugin loader preview.

Acceptance:

- `cmake --build build --target topoexec_doxygen` generates HTML.
- Generated docs include a stability disclaimer and link back to `docs/61-api/public-api.md`.
- Missing Doxygen on developer machines does not break normal builds.
- Public headers get minimal `@brief` comments on the most important types/functions touched by this goal.

## 6. GitHub Pages plan

### 6.1 Recommendation

Use GitHub Pages deployed from GitHub Actions with a static docs site built from Markdown plus Doxygen HTML under `/api/`.

Recommended stack:

- MkDocs for Markdown navigation/search, using the existing numbered docs tree.
- Doxygen for C++ API reference.
- GitHub Pages Actions deployment.

This keeps runtime dependencies untouched; docs dependencies live only in the docs build workflow.

### 6.2 Files to add

```text
mkdocs.yml
Doxyfile.in                         # or docs/Doxyfile.in
.github/workflows/pages.yml
docs/61-api/doxygen.md
docs/45-doc-standards/github-pages.md
```

Optional helper:

```text
scripts/docs_build_site.sh
```

### 6.3 MkDocs structure

Use `docs/README.md` as landing page or copy it to `index.md` during build. Suggested nav sections:

- Start Here
- Quickstart
- User Overview
- User Guide
- Integrations Preview
- Development Overview
- Architecture
- Codebase
- Testing
- Planning / Roadmap
- Specs / RFCs
- Development Tools
- CI / Build / Release
- Coding Standards
- Documentation Standards
- API
- Schemas / Protocols
- API Reference (Doxygen)

Build with strict mode so broken links fail CI:

```bash
mkdocs build --strict --site-dir site
```

### 6.4 Pages workflow outline

```yaml
name: Pages

on:
  push:
    branches: [main]
  workflow_dispatch:

permissions:
  contents: read
  pages: write
  id-token: write

concurrency:
  group: pages
  cancel-in-progress: true

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with:
          python-version: '3.x'
      - name: Install docs dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cmake g++ doxygen graphviz libcli11-dev libyaml-cpp-dev nlohmann-json3-dev ninja-build pkg-config
          python -m pip install --upgrade pip
          python -m pip install mkdocs mkdocs-material
      - name: Configure docs build
        run: cmake -S . -B build-docs -DCMAKE_BUILD_TYPE=Release -DTOPOEXEC_BUILD_DOCS=ON
      - name: Build Doxygen API reference
        run: cmake --build build-docs --target topoexec_doxygen
      - name: Build Markdown site
        run: mkdocs build --strict --site-dir site
      - name: Copy Doxygen into site/api
        run: |
          mkdir -p site/api
          cp -a build-docs/docs/doxygen/html/. site/api/
      - uses: actions/upload-pages-artifact@v3
        with:
          path: site
  deploy:
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    runs-on: ubuntu-latest
    needs: build
    steps:
      - id: deployment
        uses: actions/deploy-pages@v4
```

Adjust Doxygen output path to the actual CMake target output.

### 6.5 Repository settings

After the workflow is merged:

1. In GitHub repository settings, enable Pages.
2. Set source to GitHub Actions.
3. Run the `Pages` workflow manually once.
4. Add a docs badge/link in root README after the first successful deployment.

### 6.6 Pages acceptance

- `pages.yml` builds on `main` and manual dispatch.
- Site contains Markdown docs and `/api/` Doxygen reference.
- Broken links fail in Pages build.
- CI and Pages workflows are independent enough that a docs-site failure does not hide runtime CI failures.
- README links to the deployed site only after deployment succeeds.

## 7. CI, release, and roadmap integration

### 7.1 CI changes

Keep existing CI jobs and add:

- docs-site build job;
- optional preview target build job;
- benchmark smoke/baseline job, possibly nonblocking until stable;
- Pages workflow.

Do not make TSAN or fuzz mandatory if the repository already treats them as nonblocking smoke jobs. Preserve current `continue-on-error` policy unless explicitly changed.

### 7.2 Release evidence

Update:

- `docs/43-ci-build-release-tools/current-baseline.md` with new baseline commands and results;
- `docs/43-ci-build-release-tools/release-checklist.md` with docs-site and Doxygen artifact checks;
- `docs/43-ci-build-release-tools/release-runbook.md` if Pages/Doxygen become part of release prep;
- `CHANGELOG.md` Unreleased with bug fixes/docs/perf entries.

### 7.3 Roadmap status

Add a new goal record, for example:

```text
ID: G71-post-alpha-hardening-docs-pages
Priority: P0/P1 mixed
Status: in-progress
Scope: src/channel.cpp, src/trigger_policy.cpp, src/event_runtime.cpp, tests/*, docs/*, CMakeLists.txt, mkdocs.yml, Doxyfile.in, .github/workflows/pages.yml, scripts/docs_build_site.sh as needed
Acceptance: semantic regression tests, bounded trigger pending, previous_tick invariant, block overflow docs/tests, async/composite loop tests, benchmark baseline, Doxygen API docs, GitHub Pages workflow, updated status/release docs
Validation: scripts/agent_check.sh, sanitizer smoke, docs goal check, docs site build, Doxygen target, selected benchmarks
Blocker protocol: docs/31-planning-roadmap/goals/blockers/G71-post-alpha-hardening-docs-pages.md
```

## 8. Suggested Codex `/goal`

The following block is written to be pasted directly into Codex.

```text
/goal
You are working in sean2077/topoexec. Create and complete a new post-alpha hardening goal named G71-post-alpha-hardening-docs-pages.

Context:
- TopoExec is a C++20 single-process stateful dataflow runtime. Preserve the core boundary: do not turn preview ROS2/OpenTelemetry/Prometheus/Python/C API/plugin surfaces into production/stable surfaces unless explicitly required by this goal.
- Current docs/31-planning-roadmap/goals/backlog.md says no active implementation goal is open. Add this new goal/status entry instead of reopening G0-G70 history.
- Follow the repository goal protocol: small reviewable slices, tests before behavior changes, docs and runtime behavior aligned, all queues bounded, no new runtime dependencies without documented reason.

Primary objectives:
1. Establish a reproducible baseline and record exact pass/fail evidence.
2. Harden channel/trigger/async/composite-loop semantics with regression tests.
3. Fix verified bugs and document intentional alpha limitations.
4. Add performance baselines and low-risk hot-path optimizations.
5. Introduce Doxygen-generated C++ API reference as optional docs-only tooling.
6. Add a GitHub Pages static documentation workflow that publishes Markdown docs plus Doxygen under /api/.
7. Update README/docs/release/goal status so users understand what changed and what remains deferred.

Phase M0 — Baseline and goal record:
- Read AGENTS.md, CONTRIBUTING.md, docs/31-planning-roadmap/goals/README.md, docs/31-planning-roadmap/goals/backlog.md, docs/31-planning-roadmap/goals/status.md, docs/21-architecture/runtime-architecture.md, docs/21-architecture/runtime-semantics.md, docs/21-architecture/runtime-invariants.md, docs/61-api/public-api.md, docs/43-ci-build-release-tools/current-baseline.md, docs/43-ci-build-release-tools/release-runbook.md.
- Add a G71 goal/status entry with scope, acceptance, validation commands, and blocker protocol.
- Run baseline commands from a clean tree:
  git status --short
  cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
  cmake --build build -j
  ctest --test-dir build --output-on-failure
  cmake --build build --target topoexec_format_check
  ./scripts/agent_check.sh
- Also run sanitizer/stress/fuzz/bench scripts where repository docs indicate they are valid for local execution. If a command fails due to environment/dependency constraints, record exact blocker/evidence; do not hide it.

Phase M1 — P0 semantic bug hardening:
- Inspect src/channel.cpp for update_sequence_, wait_for_update, previous_tick, and advance_epoch behavior.
- Add tests proving the invariant: every accepted publish or visibility-state transition that can make a reader observe new data increments the update sequence and wakes waiters exactly once.
- If previous_tick publishes or epoch advancement violate the invariant, fix with the smallest safe refactor.
- Inspect overflow: block. If it currently returns would-block instead of truly blocking, keep nonblocking alpha semantics unless a documented safe blocking design already exists. Update docs/schema/CLI diagnostics/tests so block is not falsely advertised as hard blocking.
- Inspect TriggerPolicyEngine pending queues. Add bounded pending behavior for rate_limit/debounce/condition suppression, or document and block if a product decision is required. Prefer deterministic drop-oldest or keep-newest semantics with metrics/health events.
- Inspect condition event_timestamp_present head-of-line behavior. Prefer dropping/diagnosing missing-timestamp head items over permanent suppression unless existing docs require strict behavior. Add tests either way.
- Inspect async policy.max_inflight accounting. Add tests/invariants for accept/complete/cancel/drop-oldest/drop-newest/overwrite and composite-loop async staging.
- Add CompositeLoop tests for convergence, max iterations, budget/cancel stop, partial-success commit/discard, and external output visibility.

Phase M2 — Performance baseline and safe optimizations:
- Run or create benchmark baselines for graph compile, channel publish/read, publication routing, trigger policies, event runtime, and CLI validate/plan/run.
- Optimize only after tests pass.
- Prefer low-risk changes: precomputed component/lane/region indexes in EventRuntime/RuntimeRunner, moving RuntimeChannelMessage out of trigger pending queues instead of copying, reserving vectors, reducing string/map hot-path work, moving notify_all outside locks where safe.
- Keep deterministic output order by sorting at output boundaries if unordered/indexed runtime structures are introduced.
- Record before/after benchmark evidence. Do not claim absolute latency guarantees.

Phase M3 — Documentation alignment:
- Update channel docs, trigger policy docs, async docs, CompositeLoop docs, scheduler/concurrency docs, public API docs, release docs, and CHANGELOG.md for all behavior changes.
- Keep preview/deferral language explicit: production ROS2, OpenTelemetry/Prometheus exporters, native Python bindings, stable C ABI, schema v2 implementation/migration CLI, sandboxed plugin ecosystem, hard RT, package registry publication remain deferred unless this goal explicitly changes only documentation wording.
- Run docs checks and any docs command smoke tests.

Phase M4 — Doxygen API reference:
- Add optional docs-only Doxygen support. Do not make Doxygen required for normal runtime builds.
- Add Doxyfile.in (or docs/Doxyfile.in), CMake option TOPOEXEC_BUILD_DOCS=OFF by default, and custom target topoexec_doxygen when Doxygen is found.
- Generate HTML for public headers under include/topoexec/runtime, include/topoexec/common, include/topoexec/adapters, include/topoexec/c_api, include/topoexec/plugins.
- Add docs/61-api/doxygen.md explaining that Doxygen is generated API reference, not semantic source of truth, and preview APIs remain preview.
- Add minimal @brief/@ingroup comments for touched public headers if needed.

Phase M5 — GitHub Pages:
- Add mkdocs.yml or an equivalent minimal static-site build. Prefer MkDocs unless repository policy rejects Python docs deps.
- Add .github/workflows/pages.yml using GitHub Pages Actions deployment. Build Markdown docs and copy Doxygen HTML under site/api.
- Add docs/45-doc-standards/github-pages.md documenting local and CI docs-site build.
- Do not update README with a public Pages URL until the workflow is expected to deploy successfully; if adding a placeholder, state it as pending.

Phase M6 — Final validation and evidence:
- Run:
  ./scripts/agent_check.sh
  ctest --test-dir build --output-on-failure
  cmake --build build --target topoexec_format_check
  docs goal check / docs smoke commands used by the repo
  Doxygen target
  MkDocs/static site build
  sanitizer/stress/fuzz/bench checks as appropriate
- Update docs/31-planning-roadmap/goals/status.md with exact evidence.
- Update docs/31-planning-roadmap/goals/backlog.md if new deferred decisions were created.
- Leave blockers in docs/31-planning-roadmap/goals/blockers/G71-post-alpha-hardening-docs-pages.md if any acceptance criterion cannot be completed locally.

Definition of done:
- All P0 semantic tests pass.
- No unbounded queue/pending path remains in channel/trigger changes introduced by this goal.
- Docs and behavior agree for previous_tick, overflow:block, rate_limit/debounce/condition, async max_inflight, CompositeLoop output visibility.
- Doxygen API docs are generated by an optional target.
- GitHub Pages workflow builds a site from docs and Doxygen.
- Existing runtime CI remains green or exact environmental blockers are documented.
- scripts/agent_check.sh passes before declaring the repository complete.
```

## 9. Risk register

| Risk | Severity | Mitigation |
|---|---:|---|
| Changing channel/trigger semantics breaks examples or goldens | High | Add tests first, update docs/goldens intentionally, preserve existing defaults when ambiguity exists. |
| True `overflow: block` causes deadlocks | High | Do not implement true blocking without a separate design for cancellation/deadline/scheduler ownership. |
| Trigger pending bound drops data users expected | Medium/High | Make policy explicit, emit metrics/health, document drop/coalesce rules. |
| Replacing maps with unordered indexes changes output order | Medium | Keep deterministic serialization by sorting result keys before output. |
| Doxygen warnings create large documentation churn | Medium | Start with warnings nonfatal; enable gradually for stable headers. |
| MkDocs/Doxygen dependencies slow CI | Medium | Keep docs job separate; cache Python deps if needed; do not affect runtime CI. |
| Pages deployment requires repo setting changes | Medium | Add workflow and docs; repository owner must enable Pages source as GitHub Actions. |
| Preview surfaces accidentally look stable | Medium | Repeat preview disclaimers in public API, Doxygen landing, README, release notes. |

## 10. Non-goals

Do not include these in G71 unless a blocker decision explicitly opens them:

- schema v2 loader or migration CLI;
- production OpenTelemetry/Prometheus exporters;
- real ROS 2 client library integration;
- native Python bindings;
- stable C ABI beyond current preview/ABI version policy;
- sandboxed or unload-safe plugin ecosystem;
- package registry publication;
- hard real-time scheduling, CPU affinity guarantees, or preemptive cancellation;
- external Perfetto adapter;
- GUI editor or LSP server.

## 11. Final acceptance checklist

Before closing the iteration, Codex should be able to point to:

- new/updated G71 goal/status docs;
- regression tests for previous_tick update/wake behavior;
- tests/docs for `overflow: block` semantics;
- bounded trigger pending tests/docs;
- condition timestamp behavior tests/docs;
- async max_inflight tests/docs;
- CompositeLoop output visibility tests/docs;
- benchmark baseline evidence;
- Doxygen target and generated API docs path;
- Pages workflow and local docs-site build instructions;
- updated README/CHANGELOG/release docs;
- passing `scripts/agent_check.sh` or documented blocker with exact failure evidence.
