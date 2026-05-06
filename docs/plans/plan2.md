# TopoExec 完备架构发展计划（Post-G25 Codex Goal Spec）

> 适用对象：`sean2077/topoexec` 当前 `main` 已完成上一轮 G0–G25 后续迭代之后的下一阶段规划。  
> 使用方式：本文件可直接作为 **Codex Goal / Codex CLI / oh-my-codex** 的长期迭代 spec。  
> 计划目标：从“可运行、可验证的 post-alpha runtime”推进到“架构更完备、API 更稳定、可被真实 C++ 应用嵌入、可接 adapter、可发布 beta 的 semantic execution graph runtime”。

---

## 0. 当前状态判断

### 0.1 已完成基础

根据当前公开仓库状态，TopoExec 已经具备以下基础：

- 项目定位已经明确：**C++20 single-process stateful execution graph runtime**。
- 已经有核心 runtime 语义：
  - `Component`
  - `GraphSpec`
  - `immediate / delay / state / async` edge kind
  - bounded channel policy
  - trigger policy
  - CompositeLoop
  - payload ownership
  - metrics / trace
- 已经有 `topoexec::core`、`topoexec::runtime`、`topoexec::yaml` target 边界。
- 已经有纯 C++ embedding 路径和 `GraphBuilder`。
- 已经有 CLI：
  - `validate`
  - `plan`
  - `render`
  - `run`
  - `metrics`
  - `trace`
  - `lint`
  - `explain`
  - `diff-plan`
  - `bench`
  - `schema dump`
  - `schema check`
  - `doctor`
- 已经有 runnable examples：
  - minimal pipeline
  - overload latest vs queue
  - control feedback delay
  - CompositeLoop fixed point
  - async worker
  - C++ builder minimal
  - state/config snapshot
  - batch/time-sync
  - service-style async
  - boundary adapter pattern
- 已经有 runtime invariant coverage、golden tests、schema contract smoke、docs command smoke、deterministic fuzz smoke、sanitizer gates、package smoke。
- 当前 goal ledger 记录 G0–G25 已完成。
- 当前 release checklist 记录本地 `50/50 CTest`、ASAN+UBSAN、format、runtime-only option smoke、adapter policy smoke 等证据。
- 当前已发布或已准备 `v0.1.0-alpha`，`main` 已经进入 post-alpha concurrency/async admission 后的状态。

### 0.2 当前明确 limitations

当前仍应视为未完成或未产品化的方向：

- `thread_pool` lane 在 G30 后已有 persistent worker-pool v1，但仍是 experimental alpha concurrency surface：
  - priority 未真正调度化
  - CPU affinity 未执行
  - RT policy 未执行
  - portable hard worker-name guarantee 未实现
  - timeout preemption 未实现
- `async policy.max_inflight` 已用于 async edge admission，但不是完整 async task/future executor。
- `TaskExecutor` 当前偏 deterministic helper，threaded executor pool 仍是未来方向。
- deterministic fuzz smoke 已有，但 coverage-guided fuzzing 仍未完成。
- TSAN 仍可保持 non-blocking，beta 前需要更强并发信心。
- ROS 2、OpenTelemetry、Prometheus、Python、C API、dynamic plugin loading、external Perfetto adapter 仍 deferred。
- package-manager recipes 仍是 draft，不应宣称生态包已成熟。
- API 仍处于 pre-1.0，可继续调整，但必须通过版本策略和 changelog 记录。
- 当前工具和文档已较丰富，下一阶段不应优先继续堆 CLI 命令，而应强化 runtime、API、concurrency、adapter-ready boundary 和真实应用信心。

---

## 1. 下一阶段北极星

TopoExec 接下来应从：

```text
能跑、能验证、能解释的 MVP/alpha runtime
```

推进为：

```text
可嵌入、可组合、可稳定发布、可接 adapter、可被真实应用试用的 semantic graph runtime
```

核心价值仍然是：

1. **Semantic graph compiler**  
   在运行前验证 graph contract：edge visibility、trigger、CompositeLoop、channel policy、payload policy、schema compatibility、scheduler constraints。

2. **Deterministic in-process runtime**  
   让进程内组件图具备明确 epoch、transaction、commit、visibility、ready、backpressure、metrics、trace 语义。

3. **Feedback-safe architecture**  
   所有环必须通过 `delay / state / async` 打断，或显式由 CompositeLoop region 拥有。

4. **Bounded overload behavior**  
   所有 queue、channel、task、worker lane 都必须有界；不允许隐式无界 backlog。

5. **Embeddable C++ API**  
   真实 C++ 应用可以只依赖 `topoexec::runtime`，用显式 registry 和 builder 嵌入。

6. **Adapter-ready but adapter-clean core**  
   adapter 消费稳定 runtime observer、boundary bridge、result sink、component factory API；adapter 不污染 core schema 和 runtime target。

7. **Observation-first runtime**  
   metrics、trace、diagnostics、bench、debug snapshot 是 runtime 的一等输出，不是可选附属品。

---

## 2. Codex Goal 总规则

每个 Codex Goal 必须遵守：

```text
1. 一个 goal 只做一类架构变化。
2. 先写或更新测试，再改实现。
3. 语义变化必须更新 docs/runtime-semantics.md 或对应 reference doc。
4. public API 变化必须更新 docs/public-api.md、docs/versioning.md、CHANGELOG.md。
5. schema v1 语义不得 silent change；破坏性变化必须走 schema v2 或 migration。
6. 不新增重依赖；adapter 以外不得引入 adapter SDK。
7. 不允许无界队列、无界 worker backlog、无界 task backlog。
8. 不允许组件直接调用下游组件；publish 仍必须经过 runtime staging/routing。
9. Metrics/trace 只能观测，不得反向影响调度。
10. 如果需要产品/API 决策，写 blocker 到 docs/goals/blockers/，并继续安全独立 goal。
```

### 2.1 建议 goal 交付格式

每个 goal 完成后必须更新：

- `docs/goals/status.md`
- `docs/goals/backlog.md` 或新的 plan ledger
- `CHANGELOG.md`，如果 public behavior/API/docs 有变化
- 对应 docs
- 对应 tests
- PR summary

建议每个 goal 使用这个 prompt 模板：

```md
Goal ID:
Title:

Context:
- Read docs/plans/plan.md.
- Read docs/goals/status.md.
- Read AGENTS.md.
- Current goal is post-G25 next-plan work.

Objective:
- ...

Allowed files:
- ...

Do not modify:
- ...

Implementation requirements:
- ...

Acceptance criteria:
- ...

Validation:
- ./scripts/agent_check.sh
- optional focused checks:
  - ./scripts/goal_check.sh quick
  - ./scripts/goal_check.sh sanitizer
  - ./scripts/goal_check.sh package
  - ./scripts/goal_check.sh docs
  - ./scripts/goal_check.sh fuzz

Failure protocol:
- If blocked after one focused fix, write docs/goals/blockers/<goal-id>.md.
- Do not ask routine questions.
- Make reversible assumptions and record them.
```

---

## 3. 版本路线图

### v0.2.x：Architecture Stabilization Alpha

目标：把 post-alpha 已实现能力从“完成任务”变成“可维护架构”。

重点：

- public API audit
- scheduler v2 design
- worker lane hardening
- async executor separation
- observer API
- graph compiler diagnostics hardening
- runtime benchmark baseline
- schema v1 compatibility lock
- release candidate process

### v0.3.x：Adapter-Ready Alpha

目标：让 adapter 能开始以独立 package 方式接入。

重点：

- adapter SDK preview
- ResultSink / RuntimeObserver / BoundaryBridge API
- C API design draft
- dynamic plugin loader design draft
- ROS 2 fake-boundary tests
- OTel/Prometheus dry-run exporter tests
- Python config/test binding plan

### v0.4.x：Real Application Preview

目标：让真实应用可以试用 TopoExec，而不只是 demo。

重点：

- app templates
- registry/factory patterns
- hierarchical graph/subgraph
- component state snapshot/restore
- config hot reload transaction
- stress/fuzz/concurrency confidence
- benchmark regression dashboard

### v0.5.x：Beta Candidate

目标：形成 beta 质量。

重点：

- stricter API stability
- documented deprecation policy
- stable CLI JSON compatibility
- package-manager integration
- adapter preview packages
- coverage-guided fuzzing
- TSAN confidence
- reproducible release artifacts

### v1.0：Stable Semantic Runtime

目标：核心 runtime 语义、schema、public API、adapter boundary 稳定。

重点：

- schema compatibility policy locked
- C++ runtime API stable
- observer/adapter contracts stable
- plugin/FFI policy clear
- benchmark and observability contracts stable
- failure model stable

---

# 4. Goal Board：Post-G25 新阶段

下面从 `G26` 开始编号，避免和当前已完成的 `G0–G25` 冲突。

---

## G26. Release Candidate Baseline 2

Priority: P0

### Objective

为当前 post-G25 main 建立新的 release-candidate baseline，避免继续迭代时覆盖掉刚完成的目标证据。

### Tasks

1. 新增或更新：
   - `docs/current-baseline.md`
   - `docs/release-progression.md`
   - `docs/release-checklist.md`
   - `docs/goals/status.md`
2. 记录：
   - current commit SHA
   - CTest 数量
   - sanitizer 结果
   - format 结果
   - package smoke 结果
   - runtime-only option smoke 结果
   - current limitations
3. 固化 post-G25 golden outputs：
   - plan JSON
   - metrics JSON
   - trace JSON
   - Chrome trace shape
   - schema dump
   - doctor JSON
4. 若当前 main 未打 tag，建议准备：
   - `v0.2.0-alpha.0` 或 `v0.1.1-alpha` release decision note

### Acceptance

- 新 baseline 文档清楚说明“G0–G25 complete”之后的起点。
- `docs/goals/backlog.md` 不再把 G0–G25 当作 active work。
- release checklist 可直接用于下一个 prerelease tag。
- `./scripts/agent_check.sh` 通过。
- `./scripts/goal_check.sh sanitizer` 通过或记录 blocker。

---

## G27. Public API Stability Pass v2

Priority: P0

### Objective

将当前 public API 从“可用”推进到“pre-beta 可依赖”，清晰区分 stable、experimental、internal、adapter-preview。

### Tasks

1. 更新 `docs/public-api.md`：
   - header 列表
   - type 列表
   - function/class stability
   - schema/CLI JSON stability
   - adapter-preview stability
2. 给 public headers 加注释或 marker：
   - stable-v0.2
   - experimental
   - internal-use-only
3. 检查 installed headers：
   - 不暴露不该暴露的 implementation details
   - runtime-only downstream 不需要 YAML/CLI/adapters
4. 增加 public API smoke：
   - minimal app
   - component registry
   - graph builder
   - typed payload helpers
   - RuntimeRunner
   - metrics/trace result consumption
5. 增加 API diff checklist：
   - 让后续 Codex goal 不能静默修改 public API

### Acceptance

- `docs/public-api.md` 与 install/export 事实一致。
- public API smoke 覆盖主要嵌入路径。
- API 变更必须导致 docs/changelog 或 test 更新。
- `topoexec::runtime` 仍不依赖 YAML/CLI/adapters。

---

## G28. Runtime Semantic Version Contract

Priority: P0

### Objective

把 runtime 行为从“文档说明”提升成可版本化的 semantic contract，便于后续 schema v2 或 v1 additive fields 不破坏旧用户。

### Tasks

1. 新增 `docs/semantic-contract.md`。
2. 定义：
   - epoch
   - transaction
   - commit
   - staged publication
   - edge visibility
   - trigger readiness
   - channel capacity
   - CompositeLoop external output commit
   - async deferred completion
   - state/config snapshot boundary
3. 为每个语义标注：
   - v0.1 stable
   - v0.2 stable
   - experimental
   - future extension
4. 增加 `topoexec graph contract` 或不新增 CLI，只在 `doctor/schema dump` 中暴露 semantic contract version。
5. 明确 schema v1 与 semantic contract 的关系：
   - schema field compatibility
   - runtime meaning compatibility
   - breaking change policy

### Acceptance

- 未来 goal 可以引用 semantic contract 而不是分散引用多个文档。
- versioning 文档说明 schema version 与 runtime semantic version 的区别。
- golden tests 或 schema dump 包含 semantic contract version。
- 不改变现有 graph 行为。

---

## G29. Scheduler v2 Design + Contract

Priority: P0

### Objective

把当前 scheduler lane MVP 推进为 v2 contract，明确哪些是实现能力、哪些是 advisory fields、哪些是未来 extension。

### Tasks

1. 更新 `docs/scheduler.md` 和 `docs/concurrency.md`。
2. 明确 lane 类型：
   - `event_loop`
   - `fixed_rate`
   - `thread_pool`
   - future: `isolated_thread`
   - future: `manual_step`
3. 定义每种 lane 的：
   - admission
   - queue capacity
   - overflow
   - component non-reentrant lock
   - stop behavior
   - budget behavior
   - metrics
   - trace spans
4. 明确 advisory fields：
   - priority
   - cpu_affinity
   - nice_priority
   - rt_policy
   - rt_priority
   - thread_name
5. 明确不支持：
   - hard preemption
   - hard real-time guarantee
   - implicit OS scheduler tuning
6. 给 scheduler plan JSON 增加 lane capability summary。
7. 增加 validation：
   - unsupported/advisory fields should produce warnings/diagnostics where appropriate, not silent claims.

### Acceptance

- 用户能从 docs 看懂 thread_pool 到底保证什么。
- plan JSON 或 diagnostics 能显示 lane capability / advisory fields。
- 不再出现“schema 里有字段但 runtime 默默忽略且无提示”的危险情况。
- 现有 tests 通过。

---

## G30. Persistent Worker Pool v1

Priority: P1

### Objective

将 bounded `thread_pool` 从 batch-style MVP 推进到可解释的 persistent worker pool v1。

### Requirements

- 仍不承诺 hard real-time。
- 仍不实现强制 timeout preemption。
- 必须保持 bounded queue。
- 必须尊重 non-reentrant component serialization。
- 必须能 clean shutdown。

### Tasks

1. 增加 persistent worker thread lifecycle：
   - start
   - wait
   - stop
   - join
2. worker 线程命名：
   - 若平台支持则设置
   - 不支持则记录 advisory metric/diagnostic
3. bounded queue：
   - capacity
   - overflow
   - rejection/drop metrics
4. fairness：
   - define simple FIFO within lane
   - future priority queue deferred
5. stop token：
   - worker wait must unblock on stop
   - no deadlock on shutdown
6. tests：
   - workers persist across multiple runtime steps
   - stop while queue non-empty
   - non-reentrant component not overlapped
   - reentrant component can overlap up to `max_threads`
   - queue overflow under load
   - execute failure stops or reports according to current fail-fast policy

### Acceptance

- `thread_pool` lane no longer merely appears as bounded batch execution.
- Metrics expose active workers, queued invocations, completed invocations, rejected invocations.
- Trace includes worker id / lane id / component id.
- TSAN job remains green or documented non-blocking blocker.

---

## G31. Fixed-Rate Lane v1

Priority: P1

### Objective

将 `fixed_rate` 从 simulated tick 推进为可选 wall-clock fixed-rate lane v1。

### Requirements

- Runtime must support deterministic test mode and wall-clock mode separately.
- No hard real-time claim.
- Overrun behavior must be explicit.

### Tasks

1. Add lane config:
   - `wall_clock_enabled`
   - `period_ms`
   - `tick_budget_ms`
   - `overrun_policy`
2. Define overrun policies:
   - `skip_next`
   - `catch_up_once`
   - `drop_tick`
   - default: `drop_tick` or current deterministic behavior
3. Metrics:
   - tick count
   - overrun count
   - jitter
   - max lateness
4. Trace:
   - tick begin/end
   - overrun
   - skipped tick
5. Tests:
   - deterministic fixed rate stays reproducible
   - simulated overrun metric
   - wall-clock mode behind opt-in test or integration smoke
6. Docs:
   - explain difference between deterministic stepping and wall-clock scheduling

### Acceptance

- `fixed_rate` no longer relies only on bounded runtime steps for documentation.
- All wall-clock behavior is opt-in and testable.
- No timing threshold flakiness in normal CI.

---

## G32. Scheduler Priority and Admission Policy v1

Priority: P1

### Objective

实现轻量的 runtime-level priority/admission，不涉及 OS priority。

### Tasks

1. Distinguish:
   - component priority
   - invocation priority
   - lane queue priority
   - OS scheduler priority
2. Implement priority queue or bucketed queue inside lane.
3. Define tie-breakers:
   - epoch
   - enqueue order
   - component id
4. Metrics:
   - priority class counts
   - low-priority drop/rejection
   - starvation guard count
5. Validation:
   - priority allowed only where lane supports it
   - OS priority fields remain advisory
6. Tests:
   - high priority executes before low priority
   - no starvation under bounded example
   - queue overflow respects configured overflow and priority policy

### Acceptance

- Runtime-level priority has deterministic semantics.
- OS priority fields remain clearly advisory.
- `plan`/`explain` distinguish runtime priority vs OS hints.

---

## G33. Cooperative Cancellation and Timeout Semantics

Priority: P1

### Objective

为 long-running component、task、CompositeLoop 提供 cooperative cancellation contract，而不是伪装为硬 preemption。

### Tasks

1. Define `CancellationToken` public API stability.
2. Add timeout budget propagation:
   - component invocation
   - CompositeLoop
   - TaskExecutor
3. Add cooperative check helper:
   - `ctx.cancel_requested()`
   - `invocation.cancel_token`
4. Failure semantics:
   - timeout observed
   - timeout not observed
   - cancellation acknowledged
5. Metrics/trace:
   - cancellation requested
   - cancellation observed
   - timeout budget exceeded
6. Tests:
   - component observes cancel token
   - component ignores cancel token and runtime reports over-budget without forced kill
   - CompositeLoop timeout request stops between iterations
   - TaskExecutor cancellation removes pending tasks

### Acceptance

- Timeout semantics are honest and non-misleading.
- No forced thread termination.
- Docs explicitly say no hard preemption.

---

## G34. TaskExecutor v2: Threaded Executor Preview

Priority: P1/P2

### Objective

将 deterministic `TaskExecutor` 扩展为可选 threaded executor preview，同时保持 deterministic mode 作为测试默认。

### Tasks

1. Split interface:
   - `ITaskExecutor`
   - `DeterministicTaskExecutor`
   - `ThreadedTaskExecutor`
2. Threaded config:
   - max_workers
   - max_inflight
   - queue_capacity
   - overflow
   - shutdown policy
3. Completion routing:
   - task result -> component.port -> async edge
   - no direct downstream call
4. Error handling:
   - failed task
   - cancelled task
   - rejected task
5. Metrics:
   - queued
   - active
   - completed
   - failed
   - cancelled
   - rejected
6. Tests:
   - deterministic mode stays unchanged
   - threaded mode smoke
   - cancel pending
   - shutdown with tasks
   - completion publication exactly once
7. Docs:
   - explain async edge vs TaskExecutor
   - explain deterministic vs threaded mode

### Acceptance

- Core runtime can host async task work without forcing users to provide their own executor.
- Threaded executor is opt-in and bounded.
- No unbounded future/task backlog.

---

## G35. Trigger Engine v2: Watermark and Condition Triggers

Priority: P2

### Objective

扩展 trigger policy，但不破坏现有 v1 trigger semantics。

### Tasks

1. Add future trigger types:
   - `watermark`
   - `condition`
   - `debounce`
   - `rate_limit`
2. Decide whether they belong in schema v1 additive fields or schema v2.
3. For initial implementation:
   - support internal C++ only or schema experimental flag
4. Watermark:
   - based on timestamp domain
   - handles late data
   - emits late/drop metrics
5. Condition:
   - avoid arbitrary scripting
   - use declarative condition over readiness/metadata only
6. Tests:
   - watermark accepts aligned data
   - watermark drops late data
   - debounce coalesces events
   - rate limit prevents excessive ready invocations

### Acceptance

- Existing `any/all/time_sync/batch/request/task_ready` unaffected.
- New trigger types do not introduce arbitrary code execution.
- Metrics explain why trigger did or did not fire.

---

## G36. Correlation, Causality, and Invocation Metadata

Priority: P1

### Objective

让 runtime trace/metrics 能从输入事件追踪到下游 outputs，支持调试复杂 graph。

### Tasks

1. Define metadata:
   - correlation_id
   - causation_id
   - epoch_id
   - transaction_id
   - source component
   - source port
   - trigger kind
2. Propagate metadata through:
   - publish
   - channel
   - trigger
   - invocation
   - task completion
   - CompositeLoop external commit
3. Trace:
   - include correlation/causation fields
4. Metrics:
   - optional labels, bounded cardinality
5. CLI:
   - `trace --filter-correlation` optional future
6. Tests:
   - correlation stable through immediate chain
   - delay edge carries causation across epoch
   - async completion maintains original request id
   - CompositeLoop output includes loop causation

### Acceptance

- Users can explain “why did this component execute?” from trace events.
- Metadata labels do not explode metrics cardinality by default.

---

## G37. Channel v2: Explicit Backpressure Events

Priority: P1

### Objective

将 backpressure 从 metrics-only 提升为 optional runtime health event，不改变执行控制流。

### Tasks

1. Define `HealthEvent`:
   - channel overflow
   - stale drop
   - deadline miss
   - backpressure high-watermark
   - task reject
   - scheduler reject
2. Add bounded health event sink/ring buffer.
3. Expose in:
   - RuntimeRunnerResult
   - CLI metrics/doctor
   - trace
4. Allow config:
   - emit_health_events true/false
   - health_event_capacity
5. Tests:
   - high-watermark event emitted once/coalesced
   - overflow event carries edge id and policy
   - health events are bounded
   - health sink never blocks runtime

### Acceptance

- Health is observable without recursive control flow.
- Runtime does not call components because a health event was emitted unless explicitly wired via a normal graph boundary in future.

---

## G38. Channel v2: Multi-Reader and Move-Only Hardening

Priority: P1

### Objective

加强 multi-reader、single-reader、move-only、shared/loaned view 的 correctness 和 explainability。

### Tasks

1. Add more tests:
   - slow reader misses bounded history
   - multi-reader cursor under overflow
   - move-only invalid for multi-reader
   - loaned view release semantics
   - shared view lifetime
2. Add diagnostics:
   - incompatible readers/copy policy
   - high risk payload policy
3. Add docs:
   - when to use copy/shared_view/loaned_view/move_only
   - ownership diagrams
4. Add plan/explain output:
   - readers
   - copy policy
   - possible drops due to slow reader

### Acceptance

- Users can reason about multi-reader delivery and lifetime.
- Copy policy misuse produces diagnostic/lint warning.
- No surprise accidental copies for loaned/move paths.

---

## G39. Payload and Memory v2

Priority: P1/P2

### Objective

将 payload system 从 useful helper 推进为可嵌入应用的内存策略层。

### Tasks

1. Define memory concepts:
   - owned value
   - shared view
   - loaned view
   - move-only payload
   - opaque payload
   - frame/buffer payload
2. BufferPool v2:
   - fixed-size pool
   - variable-size buckets
   - alignment
   - max bytes
   - high-watermark metrics
   - release leak detection
3. Payload schema:
   - payload type name
   - payload schema id
   - summary string
   - size estimate
4. Add payload policy lints:
   - large payload copied
   - multi-reader move-only
   - loaned view without pool owner
5. Tests:
   - pool exhaustion
   - leak detection
   - release-on-drop
   - no-copy through immediate chain
   - copy only when policy says copy

### Acceptance

- TopoExec can be credibly used for large in-process data without accidental copies.
- It does not claim external SHM zero-copy.
- Memory use is observable and bounded.

---

## G40. Graph Compiler v2: Typed Ports and Constraints

Priority: P1

### Objective

从 string endpoint validation 走向 typed port contract，减少错误连接。

### Tasks

1. Extend `ComponentDescriptor`:
   - input ports
   - output ports
   - payload type/schema
   - multiplicity
   - required/optional
2. Validate:
   - endpoint exists
   - payload type compatible
   - boundary role compatible
   - trigger input exists
   - state writer target type compatible
3. Add diagnostics:
   - `payload_type_mismatch`
   - `missing_required_input`
   - `optional_input_unconnected`
   - `boundary_role_mismatch`
4. C++ builder:
   - typed helpers optional
5. Schema:
   - decide additive `ports` fields vs descriptor-only validation
6. Tests:
   - mismatch rejected
   - optional input allowed
   - multi-output descriptor validated
   - boundary role validated

### Acceptance

- Invalid graph connections fail before runtime.
- Type metadata remains lightweight and optional enough for existing examples.
- Existing schema v1 examples still work.

---

## G41. Hierarchical Graph / Subgraph Design

Priority: P2

### Objective

支持复杂应用的层次化组织，但不要过早引入复杂 runtime nesting。

### Tasks

1. Design doc:
   - subgraph as namespace
   - subgraph as CompositeComponent
   - subgraph as compile-time macro
   - subgraph as runtime region
2. Choose phase 1:
   - namespace + compile-time expansion
3. Schema proposal:
   - `subgraphs`
   - `components[].graph_ref`
   - endpoint naming rules
4. Validation:
   - no hidden immediate cycles after expansion
   - CompositeLoop ownership after expansion
5. Tooling:
   - render collapsed/expanded Mermaid
   - plan JSON includes hierarchy
6. Tests:
   - simple subgraph expands
   - namespaced ports
   - cycle detection across subgraph boundary
   - metrics preserve original component path

### Acceptance

- Larger apps can be organized without losing semantic validation.
- Hierarchy does not hide feedback loops.

---

## G42. Graph Templates and Reusable Patterns

Priority: P2/P3

### Objective

为常见 patterns 提供可复用 graph snippets，而不是复制 YAML。

### Tasks

1. Define template design:
   - no arbitrary code
   - parameter substitution only
   - no hidden edges
2. Candidate templates:
   - source-transform-sink
   - latest low-latency pipeline
   - request-worker-response
   - delay feedback controller
   - bounded async task
3. CLI:
   - maybe `topoexec template list/render` only if needed
4. Tests:
   - template expansion deterministic
   - invalid parameter fails
   - expanded graph passes normal validation

### Acceptance

- Templates help examples and app users.
- Runtime remains unaware of templates after compile.

---

## G43. Component Lifecycle v2: Reset, Snapshot, Restore

Priority: P1/P2

### Objective

支持真实应用中组件重置、状态快照和恢复，不只是 configure/activate/deactivate。

### Tasks

1. Define lifecycle hooks:
   - reset
   - pause/resume
   - snapshot_state
   - restore_state
2. Decide stable vs experimental APIs.
3. Runtime:
   - request reset at epoch boundary
   - no reset during component execution
4. State:
   - component state payload
   - snapshot version
   - restore validation
5. Metrics/trace:
   - lifecycle transition counts
   - reset failure
   - snapshot size
6. Tests:
   - reset clears state
   - snapshot/restore around epoch boundary
   - restore incompatible version rejects
   - failure path cleans up

### Acceptance

- Components can be long-lived stateful modules with controlled reset.
- Snapshot/restore does not violate transaction semantics.

---

## G44. Config Hot Reload Transaction

Priority: P1/P2

### Objective

让 graph-level config 和 component config 支持安全热更新。

### Tasks

1. Define config transaction:
   - validate
   - stage
   - apply at epoch boundary
   - rollback on failure
2. Component hook:
   - `validate_config`
   - `apply_config`
3. State/config store:
   - version id
   - timestamp
   - applied components
4. Failure:
   - partial apply rollback or fail-fast
   - old config remains active
5. Tests:
   - valid config applies next epoch
   - invalid config rejected
   - component apply failure rolls back
   - trigger sees consistent config snapshot
6. Docs:
   - config lifecycle
   - no mid-execution config mutation

### Acceptance

- Config changes are transactional and observable.
- No component observes half-applied config.

---

## G45. CompositeLoop v2: Solver-Style Policies

Priority: P2

### Objective

将 CompositeLoop 从 fixed-point MVP 推进为可用于优化/迭代算法的 region runtime。

### Tasks

1. Define typed convergence API:
   - callback returns converged/not converged
   - optional residual metric
2. Policies:
   - fixed_point
   - transaction
   - solver_iteration
   - coalesced_event
3. Loop-local state:
   - iteration index
   - residual
   - convergence reason
4. Budget:
   - max iterations
   - wall time
   - cooperative cancellation
5. External output:
   - staged until success
   - policy for partial success
6. Tests:
   - converges by residual
   - hits max iterations
   - fails component inside loop
   - budget overrun
   - cancellation between iterations

### Acceptance

- CompositeLoop can represent real iterative components without exposing unsafe cycles.
- External observers never see half-updated loop output.

---

## G46. Runtime Observer API v1

Priority: P0/P1

### Objective

在不引入 OTel/Prometheus/Perfetto 依赖的情况下，建立稳定 observer contract。

### Tasks

1. Define interfaces:
   - `RuntimeObserver`
   - `ResultSink`
   - `MetricSink`
   - `TraceSink`
   - `HealthEventSink`
2. Requirements:
   - observer cannot block runtime indefinitely
   - observer failure does not change runtime semantics
   - bounded buffering
3. Add observer registration to RuntimeRunner options.
4. Provide default:
   - no-op observer
   - in-memory observer
5. Tests:
   - observer receives metric/trace/error
   - observer failure is recorded but not fatal by default
   - bounded observer drops and reports
6. Docs:
   - adapters consume observer API
   - no adapter SDK in core

### Acceptance

- Adapter work can begin without modifying runtime internals.
- Observer is stable enough for exporter preview.

---

## G47. Metrics v2: Cardinality and Schema Contract

Priority: P1

### Objective

让 metrics 能服务真实应用和 future exporters，避免 label explosion。

### Tasks

1. Define metric schema:
   - name
   - kind
   - unit
   - labels
   - cardinality rules
   - stability level
2. Add metric descriptor registry.
3. Validate exported metrics:
   - no unbounded labels by default
   - component/edge ids allowed
   - correlation id not default label
4. Histogram policy:
   - p50/p95/p99
   - count/min/max/avg
   - reset/window semantics
5. Tests:
   - descriptor registry stable
   - metrics JSON golden includes descriptor version
   - no duplicate names
6. Docs:
   - `docs/metrics.md` with table of stable metrics

### Acceptance

- OTel/Prometheus adapters can map metrics safely.
- Metric names are not changed casually.

---

## G48. Trace v2: Timeline and Causality

Priority: P1

### Objective

让 trace 从事件列表升级为可调试 timeline。

### Tasks

1. Trace schema:
   - event name
   - phase
   - timestamp
   - duration
   - component
   - edge
   - lane
   - worker id
   - epoch/transaction
   - correlation/causation
2. Chrome trace:
   - lane tracks
   - component spans
   - channel publish/commit events
   - loop iterations
3. JSON trace:
   - stable schema
   - descriptor version
4. Tests:
   - trace ordering
   - legal durations
   - correlation fields
   - Chrome trace shape
5. Optional CLI:
   - `trace --component`
   - `trace --lane`
   - only if easy; not priority

### Acceptance

- Trace can explain latency path through graph.
- External Perfetto adapter can be built later without changing runtime events.

---

## G49. Diagnostics v2: More Actionable Graph Errors

Priority: P1/P2

### Objective

让 graph diagnostics 不只是 reject，而能告诉用户如何修图。

### Tasks

1. Extend diagnostics:
   - error
   - warning
   - info
   - advisory
2. Add codes:
   - advisory_lane_field_ignored
   - payload_type_mismatch
   - backpressure_risk
   - large_payload_copy
   - high_queue_depth_latency_risk
   - trigger_never_ready
   - subgraph_hidden_cycle
3. Add suggested fix:
   - delay edge
   - CompositeLoop
   - reduce capacity
   - use latest
   - use shared_view
4. `explain` should group diagnostics:
   - graph structure
   - scheduler
   - channel
   - payload
   - trigger
5. Tests:
   - JSON diagnostics stable
   - warning does not fail validate unless strict mode
   - strict mode fails warnings if requested

### Acceptance

- Tooling becomes useful for users building large graphs.
- Diagnostics are stable machine-readable inputs for editor/LSP later.

---

## G50. Defensive Input Handling v2

Priority: P0/P1

### Objective

将 schema/parser limits 从 smoke 推进到 robust defensive behavior。

### Tasks

1. Define parser limits:
   - max components
   - max edges
   - max id length
   - max config depth
   - max string length
   - max file size
2. CLI options:
   - allow override?
   - default safe limits
3. Fuzz:
   - deterministic smoke remains
   - add coverage-guided fuzz target if feasible
4. Tests:
   - oversized file
   - deeply nested config
   - huge edge list
   - malicious strings
   - invalid UTF-8 if relevant
5. Docs:
   - TopoExec YAML is not an untrusted code execution engine
   - dynamic plugin loading future risks

### Acceptance

- Parser failure is safe, bounded, and diagnostic.
- Fuzz harness can run locally and in optional CI.

---

## G51. Coverage-Guided Fuzzing

Priority: P1/P2

### Objective

从 deterministic fuzz smoke 进入 coverage-guided fuzzing，提升 schema/compiler robustness。

### Tasks

1. Add fuzz targets:
   - YAML loader
   - schema checker
   - graph compiler
   - endpoint parser
   - trigger policy parser
2. Use libFuzzer or equivalent with CMake option:
   - `TOPOEXEC_BUILD_FUZZERS`
3. Seed corpus:
   - valid examples
   - invalid examples
   - minimized crash cases
4. CI:
   - short fuzz smoke
   - longer nightly optional
5. Docs:
   - how to run fuzz
   - how to add regression corpus
6. Tests:
   - fuzz target builds

### Acceptance

- Parser/compiler fuzzing is no longer only random deterministic smoke.
- Crashes can be captured as corpus regressions.

---

## G52. Stress and Soak Tests

Priority: P1/P2

### Objective

验证 scheduler/channel/task 在较长运行和高负载下不会出现 obvious deadlock/leak/unbounded growth。

### Tasks

1. Add stress graphs:
   - high fan-out
   - high fan-in
   - long chain
   - mixed immediate/delay/state/async
   - thread_pool overloaded
   - task executor overloaded
2. Add soak test mode:
   - bounded steps
   - configurable duration
   - not default CI slow path
3. Metrics assertions:
   - queue depth bounded
   - drop/reject counts expected
   - no unexpected error
4. TSAN:
   - run selected stress tests in non-blocking TSAN
5. Docs:
   - stress testing is confidence, not performance claim

### Acceptance

- Concurrency surfaces have workload tests beyond unit tests.
- Stress tests can be run by release candidate process.

---

## G53. Benchmark v2 and Regression Policy

Priority: P1/P2

### Objective

将 benchmark 从 output-shape smoke 推进到可用的 baseline tracking，但避免不可靠 CI 阈值。

### Tasks

1. Expand benchmark cases:
   - single component
   - immediate chain length N
   - fan-out/fan-in
   - latest vs queue
   - thread_pool width
   - CompositeLoop iterations
   - payload copy/shared/loaned
   - task executor
2. Add benchmark metadata:
   - compiler
   - build type
   - CPU info
   - commit
   - graph hash
3. Store optional local baseline:
   - not mandatory in CI
   - generated by command
4. Regression policy:
   - no global performance claim
   - only per-machine threshold if user opts in
5. Docs:
   - how to interpret
   - how not to overclaim

### Acceptance

- Benchmark suite supports real engineering decisions.
- CI checks correctness of bench outputs, not unstable timing thresholds.

---

## G54. Packaging v2

Priority: P1

### Objective

把 CMake package 从 smoke 可用推进到可被外部用户稳定消费。

### Tasks

1. Install/export audit:
   - runtime-only
   - yaml optional
   - cli optional
2. CPack source/binary package:
   - optional
3. Package manager drafts:
   - vcpkg port draft
   - Conan recipe draft
4. Version metadata:
   - semantic contract version
   - schema version
5. Downstream examples:
   - minimal runtime-only
   - yaml graph load
   - CLI package use
6. CI:
   - install package smoke
   - runtime-only build
   - no YAML/CLI build
   - no examples build

### Acceptance

- External CMake app can consume TopoExec without cloning source.
- package recipes can be reviewed and later published.

---

## G55. Documentation System v2

Priority: P1/P2

### Objective

把文档从“齐全”推进到“用户可学习、Agent 可执行、维护可持续”。

### Tasks

1. Reorganize docs:
   - Getting started
   - Concepts
   - Runtime semantics
   - API reference
   - Graph schema
   - Cookbook
   - Adapters
   - Testing/release
2. Add cookbook:
   - low-latency latest pipeline
   - bounded queue command stream
   - delay feedback control
   - CompositeLoop solver
   - async request/response
   - state/config snapshot
   - large payload ownership
3. Add architecture diagrams:
   - runtime flow
   - publication routing
   - scheduler lanes
   - channel lifecycle
4. Docs command smoke:
   - run commands embedded in docs
5. Add “Why not …” comparison:
   - oneTBB
   - Dora
   - GStreamer
   - ROS 2
   - workflow engines
6. Add “Design principles”:
   - bounded everything
   - explicit feedback
   - no hidden recursion
   - observation is not control

### Acceptance

- A new user can understand TopoExec without reading source.
- Agent can follow docs to implement goals without re-deriving architecture.

---

## G56. Example Applications v2

Priority: P1/P2

### Objective

从 toy examples 扩展为更接近真实应用的 reference apps，但仍不引入 external adapters。

### Candidate apps

1. `apps/low_latency_sensor_pipeline`
   - source
   - preprocessor
   - detector
   - tracker
   - latest/drop semantics

2. `apps/control_loop_with_state`
   - estimator
   - controller
   - actuator boundary
   - delay feedback
   - fixed-rate lane

3. `apps/async_request_response`
   - request boundary
   - validator
   - task executor
   - response boundary

4. `apps/composite_solver`
   - iterative loop
   - convergence
   - budget overrun

5. `apps/payload_pool_pipeline`
   - BufferPool
   - loaned/shared payload
   - copy vs no-copy metrics

6. `apps/hierarchical_graph_preview`
   - if G41 implemented

### Acceptance

- Each app has README:
  - graph shape
  - semantic lesson
  - expected output
  - contrast case
  - commands
- Apps run in CTest or optional example smoke.
- No adapter dependencies.

---

## G57. Adapter SDK v0

Priority: P1/P2

### Objective

在不实现具体 adapter 的情况下，先稳定 adapter SDK 边界。

### Tasks

1. Create `include/topoexec/adapters/`?  
   Or keep preview in runtime headers if minimal. Decision required.
2. Define:
   - `ResultSink`
   - `RuntimeObserver`
   - `BoundaryBridge`
   - `ComponentFactoryProvider`
3. Contracts:
   - bounded
   - non-blocking or best-effort
   - failure semantics
   - no scheduling side effects
4. Tests:
   - fake result sink
   - fake boundary bridge
   - observer failure
5. Docs:
   - adapter authors guide
   - no adapter SDK dependency in core target unless intentionally designed
6. Policy smoke:
   - prevent ROS/OTel/Prometheus/Python includes in core

### Acceptance

- Future adapters can be built without reaching into internals.
- No real adapter implementation needed yet.

---

## G58. OpenTelemetry Exporter Preview

Priority: P2/P3

### Objective

实现第一个 optional exporter preview，验证 observer API，但不让 core 依赖 OTel。

### Tasks

1. Separate optional target:
   - `topoexec_adapters::otel`
2. Build option:
   - `TOPOEXEC_BUILD_OTEL_ADAPTER`
3. Map:
   - metrics descriptors -> OTel metrics
   - trace events -> OTel spans
   - errors -> logs/events
4. Tests:
   - fake/no-op OTel backend or compile-only if SDK unavailable
   - no core transitive dependency
5. Docs:
   - preview status
   - metric cardinality caution

### Acceptance

- Adapter boundary proven with one external exporter.
- Core remains dependency-free.

---

## G59. Prometheus Exporter Preview

Priority: P2/P3

### Objective

通过 scrape/exporter 证明 metrics schema 可被外部系统消费。

### Tasks

1. Optional target:
   - no core dependency
2. Mapping:
   - counters
   - gauges
   - histograms
3. Avoid:
   - core starting HTTP server
   - unbounded labels
4. Tests:
   - text exposition shape
   - no high-cardinality default labels
5. Docs:
   - preview only

### Acceptance

- Metrics v2 schema is validated by another adapter style.
- No core HTTP server.

---

## G60. ROS 2 Adapter Preview

Priority: P2/P3

### Objective

验证 TopoExec 在 ROS 2 系统中作为 in-process semantic runtime 的 adapter，但不让 core 变成 ROS package。

### Scope

- Separate package/target only.
- Adapter boundary first.
- Fake boundary tests before real ROS integration.
- No ROS-specific fields in core schema v1.

### Tasks

1. Adapter design finalization:
   - topics -> boundary components
   - services -> request/response trigger
   - actions -> future/task-ready pattern
   - QoS remains adapter config, not core EdgePolicy
   - ROS executor/threading interaction
   - lifecycle mapping
2. Fake ROS boundary tests:
   - input message to boundary component
   - output payload to publisher bridge
   - QoS config external to core schema
3. Optional real ROS smoke later:
   - rclcpp package
   - colcon build
   - minimal topic example
4. Docs:
   - ROS adapter is optional
   - core runtime remains standalone

### Acceptance

- TopoExec remains useful outside ROS.
- ROS adapter demonstrates boundary mapping without corrupting core semantics.

---

## G61. C API / FFI Design

Priority: P2/P3

### Objective

规划 C API，为 Python/Rust/C plugins 或 external embedding 提供未来路径，但不急于冻结 ABI。

### Tasks

1. Design doc:
   - opaque handles
   - lifecycle
   - error strings
   - payload handles
   - graph builder minimal API
   - result/metrics iteration
2. Decide:
   - C API target optional
   - ABI versioning
   - ownership rules
3. Prototype:
   - minimal create/run/destroy
   - no dynamic plugins yet
4. Tests:
   - C downstream compile smoke
5. Docs:
   - unstable preview

### Acceptance

- FFI path is understood before Python binding.
- No accidental ABI freeze.

---

## G62. Python Binding Preview for Config/Test

Priority: P3

### Objective

提供 Python 用于配置、测试、CLI-like automation，而不是高性能 payload path。

### Tasks

1. Decide binding tool:
   - pybind11 optional
   - C API based future
2. Initial scope:
   - load graph
   - validate/plan
   - run deterministic steps
   - read metrics/trace
3. Non-goals:
   - high-throughput payload
   - zero-copy image/point cloud path
4. Tests:
   - Python smoke
   - no core dependency when disabled
5. Docs:
   - preview and limitations

### Acceptance

- Python helps testing and automation.
- It does not become required runtime dependency.

---

## G63. Dynamic Plugin Loading Preview

Priority: P2/P3

### Objective

让应用可以动态注册 components，但在安全/ABI/版本策略清楚前不默认启用。

### Tasks

1. Design:
   - explicit plugin manifest
   - component factory export function
   - version negotiation
   - schema compatibility
   - error reporting
2. Security docs:
   - plugins are trusted native code
   - no sandbox claim
3. Prototype:
   - optional target
   - one sample plugin
4. Tests:
   - load plugin
   - descriptor mismatch
   - version mismatch
   - unload semantics if supported
5. Package:
   - not installed by default until stable

### Acceptance

- Dynamic loading is optional and explicit.
- Core explicit registry path remains primary stable path.

---

## G64. Schema v2 Exploration

Priority: P2

### Objective

判断哪些新增能力需要 schema v2，而不是继续往 strict schema v1 塞字段。

### Candidate schema v2 features

- hierarchical subgraphs
- typed ports
- semantic contract version
- richer scheduler lanes
- condition/watermark triggers
- health event sink config
- adapter boundary descriptors
- config hot reload policy
- plugin/component package refs

### Tasks

1. Add `docs/schema-v2-notes.md`.
2. List breaking vs additive changes.
3. Migration plan:
   - v1 loader remains
   - v2 loader added
   - `topoexec schema migrate` maybe future
4. Tests:
   - v1 examples still pass
   - v2 fixtures experimental
5. Do not implement full v2 until design reviewed.

### Acceptance

- Future schema changes are deliberate.
- No accidental v1 semantic drift.

---

## G65. Editor / LSP / JSON Schema UX

Priority: P3

### Objective

提升 graph authoring 体验，但保持 runtime 优先。

### Tasks

1. Ensure JSON Schema is installable and discoverable.
2. Add VS Code docs:
   - associate YAML with schema
3. Add diagnostics output fit for editor:
   - code
   - path
   - suggested_fix
4. Optional:
   - minimal LSP design doc
   - not implementation yet
5. Tests:
   - schema dump stable
   - schema check path diagnostics

### Acceptance

- Users can get editor completion/validation with existing schema.
- No runtime dependency added.

---

## G66. Architecture Enforcement CI v2

Priority: P0/P1

### Objective

让架构边界被 CI 自动守住，而不是只靠文档。

### Tasks

1. Expand policy checks:
   - core must not include YAML parser headers
   - runtime must not include CLI headers
   - core/runtime must not include adapter SDK tokens
   - tools must not implement new semantics bypassing runtime
2. Include path audit:
   - installed headers
   - internal headers
3. CMake dependency graph smoke.
4. Tests:
   - policy check catches planted fake dependency
   - existing code passes
5. Docs:
   - architecture guardrails updated

### Acceptance

- Future Codex changes cannot accidentally collapse module boundaries.

---

## G67. Release Automation and Artifact Reproducibility

Priority: P1

### Objective

让 prerelease 发布流程可靠、可重复、可被 Agent 执行。

### Tasks

1. Add release script:
   - checks clean tree
   - runs required gates
   - generates release notes draft
   - validates changelog section
2. Artifact generation:
   - source tarball
   - optional binary package
   - schema artifact
3. Tag policy:
   - annotated tags only
   - no retag
4. CI release workflow:
   - dry-run mode
   - tag mode later
5. Docs:
   - release runbook
   - rollback/fix-forward policy

### Acceptance

- A future Codex goal can prepare a release candidate without guessing.
- Human still approves final tag/push.

---

## G68. Community and Contribution Readiness

Priority: P2

### Objective

让开源用户和贡献者可以参与，而不需要你解释所有上下文。

### Tasks

1. Update:
   - `CONTRIBUTING.md`
   - issue templates
   - PR template
   - design proposal template
2. Add:
   - “how to propose a semantic change”
   - “how to add a component/example”
   - “how to add a metric”
   - “how to add a schema field”
3. Governance:
   - release cadence
   - API change review
   - adapter acceptance policy
4. Docs:
   - project philosophy
   - non-goals

### Acceptance

- Contributors know how to avoid unsafe runtime changes.
- Agent-generated PRs have the same structure as human PRs.

---

## G69. Real-World Pilot App

Priority: P1/P2

### Objective

选择一个真实但无外部依赖的 pilot app，证明 TopoExec 不只是 demo runtime。

### Candidate pilots

1. In-process robotics-like control loop without ROS.
2. Video-like frame pipeline without actual camera dependency.
3. Service-like request/response pipeline.
4. Simulation-like state update system.
5. Game/AI behavior-dataflow hybrid.

### Tasks

1. Choose one pilot.
2. Implement with:
   - multiple lanes
   - delay/state/async edges
   - payload pool
   - metrics/trace
   - config snapshot
   - error path
3. Add README:
   - architecture
   - graph
   - why TopoExec helps
   - expected metrics
   - failure/overload scenario
4. Tests:
   - pilot app smoke
   - golden output maybe
5. Docs:
   - case study

### Acceptance

- Pilot demonstrates unique value:
  - explicit feedback
  - bounded overload
  - observability
  - C++ embedding
- No adapter dependency required.

---

## G70. Beta Readiness Review

Priority: P0 before beta

### Objective

在进入 beta 前做一次系统审查。

### Checklist

1. API:
   - public API matrix updated
   - deprecation policy
   - no accidental unstable headers installed
2. Runtime:
   - invariants updated
   - scheduler/thread/task/channel semantics documented
   - all known limitations honest
3. Tests:
   - unit
   - semantic
   - golden
   - docs
   - package
   - sanitizer
   - fuzz
   - stress optional
4. Observability:
   - metric schema
   - trace schema
   - diagnostics registry
5. Docs:
   - getting started
   - cookbook
   - API overview
   - release notes
6. Adapters:
   - preview only unless tested
   - no core pollution
7. Packaging:
   - install smoke
   - package manager draft
   - release artifact smoke
8. Performance:
   - benchmark output stable
   - no overclaims
9. Security/defensive:
   - parser limits
   - no dynamic plugin default
10. Goal ledger:
   - all P0/P1 complete or explicitly deferred

### Acceptance

- Project can honestly publish a beta candidate.
- Deferred features are explicit, not hidden.

---

# 5. Recommended Execution Order

## Phase A：Post-G25 保护与版本化

1. G26 Release Candidate Baseline 2
2. G27 Public API Stability Pass v2
3. G28 Runtime Semantic Version Contract
4. G66 Architecture Enforcement CI v2

## Phase B：Runtime 并发与调度成熟

5. G29 Scheduler v2 Design + Contract
6. G30 Persistent Worker Pool v1
7. G31 Fixed-Rate Lane v1
8. G32 Scheduler Priority and Admission Policy v1
9. G33 Cooperative Cancellation and Timeout Semantics
10. G34 TaskExecutor v2

## Phase C：数据、信道、触发、循环完备化

11. G36 Correlation/Causality Metadata
12. G37 Explicit Backpressure Events
13. G38 Multi-Reader / Move-Only Hardening
14. G39 Payload and Memory v2
15. G40 Typed Ports and Constraints
16. G45 CompositeLoop Solver-Style Policies
17. G35 Trigger Engine v2

## Phase D：可观测性与工具产品化

18. G46 Runtime Observer API v1
19. G47 Metrics v2
20. G48 Trace v2
21. G49 Diagnostics v2
22. G53 Benchmark v2

## Phase E：质量、安全、发布

23. G50 Defensive Input Handling v2
24. G51 Coverage-Guided Fuzzing
25. G52 Stress and Soak Tests
26. G54 Packaging v2
27. G67 Release Automation

## Phase F：扩展架构和生态

28. G41 Hierarchical Graph
29. G42 Graph Templates
30. G43 Component Lifecycle v2
31. G44 Config Hot Reload Transaction
32. G55 Documentation System v2
33. G56 Example Applications v2
34. G69 Real-World Pilot App

## Phase G：Adapter 与未来接口

35. G57 Adapter SDK v0
36. G58 OpenTelemetry Exporter Preview
37. G59 Prometheus Exporter Preview
38. G60 ROS 2 Adapter Preview
39. G61 C API / FFI Design
40. G62 Python Binding Preview
41. G63 Dynamic Plugin Loading Preview
42. G64 Schema v2 Exploration
43. G65 Editor / LSP UX

## Phase H：Beta

44. G70 Beta Readiness Review

---

# 6. High-Value First 10 Goals

如果只想快速提升架构完整度，优先执行：

```text
G26  Release Candidate Baseline 2
G27  Public API Stability Pass v2
G28  Runtime Semantic Version Contract
G29  Scheduler v2 Design + Contract
G30  Persistent Worker Pool v1
G33  Cooperative Cancellation and Timeout Semantics
G36  Correlation/Causality Metadata
G37  Explicit Backpressure Events
G46  Runtime Observer API v1
G47  Metrics v2
```

这 10 个 goal 会把项目从“实现了很多功能”推进到“架构边界、执行语义、并发行为、可观测接口都可持续演进”。

---

# 7. Codex Goal Prompt 示例

```md
You are working on sean2077/topoexec.

Read:
- AGENTS.md
- docs/goals/status.md
- docs/goals/backlog.md
- docs/plans/plan.md
- docs/runtime-semantics.md
- docs/versioning.md

Execute only:
G29 Scheduler v2 Design + Contract

Context:
- G0-G25 are complete.
- The new plan starts at G26.
- Do not add adapters.
- Do not add broad CLI surface.
- Runtime/API/concurrency stability is the priority.

Allowed files:
- docs/scheduler.md
- docs/concurrency.md
- docs/runtime-semantics.md
- docs/versioning.md
- include/topoexec/runtime/*
- src/scheduler.cpp
- src/runtime_runner.cpp
- tests/test_runtime.cpp
- tests/test_graph.cpp
- CHANGELOG.md
- docs/goals/status.md

Do not modify:
- adapter docs except to preserve boundaries
- schema v1 meaning unless explicitly required
- packaging files unless a test needs a small update

Objective:
- Clarify and enforce scheduler v2 contract.
- Distinguish implemented behavior, advisory fields, and future extension.
- Add diagnostics or plan output where fields are currently accepted but not enforced.

Acceptance:
- Docs explain event_loop/fixed_rate/thread_pool behavior.
- Advisory lane fields are not silently represented as implemented.
- Tests cover at least one advisory/unsupported field diagnostic.
- ./scripts/agent_check.sh passes.

Failure:
- If a product/API decision is needed, write docs/goals/blockers/G29.md with options and recommendation.
```

---

# 8. Review Policy for Large Goals

For goals touching runtime/concurrency:

- Require at least one focused unit test.
- Require at least one semantic/invariant or golden check if output changes.
- Require docs update.
- Run sanitizer if possible.
- Update `docs/runtime-invariants.md` if a core invariant changes.
- Update `docs/goals/status.md`.

For goals touching public API:

- Update `docs/public-api.md`.
- Update `docs/versioning.md`.
- Add downstream smoke if needed.
- Update changelog.

For goals touching schema:

- Update `docs/schema-v1.md` or create `schema-v2-notes.md`.
- Update JSON schema if applicable.
- Add schema contract smoke.
- Preserve v1 compatibility unless explicitly making schema v2.

For goals touching adapters:

- Keep adapter target optional.
- No adapter SDK dependencies in core.
- Update policy smoke.
- Fake-boundary-first tests before real external dependency.

---

# 9. Design Principles to Preserve

1. **No hidden recursion**  
   `publish()` stages/routs; scheduler owns execution.

2. **No unbounded backlog**  
   Channels, lane queues, task queues, observer queues must be bounded.

3. **No silent semantic fallback**  
   Unsupported policies must be rejected, warned, or clearly advisory.

4. **No adapter contamination**  
   Core remains adapter-free.

5. **Metrics and trace are observations, not control flow**  
   They must not affect readiness or scheduler decisions.

6. **Graph compiler rejects unsafe structure before runtime**  
   Immediate cycles, invalid state writers, incompatible copy policy, trigger mismatches should fail early.

7. **Schema v1 meaning must not drift**  
   Additive fields are okay only if v1 semantics remain compatible.

8. **Threading behavior must be honest**  
   Do not claim RT, priority, affinity, or preemption before implementation and tests.

9. **Examples should teach semantics**  
   Every example should show a useful contrast, not just run.

10. **Codex goals must leave evidence**  
    Tests, docs, status ledger, and validation output are required.

---

# 10. Suggested File Placement

Place this file in the repository as:

```text
docs/plans/plan.md
```

or, if preserving the completed G0–G25 plan:

```text
docs/plans/post_g25_plan.md
```

Then update:

```text
docs/agent-goals.md
docs/goals/backlog.md
docs/goals/status.md
AGENTS.md
```

to point to the new plan and start with G26.
