# TopoExec G74 规划文档：丰富示例、README 可视化展示与项目吸引力提升

> 目标：在项目尚未进入真实生产环境之前，通过**高质量、可执行、可验证的示例**，以及**README 首页的可视化展示与快速上手体验优化**，显著提升 TopoExec 的可理解性、可传播性与试用意愿。

---

## 1. 背景与目标

TopoExec 当前已经具备较完整的 runtime、CLI、metrics、trace、validation、plan/render 等基础能力，但对于一个还没有真实生产案例背书的框架来说，潜在用户在评估时最关心的通常不是“功能列表”，而是：

1. **我能不能快速看懂它解决什么问题？**
2. **我能不能通过几个例子迅速上手？**
3. **它和一般 task graph / pipeline / workflow 框架有什么不同？**
4. **它是否具备足够的可观测性、可调试性与工程成熟度？**
5. **README 看起来是否专业、直观、有说服力？**

因此，下一阶段建议单独立项为：

# **G74 — Examples, Showcase, and README Visual Refresh**

该阶段不以新增 runtime 核心语义为主，而以**“用示例讲清楚框架价值”**为主。

---

## 2. G74 的核心目标

### 2.1 主目标

构建一套面向外部用户的**示例矩阵 + README 可视化展示体系**，让第一次进入仓库的用户可以在 3–10 分钟内理解：

- TopoExec 是什么
- TopoExec 擅长什么
- TopoExec 与其他工具的差异点是什么
- 如何最快跑起来一个例子
- 如何通过可视化图示、trace、metrics 感知 runtime 行为

### 2.2 成果目标

完成后，仓库首页和示例目录应达到下面效果：

- README 首页有**清晰的 hero 介绍、核心卖点、可视化图片、快速开始**
- 至少有 **8–12 个高质量示例**，覆盖主要语义和典型使用模式
- 每个示例都有：
  - 目的说明
  - 配置文件
  - 运行命令
  - 预期输出
  - graph 图
  - 可选 metrics/trace 截图或说明
- 所有 README 图片和示意图都能够**自动或半自动生成/校验**，避免文档腐烂
- 不夸大“生产可用”结论，明确项目阶段，同时最大化展示工程质量与开发体验

---

## 3. 非目标（Non-goals）

本阶段**不做**以下工作，避免 scope 膨胀：

1. 不以新增复杂 runtime 语义为主线
2. 不开发完整 GUI editor
3. 不开发生产级 dashboard 平台
4. 不在 README 中制造“已大规模生产应用”的误导叙述
5. 不引入重型前端栈仅为生成 README 截图
6. 不为了视觉展示而破坏现有 CLI / runtime / docs 的边界
7. 不把 examples 变成未经维护的“展示废墟”

---

## 4. G74 的设计原则

### 4.1 示例优先于口号

README 里不要堆太多抽象卖点，而要用最少文字配合最直观的图和例子。

### 4.2 示例必须可运行、可验证、可回归

所有公开展示的示例都应：

- 能被 CLI 直接运行
- 能在 CI 中 smoke test
- 输出可预测
- 文档与示例目录保持同步

### 4.3 可视化资源尽可能自动生成

graph 图、render 图、部分 JSON 摘要、benchmark 表格等，应尽量从示例和 CLI 输出自动生成，而不是手工维护。

### 4.4 README 要诚实但有吸引力

建议采用以下表达策略：

- 明确项目仍在 beta / pre-production / early adoption 阶段
- 重点强调：
  - deterministic execution model
  - validation / plan / trace / metrics
  - developer ergonomics
  - composable graph semantics
  - testing / observability
- 不暗示虚假的生产规模应用

### 4.5 “展示路径”要比“文档体系”更短

README 首页不要把用户直接丢进大量架构文档。

理想路径：

```text
README 首页
  -> Quick Start
  -> 2~3 个最有代表性的 examples
  -> examples index
  -> deeper docs
```

---

## 5. 信息架构建议

建议把“对外展示面”组织为三层：

### Layer 1：README 首页（吸引人）

服务对象：第一次访问仓库的人

关注点：

- 一句话知道项目是做什么的
- 看到 1 张图，感受 framework 长什么样
- 看到 1 个最小示例，知道怎么跑
- 看到核心能力列表
- 看到 examples 索引入口

### Layer 2：Examples Index（快速试用）

服务对象：准备试用的人

关注点：

- 示例分类清晰
- 难度从浅到深
- 每个示例知道“它想展示什么”
- 每个示例 1~2 条命令即可跑起来

### Layer 3：Docs / Architecture（深入理解）

服务对象：准备深入使用或贡献的人

关注点：

- 语义细节
- 架构与 invariants
- CLI 参考
- schema / metrics / trace 细节

---

## 6. 推荐新增的示例矩阵

建议建立 `examples/` 下的“分层示例体系”，而不是零散堆文件。

建议目录：

```text
examples/
  00-getting-started/
  10-basic-dataflow/
  20-triggers/
  30-async/
  40-composite-loop/
  50-observability/
  60-testing-validation/
  70-performance/
  README.md
```

下面是建议的核心示例集合。

---

### 6.1 示例 A：Hello TopoExec / Minimal pipeline

**目标**：最低成本展示 graph 是什么，如何 validate / render / run。

内容：

- source -> transform -> sink
- immediate edge
- 极简 YAML
- 演示命令：
  - validate
  - plan
  - render
  - run

要展示的卖点：

- graph 声明式定义
- CLI 直接可用
- 拓扑可视化
- 低门槛上手

README 首页优先引用该示例。

---

### 6.2 示例 B：Branching / Fan-out / Join

**目标**：展示分支与合流。

内容：

- source -> preprocess
- preprocess -> branch_a
- preprocess -> branch_b
- branch_a + branch_b -> merge -> sink

要展示的卖点：

- 非线性拓扑
- merge 行为
- 更接近真实 pipeline

配图建议：graph render 图。

---

### 6.3 示例 C：Trigger semantics showcase

**目标**：集中展示 trigger 语义。

内容：

- `all_inputs`
- `any_input`
- `rate_limit`
- `debounce`
- `condition`
- `batch`
- 可选择只做其中 2–3 个最有代表性的子示例

要展示的卖点：

- TopoExec 不只是 DAG 执行，而是有丰富的触发控制能力
- 可以从事件/数据到执行行为建立清晰关系

建议文档说明：

- 什么时候 ready
- 什么时候 suppressed
- 典型使用场景

---

### 6.4 示例 D：Async worker / Bounded inflight

**目标**：展示异步组件与并发控制。

内容：

- source -> async worker -> sink
- 配置 `max_inflight`
- 人为制造不同耗时

要展示的卖点：

- 异步执行路径
- inflight 限制
- runtime 可观测性

可展示资源：

- metrics 摘要
- timeline 图（如果已有 G73/G74 配套可视化能力）

---

### 6.5 示例 E：CompositeLoop / Iterative solver

**目标**：展示闭环、迭代与收敛语义。

内容：

- loop input -> solver step -> residual check -> converge or continue

要展示的卖点：

- 不是简单一次性 pipeline
- 支持受控迭代、收敛和停止条件
- 适合优化、迭代推理、状态修正类任务

该示例非常有展示价值，建议作为 README 中“高级能力”部分的核心图片来源。

---

### 6.6 示例 F：Observability / metrics / trace

**目标**：展示 TopoExec 的工程化可观测性。

内容：

- 一个小图
- 运行后输出：
  - metrics JSON
  - trace JSON
  - chrome trace / perfetto 兼容结果
  - health / diagnostics 摘要

要展示的卖点：

- 不是黑盒执行
- 能看 plan、trace、metrics
- 开发调试友好

README 中适合放一张“trace 视图截图”或“metrics 摘要图”。

---

### 6.7 示例 G：Validation / diagnostics

**目标**：展示错误检查能力。

内容：

- 故意构造一个 graph 配置错误
- 运行 `validate`
- 展示诊断输出

要展示的卖点：

- 不是“运行时报错才发现问题”
- validation 能帮助开发者尽早发现配置问题

README 中可以用一个小节展示“clear diagnostics”。

---

### 6.8 示例 H：Testing / golden / assertion-friendly usage

**目标**：展示 TopoExec 的测试友好性。

内容：

- 一个可预测 graph
- 演示如何产出稳定 JSON 输出
- 演示如何做 golden / focused test / invariant 检查

要展示的卖点：

- 适合作为工程组件被持续测试
- 适合集成到 CI

---

### 6.9 示例 I：Performance micro-benchmark

**目标**：展示项目不是只会“画图”，还可以做 benchmark。

内容：

- 使用 `graph bench`
- 选择最简单 benchmark case
- 输出简洁表格或摘要

要展示的卖点：

- 框架有性能意识
- 具备度量与回归能力

注意：不要在 README 中给过度夸张的性能结论。

---

### 6.10 示例 J：Realistic mini scenario

**目标**：提供一个比 toy example 更接近真实场景的示例。

可选主题：

- sensor fusion mini pipeline
- robot cell event/data pipeline
- ETL-like mini workflow
- multi-stage inference pre/post process pipeline

建议选择一个最贴近你未来想推广的使用场景。

这是“吸引人”的关键示例之一。

---

## 7. README 首页重构建议

建议 README 首页采用如下结构。

---

### 7.1 顶部 Hero 区

建议包含：

- 项目名称
- 一句话定位
- 3–5 个关键词
- 1 张核心图

示例文案风格（示意，不是最终定稿）：

```text
TopoExec is a C++20 graph runtime for deterministic, observable, and testable dataflow execution.
```

关键词：

```text
Deterministic · Observable · Testable · Composable · Developer-friendly
```

主图建议：

- 一个简洁 graph 可视化图
- 或 graph + metrics/trace 小型拼图

不要一开始放太复杂的架构图。

---

### 7.2 Why TopoExec?

建议用 4–6 个 bullet，而不是长段落。

例如：

- Declarative graph specification and validation
- Deterministic execution semantics
- Rich trigger and loop semantics
- Built-in metrics / trace / diagnostics
- CLI-first workflow for development and testing
- Focused on local developer experience and observability

---

### 7.3 Quick Start

README 首页必须有最短路径。

建议形式：

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
./build/topoexec graph validate examples/00-getting-started/minimal.yaml
./build/topoexec graph render examples/00-getting-started/minimal.yaml --format mermaid
./build/topoexec graph run examples/00-getting-started/minimal.yaml
```

并紧接一个小图或小输出示例。

---

### 7.4 Visual examples / Showcase

建议在 README 首页展示 3 个最有代表性的可视化卡片：

1. Minimal pipeline
2. Trigger / async / loop 中最能体现差异化的一项
3. Observability（trace/metrics）

每个卡片包含：

- 一张图片
- 一句话解释
- 链接到示例目录

---

### 7.5 Core capabilities

以表格展示能力：

| Capability | What it means |
|---|---|
| Validation | Catch graph/config problems before runtime |
| Plan & render | Understand topology and execution plan |
| Triggers | Express readiness and suppression policies |
| Async | Controlled concurrent execution |
| Composite loops | Iterative convergence workflows |
| Metrics & trace | Inspect runtime behavior and regressions |
| Testing support | Stable outputs for CI and golden tests |

---

### 7.6 Examples index

README 首页给出简短索引并跳转到：

- `examples/README.md`
- 关键示例文件夹

---

### 7.7 Project status / honesty section

建议明确写：

- 当前处于 beta / pre-production
- 尚未大规模生产验证
- 但已具备较完整的 validation / observability / testing / docs / CI 基础

这会提升可信度，而不是降低吸引力。

---

## 8. README 可视化资源规划

### 8.1 建议展示的图片类型

建议控制为 4 类，不要太杂。

#### 类型 1：Graph topology 图

来源：

- `topoexec graph render ...`
- 再转换成 SVG / PNG

用途：

- README Hero
- 各示例页面

#### 类型 2：Execution / timeline 图

来源：

- trace 数据
- 若 G73 完成，可由 runtime observe/report 生成静态截图
- 若尚未完成，可先使用精简的 trace 摘要图

用途：

- 展示可观测性
- 展示 async / loop 行为

#### 类型 3：Metrics summary 图

来源：

- metrics JSON 经过脚本转换为简洁表格或小型柱状图

用途：

- 展示工程感
- 展示 channel / component summary

#### 类型 4：Diagnostics / validation 图

来源：

- CLI 终端截图或格式化后的文本框图

用途：

- 展示“错误信息清晰”

---

### 8.2 图片风格建议

建议统一：

- 简洁、浅背景/透明背景优先
- 使用 SVG 优先，其次 PNG
- 不用复杂花哨配色
- 文案尽量少
- 与 README 深浅模式兼容

---

### 8.3 资源目录建议

```text
docs/
  assets/
    generated/
      readme/
      examples/
    static/
      brand/
      manually-curated/
examples/
  assets/
    ...
scripts/
  update_readme_assets.sh
  render_example_assets.py
```

其中：

- `generated/`：可自动重建
- `static/`：手工维护的品牌图、少量人工优化图

---

## 9. 自动生成与防腐烂策略

这部分非常关键。

README 图片如果靠手工维护，几轮迭代后一定失效。

### 9.1 目标

尽可能让以下内容自动生成：

- graph 图
- examples index 列表
- 示例运行摘要
- metrics/trace 摘要资源
- README 中引用的部分图片

### 9.2 建议脚本

#### `scripts/render_example_assets.py`

职责：

- 扫描受支持的 examples
- 执行 validate / render / run / metrics / trace
- 生成：
  - graph.svg / graph.png
  - summary.json
  - 可选 trace/metrics 图片

#### `scripts/update_examples_index.py`

职责：

- 从 examples 元数据生成 `examples/README.md`
- 生成示例目录索引表

#### `scripts/check_readme_assets.sh`

职责：

- 检查 README 引用资源是否存在
- 检查 auto-generated 资源是否过期
- CI 中 smoke 运行

---

## 10. 示例元数据机制建议

建议每个示例目录下有一个轻量元数据文件，例如：

```yaml
name: minimal-pipeline
category: getting-started
summary: Small source-transform-sink pipeline
show_in_readme: true
readme_priority: high
commands:
  - graph validate examples/00-getting-started/minimal.yaml
  - graph run examples/00-getting-started/minimal.yaml
assets:
  generate_graph: true
  generate_metrics_summary: false
  generate_trace_summary: false
ci:
  smoke: true
  golden: true
```

好处：

- 可自动生成 examples index
- 可自动决定哪些示例上 README
- 可自动生成图与摘要

---

## 11. 示例文档模板建议

建议每个示例目录包含：

```text
README.md
<graph>.yaml
(optional) expected output / goldens
(optional) helper script
(optional) generated assets
```

每个示例 README 模板统一：

1. What this example demonstrates
2. Graph structure
3. How to run
4. Expected result
5. What to inspect
6. Related docs

这能显著提升一致性。

---

## 12. README 视觉展示内容建议

### 12.1 Hero 主图建议

优先使用：

- 一张简洁的 graph 图
- 或三联图：graph / trace / metrics 小拼图

不建议上来就是复杂架构图，因为新用户很难快速吸收。

### 12.2 示例展示图建议

推荐展示以下 3 张：

1. **Minimal pipeline graph**
2. **CompositeLoop / async graph**
3. **Trace / metrics 观测图**

如果只能放 2 张，则优先：

- minimal graph
- observability 图

### 12.3 README 终端片段展示

建议精选 1~2 个 terminal block：

- validate 成功或 diagnostics 示例
- run / metrics / trace 简短输出

但不要堆太长日志。

---

## 13. 可以直接做的 README 结构重构方案

建议改造为如下大纲：

```text
# TopoExec

Hero image
Short tagline
Badges

## Why TopoExec?

## Quick Start

## Visual Showcase
- Minimal pipeline
- Trigger/loop/async showcase
- Observability showcase

## Core Capabilities

## Example Gallery
- link to examples/README.md
- selected examples

## Documentation

## Project Status

## Contributing

## License
```

---

## 14. 建议新增的 examples/README.md

建议 `examples/README.md` 成为用户第二站。

结构建议：

```text
# TopoExec Examples

## Getting Started
- minimal pipeline
- branching and merge

## Execution Semantics
- triggers
- async workers
- composite loops

## Observability
- metrics and trace
- validation and diagnostics

## Testing and Benchmarks
- golden-friendly example
- benchmark example

## Recommended learning path
1. minimal
2. branching
3. triggers
4. async
5. loop
6. observability
```

---

## 15. CI / Gate 建议

G74 不是只改文档，也要保证 examples 和图片不会腐烂。

建议新增 focused gates：

### 15.1 `./scripts/goal_check.sh examples`

检查：

- 关键示例 validate 成功
- 关键示例 run 成功
- examples index 最新
- 示例 README 结构完整

### 15.2 `./scripts/goal_check.sh showcase`

检查：

- README 引用图片存在
- 自动生成资产可重建
- examples metadata 合法
- 受支持示例的 graph 图可生成

### 15.3 可选 `./scripts/goal_check.sh readme-smoke`

检查：

- README 命令片段可执行或至少不明显失真

---

## 16. 具体文件规划建议

建议新增/更新这些文件：

```text
README.md
examples/README.md
examples/00-getting-started/...
examples/10-basic-dataflow/...
examples/20-triggers/...
examples/30-async/...
examples/40-composite-loop/...
examples/50-observability/...
examples/60-testing-validation/...
examples/70-performance/...

docs/assets/generated/readme/...
docs/assets/generated/examples/...
docs/assets/static/...

scripts/render_example_assets.py
scripts/update_examples_index.py
scripts/check_readme_assets.sh
scripts/goal_check.sh   # extend with examples/showcase

docs/41-development-tools/examples-and-showcase.md
```

---

## 17. 实施里程碑建议

### M0：Baseline

- 跑现有 build / tests / agent_check
- 盘点现有示例、现有 README、现有图片资源
- 标记哪些资源可复用，哪些需要淘汰

### M1：Examples audit & taxonomy

- 盘点当前 `examples/`
- 重构目录结构
- 为每个示例补元数据
- 删掉重复、低质量、过时示例

### M2：Build the example matrix

- 新增 8–12 个核心示例
- 为每个示例补 README
- 确保 validate / run / render 可成功

### M3：Asset generation pipeline

- 实现 graph 资产生成脚本
- 实现 examples index 生成脚本
- 初步接通 CI smoke

### M4：README redesign

- 重构 README 信息架构
- 增加 Hero、Quick Start、Showcase、Examples Gallery
- 引入 2–4 张高质量图

### M5：Observability showcase

- 选择一个示例输出 metrics/trace 可视化资源
- 将其接入 README 与 examples 文档

### M6：CI hardening

- 新增 `goal_check.sh examples`
- 新增 `goal_check.sh showcase`
- 确保 README / examples / assets 不腐烂

### M7：Final polish

- 文案统一
- 图片压缩与暗黑模式兼容检查
- 链接检查
- examples 学习路径优化

---

## 18. 验收标准

G74 完成时，至少应满足：

### 18.1 README 层面

- README 首页在 1 屏内能说明项目定位
- README 有 1 个 Quick Start
- README 有 2–4 张高质量可视化资源
- README 有 examples 入口和项目状态说明

### 18.2 Examples 层面

- 至少 8 个高质量示例
- 覆盖基础 graph、triggers、async、loop、observability、validation、testing、benchmark
- 每个示例有统一格式的 README
- 每个示例可通过至少 smoke 级验证

### 18.3 自动化层面

- 关键图资源可自动生成或可自动校验
- examples index 自动生成或自动校验
- CI 中有 examples/showcase focused gate

### 18.4 传播效果层面

- 新用户看到 README 后，可在 3–10 分钟内跑通第一个例子
- 能快速感受到 TopoExec 的差异化能力：validation、trace/metrics、trigger/loop 语义、工程化工具链

---

## 19. 风险与缓解

### 风险 1：示例过多但质量参差不齐

缓解：

- 宁可 8 个高质量示例，不要 20 个低质量示例
- 采用统一模板和 metadata 管理

### 风险 2：README 过于花哨但不真实

缓解：

- 明确项目状态
- 不夸大“生产级”背书
- 图必须来自真实示例输出

### 风险 3：可视化资源腐烂

缓解：

- 自动生成/自动校验
- CI focused gate
- generated/static 资源分层

### 风险 4：示例与语义文档不一致

缓解：

- 每个示例必须链接相关 docs
- example 作为 docs 的入口，而非语义定义来源

### 风险 5：为了展示而侵入 runtime 设计

缓解：

- README 图优先复用现有 render/trace/metrics 能力
- 避免为文档展示引入 runtime 热路径负担

---

## 20. 推荐优先级排序

如果资源有限，建议按下面顺序完成：

### P0（必须做）

1. README 首页重构
2. minimal / branching / observability 三个示例
3. examples/README.md
4. graph 图自动生成
5. examples/showcase gate

### P1（应该做）

6. triggers 示例
7. async 示例
8. CompositeLoop 示例
9. validation / diagnostics 示例
10. benchmark 示例

### P2（可选增强）

11. realistic mini scenario
12. trace / metrics 更漂亮的静态图
13. 更完整 examples metadata 自动化

---

## 21. 建议的对外叙事角度

对于还没有生产案例的项目，README 最好的叙事方式通常不是“已经被谁用了”，而是：

### 角度 1：工程能力完整

强调：

- validation
- diagnostics
- trace
- metrics
- deterministic execution
- testing support

### 角度 2：适合开发与实验阶段快速验证

强调：

- CLI-first
- examples 丰富
- 可视化图示
- 容易理解 runtime 行为

### 角度 3：语义比普通 task runner 更丰富

强调：

- triggers
- async
- loops
- observability

---

## 22. 可直接交给 Codex 的 `/goal`

下面给出一份建议的 Codex `/goal` 文本。

```text
/goal G74-examples-showcase-and-readme-refresh

Implement a focused examples, showcase, and README refresh initiative for TopoExec.

Primary objective:
Improve project attractiveness, learnability, and trialability before production adoption by adding high-quality executable examples and visual README assets.

Hard boundaries:
- Do not exaggerate production readiness or claim real production deployment.
- Do not implement a full GUI editor or heavy frontend stack in this goal.
- Do not add runtime behavior solely for cosmetic presentation unless it has clear developer/documentation value.
- Do not leave examples untested or undocumented.
- Do not introduce README assets that cannot be regenerated or validated.

Main deliverables:
1. A refreshed top-level README.md.
2. A curated examples/README.md index.
3. A structured examples hierarchy with at least 8 high-quality examples.
4. Auto-generated or validated visual assets for README and examples.
5. Focused CI checks for examples and showcase assets.

Required example categories:
- minimal getting-started pipeline
- branching / fan-out / merge
- trigger semantics showcase
- async worker / bounded inflight
- CompositeLoop / iterative convergence
- observability (metrics / trace / diagnostics)
- validation / diagnostics failure example
- testing / golden-friendly example
- benchmark example
- optionally one realistic mini scenario

README requirements:
- Add a concise hero section with one-sentence positioning.
- Add a short “Why TopoExec?” section.
- Add a real quick-start block with build + validate + render + run commands.
- Add a visual showcase section with 2–4 representative images.
- Add a core capabilities table.
- Add a clear examples index entry point.
- Add an honest project status section (beta / pre-production, not production-proven yet).

Visual asset requirements:
- Prefer assets generated from real examples.
- Include graph topology visuals, and optionally trace/metrics summaries.
- Keep asset style clean and README-friendly.
- Use docs/assets/generated for generated artifacts and docs/assets/static for hand-maintained assets.
- Add scripts to generate and/or validate showcase assets.

Automation requirements:
- Add example metadata or an equivalent mechanism to drive example indexing and asset generation.
- Add scripts/render_example_assets.py (or equivalent).
- Add scripts/update_examples_index.py (or equivalent).
- Add scripts/check_readme_assets.sh (or equivalent).
- Extend scripts/goal_check.sh with `examples` and `showcase` focused gates.

Suggested directory shape:
- examples/00-getting-started/
- examples/10-basic-dataflow/
- examples/20-triggers/
- examples/30-async/
- examples/40-composite-loop/
- examples/50-observability/
- examples/60-testing-validation/
- examples/70-performance/

Acceptance criteria:
- README quickly communicates project value within one screen.
- A new user can run a first example in 3–10 minutes.
- At least 8 curated examples exist with consistent README structure.
- Key README/example assets are generated or validated in CI.
- `./scripts/goal_check.sh examples` passes.
- `./scripts/goal_check.sh showcase` passes.
- Documentation links and example links are not broken.

Milestones:
M0 baseline and inventory
M1 example taxonomy and cleanup
M2 implement curated example set
M3 add asset generation pipeline
M4 redesign README
M5 add observability showcase
M6 add CI gates and anti-rot checks
M7 final polish and consistency review
```

---

## 23. 最终建议

如果目标是“让更多人愿意点进来并试用 TopoExec”，那么最值得优先做的不是继续堆底层能力，而是：

1. **用示例把能力讲清楚**
2. **用图把框架气质展示出来**
3. **让 README 成为一个真正可转化的入口页**
4. **让所有展示内容可运行、可回归、可维护**

换句话说，G74 的价值不只是“文档更好看”，而是把 TopoExec 从“看起来像一个技术项目”提升为“看起来像一个值得试用的框架”。
