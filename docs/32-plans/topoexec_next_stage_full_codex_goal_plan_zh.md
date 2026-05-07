# TopoExec 下一阶段全量规划文档：Release、Adoption、Dogfooding、Beta 与选择性生态化

> 面向 Codex `/goal` 逐项实现
> 文档日期：2026-05-07
> 项目：`sean2077/topoexec`
> 建议起点：G75
> 建议策略：先发布与验证，再选择性扩展生态；不要继续无约束横向加功能。

---

## 0. 当前公开状态快照

本规划基于当前公开仓库状态校准，重点事实如下：

1. `docs/31-planning-roadmap/goals/status.md` 显示当前 **Active implementation goal: None**，G71、G73、G74 已完成。
2. G71 已完成 runtime semantic hardening、Doxygen、GitHub Pages workflow、benchmark/release/docs 对齐。
3. G73 已完成 low-overhead live runtime validation，包括 `graph observe`、live assertions、record/replay、本地 SSE dashboard、`live` / `live-perf` gates。
4. G74 已完成 examples/showcase/README refresh，包括 curated examples、generated visual assets、examples/showcase gates。
5. README 目前明确声明项目处于 **beta / pre-production**，并强调尚未 production-proven。
6. GitHub Releases 当前还没有正式 release。
7. release progression 文档建议下一条公开 prerelease 候选线为 `v0.2.0-alpha.0`，但要求人类 release owner 对 exact candidate commit 执行 release gate。
8. backlog 明确 deferred 的方向包括：schema v2 implementation、full editor/LSP、production OTel/Prometheus、real ROS 2 adapter、stable C ABI、native Python bindings、sandboxed/stable plugin ecosystem、package registry publication、hard real-time scheduling。
9. README 末尾当前写 `MIT license`，但仓库 `LICENSE` 文件是 Apache License 2.0。该不一致应作为下一阶段 P0 修复项。

---

## 1. 总体判断

TopoExec 当前已经过了“继续证明能实现核心功能”的阶段。下一步最重要的不是继续加更多 runtime 特性，而是证明：

```text
1. 外部用户可以顺利安装、构建、运行、嵌入。
2. 准真实场景下 runtime、trace、metrics、live observe、examples 能稳定工作。
3. 核心 API、schema、metrics、trace/live observe contract 足够稳定，可以开始被依赖。
4. release、docs、package、example、issue feedback 形成闭环。
5. 后续生态化方向必须基于 adoption signal 选择，而不是一次性全开。
```

因此建议路线是：

```text
G75  Release & Adoption Readiness
G76  Production-like Dogfooding Pilot
G77  Core API / Schema / CLI Compatibility Harness
G78  Distribution & Packaging Hardening
G79  Reliability, Soak, Perf Regression Program
G80  External Feedback & Adoption Loop
G81  Ecosystem Direction Decision Gate
G82x Conditional Integration Track
G83  Schema v2 / Migration Program, only if adoption demands it
G84  Core Runtime Beta Candidate
G85  v1.0 Readiness Program
```

其中：

- **G75–G80** 是建议顺序执行的核心路线。
- **G81** 是生态方向选择 gate。
- **G82x/G83/G85** 是条件路线，不建议立即全部打开。
- **G84** 可在 G75–G80 证据足够后启动。

---

## 2. 全局执行原则

### 2.1 每个 Codex goal 必须独立闭环

每个 goal 必须包含：

```text
- M0 baseline
- scope
- allowed files
- non-goals
- blocker protocol
- implementation milestones
- acceptance criteria
- final validation commands
- docs/status/backlog/changelog updates
```

不要让 Codex 在一个 goal 里跨越多个阶段。

### 2.2 从 release/adoption 面而不是功能面推进

优先级排序：

```text
1. 可发布
2. 可安装
3. 可运行
4. 可嵌入
5. 可复现
6. 可回归
7. 可反馈
8. 再考虑生态扩展
```

### 2.3 保持项目状态诚实

禁止引入以下表述：

```text
- production-ready
- deployed at scale
- hard real-time
- production ROS2/OpenTelemetry/Prometheus exporter
- stable Python/native plugin ecosystem
- mature package-registry distribution
```

除非相关目标已经完成并有证据。

### 2.4 不默认打开 deferred scope

以下方向只能在对应 goal 明确打开后做：

```text
- schema v2 loader / migration CLI
- production telemetry exporters
- real ROS2 adapter
- native Python bindings
- stable C ABI
- full editor extension / LSP
- package registry publication
- hard real-time scheduling
```

### 2.5 维护 G73 低扰动实时可视化原则

任何后续工具或 dashboard 增强都不能破坏：

```text
- observe 默认关闭
- runtime hot path 不做 JSON/file/socket/UI work
- bounded non-blocking event transport
- observer drop 可见但默认不改变 runtime ok/fail
- assertions 不进入 scheduler/channel/trigger hot path
```

### 2.6 维护 G74 示例展示原则

README 和 examples 的展示资源必须：

```text
- 来自真实示例或真实 CLI 输出
- 可生成或可校验
- 不手工维护核心展示图导致腐烂
- 不夸大生产背书
```

---

## 3. 建议 release ladder

建议不要直接跳 beta。更稳妥的演进是：

```text
v0.2.0-alpha.0
  第一条外部可试用 prerelease。
  目标：release/adoption smoke 成立。

v0.2.0-alpha.1
  dogfood pilot 和 packaging hardening 后的小版本。
  目标：准真实案例、安装体验、package smoke 更扎实。

v0.3.0-alpha
  可选生态 preview 扩展版本。
  目标：只选择一个 integration track，不同时全开。

v0.5.0-beta
  core-runtime beta candidate。
  目标：核心 runtime/API/schema/metrics/trace/live observe contract 明确稳定边界。

v1.0.0
  暂不规划近期实现。
  目标：稳定 API/schema/package/adoption/生态边界后再考虑。
```

---

## 4. 总体路线图表

| Goal | Priority | Type | 建议顺序 | 目标 |
|---|---:|---|---:|---|
| G75 | P0 | Core | 1 | Release & Adoption Readiness |
| G76 | P0/P1 | Core | 2 | Production-like Dogfooding Pilot |
| G77 | P1 | Core | 3 | API / Schema / CLI Compatibility Harness |
| G78 | P1 | Core | 4 | Distribution & Packaging Hardening |
| G79 | P1 | Core | 5 | Reliability, Soak, Perf Regression Program |
| G80 | P1/P2 | Core | 6 | External Feedback & Adoption Loop |
| G81 | P2 | Decision | 7 | Ecosystem Direction Decision Gate |
| G82a | P2 | Conditional | after G81 | Production Observability Exporter Track |
| G82b | P2 | Conditional | after G81 | Native Python Binding Track |
| G82c | P2 | Conditional | after G81 | Real ROS2 Adapter Track |
| G82d | P2 | Conditional | after G81 | Editor / LSP Track |
| G82e | P2 | Conditional | after G81 | Package Registry Publication Track |
| G83 | P2/P3 | Conditional | after adoption evidence | Schema v2 / Migration Program |
| G84 | P1 | Release | after G75–G80 | Core Runtime Beta Candidate |
| G85 | P3 | Long-term | after beta adoption | v1.0 Readiness Program |

---

# 5. G75 — Release & Adoption Readiness

## 5.1 目标

把当前成熟但尚未 release 的仓库，推进到一个外部用户可以诚实试用的 prerelease candidate。

重点不是“发 tag 这个动作本身”，而是让用户能完成：

```text
README -> clone/download -> build -> install -> find_package -> run example -> read docs -> report issue
```

## 5.2 为什么现在做

当前项目已有 G71/G73/G74 的 runtime、live tooling、examples、docs、showcase 证据，但 GitHub Releases 仍为空。项目现在需要一个可信外部入口，而不是继续内部迭代。

## 5.3 Scope

```text
- 修复 public metadata 不一致
- release notes 准备
- prerelease candidate checklist
- docs / README / CHANGELOG / license / package metadata 对齐
- GitHub Pages public URL owner action checklist
- fresh clone / build / install / downstream CMake smoke
- source/package/release artifact dry-run
- adoption quickstart 验证
```

## 5.4 P0 修复项

```text
README license text 当前写 MIT，但 LICENSE 是 Apache-2.0。
必须统一为 Apache-2.0，除非项目 owner 明确更换 LICENSE。
```

## 5.5 Non-goals

```text
- 不声明 production-ready
- 不声明 hard real-time
- 不实现 schema v2
- 不实现 production adapter
- 不发布 package registry
- 不做 beta tag，除非 release owner 明确要求
```

## 5.6 Allowed files

```text
README.md
LICENSE / NOTICE, if needed
CHANGELOG.md
docs/43-ci-build-release-tools/
docs/31-planning-roadmap/goals/
docs/01-quickstart/
docs/README.md
CMake/package metadata files, only if metadata inconsistency exists
.github/workflows/, only if release/docs workflow correctness requires
scripts/release_prepare.sh
scripts/goal_check.sh
tests/cmake/
```

## 5.7 Milestones

### M0 baseline

```bash
git status --short
git diff --check
./scripts/agent_check.sh
./scripts/goal_check.sh docs
./scripts/goal_check.sh golden
./scripts/goal_check.sh package
./scripts/goal_check.sh release
```

### M1 public metadata cleanup

检查并修复：

```text
- README license text
- LICENSE / NOTICE consistency
- package metadata
- CMake project version
- CHANGELOG version section
- release-progression language
- docs status language
- public status: beta/pre-production
```

### M2 release candidate notes

准备：

```text
- v0.2.0-alpha.0 release notes draft
- known limitations
- explicit deferrals
- validation evidence checklist
- what is stable / experimental / preview
```

### M3 docs and Pages readiness

```text
- 本地 docs build
- Doxygen/MkDocs output smoke
- Pages workflow check
- README docs link only after URL is valid
- 如果 GitHub Pages 需要 owner action，写清楚 manual step，不伪造已部署状态
```

### M4 fresh clone / install / downstream smoke

验证：

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

### M5 release artifact dry-run

```bash
./scripts/release_prepare.sh --version v0.2.0-alpha.0
./scripts/goal_check.sh package
./scripts/goal_check.sh release
```

### M6 final validation

至少：

```bash
git status --short
git diff --check
./scripts/agent_check.sh
./scripts/goal_check.sh docs
./scripts/goal_check.sh golden
./scripts/goal_check.sh package
./scripts/goal_check.sh policy
./scripts/goal_check.sh release
./scripts/goal_check.sh examples
./scripts/goal_check.sh showcase
```

## 5.8 Acceptance

```text
- README/license/package/release metadata 一致
- release notes 可用于 v0.2.0-alpha.0
- 新用户 adoption path 文档成立
- fresh clone/build/install/find_package/run minimal example 可验证
- GitHub release 可以由人类 owner 在 exact candidate commit 上发布
- 没有 production/beta/ecosystem 过度声明
```

## 5.9 Codex `/goal`

```text
/goal G75-release-and-adoption-readiness

Prepare TopoExec for an honest external prerelease and first-user adoption path.

Primary objective:
Convert the current mature pre-production repository into a clean, installable, documented, and externally testable prerelease candidate.

Scope:
- Fix public metadata and documentation inconsistencies.
- Prepare v0.2.0-alpha.0 release notes unless the release owner chooses another prerelease version.
- Make GitHub release, documentation site, install, package, and downstream CMake consumption paths coherent.
- Preserve honest project status: beta/pre-production, not production-proven, not adapter/ecosystem beta.
- Add anti-regression gates for release/adoption surfaces where missing.

Hard requirements:
- Do not claim production deployment.
- Do not claim hard real-time support.
- Do not claim production ROS2/OpenTelemetry/Prometheus/native Python/plugin ecosystem readiness.
- Do not open schema v2 or editor/LSP implementation in this goal.
- Do not weaken existing runtime/API/docs/example/live gates.
- Fix README license text to match the repository LICENSE unless the human owner explicitly changes the license.

Milestones:
M0 baseline:
- Run git status, git diff --check, agent_check, docs, golden, package, policy, release, examples, showcase.
- Record exact baseline state in the goal ledger.

M1 public metadata cleanup:
- Fix README/license/package/CHANGELOG/release-doc inconsistencies.
- Confirm README and docs keep beta/pre-production status honest.
- Ensure deferrals are consistent across README, release progression, beta readiness, and backlog.

M2 release candidate preparation:
- Prepare v0.2.0-alpha.0 release notes or the release owner’s chosen prerelease version.
- Include explicit deferrals and known limitations.
- Distinguish core runtime readiness from adapter/ecosystem previews.

M3 documentation site:
- Verify local docs site build.
- Verify Doxygen/MkDocs path and Pages workflow.
- Add public docs URL only if deployment is already valid; otherwise document the owner action.

M4 install and downstream adoption:
- Verify fresh clone build.
- Verify install.
- Verify find_package(topoexec CONFIG REQUIRED).
- Verify runtime-only downstream app.
- Verify YAML/CLI optional surfaces separately.
- Update install/adoption docs.

M5 release artifact smoke:
- Run package and release preparation scripts.
- Verify source archive, CPack artifacts, package config metadata, examples smoke, and release notes.
- Keep generated local artifacts out of git unless explicitly intended.

M6 final validation:
- Run agent_check, docs, golden, package, policy, release, examples, showcase, and git diff --check.
- Run sanitizer/stress/fuzz/bench/live gates where release scope depends on those surfaces.
- Update status.md, backlog.md, CHANGELOG, and release docs.

Acceptance:
- GitHub README, docs, license, release docs, and package metadata are consistent.
- A new user can build, install, find_package, and run a minimal example from documented steps.
- A prerelease can be published without contradicting current project boundaries.
- No production-readiness overclaim is introduced.
```

---

# 6. G76 — Production-like Dogfooding Pilot

## 6.1 目标

做一个准真实的 dogfooding pilot，用来证明 TopoExec 不只是 toy examples，而能承载一个多组件、多触发、多通道、多观测面的中等复杂本地 runtime 场景。

## 6.2 推荐场景

优先选择一个，不要同时铺开。

### 方案 A：Robot-cell / sensor-fusion pilot

适合展示：

```text
- time_sync
- bounded channels
- async planner
- CompositeLoop-like correction
- live observe
- trace / metrics
- failure diagnostics
```

### 方案 B：Local inference pipeline pilot

适合展示：

```text
- preprocess
- inference-like async stage
- postprocess
- bounded inflight
- benchmark
- live observe / trace
```

### 方案 C：ETL/dataflow validation pilot

适合展示：

```text
- deterministic dataflow
- validation
- golden-friendly output
- CI integration
```

建议优先选 **Robot-cell** 或 **Local inference**，因为它们更能体现 TopoExec 的 trigger/async/loop/observability 差异化。

## 6.3 Scope

```text
- 一个准真实 pilot graph
- long-run smoke
- trace/metrics/live observe artifact
- replay bundle
- failure injection by input/config, not runtime control
- case-study style docs
- benchmark baseline
```

## 6.4 Non-goals

```text
- 不接真实硬件
- 不声明生产部署
- 不做 hard real-time
- 不接真实 ROS2 client library
- 不引入外部 ML runtime dependency
- 不做远程 dashboard 或控制协议
```

## 6.5 Allowed files

```text
examples/90-dogfood-pilot/
benchmarks/
tests/dogfood/
docs/11-user-guide/
docs/41-development-tools/
docs/assets/generated/
scripts/
README.md, only for linking pilot
CHANGELOG.md
docs/31-planning-roadmap/goals/
```

## 6.6 Milestones

### M0 baseline

```bash
./scripts/agent_check.sh
./scripts/goal_check.sh examples
./scripts/goal_check.sh showcase
./scripts/goal_check.sh live
./scripts/goal_check.sh bench
```

### M1 scenario design

写清楚：

```text
- pilot 业务叙事
- graph topology
- components
- channel modes
- trigger policies
- async behavior
- expected failure modes
- observability outputs
```

### M2 implement pilot example

新增：

```text
examples/90-dogfood-pilot/
  README.md
  pilot.yaml
  example.json
  assertions.yaml
  expected-output.md 或 summary
```

### M3 pilot run and artifact generation

产生：

```text
- plan.json
- render.mmd / svg
- metrics.final.json
- trace.final.json
- trace.chrome.json
- observe.jsonl
- assertion_result.json
- dashboard.html
```

### M4 long-run smoke

目标：

```text
- N ticks 或 N seconds
- no runtime error
- no unexpected observer failure
- expected channel drops/suppression 可解释
- no unbounded memory growth
```

### M5 benchmark baseline

```bash
./scripts/goal_check.sh bench
```

补充 dogfood benchmark case，但不做跨机器性能夸张声明。

### M6 docs/case study

新增 case-study style 文档：

```text
- 场景
- graph
- 怎么跑
- 怎么看 trace/metrics/live dashboard
- 失败如何复盘
- 明确这是 synthetic dogfood，不是真实生产部署
```

### M7 final validation

```bash
./scripts/agent_check.sh
./scripts/goal_check.sh examples
./scripts/goal_check.sh showcase
./scripts/goal_check.sh live
./scripts/goal_check.sh bench
./scripts/goal_check.sh stress
git diff --check
```

## 6.7 Acceptance

```text
- 一个准真实 pilot 可运行、可验证、可回放
- 文档以 case-study 方式说明 TopoExec 的价值
- live observe/metrics/trace 都能用于解释该 pilot
- long-run smoke 有证据
- 不产生真实生产或硬实时过度声明
```

## 6.8 Codex `/goal`

```text
/goal G76-production-like-dogfooding-pilot

Build a production-like synthetic dogfooding pilot that demonstrates TopoExec in a richer, realistic local runtime scenario without claiming real production deployment.

Primary objective:
Show that TopoExec can run and explain a non-trivial, multi-component, observable graph using existing runtime semantics, examples, metrics, trace, live observe, assertions, and benchmark tools.

Scenario:
Choose exactly one primary pilot unless the human owner overrides:
- Robot-cell / sensor-fusion inspired pilot, or
- Local inference pipeline pilot, or
- ETL/dataflow validation pilot.

Recommended default:
Robot-cell / sensor-fusion inspired pilot, because it exercises triggers, bounded channels, async behavior, live observe, trace, metrics, and diagnostics.

Hard requirements:
- Do not connect to real hardware.
- Do not add real ROS2 client-library dependencies.
- Do not add external ML runtime dependencies.
- Do not claim production deployment.
- Do not claim hard real-time scheduling.
- Do not add runtime control endpoints.
- Preserve G73 low-overhead live observe invariants.
- Preserve G74 generated example/showcase anti-rot approach.

Milestones:
M0 baseline:
- Run agent_check, examples, showcase, live, and bench focused gates.
- Record baseline evidence.

M1 pilot design:
- Document scenario, components, channels, triggers, async behavior, expected outputs, expected failure modes, and observability surfaces.
- Decide which existing runtime semantics are exercised.
- Do not invent new runtime semantics unless an explicit blocker is written.

M2 implement pilot:
- Add examples/90-dogfood-pilot or equivalent.
- Add README.md, graph YAML, example metadata, assertions, and expected summary.
- Keep the example deterministic and smokeable.

M3 observability artifacts:
- Generate plan/render assets.
- Generate metrics, trace, Chrome trace, live observe NDJSON, assertion result, replay bundle, and dashboard HTML where supported.
- Ensure artifacts are generated or validated, not hand-maintained silently.

M4 long-run smoke:
- Add a bounded long-run smoke or soak-lite check.
- Verify no runtime error, no unexpected assertion failure, and no unbounded memory growth.
- Ensure expected drops/suppression/overwrites are documented if present.

M5 benchmark:
- Add a benchmark case for the pilot.
- Keep benchmark outputs as local baselines or contract checks, not cross-machine performance claims.

M6 docs/case study:
- Add a case-study style guide explaining how to run the pilot and inspect runtime behavior.
- Update examples index and README links if appropriate.
- State clearly that this is synthetic dogfooding, not real production deployment.

M7 final validation:
- Run agent_check, examples, showcase, live, bench, stress where supported, and git diff --check.
- Update status.md, backlog.md, CHANGELOG, and relevant docs.

Acceptance:
- A user can run the pilot, inspect graph/metrics/trace/live observe, and replay artifacts.
- The pilot demonstrates TopoExec value beyond toy examples.
- The pilot is deterministic enough for CI smoke and documented enough for learning.
- No production-readiness or hard-real-time overclaim is introduced.
```

---

# 7. G77 — Core API / Schema / CLI Compatibility Harness

## 7.1 目标

把“当前 public API / schema / CLI JSON / metrics / trace / live observe 的稳定边界”从文档约定推进到可测试约束。

## 7.2 为什么现在做

G75/G76 后会有外部用户试用。此时最重要的是避免后续改动无意破坏：

```text
- install headers
- CMake targets
- GraphSpec / RuntimeRunner / GraphBuilder 等核心 API
- schema v1
- CLI JSON contracts
- metrics schema
- trace schema
- live observe schema
```

## 7.3 Scope

```text
- public API stability inventory
- installed-header policy checks
- downstream compile compatibility examples
- schema/metrics/trace/live observe compatibility tests
- CLI JSON additive compatibility policy
- deprecation policy enforcement
```

## 7.4 Non-goals

```text
- 不冻结 v1.0 API
- 不承诺所有 preview API 稳定
- 不实现 schema v2
- 不做 stable C ABI beyond ABI version 0
```

## 7.5 Milestones

### M0 baseline

```bash
./scripts/agent_check.sh
./scripts/goal_check.sh policy
./scripts/goal_check.sh package
./scripts/goal_check.sh golden
./scripts/goal_check.sh live
```

### M1 API surface inventory

输出矩阵：

```text
stable-v0.2
mixed
experimental
preview
internal
```

### M2 installed header compatibility check

新增/强化：

```text
tests/policy/check_installed_headers.py
tests/cmake/public_api_smoke/
```

### M3 CLI JSON compatibility

检查：

```text
- graph plan --format json
- graph metrics --format json
- graph trace --format json
- graph observe --format ndjson / json-summary
- doctor --format json
```

策略：

```text
- stable surfaces 允许 additive fields
- 不允许无记录删字段或改类型
- preview surfaces 可变，但必须标注
```

### M4 schema compatibility

检查：

```text
- graph schema v1
- metrics schema version
- trace schema version
- observe schema version
- assertion schema version
```

### M5 deprecation check

新增或强化：

```text
- docs/versioning consistency
- public-api.md consistency
- CHANGELOG migration notes
```

### M6 final validation

```bash
./scripts/agent_check.sh
./scripts/goal_check.sh policy
./scripts/goal_check.sh package
./scripts/goal_check.sh golden
./scripts/goal_check.sh live
./scripts/goal_check.sh docs
git diff --check
```

## 7.6 Acceptance

```text
- stable-v0.2 public surface 有机器可检验的边界
- downstream CMake smoke 覆盖 runtime-only/YAML/CLI optional surfaces
- schema/metric/trace/live observe contract 破坏能被检测
- preview/experimental surfaces 明确不伪装稳定
```

## 7.7 Codex `/goal`

```text
/goal G77-core-api-schema-cli-compatibility-harness

Turn TopoExec’s documented public API, schema, CLI JSON, metrics, trace, and live observe compatibility boundaries into executable checks.

Primary objective:
Prevent accidental public-surface breakage as TopoExec moves toward external prerelease and core-runtime beta candidacy.

Scope:
- Inventory stable-v0.2, mixed, experimental, preview, and internal surfaces.
- Add or strengthen installed-header and downstream CMake compatibility checks.
- Add schema/metric/trace/live observe/CLI JSON compatibility checks.
- Enforce public API and deprecation policy consistency in docs and tests.

Hard requirements:
- Do not claim v1.0 stability.
- Do not stabilize preview adapter/native/plugin/Python/C ABI surfaces beyond their documented status.
- Do not implement schema v2.
- Do not weaken existing goldens or schema checks.
- Do not silently change public CLI JSON field names or types.

Milestones:
M0 baseline:
- Run agent_check, policy, package, golden, live, docs.
- Record baseline.

M1 public surface inventory:
- Update or verify docs/61-api/public-api.md and versioning docs.
- Classify headers, CMake targets, CLI JSON outputs, schemas, and preview surfaces.

M2 installed header and downstream compile checks:
- Add policy checks for installed headers and forbidden internal leakage.
- Add downstream runtime-only, YAML, CLI, and preview-boundary smoke projects where missing.

M3 CLI JSON compatibility:
- Add compatibility checks for stable JSON outputs: plan, metrics, trace, doctor, observe summary.
- Treat stable fields as additive-compatible.
- Mark preview outputs clearly if not stable.

M4 schema compatibility:
- Add or strengthen checks for graph schema v1, metrics schema, trace schema, live observe schema, and assertion schema.
- Fail on undocumented version or field-contract breakage.

M5 deprecation and docs consistency:
- Ensure deprecation policy, public API docs, CHANGELOG, and release docs agree.
- Add a focused policy gate if appropriate.

M6 final validation:
- Run agent_check, policy, package, golden, live, docs, and git diff --check.
- Update status.md, backlog.md, CHANGELOG, and relevant release/API docs.

Acceptance:
- Stable-v0.2 public surface boundaries are machine-checkable.
- Downstream CMake users can consume runtime/YAML/CLI surfaces as documented.
- Schema, metrics, trace, live observe, and stable CLI JSON breaks are detected.
- Experimental/preview surfaces remain honestly labeled.
```

---

# 8. G78 — Distribution & Packaging Hardening

## 8.1 目标

降低外部用户安装和嵌入成本。先把 GitHub release artifact、CMake install、CPack、source install 做扎实；registry publication 暂不默认打开。

## 8.2 Scope

```text
- install-from-source polish
- CPack artifact validation
- package config metadata validation
- downstream runtime-only/yaml/cli smoke
- runtime-only minimal dependency path
- packaging docs
- optional vcpkg/conan draft validation, not publication
```

## 8.3 Non-goals

```text
- 不发布 vcpkg/conan registry，除非人类 release owner 明确批准
- 不引入系统级复杂安装器
- 不捆绑生产 adapter dependency
```

## 8.4 Milestones

### M0 baseline

```bash
./scripts/goal_check.sh package
./scripts/goal_check.sh release
./scripts/agent_check.sh
```

### M1 package matrix

定义并验证：

```text
- runtime-only
- runtime + yaml
- runtime + yaml + cli
- examples on/off
- docs on/off
- preview options off by default
```

### M2 CPack artifact check

```text
- source package
- binary package if supported
- installed CMake config
- imported targets
- package metadata variables
```

### M3 dependency minimization check

确保：

```text
runtime-only 不要求 YAML/CLI/adapters
preview adapters 不污染 core runtime
optional targets not built by default unless documented
```

### M4 install docs

补充：

```text
- install from source
- consume from CMake
- runtime-only app
- YAML graph tool
- CLI component
- troubleshooting
```

### M5 optional package manager drafts

只做 validation：

```text
- vcpkg port dry-run
- conan recipe dry-run
```

不做 registry publication。

### M6 final validation

```bash
./scripts/agent_check.sh
./scripts/goal_check.sh package
./scripts/goal_check.sh release
./scripts/goal_check.sh policy
./scripts/goal_check.sh docs
git diff --check
```

## 8.5 Acceptance

```text
- fresh install + find_package 路径稳定
- runtime-only embedder 路径清晰
- CPack/source package smoke 通过
- registry publication 仍由人类 owner 决定
```

## 8.6 Codex `/goal`

```text
/goal G78-distribution-and-packaging-hardening

Harden TopoExec’s install, package, and downstream consumption paths without publishing package registry artifacts.

Primary objective:
Make TopoExec easy and reliable to install and consume from CMake for runtime-only, YAML, and CLI users.

Scope:
- Validate install-from-source.
- Harden CPack/source package artifact checks.
- Strengthen package config metadata and imported target checks.
- Validate runtime-only and optional component boundaries.
- Improve packaging and install documentation.
- Validate vcpkg/conan drafts only if already present, without publishing.

Hard requirements:
- Do not publish to vcpkg/conan/other registries in this goal unless the human release owner explicitly approves.
- Do not add production adapter dependencies.
- Do not make runtime-only users depend on YAML/CLI/adapters.
- Do not weaken existing package/policy checks.

Milestones:
M0 baseline:
- Run package, release, policy, docs, and agent_check gates.
- Record baseline.

M1 package matrix:
- Define runtime-only, YAML, CLI, examples, docs, and preview option matrix.
- Ensure defaults match docs.

M2 CPack and install artifact checks:
- Verify install tree, CMake config, imported targets, package variables, and package artifacts.
- Add or update tests/cmake/package smoke cases where needed.

M3 dependency boundary checks:
- Confirm runtime-only install does not require YAML/CLI/adapters.
- Confirm preview adapters remain default-off and do not pollute runtime.

M4 docs:
- Update build-and-package, getting-started, and embedding docs.
- Add troubleshooting for common install/find_package failures.

M5 package manager draft validation:
- Validate vcpkg/conan drafts if present.
- Keep registry publication deferred unless explicitly opened.

M6 final validation:
- Run agent_check, package, release, policy, docs, golden where relevant, and git diff --check.
- Update status.md, backlog.md, CHANGELOG, and packaging docs.

Acceptance:
- A downstream CMake app can consume topoexec::runtime from an install prefix.
- YAML and CLI optional components work as documented.
- Package artifacts and metadata are validated.
- Registry publication remains a separate human-approved goal.
```

---

# 9. G79 — Reliability, Soak, and Performance Regression Program

## 9.1 目标

把当前 stress/fuzz/bench/sanitizer 从 smoke evidence 推进到更系统的 reliability program，为 beta candidate 提供更强证据。

## 9.2 Scope

```text
- soak-lite profile
- stress matrix
- fuzz corpus management
- perf regression baseline policy
- live observe overhead regression guard
- memory/leak/sanitizer evidence
- failure triage artifacts
```

## 9.3 Non-goals

```text
- 不把所有长测放进默认 gate
- 不做跨机器绝对性能承诺
- 不声称 hard real-time
- 不要求 TSAN 成为 blocking，除非 release governance 决定
```

## 9.4 Milestones

### M0 baseline

```bash
./scripts/goal_check.sh bench
./scripts/goal_check.sh stress
./scripts/goal_check.sh fuzz
./scripts/goal_check.sh sanitizer
./scripts/goal_check.sh live-perf
```

### M1 define test tiers

```text
tier 0: default agent_check
tier 1: focused smoke
tier 2: release candidate checks
tier 3: overnight/long-run optional
```

### M2 soak-lite

新增可控 profile：

```bash
TOPOEXEC_STRESS_PROFILE=soak-lite TOPOEXEC_STRESS_DURATION_SECONDS=60 ./scripts/stress_smoke.sh
```

### M3 perf regression policy

明确：

```text
- per-machine local baseline
- no universal threshold by default
- opt-in thresholds for CI hardware
- live observe disabled/summary/detailed/debug overhead tracking
```

### M4 fuzz corpus

整理：

```text
- seed corpus
- crash reproducer location
- minimization docs
- fuzz smoke remains bounded
```

### M5 sanitizer/TSAN policy

明确：

```text
- ASAN+UBSAN release-candidate required
- TSAN status：non-blocking or blocking，由 release owner 决定
- 如果 non-blocking，release notes 说明
```

### M6 failure artifact convention

定义：

```text
artifacts/reliability/<run-id>/
  command.txt
  environment.txt
  logs.txt
  metrics.json
  trace.json
  observe.jsonl
  assertion_result.json
```

### M7 final validation

```bash
./scripts/agent_check.sh
./scripts/goal_check.sh bench
./scripts/goal_check.sh stress
./scripts/goal_check.sh fuzz
./scripts/goal_check.sh sanitizer
./scripts/goal_check.sh live-perf
git diff --check
```

## 9.5 Acceptance

```text
- 有清晰 test tier
- release candidate gate 比默认 gate 更强
- benchmark/live-perf 不做跨机器过度声明
- fuzz/stress/sanitizer 失败可以留下复现证据
```

## 9.6 Codex `/goal`

```text
/goal G79-reliability-soak-and-performance-regression-program

Strengthen TopoExec’s reliability evidence for prerelease and future core-runtime beta review.

Primary objective:
Move beyond smoke-only confidence by defining repeatable stress, fuzz, sanitizer, soak-lite, benchmark, and live-observe overhead evidence tiers.

Scope:
- Define default, focused, release-candidate, and optional long-run test tiers.
- Add or refine soak-lite profile.
- Strengthen stress/fuzz/sanitizer/bench/live-perf documentation and artifact conventions.
- Add failure triage artifact conventions.
- Keep default developer gates practical.

Hard requirements:
- Do not make long-running tests mandatory for every local edit.
- Do not claim cross-machine absolute performance.
- Do not claim hard real-time.
- Do not make TSAN blocking unless release governance explicitly decides.
- Preserve G73 live-observe low-overhead contract.

Milestones:
M0 baseline:
- Run bench, stress, fuzz, sanitizer, live-perf, and agent_check where locally supported.
- Record baseline.

M1 test tiers:
- Document tier 0 default, tier 1 focused, tier 2 release-candidate, and tier 3 optional long-run tests.
- Ensure scripts and docs agree.

M2 soak-lite:
- Add or refine a bounded soak-lite profile.
- Keep duration configurable.
- Ensure failures produce actionable logs.

M3 performance regression policy:
- Clarify local baseline handling.
- Track disabled/summary/detailed/debug live observe overhead.
- Keep thresholds opt-in unless CI hardware is controlled.

M4 fuzz corpus and reproduction:
- Organize seed corpus and crash reproducer conventions.
- Keep fuzz smoke bounded.
- Document minimization workflow.

M5 sanitizer and TSAN policy:
- Ensure ASAN+UBSAN release-candidate gate remains documented.
- Document TSAN as blocking or non-blocking according to release owner decision.

M6 failure artifacts:
- Define reliability artifact layout for logs, environment, metrics, trace, observe, assertion result, and command reproduction.

M7 final validation:
- Run agent_check, bench, stress, fuzz, sanitizer, live-perf, docs, and git diff --check where supported.
- Update status.md, backlog.md, CHANGELOG, and testing/release docs.

Acceptance:
- Reliability test tiers are explicit.
- Release-candidate evidence is stronger than default local smoke.
- Perf/live-perf policy avoids misleading claims.
- Failures are easier to reproduce and triage.
```

---

# 10. G80 — External Feedback & Adoption Loop

## 10.1 目标

建立外部用户试用反馈闭环，让项目从“自测完整”走向“有人能反馈、能复现、能改进”。

## 10.2 Scope

```text
- issue templates
- adoption feedback template
- debug pack guidance
- minimal repro guidance
- README/docs feedback links
- release feedback process
- triage labels
```

## 10.3 Non-goals

```text
- 不承诺 SLA
- 不建立商业支持通道
- 不承诺 private vulnerability program，除非 owner 另行决定
- 不做社区运营大工程
```

## 10.4 Milestones

### M0 baseline

```bash
./scripts/goal_check.sh docs
./scripts/goal_check.sh examples
./scripts/goal_check.sh showcase
```

### M1 issue template audit

新增或更新：

```text
.github/ISSUE_TEMPLATE/bug_report.yml
.github/ISSUE_TEMPLATE/feature_request.yml
.github/ISSUE_TEMPLATE/adoption_feedback.yml
.github/ISSUE_TEMPLATE/example_request.yml
.github/ISSUE_TEMPLATE/build_install_failure.yml
```

### M2 debug pack / repro guidance

文档说明用户提交问题时附带：

```text
- topoexec doctor --format json
- graph YAML
- validate output
- plan JSON
- metrics/trace/live observe artifact
- package/install logs
```

### M3 labels and triage workflow

建议 labels：

```text
area:runtime
area:cli
area:docs
area:examples
area:package
area:live-observe
area:api
kind:bug
kind:adoption-feedback
kind:question
kind:feature
status:needs-repro
status:accepted
```

### M4 first-user path

README/docs 增加：

```text
Try this first
Report a build/install failure
Report a graph semantics confusion
Share an adoption attempt
```

### M5 final validation

```bash
./scripts/goal_check.sh docs
./scripts/goal_check.sh showcase
git diff --check
```

## 10.5 Acceptance

```text
- 外部用户有明确反馈入口
- issue 模板要求可复现证据
- adoption feedback 可以帮助选择后续生态方向
- 不承诺超出现阶段能力的 SLA
```

## 10.6 Codex `/goal`

```text
/goal G80-external-feedback-and-adoption-loop

Create a lightweight external feedback and adoption loop for TopoExec prerelease users.

Primary objective:
Make it easy for first users to report build failures, graph semantics confusion, example requests, adoption attempts, and reproducible runtime issues.

Scope:
- Add or update GitHub issue templates.
- Add adoption feedback template.
- Add build/install failure template.
- Add minimal repro and debug artifact guidance.
- Add README/docs links for trying, reporting, and sharing feedback.
- Add label/triage guidance where appropriate.

Hard requirements:
- Do not promise maintainer SLA.
- Do not claim production support.
- Do not create a private vulnerability program unless the owner explicitly requests it.
- Do not add community infrastructure that cannot be maintained.
- Do not weaken docs/examples gates.

Milestones:
M0 baseline:
- Run docs, examples, showcase, and git diff --check.
- Record baseline.

M1 issue templates:
- Add or update bug, feature request, adoption feedback, example request, and build/install failure templates.
- Ensure templates ask for version, OS, compiler, CMake, command, graph YAML, logs, and artifacts where relevant.

M2 reproducibility guidance:
- Document minimal repro expectations.
- Document topoexec doctor, validate, plan, metrics, trace, live observe, and package logs as useful artifacts.

M3 labels and triage:
- Add or document labels for area, kind, and status.
- Keep workflow lightweight.

M4 first-user links:
- Update README/docs with “Try this first” and “Report a problem” paths.
- Link examples and release docs appropriately.

M5 final validation:
- Run docs/showcase checks and git diff --check.
- Update status.md, backlog.md, CHANGELOG, and contribution docs.

Acceptance:
- A first user knows how to report a reproducible problem.
- Adoption feedback can guide future ecosystem decisions.
- No support or production-readiness overclaim is introduced.
```

---

# 11. G81 — Ecosystem Direction Decision Gate

## 11.1 目标

在打开任何重大生态实现之前，先做一次选择性决策，避免同时打开 ROS2、Python、OTel/Prometheus、LSP、registry、schema v2 等多个方向。

## 11.2 Scope

```text
- 收集 G75–G80 的 adoption feedback
- 分析目标用户
- 选择最多一个主生态方向
- 写 decision note
- 更新 backlog
```

## 11.3 候选方向

```text
A. Production Observability Exporter Track
B. Native Python Binding Track
C. Real ROS2 Adapter Track
D. Editor / LSP Track
E. Package Registry Publication Track
F. Keep core/runtime hardening only
```

## 11.4 Decision criteria

| Criterion | 权重 |
|---|---:|
| 有真实用户需求或 adoption feedback | 高 |
| 与 TopoExec 核心定位匹配 | 高 |
| 对 runtime core 污染程度 | 高 |
| 实现/维护成本 | 中 |
| 文档/测试/分发复杂度 | 中 |
| 是否能带来外部采用 | 高 |
| 是否过早冻结不成熟 API | 高 |

## 11.5 Codex `/goal`

```text
/goal G81-ecosystem-direction-decision-gate

Perform a scoped decision gate before opening any major ecosystem implementation track.

Primary objective:
Choose at most one next ecosystem direction based on adoption feedback, project positioning, maintenance cost, runtime boundary risk, and release readiness.

Scope:
- Review feedback and evidence from G75-G80.
- Compare candidate tracks:
  A. production observability exporter,
  B. native Python bindings,
  C. real ROS2 adapter,
  D. editor/LSP,
  E. package registry publication,
  F. continue core/runtime hardening only.
- Produce a decision note.
- Update backlog and release planning docs.

Hard requirements:
- Do not implement the chosen track in this goal.
- Do not open multiple major ecosystem tracks at once.
- Do not claim readiness for unimplemented tracks.
- Do not change runtime semantics.
- Do not add heavy dependencies.

Milestones:
M0 baseline:
- Run docs and git diff --check.
- Review status/backlog/release docs and recent feedback artifacts.

M1 evidence inventory:
- Summarize adoption feedback, issue themes, build/install failures, example requests, and dogfood results.

M2 option analysis:
- Compare each candidate track by user demand, fit, dependency risk, runtime boundary risk, testing cost, packaging cost, and release impact.

M3 recommendation:
- Recommend exactly one next implementation track, or recommend no ecosystem track yet.
- State why other tracks remain deferred.

M4 docs update:
- Add a decision note under docs/31-planning-roadmap or docs/33-specs-rfcs.
- Update backlog/status accordingly.

M5 final validation:
- Run docs and git diff --check.

Acceptance:
- The next ecosystem direction is explicit.
- Deferred tracks remain clearly deferred.
- No implementation scope leaks into the decision goal.
```

---

# 12. G82x — Conditional Integration Tracks

以下目标不是都要实现。G81 之后最多选择一个优先启动。

---

## 12.1 G82a — Production Observability Exporter Track

### 目标

把已有 dependency-free telemetry preview 发展成可测试的 production-adjacent exporter，但仍要控制 runtime dependency boundary。

### 关键边界

```text
- exporter 不能污染 topoexec::runtime core
- 默认不启用
- 高基数 label policy 必须保留
- exporter failure 不改变 runtime semantics
- production claim 只限 exporter surface，不代表整项目 production-proven
```

### Codex `/goal`

```text
/goal G82a-production-observability-exporter-track

Implement the selected production observability exporter track only if G81 chose this direction.

Primary objective:
Develop a production-adjacent Prometheus or OpenTelemetry exporter surface while preserving TopoExec runtime dependency boundaries and metric cardinality policy.

Scope:
- Choose exactly one exporter target: Prometheus or OpenTelemetry.
- Keep exporter optional and default-off.
- Preserve runtime core independence from exporter dependencies.
- Add exporter tests, docs, package option, and failure semantics.

Hard requirements:
- Do not implement both Prometheus and OpenTelemetry unless G81 explicitly approved both, which should be avoided.
- Do not add exporter dependencies to topoexec::runtime.
- Do not allow exporter failure to change runtime ok/fail semantics by default.
- Do not violate metric bounded-label policy.
- Do not claim full production observability platform readiness.

Milestones:
M0 baseline:
- Run agent_check, policy, package, docs, metrics/golden where applicable.

M1 exporter design:
- Write dependency, threading, lifecycle, label cardinality, error handling, and package boundary design.

M2 implementation:
- Add optional exporter target.
- Add config and lifecycle hooks outside runtime hot path.
- Keep default build unaffected.

M3 tests:
- Add unit/integration tests for export format, lifecycle, failure behavior, and cardinality guardrails.

M4 docs/package:
- Update adapter/exporter docs, build options, package config, and release notes.

M5 final validation:
- Run agent_check, package, policy, docs, golden, metrics-related tests, and git diff --check.

Acceptance:
- One optional exporter works as documented.
- Runtime core dependency boundary remains clean.
- Exporter failures are bounded and observable.
```

---

## 12.2 G82b — Native Python Binding Track

### 目标

当 CLI-backed automation 不足时，提供 native Python bindings，但必须先定义 ownership/performance/threading 边界。

### 关键边界

```text
- 不替代 C++ core
- 不把 Python 作为 runtime dependency
- 不默认构建
- ownership/error/versioning 明确
```

### Codex `/goal`

```text
/goal G82b-native-python-binding-track

Implement native Python bindings only if G81 selected this direction and CLI-backed automation is proven insufficient.

Primary objective:
Expose a narrow, stable-enough Python binding surface for building, validating, and running TopoExec graphs without turning Python into a runtime dependency.

Scope:
- Define Python binding ownership, error, threading, and versioning rules.
- Bind a minimal graph construction/validation/run surface.
- Keep native bindings optional and default-off.
- Preserve CLI-backed automation preview unless replaced deliberately.

Hard requirements:
- Do not make Python a runtime core dependency.
- Do not expose unstable internal C++ types without a compatibility plan.
- Do not implement broad Python framework features.
- Do not claim Python ecosystem maturity.
- Do not destabilize C++ public API.

Milestones:
M0 baseline:
- Run agent_check, package, policy, python preview checks, docs.

M1 binding design:
- Document ownership, exceptions/errors, GIL/threading, lifetime, versioning, and package boundary.

M2 minimal binding:
- Add optional build target.
- Bind minimal GraphSpec/validation/run result or a safer facade.
- Keep API intentionally small.

M3 tests:
- Add Python tests for import, validate, run minimal graph, error handling, and package smoke.

M4 docs:
- Add Python binding docs and migration from CLI-backed automation.

M5 final validation:
- Run agent_check, package, policy, python checks, docs, and git diff --check.

Acceptance:
- Native Python binding is optional, narrow, tested, and documented.
- It does not pollute runtime core or overpromise ecosystem maturity.
```

---

## 12.3 G82c — Real ROS2 Adapter Track

### 目标

如果目标用户偏 robotics，则实现 real ROS2 adapter。该目标需要最严格的 dependency boundary。

### 关键边界

```text
- ROS2 dependency 只在 adapter target
- 不声明 hard real-time
- 不让 ROS2 lifecycle 改变 core runtime semantics
- fake-boundary preview 与 real adapter 状态要区分
```

### Codex `/goal`

```text
/goal G82c-real-ros2-adapter-track

Implement a real ROS2 adapter only if G81 selected this direction and dependency policy is approved.

Primary objective:
Add a concrete ROS2 adapter target while preserving TopoExec core runtime boundaries and avoiding hard-real-time claims.

Scope:
- Define ROS2 dependency and target boundary.
- Implement minimal publish/subscribe or node integration adapter.
- Keep adapter optional and default-off.
- Add tests that can run without requiring ROS2 where possible, plus ROS2-enabled tests when environment supports it.

Hard requirements:
- Do not add ROS2 dependency to topoexec::runtime.
- Do not claim hard-real-time support.
- Do not change core runtime scheduling semantics for ROS2.
- Do not hide ROS2-disabled CI limitations.
- Do not break fake-boundary preview docs without migration notes.

Milestones:
M0 baseline:
- Run agent_check, package, policy, adapter checks, docs.

M1 design:
- Document ROS2 dependency, lifecycle, threading, QoS, message boundary, and test strategy.

M2 adapter implementation:
- Add optional ROS2 adapter target.
- Implement minimal integration surface.

M3 tests:
- Add dependency-free boundary tests.
- Add ROS2-enabled tests behind environment guards.

M4 docs/package:
- Update adapter docs, build options, package config, examples, and release notes.

M5 final validation:
- Run agent_check, package, policy, docs, adapter checks, and ROS2 checks where available.

Acceptance:
- Real ROS2 adapter is optional, documented, and boundary-safe.
- Core runtime remains ROS2-free.
- No hard-real-time claim is introduced.
```

---

## 12.4 G82d — Editor / LSP Track

### 目标

如果用户主要痛点是 graph authoring 和 diagnostics，则做 LSP/editor。先做 schema/diagnostic bridge，而不是完整 GUI。

### Codex `/goal`

```text
/goal G82d-editor-lsp-track

Implement an editor/LSP workflow only if G81 selected this direction and current JSON Schema plus diagnostic JSON workflow is insufficient.

Primary objective:
Improve graph authoring and diagnostics through a lightweight editor/LSP surface without building a full GUI editor.

Scope:
- Define language server or editor-extension boundary.
- Reuse existing schema and diagnostic JSON.
- Provide validate-on-save or diagnostic mapping.
- Keep runtime independent from editor tooling.

Hard requirements:
- Do not build a full GUI editor.
- Do not change graph schema only for editor convenience.
- Do not add editor dependencies to runtime.
- Do not claim mature IDE ecosystem support.

Milestones:
M0 baseline:
- Run docs, schema, golden, policy, and agent_check.

M1 design:
- Decide LSP vs editor helper.
- Document diagnostic protocol, schema discovery, and file watching.

M2 implementation:
- Add lightweight tooling.
- Reuse CLI/schema diagnostics.

M3 tests:
- Add protocol/diagnostic smoke.
- Add schema discovery checks.

M4 docs:
- Update editor setup guide and examples.

M5 final validation:
- Run docs, schema/golden/policy checks, agent_check, and git diff --check.

Acceptance:
- Graph authoring UX improves without a full GUI.
- Runtime remains independent from editor tooling.
```

---

## 12.5 G82e — Package Registry Publication Track

### 目标

当 release owner 批准后，发布到 vcpkg/Conan 等 registry。该目标必须由人类 owner 明确批准。

### Codex `/goal`

```text
/goal G82e-package-registry-publication-track

Prepare and submit package registry publication only if a human release owner explicitly approves exact registry targets and artifacts.

Primary objective:
Move from draft package-manager recipes to approved registry publication for selected package managers.

Scope:
- Choose exact registry targets.
- Validate package recipes against exact release tag/artifacts.
- Prepare submission metadata and docs.
- Do not publish without owner approval.

Hard requirements:
- Require explicit human approval before publication.
- Do not publish untagged or unverified artifacts.
- Do not overclaim package maturity.
- Do not make registry publication a prerequisite for source install.

Milestones:
M0 approval and baseline:
- Record owner approval and chosen registries.
- Run package, release, policy, docs, and agent_check.

M1 recipe validation:
- Validate recipes against exact tag or release artifact.
- Verify dependencies and options.

M2 submission prep:
- Prepare metadata, version, checksums, license, description, and docs.

M3 local install smoke:
- Validate registry-style install/consume flow locally.

M4 submission:
- Only after explicit approval, submit to chosen registry.

M5 final validation:
- Update docs, status, backlog, release notes, and run relevant gates.

Acceptance:
- Registry publication is tied to exact approved artifacts.
- Source install remains supported.
- Package metadata is correct and reproducible.
```

---

# 13. G83 — Schema v2 / Migration Program

## 13.1 何时打开

只有满足至少一个条件才建议打开：

```text
- 多个真实用户反馈 schema v1 表达力不足
- dogfood pilot 暴露 schema v1 结构限制
- editor/LSP 或 integration track 明确需要 v2
- 已有 reviewed v2 loader/migration design
```

## 13.2 Non-goals

```text
- 不因为“想升级”而升级
- 不破坏 schema v1 用户
- 不缺少 migration CLI 就强推 v2
```

## 13.3 Codex `/goal`

```text
/goal G83-schema-v2-and-migration-program

Open schema v2 implementation only if adoption evidence and a reviewed design justify it.

Primary objective:
Implement schema v2 and migration tooling without breaking schema v1 users.

Scope:
- Finalize schema v2 design.
- Implement v2 loader and validation.
- Implement v1-to-v2 migration CLI.
- Preserve schema v1 compatibility policy.
- Add docs, examples, goldens, and release notes.

Hard requirements:
- Do not remove schema v1 support.
- Do not silently reinterpret v1 graphs.
- Do not implement v2 without migration and compatibility tests.
- Do not change runtime semantics unless explicitly documented and tested.

Milestones:
M0 decision evidence:
- Confirm G81/G83 entry criteria.
- Run schema, golden, docs, agent_check.

M1 design finalization:
- Write schema v2 RFC.
- Document differences from v1 and migration rationale.

M2 implementation:
- Add v2 parser/validator.
- Keep v1 path intact.

M3 migration CLI:
- Add v1-to-v2 migration command.
- Add idempotency and roundtrip checks where possible.

M4 tests:
- Add schema v1 compatibility tests.
- Add schema v2 goldens.
- Add migration goldens.

M5 docs/examples:
- Update schema docs, examples, quickstart, and release notes.

M6 final validation:
- Run agent_check, schema/golden/docs/package/policy checks, and git diff --check.

Acceptance:
- v2 exists with migration tooling.
- v1 remains supported and tested.
- Runtime semantics changes, if any, are explicit.
```

---

# 14. G84 — Core Runtime Beta Candidate

## 14.1 目标

在 G75–G80 证据足够后，准备一个 honest core-runtime beta candidate。注意，这不是 ecosystem beta。

## 14.2 Scope

```text
- beta candidate release gate
- core-runtime-only readiness claim
- deferral acceptance
- API/schema/metrics/trace/live observe contract evidence
- package/install/downstream evidence
- dogfood/reliability evidence
```

## 14.3 Non-goals

```text
- 不声明 adapter ecosystem beta
- 不声明 hard real-time
- 不声明 v1.0
- 不声明 production deployment
```

## 14.4 Required evidence

```bash
git status --short
git diff --check
./scripts/agent_check.sh
cmake --build build --target topoexec_format_check
./scripts/goal_check.sh quick
./scripts/goal_check.sh docs
./scripts/goal_check.sh package
./scripts/goal_check.sh policy
./scripts/goal_check.sh python
./scripts/goal_check.sh plugins
./scripts/goal_check.sh fuzz
./scripts/goal_check.sh stress
./scripts/goal_check.sh bench
./scripts/goal_check.sh live
./scripts/goal_check.sh live-perf
TOPOEXEC_BUILD_DIR=build-asan-ubsan TOPOEXEC_SANITIZER_MODE=address-undefined ./scripts/goal_check.sh sanitizer
./scripts/release_prepare.sh --version <beta-version>
```

## 14.5 Codex `/goal`

```text
/goal G84-core-runtime-beta-candidate

Prepare an honest core-runtime beta candidate for TopoExec after prerelease, dogfood, compatibility, packaging, reliability, and feedback goals have completed.

Primary objective:
Validate whether TopoExec can tag a core-runtime beta without claiming adapter/ecosystem beta, production deployment, or hard-real-time readiness.

Scope:
- Gather fresh evidence for exact candidate commit.
- Validate stable-v0.2/core-runtime API and schema contracts.
- Validate package/install/downstream consumption.
- Validate dogfood/reliability/performance evidence.
- Prepare beta release notes with explicit deferrals.
- Require human release owner acceptance before tagging.

Hard requirements:
- Do not claim adapter/ecosystem beta readiness.
- Do not claim production deployment.
- Do not claim hard-real-time support.
- Do not tag or publish release without human release owner approval.
- Do not hide TSAN/fuzz/soak limitations.

Milestones:
M0 prerequisite check:
- Confirm G75-G80 are complete or explicitly waived by owner.
- Confirm no active blockers.

M1 candidate evidence:
- Run full beta-candidate gate on exact commit.
- Capture command evidence.

M2 release notes:
- Prepare beta release notes.
- Explicitly state stable, mixed, experimental, preview, and deferred surfaces.

M3 docs and metadata:
- Align README, CHANGELOG, release progression, beta readiness review, package metadata, and docs.

M4 human decision:
- Record release owner acceptance or rejection.
- If rejected, keep alpha line and document reasons.

M5 final validation:
- Run final gate again if candidate changed.
- Update status/backlog/release docs.

Acceptance:
- A core-runtime beta candidate can be honestly tagged or explicitly deferred.
- All deferrals remain visible.
- No ecosystem/hard-real-time/production overclaim is introduced.
```

---

# 15. G85 — v1.0 Readiness Program

## 15.1 当前不建议立即打开

v1.0 需要更多真实采用、稳定 API、package maturity、ecosystem decisions 和 compatibility evidence。当前只能作为长期路线。

## 15.2 v1.0 readiness 条件

```text
- 至少一个 prerelease adoption cycle
- core runtime beta feedback closed
- stable schema/API/metrics/trace/live observe policy
- package distribution mature
- critical examples/dogfood stable
- no known MVP-only scheduler limitations that affect target users
- explicit ecosystem posture
```

## 15.3 Codex `/goal`

```text
/goal G85-v1-readiness-program

Open a v1.0 readiness program only after core-runtime beta adoption feedback has been collected and resolved.

Primary objective:
Define and close the remaining gaps required for a responsible TopoExec v1.0 release.

Scope:
- Review beta feedback.
- Define stable API/schema/metrics/trace/live observe contracts.
- Resolve or document known scheduler/runtime limitations.
- Mature package distribution.
- Decide ecosystem posture.
- Prepare v1.0 release criteria.

Hard requirements:
- Do not open this goal before beta feedback exists.
- Do not declare v1.0 readiness without compatibility and adoption evidence.
- Do not stabilize preview surfaces accidentally.
- Do not claim hard-real-time unless explicitly implemented and tested.

Milestones:
M0 beta feedback review:
- Collect beta/adoption issues, package failures, API breakage reports, and dogfood evidence.

M1 v1.0 criteria:
- Define required stable surfaces, tests, docs, packages, and compatibility policy.

M2 gap closure:
- Implement only necessary fixes and compatibility improvements.

M3 release candidate:
- Prepare v1.0-rc release notes and exact gates.

M4 final decision:
- Human owner accepts or defers v1.0.

Acceptance:
- v1.0 criteria are explicit and evidence-backed.
- Preview/deferred surfaces are not accidentally stabilized.
- Release owner can make an informed v1.0 decision.
```

---

# 16. Recommended immediate execution order

## 16.1 Minimal high-confidence path

如果想最快进入外部试用：

```text
1. G75 Release & Adoption Readiness
2. Publish v0.2.0-alpha.0 manually after exact gate
3. G76 Dogfooding Pilot
4. G77 Compatibility Harness
5. G78 Packaging Hardening
```

## 16.2 Beta-preparation path

如果目标是未来 core runtime beta：

```text
1. G75
2. G76
3. G77
4. G78
5. G79
6. G80
7. G84
```

## 16.3 Ecosystem path

如果已有明确目标用户：

```text
1. G75
2. G76
3. G80
4. G81
5. exactly one G82x
6. then G84 or another alpha release
```

---

# 17. Codex 投喂建议

## 17.1 不要一次投喂整份文档让 Codex 全部实现

建议每次只投喂一个 `/goal`：

```text
先投 G75。
完成、验证、review 后再投 G76。
```

## 17.2 每个 goal 开始前要求 Codex 做 M0

Codex 必须先跑 baseline，特别是：

```bash
git status --short
git diff --check
./scripts/agent_check.sh
```

再根据 goal 增加 focused gates。

## 17.3 每个 goal 结束必须更新 ledgers

至少：

```text
docs/31-planning-roadmap/goals/status.md
docs/31-planning-roadmap/goals/backlog.md
CHANGELOG.md
相关 docs
```

## 17.4 遇到 product/API blocker 时不要瞎改

要求 Codex 写：

```text
docs/31-planning-roadmap/goals/blockers/gXX-*.md
```

包含：

```text
- decision needed
- options
- recommendation
- API/runtime/test/doc impact
- safe independent work
```

## 17.5 不让 Codex 自动发布 release

Codex 可以准备 release notes、runbook、artifact dry-run，但 tag/release publish/package registry publish 必须由 human owner 决定。

---

# 18. 建议立刻执行的第一个 Codex goal

如果只能选择一个，先执行：

```text
G75-release-and-adoption-readiness
```

原因：

```text
- 当前 release 页面没有 release
- 当前 README/LICENSE 有明显不一致
- 当前项目已经具备足够 examples/live/docs/runtime 基础
- 外部用户需要一个正式 prerelease 入口
- 后续 dogfood、beta、package、ecosystem 都依赖 release/adoption 基线
```

---

# 19. 总结

TopoExec 下一步的关键不是继续“做更多”，而是让项目从内部完成度转向外部可信度：

```text
G75：能发布，能试用
G76：能用准真实场景说明价值
G77：核心 API/schema/CLI contract 可守住
G78：能被别人安装和嵌入
G79：可靠性和性能证据更强
G80：能接收外部反馈
G81：基于反馈选择生态方向
G82x：只做一个明确生态方向
G84：准备 core runtime beta
G85：长期 v1.0
```

建议当前立即推进 G75。
