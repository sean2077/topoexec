# TopoExec 后 MVP 发展与完善计划（Codex Goal Spec）

> 目标：把 TopoExec 从当前 MVP / alpha 实现推进为一个可嵌入、可验证、可扩展、可观测的 C++20 进程内语义执行图 runtime。
>
> 使用方式：本文件可直接作为 Codex CLI / Codex Goal / oh-my-codex 的长期迭代 spec。每个 `Gx` 都可以拆成一个或多个 Codex goal；每个 goal 必须保留验收标准、测试命令和变更边界。

---

## 0. 当前仓库状态快照

日期：2026-05-05。

当前公开仓库已经不再只是初始 MVP。根据 README / docs / CHANGELOG 的公开内容，当前状态大致是：

- 项目定位已经明确为 **C++20 单进程 stateful in-process execution graph runtime**。
- 已经有核心概念：`Component`、`GraphSpec`、`immediate/delay/state/async` edge kind、bounded channel policy、trigger policy、CompositeLoop。
- 已经有 `topoexec::runtime` 和 `topoexec::yaml` 的可嵌入 API 边界。
- 已经有 CLI：`validate / plan / render / run / metrics / trace / lint / explain / diff-plan / bench`。
- 已经有 runnable examples：minimal pipeline、latest-vs-queue、delay feedback、CompositeLoop fixed point、async worker、pure C++ graph builder。
- 已经有 runtime semantics 文档，定义 epoch、transaction、commit、edge visibility、trigger readiness、CompositeLoop ownership。
- 已经有 payload ownership 文档，包含 `TextPayload`、`BinaryBlobPayload`、`FrameView`、`RuntimePayloadPtr`、`copy/shared_view/loaned_view/move_only`。
- 已经有 worker lane MVP 与 async `max_inflight` admission 的 Unreleased 进展。
- 已经有 metrics / trace 文档和 Chrome Trace 兼容导出。
- 已经有 release checklist，当前目标为 `v0.1.0-alpha` 之后继续完善。
- 已明确 deferred：ROS 2、OpenTelemetry、Prometheus、Python、外部 Perfetto adapter。

因此，本计划不再以“补齐 MVP”为核心，而是以 **架构完备化、语义强化、并发成熟、API 稳定、生态扩展** 为核心。

---

## 1. 总体北极星

### 1.1 项目定位

TopoExec 应定位为：

> **A lightweight C++20 semantic execution graph runtime for composing stateful components inside a single process, with explicit contracts for edge visibility, feedback, bounded channels, triggers, scheduling, payload ownership, metrics, and traceability.**

中文：

> **TopoExec 是一个轻量 C++20 进程内语义执行图 runtime，用于编排有状态组件，并显式管理边可见性、反馈环、有界信道、触发策略、调度、payload 所有权、metrics 与 trace。**

### 1.2 长期价值

TopoExec 不应只成为“能跑 DAG 的 executor”，而应成为：

1. **语义图编译器**：在运行前验证 edge kind、trigger、CompositeLoop、channel policy、payload policy、schema compatibility。
2. **进程内 runtime**：稳定执行 graph region、transaction、commit、trigger、scheduler lane、bounded channel。
3. **反馈安全 runtime**：所有环必须通过 `delay/state/async` 打断，或显式声明为 CompositeLoop。
4. **过载显式 runtime**：所有 channel 都有 capacity、overflow、drop/reject/overwrite/block 语义和指标。
5. **可嵌入 C++ 库**：纯 C++ app 可只依赖 `topoexec::runtime`，不被 YAML/CLI/adapter 污染。
6. **可观测 runtime**：metrics、trace、timeline、debug snapshot、plan diff、bench 形成内建诊断能力。
7. **adapter-friendly core**：ROS 2、OpenTelemetry、Prometheus、Python、Perfetto 只消费稳定 API，不污染 core schema。

### 1.3 非目标

短中期不要做这些，除非 core 已稳定：

- 不做分布式 runtime。
- 不做 GUI/低代码编辑器。
- 不做 ROS 2 替代品。
- 不做 Python 高性能数据通路。
- 不做完整 workflow engine。
- 不做 OpenTelemetry/Prometheus/Perfetto SDK 的硬依赖。
- 不承诺硬实时；可支持实时友好的 API、调度约束和测试，但不宣称 hard real-time。

---

## 2. Codex Goal 使用规则

### 2.1 每个 goal 的固定模板

Codex goal 应使用这个模板：

```md
Goal ID:
Title:

Context:
- Read PLAN.md.
- Current priority:
- Relevant docs:
- Relevant headers:
- Relevant tests:

Objective:
- ...

Scope:
- Allowed files:
- Do not modify:

Implementation requirements:
- ...

Acceptance criteria:
- ...

Validation:
Run:
  ./scripts/agent_check.sh
Additional optional checks:
  ...

Failure protocol:
- If blocked after one focused fix, write docs/agent-blockers/<goal-id>.md.
- Do not rewrite unrelated architecture.
- Do not ask routine questions; make reversible assumptions and record them.

Deliverables:
- Code
- Tests
- Docs
- CHANGELOG note if public behavior changes
- PR summary
```

### 2.2 Agent 全局约束

每次 Codex goal 都必须遵守：

- **小步变更**：一个 PR 只完成一个明确目标。
- **测试优先**：行为变更必须先有失败测试或新增 golden test。
- **不 silent fallback**：runtime 不应在不支持的语义上默默降级。
- **不重写大模块**：除非 goal 明确允许，不得重写 scheduler/runtime/channel 整体架构。
- **不新增重依赖**：adapter 以外不得新增 heavyweight production dependency。
- **不改变 schema v1 意义**：破坏性语义变化必须走 schema version bump 或明确 migration。
- **不合并 main**：Agent 可创建分支和 commit，不应自动 merge main。
- **所有 public API 改动必须更新**：`docs/public-api.md`、`docs/versioning.md`、`CHANGELOG.md`。

### 2.3 默认验收命令

基础验收：

```bash
./scripts/agent_check.sh
```

若 goal 涉及 build/package：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
cmake --install build --prefix /tmp/topoexec-install
cmake -S tests/cmake/runtime_smoke -B /tmp/topoexec-runtime-smoke \
  -DCMAKE_PREFIX_PATH=/tmp/topoexec-install
cmake --build /tmp/topoexec-runtime-smoke -j
/tmp/topoexec-runtime-smoke/topoexec_runtime_smoke
```

若涉及并发：

```bash
# 如果 CI/本机支持
ctest --test-dir build --output-on-failure -R "runtime|scheduler|concurrency|async"
# 运行 non-blocking TSAN job 或本地等价配置
```

若涉及 CLI JSON：

```bash
./build/topoexec graph metrics examples/minimal.yaml --steps 1 --format json
./build/topoexec graph trace examples/minimal.yaml --steps 1 --format json
./build/topoexec graph trace examples/minimal.yaml --steps 1 --format chrome > /tmp/topoexec-trace.json
```

---

## 3. 版本路线图

### v0.1.x：Alpha Stabilization

目标：保住当前语义，强化测试、文档、API 边界。

重点：

- runtime semantics 与实现完全一致。
- thread_pool MVP 与 async max_inflight 不再只是概念，行为可测。
- public headers 可作为 alpha 用户入口。
- package smoke 和 CI 稳定。
- examples 都可运行且输出文档匹配。

### v0.2.x：Runtime Completeness

目标：形成完整可用的 core runtime。

重点：

- scheduler lane v1。
- trigger engine v1。
- channel/backpressure v1。
- payload/buffer pool v1。
- CompositeLoop/region v1。
- metrics/trace v1。
- benchmark suite v1。

### v0.3.x：Adapter-Ready Core

目标：让 adapter 可以安全接入。

重点：

- stable observer/exporter API。
- graph boundary API。
- C API 或 minimal FFI plan。
- adapter plugin layout。
- ROS 2 / OTel / Prometheus / Python 只做 preview，不污染 core。

### v0.5.x：Production Beta

目标：用户可在真实 app 中试用。

重点：

- sanitizer / fuzz / property tests。
- documented failure modes。
- performance baselines。
- mature docs/tutorials。
- release artifacts。
- API deprecation policy。

### v1.0：Stable Semantic Runtime

目标：核心语义和 API 稳定。

重点：

- schema compatibility policy 固化。
- public C++ API 明确。
- runtime behavior stable。
- adapter boundary stable。
- benchmarks and observability stable enough for comparison.

---

# 4. Goal Board

下面每个 `Gx` 都可作为 Codex Goal 独立执行。优先级用 P0/P1/P2 标识。

---

## G0. Baseline Lock 与现状审计

Priority: P0

### Objective

建立当前 main 分支的可复现 baseline，避免后续大规模 goal 导致行为漂移。

### Tasks

1. 新增或更新 `docs/current-baseline.md`：
   - commit SHA；
   - 版本；
   - CTest 数量；
   - examples 列表；
   - CLI 命令列表；
   - 已知限制；
   - CI 状态；
   - public API headers 列表。
2. 新增 `tests/golden/` 或等价目录：
   - CLI `metrics` JSON golden；
   - CLI `trace` JSON golden；
   - CLI `plan` JSON golden；
   - CLI `render` Mermaid smoke golden。
3. 给每个 golden 添加稳定字段过滤规则：
   - 时间、duration、trace id 可以 normalize；
   - semantic fields 必须稳定。
4. 更新 `scripts/agent_check.sh`：
   - 确保 golden tests 被跑到；
   - 保持本地执行时间合理。

### Acceptance

- `docs/current-baseline.md` 存在且内容与当前 main 一致。
- 每个 CLI output 至少有一个 golden test。
- 任何后续语义漂移会造成测试失败，而不是只靠人工发现。
- `./scripts/agent_check.sh` 通过。

---

## G1. Public API Matrix 与 Internal API 边界

Priority: P0

### Objective

把“哪些 API 可依赖、哪些实验性、哪些内部实现”从文档承诺转化为目录/target/安装规则约束。

### Tasks

1. 为 public headers 建立 matrix：
   - header path；
   - target；
   - stability level；
   - allowed dependencies；
   - public types；
   - planned deprecation policy。
2. 检查 install rules：
   - 只安装 public headers；
   - internal headers 不安装；
   - `topoexec::runtime` 不依赖 YAML/CLI；
   - `topoexec::yaml` 可选。
3. 新增 CMake smoke：
   - downstream 只 link `topoexec::runtime`；
   - downstream 使用 graph builder；
   - downstream 使用 typed payload helper；
   - downstream 不需要 yaml-cpp/CLI11 include path。
4. 为 API change 添加 review checklist：
   - 新增 API 是否在 docs 中记录；
   - 是否影响 schema v1；
   - 是否需要 CHANGELOG；
   - 是否破坏 v0.1.x stable headers。

### Acceptance

- `docs/public-api.md` 与实际 installed headers 一致。
- Downstream runtime-only smoke test 不依赖 YAML/CLI。
- Internal headers 不被暴露为 public include surface。
- 如果 public API 改动，CI 能通过 package smoke 捕捉常见破坏。

---

## G2. Status / Error Propagation v1

Priority: P0

### Objective

将组件生命周期与执行错误从“可记录”提升为完整、可诊断、可测试的失败模型。

### Required Semantics

- `configure` failure：
  - 组件不 activate；
  - runtime stop reason 为 configure failure；
  - 已 configure 的组件按逆序 cleanup/deactivate；
  - result.errors 包含 component id、phase、message、optional code。
- `activate` failure：
  - 已 activate 组件 deactivate；
  - failed component 状态可观测；
  - no execution starts。
- `execute` failure：
  - 默认 stop runtime；
  - 可选 policy: continue / isolate / fail-fast；
  - metrics: component error count, runtime error count。
- `deactivate/cleanup` failure：
  - 不掩盖原始错误；
  - result.errors 可容纳多个 errors。
- Exception policy：
  - 默认 catch std::exception；
  - catch-all 可选；
  - exception message 进入 errors。

### Tasks

1. 定义 `RuntimeError` 结构：
   - phase；
   - component_id；
   - lane；
   - message；
   - optional code；
   - optional trace id；
   - fatal flag。
2. 为 component failure policy 增加 schema / C++ GraphSpec 字段：
   - `on_error: fail_fast | continue | isolate`；
   - 初期只实现 `fail_fast`，其他解析但 reject 或 warn。
3. 增加 tests：
   - configure failure；
   - activate failure；
   - execute Status::error；
   - execute throw；
   - deactivate failure after execute failure；
   - CompositeLoop internal failure；
   - worker lane failure。
4. 更新 docs：
   - `docs/public-api.md`；
   - `docs/runtime-semantics.md`；
   - `docs/metrics.md`；
   - `docs/trace-events.md`。

### Acceptance

- 所有 phase 的错误路径有测试。
- `RuntimeRunnerResult` 能完整表示多错误。
- 错误路径不会泄漏启动后的组件。
- 错误路径 metrics/trace 可见。
- 不引入 silent fallback。

---

## G3. Runtime Invariant Test Suite

Priority: P0

### Objective

把 TopoExec 最核心的语义转化为不可回归的 invariant tests。

### Invariants

1. `GraphContext::publish()` 绝不直接调用下游 component。
2. `immediate` edge 在 DAG 中可同 epoch 可见。
3. `delay` edge 只在 next epoch 可见。
4. `state` edge 读者在当前 epoch 看到旧 snapshot，next epoch 看到新 snapshot。
5. `async` edge 不参与 immediate SCC，且只作为 deferred event 可见。
6. 非声明 immediate SCC 必须 reject。
7. CompositeLoop 必须 exact-match immediate SCC。
8. CompositeLoop 内部输出对外 commit 发生在 loop 完成之后。
9. `latest overwrite` 不产生无界 backlog。
10. `queue capacity` 满时按 overflow policy 处理。
11. `move_only + multi-reader` invalid。
12. 多 state writers invalid，除非未来显式 merge policy 存在。
13. batch/time_sync/all_input trigger 消费顺序稳定。
14. stop token 在 scheduler iteration 之间生效。
15. worker lane 不破坏 compiled region boundary。
16. non-reentrant component 不会 overlap。
17. reentrant component 可 overlap，但不超过 lane max_threads。
18. metrics 与实际行为一致。
19. trace events 成对出现，duration 合法。
20. schema unknown fields 被 reject。

### Tasks

- 新增 `tests/test_runtime_invariants.cpp` 或拆分为多个文件。
- 为每个 invariant 写最小 graph builder 测试。
- 对 YAML 与 C++ builder 各抽样测试。
- 给每个 invariant 一句注释说明对应文档条款。

### Acceptance

- Invariant tests 成为 CI 必跑项。
- 任何 core semantics 修改都会至少影响一个 invariant test。
- 测试不依赖 flaky timing；并发测试可用 barriers/futures 控制。

---

## G4. Graph Compiler v1：Region、SCC、Plan、Explainable Diagnostics

Priority: P0/P1

### Objective

把 graph compiler 从“能生成 plan”提升为“可解释的语义编译器”。

### Tasks

1. 将 validator/compiler 结果结构化：
   - errors；
   - warnings；
   - compiled regions；
   - region order；
   - immediate SCCs；
   - CompositeLoop ownership；
   - edge visibility table；
   - trigger dependency table。
2. 定义 error code：
   - `unknown_component`;
   - `unknown_port`;
   - `duplicate_id`;
   - `immediate_cycle_without_loop`;
   - `partial_composite_loop`;
   - `decorative_composite_loop`;
   - `multi_state_writer`;
   - `invalid_move_only_multireader`;
   - `invalid_channel_policy`;
   - `unsupported_lane_type`;
   - `schema_unknown_field`;
   - `trigger_missing_input`;
   - `incompatible_trigger_edge_mode`。
3. 每个 error 有：
   - code；
   - message；
   - graph path；
   - involved components/edges；
   - suggested fix；
   - severity。
4. `topoexec graph explain` 使用同一 error model，不重复逻辑。
5. `topoexec graph plan --format json` 输出 semantic plan。
6. 增加 plan golden tests。

### Acceptance

- validator 不只是字符串错误。
- CLI 和 C++ API 使用同一 diagnostic model。
- 错误消息足够让 Codex 自动修复简单 graph。
- plan JSON 可作为 downstream tooling 输入。

---

## G5. Schema v1 完整参考与 JSON Schema

Priority: P1

### Objective

让外部用户和 Codex 可以严格生成合法 graph。

### Tasks

1. 扩展 `docs/schema-v1.md`：
   - 所有 root fields；
   - 所有 section fields；
   - 所有 enum；
   - 默认值；
   - required/optional；
   - validation rules；
   - examples；
   - invalid examples。
2. 新增机器可读 schema：
   - `schema/topoexec.schema.v1.json` 或等价；
   - YAML 通过 JSON Schema 验证；
   - unknown fields strict。
3. 增加 schema fixtures：
   - valid minimal；
   - valid delay feedback；
   - valid CompositeLoop；
   - valid async max_inflight；
   - invalid unknown field；
   - invalid enum；
   - invalid immediate cycle；
   - invalid trigger input。
4. CLI：
   - `topoexec graph validate --schema-only`；
   - `topoexec graph validate --semantic`。
5. 版本策略：
   - schema v1 additive policy；
   - v2 migration skeleton。

### Acceptance

- Codex 可仅凭 schema docs 生成合法 YAML。
- Unknown fields reject 行为有测试。
- JSON Schema 与 runtime validator 不冲突。
- Schema docs 与 examples 全部同步。

---

## G6. Scheduler Lane v1

Priority: P0/P1

### Objective

把 scheduler 从 event-loop + worker MVP 推进为可描述、可测试、可扩展的 lane system。

### Lane Types

1. `event_loop`
   - 单线程；
   - deterministic；
   - compiled region order；
   - no overlap。
2. `fixed_rate`
   - runtime tick；
   - optional wall-clock mode；
   - period；
   - jitter metrics；
   - overrun metrics；
   - no sleep in test mode。
3. `thread_pool`
   - bounded workers；
   - worker batch or persistent worker pool；
   - reentrant control；
   - admission policy；
   - queue depth metrics；
   - no downstream visibility before barrier。
4. Future:
   - `isolated_thread`;
   - `priority_lane`;
   - `io_pool`;
   - `manual_lane`.

### Tasks

1. 明确 `SchedulerLaneSpec`：
   - type；
   - max_threads；
   - queue_capacity；
   - overflow；
   - priority；
   - affinity；
   - wall_clock_enabled；
   - period；
   - tick_budget；
   - isolation。
2. 实现 `thread_pool` v1：
   - persistent workers or bounded batch；
   - deterministic barrier；
   - non-reentrant serialization；
   - reentrant overlap；
   - stop token drain；
   - metrics；
   - trace spans。
3. 实现 `fixed_rate` v1：
   - simulated tick in tests；
   - optional sleep mode；
   - overrun detection；
   - jitter metrics。
4. Admission model：
   - fail_fast；
   - drop_oldest；
   - reject_new；
   - block only when explicit。
5. Tests：
   - reentrant overlap；
   - non-reentrant serialization；
   - queue capacity；
   - stop while worker active；
   - failure in worker；
   - barrier visibility；
   - fixed-rate overrun。
6. Docs：
   - `docs/scheduler.md`；
   - `docs/concurrency.md`；
   - `docs/metrics.md`；
   - `docs/trace-events.md`。

### Acceptance

- thread_pool 不再只是 MVP 文档能力，而是有完整测试。
- fixed_rate 的 simulated mode 稳定，wall-clock mode 有清晰限制。
- scheduler lane 不破坏 runtime semantics。
- TSAN 相关测试不应 flaky。

---

## G7. Async Task Runtime v1

Priority: P1

### Objective

把 async edge 从“deferred completion admission”升级为可选 async task/future runtime，同时保留当前 async edge 语义。

### Design Split

- `async edge`：表示 completion/event delivery，不自动执行任务。
- `async task runtime`：可选设施，用于组件提交任务，并在完成后通过 async edge 发布 completion。

### Tasks

1. 新增 `TaskExecutor` abstraction：
   - submit；
   - cancel；
   - max_inflight；
   - queue_capacity；
   - completion callback；
   - error handling。
2. GraphSpec 支持 task source：
   - `event_source: task_ready`;
   - optional task pool name；
   - max inflight per component。
3. 保持 core 可选：
   - 不强制所有 async edge 使用 task executor。
4. Runtime API：
   - `GraphContext::submit_task(...)`；
   - completion publishes immutable payload；
   - errors become RuntimeError and metrics。
5. Metrics：
   - submitted；
   - active；
   - completed；
   - cancelled；
   - rejected；
   - failed；
   - max_inflight；
   - queue_depth。
6. Tests：
   - task completion triggers downstream；
   - max_inflight admission；
   - cancellation；
   - failure；
   - stop token；
   - deterministic simulated executor。
7. Examples：
   - async inference-like worker；
   - async IO-like worker；
   - bounded max_inflight demo。

### Acceptance

- 当前 async edge 行为不破坏。
- Async task runtime 可选、可测、可观测。
- 没有无界 task backlog。
- Stop/cancel 语义明确。

---

## G8. Channel / Backpressure v1

Priority: P0/P1

### Objective

把 channel policy 做成 TopoExec 核心竞争力。

### Channel Modes

- `latest`
- `queue`
- `ring_buffer`
- `latched`
- `previous_tick`
- `barrier`
- future: `time_window`, `priority_queue`, `sampled`

### Overflow Policies

- `overwrite`
- `drop_oldest`
- `drop_newest`
- `reject`
- `fail_fast`
- `block` only explicit
- future: `backpressure_signal`

### Tasks

1. 所有 channel 有 capacity，禁止默认无界。
2. 所有 overflow 都有 metrics。
3. 所有 channel 支持 `lifespan` 和 `deadline`：
   - stale detection；
   - deadline miss；
   - health event；
   - metrics。
4. Read semantics：
   - peek；
   - consume；
   - snapshot；
   - batch consume；
   - time_sync consume。
5. Backpressure event：
   - optional event to upstream/downstream；
   - not default recursive execution。
6. Multi-reader semantics：
   - `readers: single | multiple`;
   - move-only restrictions；
   - copy/shared behavior；
   - per-reader cursor for queue/ring if needed。
7. Tests：
   - latest overwrite；
   - queue drop；
   - barrier；
   - previous_tick；
   - latched late reader；
   - lifespan；
   - deadline；
   - multi-reader；
   - copy policy interaction。
8. Docs:
   - `docs/channels.md`；
   - update `docs/runtime-semantics.md`；
   - update `docs/metrics.md`。

### Acceptance

- 用户可以明确知道每条 edge 过载时发生什么。
- 所有 channel 行为都有 tests + metrics。
- Runtime 无 silent unbounded queue。
- Low-latency graphs 可配置 latest-only 不堆旧数据。

---

## G9. Payload / Memory / BufferPool v1

Priority: P1

### Objective

让 TopoExec 能支撑大 payload / 低拷贝场景，而不是只适合小字符串 demo。

### Tasks

1. 稳定 payload model：
   - `TextPayload`;
   - `BinaryBlobPayload`;
   - `FrameView`;
   - `RuntimePayload`;
   - `RuntimePayloadPtr`;
   - schema string；
   - typed helpers。
2. 增强 custom payload registration：
   - schema id；
   - type-erased immutable payload；
   - optional clone/copy function；
   - optional size estimator；
   - optional debug summary。
3. BufferPool：
   - fixed-size block；
   - variable-size blob；
   - loaned frame；
   - recycle；
   - pool metrics。
4. Copy policy enforcement：
   - large copy rejection threshold；
   - shared_view immutable check by convention；
   - loaned_view identity preservation；
   - move_only single reader。
5. Payload lifetime tests：
   - no use after free；
   - shared buffer remains alive through visibility window；
   - loaned frame released after last reader；
   - move_only invalid multi-reader；
   - copy threshold rejection。
6. Optional zero-copy examples：
   - large binary blob pipeline；
   - frame view no-copy demo；
   - latest overwrite with loaned buffer recycle。
7. Docs:
   - expand `docs/payloads.md`;
   - add `docs/memory.md`.

### Acceptance

- Large payload path has runnable demo and metrics.
- Copy counts are observable.
- Buffer pool ownership can be reasoned about.
- No hidden copy for loaned/shared paths unless metrics say so.

---

## G10. Trigger Engine v1

Priority: P1

### Objective

将 trigger semantics 从文档/基础实现扩展为完整、可组合、可测试的触发系统。

### Trigger Types

- `manual`
- `timer`
- `any_input`
- `all_inputs`
- `time_sync`
- `batch`
- `request`
- `task_ready`
- `future_ready`
- future: `condition`, `watermark`, `deadline_miss`, `backpressure`

### Tasks

1. 统一 trigger engine API：
   - input update；
   - timer tick；
   - request event；
   - task completion；
   - health event；
   - ready invocation generation。
2. Coalescing：
   - latest-only coalescing；
   - bounded pending invocations；
   - min_interval；
   - rate_limit。
3. Batch：
   - batch_size；
   - batch_window；
   - partial flush；
   - metrics。
4. Time sync：
   - exact sync；
   - approximate sync with slop；
   - stale drop；
   - missing timestamps fallback；
   - deterministic input order。
5. Request/future:
   - correlation id；
   - response payload；
   - timeout；
   - cancellation。
6. Tests：
   - any/all；
   - time_sync aligned；
   - time_sync out-of-window drop；
   - batch full；
   - batch timeout；
   - min_interval；
   - coalesce；
   - request；
   - task_ready。
7. Docs:
   - add `docs/triggers.md`;
   - update schema docs；
   - update metrics/trace。

### Acceptance

- Trigger readiness is runtime-owned, not component-owned.
- Complex triggers do not require components to hand-roll caches.
- All trigger types produce observable metrics/trace.
- Time-sensitive tests are deterministic through simulated clock.

---

## G11. CompositeLoop / Region Runtime v1

Priority: P1

### Objective

让 CompositeLoop 不只是 SCC escape hatch，而成为显式的 loop/region execution abstraction。

### Loop Policies

- `single_pass`
- `fixed_point`
- `bounded_iterations`
- `until_converged`
- future: `solver`, `transactional_state_machine`

### Tasks

1. Region model：
   - component region；
   - CompositeLoop region；
   - future nested region policy。
2. Loop transaction：
   - snapshot inputs at loop start；
   - internal publications staged；
   - external outputs committed at loop end；
   - loop-local metrics。
3. Convergence：
   - `single_pass`;
   - max_iterations；
   - convergence epsilon or callback；
   - no convergence => warning/error depending policy。
4. Budget:
   - budget_ms；
   - overrun metrics；
   - stop behavior。
5. Internal failure:
   - failure stops loop；
   - cleanup/deactivate semantics；
   - result errors。
6. Tests:
   - exact SCC ownership；
   - partial declaration invalid；
   - loop executes deterministic order；
   - max_iterations；
   - convergence；
   - budget overrun；
   - internal failure；
   - external commit after loop only。
7. Examples:
   - estimator-controller loop；
   - iterative optimizer demo；
   - invalid loop contrast cases。

### Acceptance

- CompositeLoop behavior is predictable and observable.
- Loops cannot run unbounded.
- External components never observe half-updated loop state.
- Diagnostics explain why a loop was accepted/rejected.

---

## G12. State / Blackboard / Config Snapshot v1

Priority: P1/P2

### Objective

把 `state` edge 语义扩展为安全的 snapshot/config/blackboard 机制。

### Tasks

1. State channel semantics：
   - current committed snapshot；
   - staged next snapshot；
   - commit boundary；
   - reader snapshot isolation。
2. Merge policy：
   - initial: single writer only；
   - future: explicit merge function；
   - reject multiple writers unless declared merge。
3. Config snapshot:
   - graph-level config object；
   - component config update event；
   - apply_on_epoch_boundary。
4. Blackboard:
   - optional namespaced state store；
   - immutable snapshots；
   - explicit write ports。
5. Tests:
   - state old snapshot during epoch；
   - next epoch visibility；
   - multi-writer reject；
   - config update boundary；
   - state edge metrics。
6. Docs:
   - `docs/state.md`;
   - examples/state_config_snapshot。

### Acceptance

- Slow-to-fast and config update use cases are safe.
- State updates are not hidden mutable globals.
- Multiple writers require explicit policy.

---

## G13. Observability v1：Metrics、Trace、Diagnostics

Priority: P1

### Objective

让 observability 成为用户能实际依赖的调试与性能分析能力。

### Metrics

现有 metrics 应扩展为：

- Component:
  - execution_count；
  - error_count；
  - last_duration_ns；
  - max_duration_ns；
  - mean/p50/p95/p99 duration optional；
  - budget_overrun_count；
  - max_in_flight_count。
- Channel:
  - publish_count；
  - commit_count；
  - consume_count；
  - drop_count；
  - overwrite_count；
  - reject_count；
  - depth；
  - max_depth；
  - stale_count；
  - deadline_miss_count；
  - copy_count；
  - large_copy_reject_count。
- Scheduler:
  - completed_count；
  - active_count；
  - queue_depth；
  - rejected_count；
  - tick_overrun_count；
  - jitter_ns；
  - stop_reason。
- Trigger:
  - ready_count；
  - coalesced_count；
  - suppressed_count；
  - batch_flush_count；
  - time_sync_drop_count。
- CompositeLoop:
  - iteration_count；
  - convergence_count；
  - non_convergence_count；
  - budget_overrun_count。
- Async:
  - submitted；
  - admitted；
  - rejected；
  - in_flight；
  - completed；
  - cancelled；
  - failed。

### Trace

Trace events should include:

- scheduler iteration span；
- component execute span；
- channel publish/commit point；
- trigger ready event；
- loop iteration span；
- async admission/completion；
- error event；
- state commit event；
- payload copy event。

### Tasks

1. Metrics registry:
   - stable names；
   - units；
   - tags；
   - docs；
   - JSON export。
2. Histograms:
   - lightweight optional histograms；
   - no heavy dep；
   - percentiles in CLI optional。
3. Trace:
   - Chrome Trace output stable；
   - nested spans；
   - thread/lane names；
   - component ids；
   - edge kind attributes。
4. Diagnostic snapshot:
   - current channels；
   - pending triggers；
   - component states；
   - lane state；
   - error list。
5. CLI:
   - `topoexec graph inspect-runtime` maybe；
   - `metrics --format json`;
   - `trace --format chrome`;
   - `trace --format json`;
   - `bench --metrics`。
6. Docs:
   - `docs/metrics.md`;
   - `docs/trace-events.md`;
   - `docs/diagnostics.md`.

### Acceptance

- Observability output can be consumed by scripts.
- Metric names are stable and documented.
- Trace can explain latency in examples.
- Error paths generate trace and metrics.

---

## G14. Benchmark Suite v1

Priority: P1/P2

### Objective

从“CLI bench 可脚本化”升级为有意义的性能基线。

### Benchmark Cases

1. single component invocation overhead。
2. immediate chain length N。
3. fan-out/fan-in。
4. latest overwrite throughput。
5. bounded queue throughput。
6. delay edge epoch overhead。
7. state edge snapshot overhead。
8. async admission overhead。
9. CompositeLoop iteration overhead。
10. trigger any/all/batch/time-sync overhead。
11. large payload copy vs shared_view vs loaned_view。
12. worker-pool throughput。
13. worker-pool non-reentrant serialization overhead。
14. trace on/off overhead。
15. metrics on/off overhead。

### Tasks

1. Add `benchmarks/` or CLI bench cases。
2. Use deterministic workloads。
3. Output JSON:
   - case；
   - params；
   - runs；
   - elapsed；
   - throughput；
   - p50/p95/p99 optional；
   - environment summary。
4. Baseline docs:
   - no hard claims before stable；
   - store sample outputs；
   - define regression thresholds later。
5. CI:
   - correctness only, not perf gating；
   - optional nightly perf。

### Acceptance

- Benchmark command covers core runtime paths.
- Users can compare edge/channel/payload choices.
- No misleading performance claims.
- Future regression gates can be added.

---

## G15. CLI / Tooling Productization

Priority: P2

### Objective

把 CLI 从开发工具提升为用户理解和调试 graph 的入口。

### Commands

现有：

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

可扩展：

- `schema dump`
- `schema check`
- `doctor`
- `inspect-runtime`
- `normalize`
- `format-graph`
- `example list`
- `example run`
- `replay`
- `record`（如果 runtime event log 稳定）

### Tasks

1. Unified output:
   - `--format text|json|yaml`；
   - consistent error codes；
   - non-zero exit codes。
2. `doctor`:
   - build version；
   - linked targets；
   - available examples；
   - sanitizer support；
   - dependency versions。
3. `lint`:
   - large payload copy warnings；
   - unbounded-like configs；
   - blocking overflow on event_loop；
   - unused components；
   - decorative CompositeLoop；
   - big SCC；
   - missing budgets；
   - missing capacity；
   - unsafe async policies。
4. `explain`:
   - explain component readiness；
   - explain edge visibility；
   - explain why cycle valid/invalid；
   - explain why component not executed。
5. `diff-plan`:
   - semantic diff；
   - region change；
   - edge visibility change；
   - trigger change；
   - risk warnings。
6. `render`:
   - Mermaid；
   - Graphviz；
   - color by edge kind；
   - mark CompositeLoop；
   - include capacity/policy labels optional。
7. Tests:
   - CLI golden outputs；
   - invalid graph errors；
   - JSON parseable outputs。

### Acceptance

- CLI output is stable enough for Codex/CI scripts.
- Explain/lint uses compiler diagnostics, not duplicate logic.
- CLI helps users fix graph design mistakes.

---

## G16. Examples / Applications Expansion

Priority: P1/P2

### Objective

用真实-ish examples 展示 TopoExec 的存在价值，而不只是 demo。

### Example Categories

1. Minimal:
   - source -> transform -> sink。
2. Low latency:
   - latest image/frame overwrite。
3. Event stream:
   - queue command processor。
4. Delay feedback:
   - previous tick control feedback。
5. State/config:
   - config snapshot applies next epoch。
6. CompositeLoop:
   - fixed point estimator/controller。
7. Async:
   - async worker completion。
8. Batch/time sync:
   - two sensors sync。
9. Large payload:
   - shared/loaned frame path。
10. Service pipeline:
   - request -> validate -> async -> response。
11. Plugin architecture:
   - component registry with app-defined factories。
12. ROS-like boundary:
   - not ROS dependency, just boundary adapter pattern example。

### Tasks

For each example:

- README:
  - graph shape；
  - why it exists；
  - run command；
  - expected output；
  - semantic lesson；
  - contrast invalid graph。
- YAML graph if relevant。
- Pure C++ builder version for at least 3 examples。
- CLI run/metrics/trace examples。
- Test that example output still matches README.

### Acceptance

- 用户能通过 examples 学会 TopoExec。
- Examples 覆盖 core semantics。
- Examples 不引入 adapter dependency。
- Example README 不夸大能力。

---

## G17. Documentation System

Priority: P1/P2

### Objective

把文档从零散 reference 组织为用户学习路径。

### Docs Structure

建议形成：

```text
docs/
  getting-started.md
  concepts.md
  runtime-semantics.md
  graph-spec.md
  schema-v1.md
  components.md
  lifecycle.md
  channels.md
  triggers.md
  scheduler.md
  concurrency.md
  composite-loops.md
  payloads.md
  memory.md
  state.md
  metrics.md
  trace-events.md
  diagnostics.md
  cli.md
  examples.md
  adapters.md
  versioning.md
  release-checklist.md
  faq.md
```

### Tasks

1. 新增 tutorial path:
   - 5-minute quickstart；
   - embed in C++；
   - write component；
   - build graph；
   - run graph；
   - debug graph。
2. Reference path:
   - API；
   - schema；
   - CLI；
   - metrics；
   - trace。
3. Design path:
   - why edge kinds；
   - feedback loops；
   - overload/backpressure；
   - scheduling model。
4. Architecture path:
   - core/runtime/yaml/tools/adapters；
   - dependency boundaries；
   - extension points。
5. Docs tests:
   - commands in docs run；
   - snippets compile where possible。

### Acceptance

- 新用户能在 30 分钟内跑通 C++ builder app。
- Codex 能通过 docs 生成合法 component/graph。
- 所有重要 runtime semantics 有一页稳定 reference。

---

## G18. Testing Strategy v1

Priority: P0/P1

### Objective

建立长期可维护的测试金字塔。

### Test Layers

1. Unit tests:
   - channel；
   - graph compiler；
   - trigger；
   - scheduler；
   - payload；
   - metrics；
   - trace。
2. Semantic tests:
   - edge visibility；
   - commit；
   - CompositeLoop；
   - trigger readiness；
   - worker barrier。
3. Property tests:
   - random DAG；
   - random SCC；
   - random channel policy；
   - deterministic seed；
   - graph builder vs YAML equivalence。
4. Golden tests:
   - CLI JSON；
   - trace；
   - metrics；
   - plan；
   - render。
5. Fuzz:
   - YAML parser；
   - schema validator；
   - graph compiler。
6. Sanitizers:
   - ASAN；
   - UBSAN；
   - TSAN non-blocking initially, blocking before beta。
7. Package tests:
   - install；
   - downstream find_package；
   - runtime-only；
   - yaml optional。
8. Example tests:
   - each app builds；
   - output smoke；
   - README commands stay valid。

### Acceptance

- CI can tell if semantics changed.
- Fuzzer can reject invalid inputs safely.
- Sanitizer failures are tracked.
- Package usage is continuously tested.

---

## G19. Build / Packaging / Distribution

Priority: P1/P2

### Objective

让外部用户可以可靠安装和嵌入 TopoExec。

### Tasks

1. CMake:
   - install targets；
   - exported config；
   - version file；
   - component install；
   - runtime-only package；
   - yaml optional；
   - build options documented。
2. Dependency handling:
   - no forced FetchContent for users unless option；
   - allow system packages；
   - option to disable CLI；
   - option to disable tests/examples。
3. Package managers:
   - vcpkg port draft；
   - Conan recipe draft；
   - maybe CPM example。
4. Release:
   - source tarball；
   - checksums；
   - CI artifact；
   - tagged releases；
   - release notes。
5. Compatibility:
   - GCC/Clang versions；
   - C++20 requirements；
   - Linux primary；
   - macOS optional；
   - Windows optional after core stable。
6. Docs:
   - install from source；
   - consume with find_package；
   - build options；
   - troubleshooting。

### Acceptance

- Clean checkout can build/install/consume.
- Runtime-only app can link without YAML/CLI.
- Release checklist covers package smoke.
- Dependencies are transparent.

---

## G20. Adapter Architecture Preview

Priority: P2

### Objective

在不污染 core 的前提下，为未来 adapter 提供稳定扩展点。

### Candidate Adapters

1. OpenTelemetry exporter:
   - metrics/trace export only。
2. Prometheus exporter:
   - metrics scrape only。
3. Perfetto adapter:
   - richer trace export。
4. ROS 2 adapter:
   - graph boundary topics/services/actions；
   - no ROS in core。
5. Python:
   - configuration/testing/scripting；
   - not high-performance path。
6. C API:
   - stable FFI for other languages。
7. Plugin loader:
   - dynamic component factories；
   - optional。

### Tasks

1. Define adapter interface:
   - consume RuntimeRunnerResult；
   - observe runtime events；
   - register boundary inputs/outputs；
   - no schema pollution unless generic。
2. Add `docs/adapters.md` with detailed contracts。
3. Add `examples/adapters/` only with stub/no heavy dependency。
4. Add `topoexec_adapters` namespace plan。
5. Keep adapters off by default.

### Acceptance

- Adapter code cannot introduce dependency into `topoexec::runtime`.
- Runtime APIs are sufficient for adapters.
- Adapter-specific schema fields are avoided unless generally useful.

---

## G21. ROS 2 Adapter Plan（Deferred but Designed）

Priority: P2/P3

### Objective

为 ROS 2 适配做设计，但不急于实现。

### Adapter Model

- TopoExec process can be one ROS 2 node with internal graph。
- ROS subscriptions become external event sources。
- ROS publishers consume boundary output ports。
- QoS maps only at ROS boundary。
- Internal EdgePolicy remains TopoExec-owned。
- Component internals should not require `rclcpp::Node`。
- ROS adapter optional package, not core dependency。

### Tasks

1. `docs/adapters/ros2.md`:
   - boundary mapping；
   - QoS mapping；
   - executor interaction；
   - threading；
   - shutdown；
   - lifecycle；
   - parameters；
   - diagnostics；
   - tracing。
2. Prototype after core stable:
   - one input topic；
   - one output topic；
   - no actions/services initially。
3. Ensure no ROS symbols in core target。
4. Design tests with fake boundary first。

### Acceptance

- ROS 2 adapter can be built separately.
- TopoExec core remains generic.
- No conflation between ROS QoS and internal EdgePolicy.

---

## G22. Developer Experience / Agent Workflow

Priority: P1

### Objective

让 Codex/AI Agent 能安全高效迭代项目。

### Tasks

1. Update `AGENTS.md`:
   - current priorities；
   - goal protocol；
   - branch rules；
   - validation commands；
   - failure protocol。
2. Add `docs/agent-goals.md`:
   - goal queue；
   - current goal status；
   - blockers；
   - safe next tasks。
3. Add `scripts/goal_check.sh`:
   - goal-specific command dispatch；
   - optional sanitizer；
   - optional golden tests。
4. Add PR template:
   - goal id；
   - semantic changes；
   - tests run；
   - public API changes；
   - schema changes；
   - known limitations。
5. Add issue templates:
   - bug；
   - semantic mismatch；
   - feature；
   - adapter request；
   - performance issue。
6. Add `docs/contributing.md`:
   - build；
   - test；
   - style；
   - API policy；
   - release policy。

### Acceptance

- Codex can pick a goal and produce a focused PR.
- Human review gets all necessary context.
- Bad broad rewrites are discouraged.

---

## G23. Architecture Refactoring Guardrails

Priority: P0/P1

### Objective

避免后续“越修越乱”，建立模块职责边界。

### Target Modules

```text
include/topoexec/common/
  logging, status, clock, metrics, trace primitives

include/topoexec/runtime/
  component, registry, graph model, graph builder, payload, channel,
  scheduler, trigger, runtime runner

src/
  implementation only

tools/topoexec/
  CLI only

yaml/
  graph loader and JSON plan helper

examples/
  runnable apps

tests/
  unit/semantic/golden/fuzz/package
```

### Rules

- `runtime` 不依赖 `yaml`。
- `common` 不依赖 `runtime`。
- `tools` 依赖 runtime/yaml，但 runtime 不依赖 tools。
- Adapter 不依赖 tools。
- Public headers 不 include CLI/YAML unless in yaml target。
- No global mutable runtime singleton。
- Component code uses GraphContext, not internal event runtime。
- Runtime owns publication routing。
- Scheduler owns execution decisions。
- Trigger engine owns readiness。
- Channel owns capacity/backpressure。
- Metrics/trace are append-only observation surfaces。

### Acceptance

- Dependency graph documented。
- CMake target dependencies enforce boundaries。
- CI has at least one dependency smoke test。
- No circular module dependencies.

---

## G24. Security / Robustness / Defensive Input Handling

Priority: P2

### Objective

让 YAML/CLI/graph input 不会轻易导致 crash、OOM 或 undefined behavior。

### Tasks

1. Parser limits:
   - max components；
   - max edges；
   - max id length；
   - max nested config depth；
   - max payload size for CLI demos。
2. Validation before allocation-heavy runtime。
3. Fuzz parser and compiler。
4. Clear errors for malicious/huge inputs。
5. No path traversal in CLI file output。
6. Safe default for `block` overflow in single-thread runtime。
7. No unchecked integer overflow for capacities/timeouts。
8. Error messages no secrets; no environment dumping by default。

### Acceptance

- Malformed graphs fail safely.
- Fuzz can run without crashes.
- Large inputs are bounded.
- CLI validates output file paths if any.

---

## G25. Release Progression Plan

Priority: P1/P2

### v0.1.1-alpha

- Fix baseline gaps.
- Add invariant tests.
- Clarify docs.
- Keep API largely unchanged.

### v0.2.0-alpha

- Scheduler lane v1.
- Channel/backpressure v1.
- Trigger engine v1.
- Payload memory v1.
- Metrics/trace v1.

### v0.3.0-alpha

- Adapter API preview.
- Async task runtime.
- State/config snapshot.
- Better benchmarks.

### v0.5.0-beta

- Blocking sanitizer CI.
- Fuzz CI.
- Stable public headers subset.
- Stable schema v1.
- Real examples and docs.
- Initial adapter preview.

### v1.0.0

- Stable schema semantics.
- Stable runtime API.
- Stable metrics/trace names.
- Mature package.
- Clear deprecation policy.
- No known MVP-only scheduler limitations.

---

# 5. Suggested Codex Goal Sequence

如果使用 Codex 新的 goal feature，建议按以下顺序推进。不要一次让 Codex 执行整份计划。

## Batch A：Baseline 与防回归

1. `G0 Baseline Lock`
2. `G3 Runtime Invariant Test Suite`
3. `G4 Diagnostic Error Codes`
4. `G1 Public API Matrix`

## Batch B：Runtime 完整性

5. `G2 Status/Error v1`
6. `G8 Channel/Backpressure v1`
7. `G10 Trigger Engine v1`
8. `G11 CompositeLoop/Region v1`

## Batch C：并发与异步

9. `G6 Scheduler Lane v1`
10. `G7 Async Task Runtime v1`
11. `G18 Concurrency/Sanitizer Tests`

## Batch D：Payload 与内存

12. `G9 Payload/Memory/BufferPool v1`
13. `G12 State/Blackboard/Config Snapshot`

## Batch E：可观测与工具

14. `G13 Observability v1`
15. `G14 Benchmark Suite v1`
16. `G15 CLI Productization`

## Batch F：应用、文档、生态

17. `G16 Examples Expansion`
18. `G17 Documentation System`
19. `G19 Build/Packaging`
20. `G20 Adapter Architecture Preview`
21. `G21 ROS 2 Adapter Plan`

---

# 6. PR Slicing Guidelines

为了避免大 PR 失控：

- 每个 PR 最多触及一个 core subsystem。
- 如果同时改 runtime + docs + tests 是合理的；如果同时改 scheduler + payload + CLI，则拆分。
- 每个 PR 必须有至少一个测试。
- 每个 PR 必须说明是否改变：
  - schema；
  - public API；
  - runtime semantics；
  - metrics/trace names；
  - CLI JSON。
- 每个 PR 如改变 runtime semantics，必须更新：
  - `docs/runtime-semantics.md`；
  - related docs；
  - `CHANGELOG.md`。
- 每个 PR 如改变 public API，必须更新：
  - `docs/public-api.md`；
  - downstream smoke if needed。

---

# 7. Quality Gates

## Always

```bash
./scripts/agent_check.sh
git diff --check
```

## Before release

```bash
cmake -S . -B build-debug-gcc -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug-gcc -j
ctest --test-dir build-debug-gcc --output-on-failure

cmake -S . -B build-rel-gcc -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-rel-gcc -j
ctest --test-dir build-rel-gcc --output-on-failure
```

## Sanitizers

Before beta:

- ASAN green。
- UBSAN green。
- TSAN green or documented false positives。
- Fuzz smoke green。

## Docs

- README quickstart works。
- Each example README command works。
- All CLI JSON examples parse。
- Schema docs match validator behavior。
- Public API docs match installed headers。

---

# 8. Architecture Acceptance Criteria

TopoExec can be considered architecturally mature when:

1. A pure C++ user can embed `topoexec::runtime` without YAML/CLI dependencies.
2. A graph with feedback cannot accidentally recurse; feedback is either delayed/state/async or CompositeLoop-owned.
3. Every channel is bounded and overload behavior is explicit.
4. Trigger readiness is runtime-owned and covered by tests.
5. Scheduler lane behavior is documented, tested, and observable.
6. Component lifecycle and failure semantics are deterministic.
7. Payload ownership and copy behavior are explicit and observable.
8. Metrics and trace explain runtime behavior without source debugging.
9. CLI can validate, explain, render, run, trace, metric, diff, and benchmark graphs with stable output.
10. Public schema/runtime semantics do not silently drift.
11. Adapter surfaces can be added without core dependency pollution.
12. Examples demonstrate real patterns, not only toy graphs.
13. Package install and downstream `find_package` are continuously tested.
14. Sanitizer/property/fuzz coverage protect concurrency and parser/compiler behavior.
15. Versioning and deprecation policy are clear.

---

# 9. Recommended First Five Codex Goals

从当前状态继续推进，我建议先执行：

## 1. G0 Baseline Lock

Reason: 当前已经有很多功能。先把 golden output 和 baseline 锁住，否则后续 Codex 大量迭代容易出现不可见语义漂移。

## 2. G3 Runtime Invariant Test Suite

Reason: TopoExec 的核心价值在语义。先把 edge visibility、commit、CompositeLoop、channel、trigger 的不可变行为固化。

## 3. G2 Status/Error Propagation v1

Reason: 完备架构必须有清晰 failure model。否则 scheduler、async、adapter 都会在错误路径上变复杂。

## 4. G6 Scheduler Lane v1

Reason: 当前 Unreleased 已推进 worker-pool MVP。下一步应把 scheduler 作为正式子系统完善，而不是继续 patch。

## 5. G8 Channel/Backpressure v1

Reason: bounded channel 和 overload semantics 是 TopoExec 的核心差异化，值得优先成熟。

---

# 10. One-shot Codex Goal Prompt 示例

可直接复制给 Codex Goal：

```md
Use PLAN.md as the long-term project spec.

Execute Goal:
G3 Runtime Invariant Test Suite

Context:
TopoExec is now beyond MVP. Current priority is to lock runtime semantics and prevent regressions before expanding architecture.

Requirements:
- Add focused runtime invariant tests for edge visibility, publish staging, immediate SCC rejection, delay/state/async next-epoch visibility, CompositeLoop ownership, and channel overload behavior.
- Prefer C++ GraphBuilder-based tests where possible.
- Do not change public API unless tests reveal an unavoidable mismatch.
- If implementation and docs disagree, write a blocker instead of silently changing semantics.
- Update docs only if current docs are inaccurate.
- Run ./scripts/agent_check.sh.
- Write docs/agent-reports/G3.md with changed files, tests run, known limitations, and follow-up goals.

Acceptance:
- Every invariant listed in PLAN.md G3 has at least one test or an explicit blocker note.
- All tests pass.
- No unrelated scheduler/payload/CLI rewrite.
```

---

# 11. 最终提醒

TopoExec 的核心竞争力不是“图能执行”，而是：

> **图的时间、反馈、触发、信道、调度、payload、观测语义都能被显式声明、编译验证、稳定执行、事后解释。**

后续所有迭代都应该围绕这个句子收敛。
