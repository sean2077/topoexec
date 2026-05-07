对，你这个判断是正确的。后续不应该再以“增加工具/流程系统”为主，而应该改成 **Codex 持续审计全项目、发现缺陷、修复缺陷、优化实现、补回归测试、再验证** 的迭代模式。

我建议把后续路线重置为：

# **Core Iteration Program：持续代码审计、Bug 修复与性能优化**

目标不是再造 GoalOps/TrustOps，而是让 Codex 一轮一轮地做：

```text
读代码 -> 跑测试 -> 找问题 -> 修 bug -> 补测试 -> 优化性能 -> 跑 gates -> 记录变更
```

只允许增加最小必要的测试、benchmark、golden、fuzz corpus 或示例修正。**不再新增大型工具系统**。

---

## 1. 当前应该承认的真实状态

公开 main 上，TopoExec 已经具备较完整的 runtime、examples、live observe、docs、package/release gate 基础。goal ledger 记录当前 active implementation goal 为空，G71、G73、G74 已完成，并且 `scripts/agent_check.sh` 是声明 repo 变更完成前的 required gate。([GitHub][1])

测试体系也已经比较丰富，包括 default `agent_check.sh`、semantic runtime、golden CLI、schema、docs、examples、fuzz、stress、bench、live、live-perf、package、release、sanitizer 等 focused gates。也就是说，后续 Codex 的价值不是“再搭工具”，而是充分利用这些现有 gates 对代码本体做持续迭代。([GitHub][2])

README 也已经明确项目仍是 beta / pre-production，不宣传 production-proven，并列出 runtime、validation、plan/render、metrics、trace、live observe、examples 等能力。([GitHub][3])

还有一个已经能肉眼确认的具体问题：README 末尾写的是 “MIT license”，但仓库 `LICENSE` 文件是 Apache License 2.0，GitHub 页面也识别为 Apache-2.0。这个属于 public metadata bug，不是 runtime bug，但应该纳入下一轮综合 bug sweep 修复。([GitHub][3])

---

## 2. 新的 Codex 工作模式

后续每个 `/goal` 都应该是一个 **代码本体迭代目标**，而不是流程建设目标。

每轮固定要求：

```text
1. 深读指定范围的源码、测试、docs、goldens。
2. 跑 baseline gates。
3. 列出发现的问题，按 P0/P1/P2/P3 分级。
4. 只修本轮 scope 内的问题。
5. 每个 bug 修复必须配最小回归测试。
6. 每个性能优化必须配 benchmark 或至少 micro evidence。
7. 不引入大型新工具。
8. 不扩大 deferred ecosystem scope。
9. 最后跑 focused gates + agent_check。
```

Codex 输出应该包括：

```text
- Bugs found
- Bugs fixed
- Optimizations made
- Tests added
- Behavior intentionally unchanged
- Gates run
- Remaining risks
```

---

## 3. 后续 Goal 路线重排

我建议把后续目标改成下面这组。

## **G86 — Whole-Project Bug Sweep 1**

第一轮全仓 bug sweep，不追求一次性重构，只做“高信噪比问题”。

重点范围：

```text
- README / docs / license / release metadata 明显矛盾
- CLI 命令与 docs/examples 不一致
- schema / examples / goldens 不一致
- tests 中已知边界但源码未完全覆盖的缺口
- RuntimeRunner / EventRuntime / Channel / Trigger / CompositeLoop 的显式边界条件
- CMake install / package / downstream smoke 的明显缺陷
```

这轮可以修 README license mismatch，也可以修文档命令与实际路径不一致，但不能变成 docs refresh 大工程。

---

## **G87 — Runtime Correctness Sweep**

专门审计 runtime 语义。

重点：

```text
- Channel visibility:
  latest / queue / latched / previous_tick / barrier

- Overflow behavior:
  drop_oldest / drop_newest / overwrite / would_block / block alpha semantics

- TriggerPolicyEngine:
  all_inputs / any_input / time_sync / batch / condition / debounce / rate_limit

- Async:
  max_inflight accounting
  deferred completion release
  cancellation / timeout observation
  dropped/discarded async result handling

- CompositeLoop:
  iteration budget
  convergence result
  partial-success policy
  output commit/discard behavior
  trace/metrics visibility

- RuntimeRunner lifecycle:
  state restore/reset
  graph compile/validate handoff
  stop reason correctness
  runtime result ok/error consistency
```

每个发现都要配 C++ semantic test。不要只改实现。

---

## **G88 — Concurrency, Lifetime, and Sanitizer Sweep**

专门审计并发、生命周期、内存安全。

重点：

```text
- shared_ptr / unique_ptr / moved payload 生命周期
- async deferred result ownership
- channel queue pop/move/copy 边界
- observer/live observe disabled path 是否零副作用
- thread_pool admission/rejection/cancellation
- state snapshot restore/reset 是否有悬垂引用
- TSAN suspicious paths
- ASAN/UBSAN failure or undefined behavior risk
```

必须跑：

```bash
./scripts/goal_check.sh sanitizer
./scripts/goal_check.sh stress
./scripts/goal_check.sh fuzz
./scripts/goal_check.sh live
```

TSAN 如果仍是 non-blocking，可以记录发现但不要伪装成 release blocker。testing strategy 当前也明确 TSAN 仍是 non-blocking。([GitHub][2])

---

## **G89 — CLI, Schema, YAML Loader, and Error-Path Hardening**

这轮重点是用户输入和错误路径。

审计：

```text
- malformed YAML
- unknown fields
- invalid enum
- invalid channel/trigger/composite-loop config
- missing component/channel references
- bad CLI flags
- invalid --format
- invalid --steps / duration / observe options
- diagnostics JSON consistency
- schema validation vs semantic validation split
```

必须补：

```text
- fuzz corpus regression
- CLI golden for diagnostics
- schema test
- docs command marker only if docs changed
```

---

## **G90 — Performance Hot-Path Optimization Sweep**

这轮只做性能，不做功能。

重点路径：

```text
- EventRuntime scheduling loop
- Trigger ready collection
- channel publish / deliver / pop
- metrics/trace emission cost
- live observe disabled-path overhead
- map/string lookup in hot path
- repeated component/channel lookup
- unnecessary payload copy
- CompositeLoop iteration overhead
```

规则：

```text
- 优化前先跑 bench baseline。
- 每个优化要说明预期收益。
- 不允许为了快而改变语义。
- 不允许引入复杂缓存导致状态不一致。
- 不做跨机器性能吹嘘。
```

当前 benchmark strategy 明确 local baseline 不做 portable performance guarantee；这点应继续保持。([GitHub][2])

---

## **G91 — Test Coverage Gap Closure**

这轮不优先改实现，而是找 coverage blind spots。

重点：

```text
- runtime semantic tests 是否覆盖所有 documented invariants
- examples 是否只 smoke 而没有 negative cases
- CLI golden 是否覆盖 failure paths
- fuzz corpus 是否缺少 minimization regressions
- stress 是否缺少 mixed edge/channel/trigger cases
- package downstream 是否覆盖 runtime-only/YAML/CLI 分离
```

允许增加测试，但不允许顺手大改 runtime。

---

## **G92 — API and Package Hardening Sweep**

这轮针对外部嵌入者。

重点：

```text
- public headers 是否泄漏 internal
- install/export targets 是否正确
- runtime-only target 是否意外依赖 YAML/CLI/adapters
- CMake config 是否稳定
- examples app 是否能作为 downstream copy-paste starter
- Doxygen/public API docs 是否与 header contract 不矛盾
```

必须跑：

```bash
./scripts/goal_check.sh package
./scripts/goal_check.sh policy
./scripts/agent_check.sh
```

---

## **G93 — Reliability Farming Sweep**

这轮让 Codex 主动“找崩溃”。

方法：

```text
- 运行 fuzz smoke
- 扩展少量高价值 fuzz corpus
- 运行 stress smoke
- 运行 sanitizer
- 从失败或 suspicious output 中提取最小复现
- 修复后把复现纳入 regression test
```

这不是增加工具，而是利用现有 fuzz/stress/sanitizer gate 反复挖 bug。

---

## **G94 — Public Consistency Bug Sweep**

这轮修“用户会立刻看到的不可信问题”。

范围：

```text
- README 与 LICENSE 不一致
- README 与 release/tag 状态不一致
- README Quick Start 与实际 examples 路径不一致
- docs links broken
- generated showcase assets stale
- examples index stale
- docs claim 与 runtime 实际行为不一致
```

但注意：这不是重新设计 README，不是再做 showcase。只修 bug 和不一致。

---

## **G95 — Release-Candidate Stabilization Sweep**

在前面几轮完成后，进入 release candidate bugfix 模式。

规则：

```text
- 不加新 feature
- 只修 bug、测试、docs inconsistency、package issue
- 所有 public behavior change 都必须进 CHANGELOG
- 每个修复必须有 regression evidence
```

---

# 4. 第一轮应该直接给 Codex 的 `/goal`

你现在可以把下面这个直接投给 Codex。

```text
/goal G86-whole-project-bug-sweep-1

Perform a whole-project bug sweep for TopoExec.

Primary objective:
Audit the current repository end-to-end, find real bugs, inconsistencies, edge-case failures, and low-risk optimization opportunities, then fix only high-confidence issues with regression tests.

This is not a tooling-expansion goal.

Scope:
- Read the current README, CHANGELOG, goal status/backlog, testing strategy, core runtime docs, examples index, and public API docs.
- Inspect core runtime implementation paths: RuntimeRunner, EventRuntime, Channel, TriggerPolicyEngine, async handling, CompositeLoop, metrics/trace/live observe integration.
- Inspect CLI/schema/YAML/error-path code.
- Inspect examples/goldens/package/downstream smokes.
- Identify P0/P1/P2 issues.
- Fix only issues that are well understood and testable.
- Add regression tests for every behavioral fix.
- Add or update goldens only for intentional public-output changes.
- Make small performance improvements only if they do not change semantics and can be validated.

Hard requirements:
- Do not add new major tools, dashboards, frameworks, or workflow systems.
- Do not implement new ecosystem surfaces.
- Do not open schema v2.
- Do not add GUI/editor/LSP.
- Do not add production telemetry exporters.
- Do not claim production readiness.
- Do not change documented runtime semantics unless the bug is clearly in the documentation and the code is already correct, or vice versa.
- Do not make broad refactors without a direct bug/performance reason.

M0 baseline:
- Run:
  - git status --short
  - git diff --check
  - ./scripts/agent_check.sh
  - ./scripts/goal_check.sh docs
  - ./scripts/goal_check.sh golden
  - ./scripts/goal_check.sh schema
  - ./scripts/goal_check.sh examples
  - ./scripts/goal_check.sh showcase
  - ./scripts/goal_check.sh package
- If any baseline gate fails, first classify whether it is an existing bug or local environment issue.

Audit checklist:
1. Public consistency:
   - README vs LICENSE
   - README vs release/tag state
   - README Quick Start vs actual examples
   - docs links and docs command markers
2. Runtime correctness:
   - channel visibility and overflow edge cases
   - trigger pending/drop/suppression edge cases
   - async max_inflight release accounting
   - CompositeLoop commit/discard/fail/partial-output cases
   - RuntimeRunner result ok/error/stop_reason consistency
3. CLI/schema/error paths:
   - invalid graph configs
   - malformed YAML
   - invalid CLI flags
   - diagnostics JSON consistency
4. Package/API:
   - runtime-only target boundaries
   - installed headers
   - downstream find_package smoke
5. Performance opportunities:
   - avoid repeated hot-path lookups
   - reduce unnecessary payload copies
   - avoid string/JSON work in hot paths
   - preserve live observe disabled-path behavior

Implementation rules:
- For each issue fixed, add the smallest regression test.
- Prefer semantic C++ tests for runtime bugs.
- Prefer CLI golden/schema tests for CLI/schema bugs.
- Prefer package/downstream tests for install/API bugs.
- Prefer benchmark evidence for performance changes.
- Keep changes narrowly scoped.
- Document remaining unfixed issues as follow-up candidates, not as new tooling requests.

Required final validation:
- git diff --check
- ./scripts/agent_check.sh
- ./scripts/goal_check.sh docs
- ./scripts/goal_check.sh golden
- ./scripts/goal_check.sh schema
- ./scripts/goal_check.sh examples
- ./scripts/goal_check.sh showcase
- ./scripts/goal_check.sh package
- ./scripts/goal_check.sh bench
- Run stress/fuzz/sanitizer if touched code involves runtime, parser, scheduler, channel, trigger, async, CompositeLoop, or ownership.

Expected final report:
- Bugs found
- Bugs fixed
- Tests added
- Optimizations made
- Public behavior changes
- Gates run
- Remaining risks and suggested next bug-sweep goal
```

---

# 5. 第二轮 `/goal`：Runtime Correctness Sweep

G86 做完后，接着投这个。

```text
/goal G87-runtime-correctness-sweep

Perform a focused runtime correctness audit and bug-fix sweep.

Primary objective:
Find and fix correctness bugs in TopoExec runtime semantics, especially edge visibility, channel policy, trigger readiness, async completion accounting, CompositeLoop behavior, and RuntimeRunner result consistency.

Scope:
- Audit RuntimeRunner, EventRuntime, Channel, TriggerPolicyEngine, async/deferred completion paths, CompositeLoop execution paths, metrics/trace side effects.
- Compare implementation against runtime semantics docs and existing tests.
- Add missing regression tests.
- Fix behavior only when testable and clearly incorrect.

Non-goals:
- No new tools.
- No dashboard/UI work.
- No schema v2.
- No ecosystem integrations.
- No broad refactor without correctness bug.

Required baseline:
- ./scripts/agent_check.sh
- ./scripts/goal_check.sh golden
- ./scripts/goal_check.sh schema
- ./scripts/goal_check.sh live
- ./scripts/goal_check.sh bench

Audit targets:
- previous_tick visibility and update sequence
- barrier edge publication behavior
- queue overflow and would-block behavior
- latched/latest overwrite semantics
- all_inputs readiness with missing/stale inputs
- time_sync missing timestamp/drop behavior
- batch flush boundaries
- debounce/rate_limit pending suppression behavior
- async max_inflight admission/release on success/error/cancel/discard
- CompositeLoop convergence, max iteration, timeout, partial output, discard/fail
- runtime ok/error/diagnostics/health/stop_reason consistency
- metrics/trace/live observe consistency after runtime errors

Final validation:
- ./scripts/agent_check.sh
- ./scripts/goal_check.sh golden
- ./scripts/goal_check.sh schema
- ./scripts/goal_check.sh live
- ./scripts/goal_check.sh bench
- ./scripts/goal_check.sh stress
- ./scripts/goal_check.sh sanitizer if supported
- git diff --check

Acceptance:
- Every runtime bug fix has a semantic regression test.
- No documented runtime semantics are silently changed.
- Performance-sensitive fixes do not add unnecessary hot-path overhead.
```

---

# 6. 第三轮 `/goal`：Performance Sweep

```text
/goal G88-runtime-performance-and-allocation-sweep

Perform a focused performance and allocation audit of TopoExec runtime hot paths.

Primary objective:
Reduce avoidable hot-path overhead without changing runtime semantics.

Scope:
- Audit EventRuntime scheduling loops, channel publish/deliver paths, trigger readiness collection, async completion handling, CompositeLoop iteration, metrics/trace/live observe disabled path.
- Identify repeated lookups, unnecessary string work, unnecessary payload copies, avoidable heap allocations, and avoidable JSON work.
- Apply only low-risk optimizations with benchmark or test evidence.

Non-goals:
- No semantic changes.
- No new tools.
- No broad architecture rewrite.
- No cross-machine performance claims.
- No production timing or hard real-time claims.

Baseline:
- ./scripts/goal_check.sh bench
- ./scripts/goal_check.sh live-perf
- ./scripts/agent_check.sh

Implementation rules:
- Benchmark before and after where possible.
- Keep deterministic output order.
- Preserve metrics/trace/live observe contracts.
- Add tests if optimization touches behavior or ownership.

Final validation:
- ./scripts/agent_check.sh
- ./scripts/goal_check.sh bench
- ./scripts/goal_check.sh live-perf
- ./scripts/goal_check.sh golden
- ./scripts/goal_check.sh stress if scheduling/channel paths changed
- git diff --check

Acceptance:
- Optimizations are small, justified, and measured.
- No behavior changes unless explicitly tested and documented.
- Local benchmark evidence is recorded without portable performance claims.
```

---

# 7. 第四轮 `/goal`：Fuzz / Stress / Sanitizer Bug Farming

```text
/goal G89-fuzz-stress-sanitizer-bug-farming

Use existing fuzz, stress, and sanitizer gates to find and fix real robustness bugs.

Primary objective:
Actively farm parser, scheduler, channel, trigger, async, CompositeLoop, and ownership bugs using existing robustness gates.

Scope:
- Run fuzz, stress, sanitizer, and relevant CTest subsets.
- Investigate crashes, timeouts, sanitizer findings, flaky failures, suspicious warnings, and unexpected diagnostics.
- Minimize each reproduced issue.
- Fix confirmed bugs.
- Add minimized regression corpus/test cases.

Non-goals:
- No new fuzzing framework.
- No long-running mandatory CI changes.
- No new production claims.
- No broad tooling.

Required baseline:
- ./scripts/goal_check.sh fuzz
- ./scripts/goal_check.sh stress
- ./scripts/goal_check.sh sanitizer
- ./scripts/agent_check.sh

Final validation:
- ./scripts/goal_check.sh fuzz
- ./scripts/goal_check.sh stress
- ./scripts/goal_check.sh sanitizer
- ./scripts/agent_check.sh
- git diff --check

Acceptance:
- Any confirmed crash/UB/timeout has a regression.
- Any unreproduced suspicious issue is documented as follow-up, not silently ignored.
- No mandatory long-running gate is added.
```

---

# 8. 后续节奏建议

后续你可以用这个循环持续推进：

```text
Round 1: G86 Whole-project bug sweep
Round 2: G87 Runtime correctness sweep
Round 3: G88 Performance/allocation sweep
Round 4: G89 Fuzz/stress/sanitizer bug farming
Round 5: G90 CLI/schema/error-path sweep
Round 6: G91 Package/API/downstream sweep
Round 7: G92 Public consistency and release-candidate cleanup
Round 8: Repeat from runtime correctness with new findings
```

每一轮都只解决一类问题，避免 Codex 发散。

---

# 9. 关键约束

你要给 Codex 明确写死这一句：

```text
This goal is for finding and fixing bugs, improving correctness, reducing overhead, and strengthening tests. Do not add new project-management tooling, dashboards, workflow systems, or ecosystem integrations unless absolutely necessary to prove a bug fix.
```

中文就是：

```text
本 goal 只用于找 bug、修正确性、降低开销、补测试。不要新增项目管理工具、dashboard、流程系统或生态集成，除非它是证明某个 bug 修复的最小必要测试夹具。
```

这能防止它继续往“造工具”方向跑。

---

## 最终建议

你下一步直接投 **G86 Whole-Project Bug Sweep 1**。这轮目标不是漂亮、不是工具、不是发布流程，而是让 Codex 对整个项目做一次硬核质量审计：

```text
找出真实 bug；
修掉高置信问题；
补最小回归测试；
跑现有 gates；
输出下一轮更聚焦的 bug/性能清单。
```

这才符合你想要的“让 Codex 持续迭代完整项目”的方向。

[1]: https://raw.githubusercontent.com/sean2077/topoexec/main/docs/31-planning-roadmap/goals/status.md "raw.githubusercontent.com"
[2]: https://raw.githubusercontent.com/sean2077/topoexec/main/docs/24-testing/testing-strategy.md "raw.githubusercontent.com"
[3]: https://raw.githubusercontent.com/sean2077/topoexec/main/README.md "raw.githubusercontent.com"
