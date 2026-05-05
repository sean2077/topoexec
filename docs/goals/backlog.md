# Goal Backlog

This backlog is now derived from `docs/plans/plan2.md` and starts at G26. The completed G0-G25 board from `docs/plans/plan.md` is archived and must not be treated as active work.

## Active ordering rule

1. Finish the earliest unfinished P0/P1 goal before starting a new subsystem.
2. Prefer runtime semantics, public API stability, concurrency hardening, tests, and packaging over new adapters or broad CLI surface.
3. Keep adapter implementation deferred until core/runtime/API/concurrency P0/P1 goals are complete or explicitly opened by the user.
4. For each goal, update this backlog, `docs/goals/status.md`, docs/tests, and `CHANGELOG.md` when behavior, API, release evidence, or public docs change.

## Archived prior board

- G0-G25 from `docs/plans/plan.md`: complete as of the post-G25 baseline commit `b86a586d3a48d84bf4e03ccabde3d061e3073579`.
- Evidence remains in git history, `docs/goals/status.md`, release docs, and the golden/package/sanitizer checks. Do not reopen G0-G25 unless a regression is found.

## Plan2 goal board

| ID | Priority | Status | Scope | Acceptance / evidence gate | Validation |
| --- | --- | --- | --- | --- | --- |
| G26 | P0 | complete | Release Candidate Baseline 2 | G26 baseline docs/goldens/checks complete | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G27 | P0 | complete | Public API Stability Pass v2 | `docs/public-api.md`, installed header stability markers, `docs/api-change-checklist.md`, runtime-only smoke metrics/trace consumption, and package/policy checks. | `./scripts/agent_check.sh`; `./scripts/goal_check.sh package`; `./scripts/goal_check.sh policy`; format |
| G28 | P0 | complete | Runtime Semantic Version Contract | `docs/semantic-contract.md`, `docs/versioning.md`, doctor/schema dump semantic contract version output, schema/golden coverage, and no runtime behavior change. | `./scripts/agent_check.sh`; `./scripts/goal_check.sh schema`; `./scripts/goal_check.sh golden`; format |
| G29 | P0 | complete | Scheduler v2 Design + Contract | Scheduler docs/concurrency docs, plan JSON lane capabilities, advisory diagnostics, registry/docs updates, graph tests, and golden drift coverage distinguish implemented/advisory/future scheduler behavior. | `./scripts/agent_check.sh`; `./scripts/goal_check.sh quick`; `ctest -R test_graph`; format |
| G30 | P1 | pending | Persistent Worker Pool v1 | 将 bounded `thread_pool` 从 batch-style MVP 推进到可解释的 persistent worker pool v1。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G31 | P1 | pending | Fixed-Rate Lane v1 | 将 `fixed_rate` 从 simulated tick 推进为可选 wall-clock fixed-rate lane v1。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G32 | P1 | pending | Scheduler Priority and Admission Policy v1 | 实现轻量的 runtime-level priority/admission，不涉及 OS priority。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G33 | P1 | pending | Cooperative Cancellation and Timeout Semantics | 为 long-running component、task、CompositeLoop 提供 cooperative cancellation contract，而不是伪装为硬 preemption。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G34 | P1/P2 | pending | TaskExecutor v2: Threaded Executor Preview | 将 deterministic `TaskExecutor` 扩展为可选 threaded executor preview，同时保持 deterministic mode 作为测试默认。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G35 | P2 | pending | Trigger Engine v2: Watermark and Condition Triggers | 扩展 trigger policy，但不破坏现有 v1 trigger semantics。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G36 | P1 | pending | Correlation, Causality, and Invocation Metadata | 让 runtime trace/metrics 能从输入事件追踪到下游 outputs，支持调试复杂 graph。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G37 | P1 | pending | Channel v2: Explicit Backpressure Events | 将 backpressure 从 metrics-only 提升为 optional runtime health event，不改变执行控制流。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G38 | P1 | pending | Channel v2: Multi-Reader and Move-Only Hardening | 加强 multi-reader、single-reader、move-only、shared/loaned view 的 correctness 和 explainability。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G39 | P1/P2 | pending | Payload and Memory v2 | 将 payload system 从 useful helper 推进为可嵌入应用的内存策略层。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G40 | P1 | pending | Graph Compiler v2: Typed Ports and Constraints | 从 string endpoint validation 走向 typed port contract，减少错误连接。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G41 | P2 | pending | Hierarchical Graph / Subgraph Design | 支持复杂应用的层次化组织，但不要过早引入复杂 runtime nesting。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G42 | P2/P3 | pending | Graph Templates and Reusable Patterns | 为常见 patterns 提供可复用 graph snippets，而不是复制 YAML。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G43 | P1/P2 | pending | Component Lifecycle v2: Reset, Snapshot, Restore | 支持真实应用中组件重置、状态快照和恢复，不只是 configure/activate/deactivate。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G44 | P1/P2 | pending | Config Hot Reload Transaction | 让 graph-level config 和 component config 支持安全热更新。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G45 | P2 | pending | CompositeLoop v2: Solver-Style Policies | 将 CompositeLoop 从 fixed-point MVP 推进为可用于优化/迭代算法的 region runtime。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G46 | P0/P1 | pending | Runtime Observer API v1 | 在不引入 OTel/Prometheus/Perfetto 依赖的情况下，建立稳定 observer contract。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G47 | P1 | pending | Metrics v2: Cardinality and Schema Contract | 让 metrics 能服务真实应用和 future exporters，避免 label explosion。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G48 | P1 | pending | Trace v2: Timeline and Causality | 让 trace 从事件列表升级为可调试 timeline。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G49 | P1/P2 | pending | Diagnostics v2: More Actionable Graph Errors | 让 graph diagnostics 不只是 reject，而能告诉用户如何修图。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G50 | P0/P1 | pending | Defensive Input Handling v2 | 将 schema/parser limits 从 smoke 推进到 robust defensive behavior。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G51 | P1/P2 | pending | Coverage-Guided Fuzzing | 从 deterministic fuzz smoke 进入 coverage-guided fuzzing，提升 schema/compiler robustness。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G52 | P1/P2 | pending | Stress and Soak Tests | 验证 scheduler/channel/task 在较长运行和高负载下不会出现 obvious deadlock/leak/unbounded growth。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G53 | P1/P2 | pending | Benchmark v2 and Regression Policy | 将 benchmark 从 output-shape smoke 推进到可用的 baseline tracking，但避免不可靠 CI 阈值。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G54 | P1 | pending | Packaging v2 | 把 CMake package 从 smoke 可用推进到可被外部用户稳定消费。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G55 | P1/P2 | pending | Documentation System v2 | 把文档从“齐全”推进到“用户可学习、Agent 可执行、维护可持续”。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G56 | P1/P2 | pending | Example Applications v2 | 从 toy examples 扩展为更接近真实应用的 reference apps，但仍不引入 external adapters。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G57 | P1/P2 | pending | Adapter SDK v0 | 在不实现具体 adapter 的情况下，先稳定 adapter SDK 边界。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G58 | P2/P3 | pending | OpenTelemetry Exporter Preview | 实现第一个 optional exporter preview，验证 observer API，但不让 core 依赖 OTel。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G59 | P2/P3 | pending | Prometheus Exporter Preview | 通过 scrape/exporter 证明 metrics schema 可被外部系统消费。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G60 | P2/P3 | pending | ROS 2 Adapter Preview | 验证 TopoExec 在 ROS 2 系统中作为 in-process semantic runtime 的 adapter，但不让 core 变成 ROS package。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G61 | P2/P3 | pending | C API / FFI Design | 规划 C API，为 Python/Rust/C plugins 或 external embedding 提供未来路径，但不急于冻结 ABI。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G62 | P3 | pending | Python Binding Preview for Config/Test | 提供 Python 用于配置、测试、CLI-like automation，而不是高性能 payload path。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G63 | P2/P3 | pending | Dynamic Plugin Loading Preview | 让应用可以动态注册 components，但在安全/ABI/版本策略清楚前不默认启用。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G64 | P2 | pending | Schema v2 Exploration | 判断哪些新增能力需要 schema v2，而不是继续往 strict schema v1 塞字段。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G65 | P3 | pending | Editor / LSP / JSON Schema UX | 提升 graph authoring 体验，但保持 runtime 优先。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G66 | P0/P1 | complete | Architecture Enforcement CI v2 | `tests/policy/check_no_adapter_deps.py`, `policy_architecture_self_test`, CMake target/include audits, and `docs/architecture-guardrails.md` enforce module boundaries. | `./scripts/agent_check.sh`; `./scripts/goal_check.sh policy`; format |
| G67 | P1 | pending | Release Automation and Artifact Reproducibility | 让 prerelease 发布流程可靠、可重复、可被 Agent 执行。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G68 | P2 | pending | Community and Contribution Readiness | 让开源用户和贡献者可以参与，而不需要你解释所有上下文。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G69 | P1/P2 | pending | Real-World Pilot App | 选择一个真实但无外部依赖的 pilot app，证明 TopoExec 不只是 demo runtime。 | `./scripts/agent_check.sh`; focused goal checks as applicable |
| G70 | P0 before beta | pending | Beta Readiness Review | 在进入 beta 前做一次系统审查。 | `./scripts/agent_check.sh`; focused goal checks as applicable |

## Next goal

G30 Persistent Worker Pool v1 is the next unfinished P1 goal after G29.

## Blockers

No active blockers. Use `docs/goals/blockers/<goal-id>.md` if a product/API decision is required before implementation.
