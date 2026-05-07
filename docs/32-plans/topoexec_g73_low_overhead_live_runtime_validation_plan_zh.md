# TopoExec G73 规划文档：低扰动实时 Runtime 验证工作台

**目标代号**：`G73-low-overhead-live-runtime-validation`
**日期**：2026-05-07
**适用仓库**：`sean2077/topoexec` `main` 分支公开状态
**核心产物**：`graph observe`、低扰动 runtime observer、实时断言、SSE dashboard、record/replay artifact、性能回归门禁
**主要原则**：实时可视化必须尽可能降低对 runtime 性能画像与调度语义的影响。

---

## 0. 结论

下一阶段建议不要把目标命名为“实时可视化工具”，而应命名为：

```text
G73 — Low-Overhead Live Runtime Validation
```

原因是本阶段真正需要交付的不是 GUI，而是一个低扰动、可回放、可验证的本地实时开发测试工作台：

```text
Runtime hot path
  -> fixed-size lightweight events
  -> bounded non-blocking ring buffers
  -> collector / assertion engine / recorder
  -> local SSE dashboard
  -> offline replay and CI evidence
```

第一版应坚持以下边界：

```text
- observe 默认关闭；
- runtime 热路径不做 JSON 序列化；
- runtime 热路径不做 socket/file I/O；
- runtime 热路径不拷贝 payload body；
- observer queue 满了只 drop/coalesce，不阻塞 scheduler；
- assertion 在 collector/tooling 层执行，不进入 scheduler/channel/trigger 内核；
- dashboard 是观察端，不是 runtime 控制输入；
- 高频正常事件默认聚合或采样，异常事件逐条显示；
- 所有性能影响必须 benchmark，并进入 focused gate。
```

---

## 1. 当前仓库约束与设计依据

### 1.1 项目定位

TopoExec 当前公开 README 将项目定义为一个 compact C++20 runtime，用于单进程内 stateful execution graph，提供 edge visibility、feedback loop、bounded channel、trigger readiness、payload ownership、metrics 和 trace events 等显式语义。README 同时明确它不是 distributed runtime、ROS adapter、Python framework、GUI editor、OpenTelemetry/Prometheus exporter 或 sandboxed plugin system。
来源：<https://github.com/sean2077/topoexec>、<https://raw.githubusercontent.com/sean2077/topoexec/main/README.md>

因此 G73 不能把项目推向完整 GUI editor、生产监控平台或分布式 tracing 系统。它应保持为本地开发测试工具。

### 1.2 当前 goal 状态

`docs/31-planning-roadmap/goals/status.md` 当前记录：

```text
Active implementation goal: None.
G71-post-alpha-hardening-docs-pages is complete.
Required repository gate: scripts/agent_check.sh
Focused docs gate: scripts/goal_check.sh docs
```

G71 已覆盖 runtime semantic hardening、benchmark、Doxygen、GitHub Pages workflow、release docs 等。
来源：<https://raw.githubusercontent.com/sean2077/topoexec/main/docs/31-planning-roadmap/goals/status.md>

因此 G73 可以作为新的 active implementation goal 打开。

### 1.3 当前 CLI 可复用能力

当前 CLI 已包含：

```bash
topoexec graph validate ...
topoexec graph plan ... --format json
topoexec graph render ... --format mermaid
topoexec graph run ...
topoexec graph metrics ... --format json
topoexec graph trace ... --format json
topoexec graph trace ... --format chrome
topoexec graph explain ...
topoexec graph diff-plan ...
topoexec graph bench ... --format json
```

CLI 文档明确 JSON 输出用于 scripts 和 golden tests，Chrome trace 输出兼容 Perfetto/Chrome trace viewers。
来源：<https://raw.githubusercontent.com/sean2077/topoexec/main/docs/41-development-tools/cli.md>

因此 G73 应复用这些输出面，并新增 `graph observe` 作为运行中实时事件流，而不是重写一个独立执行器。

### 1.4 当前 runtime 架构

runtime 文档描述的主流程是：

```text
GraphSpec
  -> validation
  -> compiler / CompiledGraphRegion
  -> RuntimeRunner
  -> EventRuntime
  -> TriggerPolicyEngine
  -> RuntimeChannelBus / RuntimePublicationRouter
  -> metrics / trace / health / errors
```

文档还明确 metrics、trace events、health events、diagnostics、structured runtime errors 是 runtime 输出，不是控制输入。
来源：<https://raw.githubusercontent.com/sean2077/topoexec/main/docs/21-architecture/runtime-architecture.md>

G73 的实时观察层必须保持这个原则：observer 只能观察，不能改变调度、触发、channel delivery、CompositeLoop convergence 或 runtime ok/fail 语义。

### 1.5 当前 observer 与 invariant 基础

`RuntimeRunnerOptions` 已有 `std::vector<std::shared_ptr<RuntimeObserver>> observers`，`RuntimeObserver` 支持 result、metric、trace、health event、runtime error 回调，`InMemoryRuntimeObserver` 已有 bounded capacity 和 dropped event count。
来源：<https://raw.githubusercontent.com/sean2077/topoexec/main/include/topoexec/runtime/runtime_runner.hpp>

runtime invariant 文档已有：

```text
Runtime observer failures and bounded observer drops are observable and non-fatal to runtime semantics.
```

覆盖测试包括：

```text
Runtime.ObserverFailureIsRecordedButDoesNotChangeRuntimeSemantics
Runtime.InMemoryObserverDropsBoundedRecordsAndReportsDrops
AdapterSdk.ObserverFailureIsNonFatalRuntimeEvidence
```

来源：<https://raw.githubusercontent.com/sean2077/topoexec/main/docs/21-architecture/runtime-invariants.md>

G73 应沿用这个非致命 observer 模型，但不能直接把现有 `RuntimeObserver` 扩展成高频热路径 JSON/metric/trace 回调。应新增更轻量的 live event path。

### 1.6 当前 metrics / trace contract

metrics 文档要求默认 runtime metrics 只使用 bounded labels：`lane`、`component_id`、`channel_id`，避免 correlation、causation、transaction、trace、request 等高基数字段成为 metric label。
来源：<https://raw.githubusercontent.com/sean2077/topoexec/main/docs/62-schemas-protocols/metrics.md>

trace 文档已有 `trace_schema_version = "1"`，事件包含 `component_id`、`channel_id`、`lane`、`worker_id`、`epoch_id`、`transaction_id`、`correlation_id`、`causation_id`、`start_offset_ns`、`duration_ns` 和 bounded attributes。trace 按 `start_offset_ns` 排序，可重建 timeline。
来源：<https://raw.githubusercontent.com/sean2077/topoexec/main/docs/62-schemas-protocols/trace-events.md>

G73 应新增独立 `observe_schema_version = "1"`，不要污染 metrics schema v1 或 trace schema v1。observe 可以引用 trace/metric 概念，但必须声明哪些事件是 exact、aggregated、sampled 或 lossy。

### 1.7 当前测试体系

默认 gate 是：

```bash
./scripts/agent_check.sh
```

focused gates 包括 golden、schema、docs、fuzz、stress、bench、package、adapters、release、sanitizer、format、tidy 等。
来源：<https://raw.githubusercontent.com/sean2077/topoexec/main/docs/24-testing/testing-strategy.md>

G73 应新增 focused gate：

```bash
./scripts/goal_check.sh live
./scripts/goal_check.sh live-perf
```

`live` 验证功能与 schema，`live-perf` 验证低扰动性能预算。

---

## 2. 非目标

G73 明确不做：

```text
- 完整 graph editor；
- Electron/Tauri/Qt 桌面应用；
- React/Vite/Node 默认依赖；
- VS Code extension 或 LSP server；
- production OpenTelemetry/Prometheus exporter；
- WebSocket 双向控制协议；
- pause/resume/step/fault injection；
- payload 全量抓取；
- 多机/分布式 tracing；
- schema v2 assertion 内嵌；
- native Python binding；
- hard real-time guarantees；
- adapter productionization。
```

这些都可能将工具目标扩大成 IDE、生产可观测平台或 runtime 控制系统，不符合当前仓库边界。

---

## 3. 设计原则

### 3.1 低扰动优先

G73 的第一原则：

```text
Live visualization must not materially perturb runtime execution.
```

更具体地说：

```text
- observe disabled 时 runtime 成本应接近零；
- observe enabled 时 runtime 热路径只做固定上限、非阻塞、allocation-light 的事件投递；
- UI、SSE、NDJSON、文件写入、JSON 序列化和 assertion evaluation 都必须在 runtime 外层执行；
- 观察失败、队列满、dashboard 掉线不得改变 runtime 语义。
```

### 3.2 语义不可变

Live observation 不能影响：

```text
- component execution order；
- trigger readiness；
- channel delivery semantics；
- async max_inflight admission/release；
- CompositeLoop iteration/convergence/output visibility；
- scheduler stop reason；
- runtime ok/fail result；
- metrics/trace result contract。
```

只有 live assertion failure 可以影响 `graph observe` 命令退出码，但它不能反向改变 runtime execution。

### 3.3 异常优先，正常路径聚合

高频正常事件不应逐条推 UI。默认 live dashboard 应展示 summary：

```text
- component execution count / latency；
- channel publish/delivery/drop/overwrite/depth；
- trigger ready/suppressed/drop；
- loop iteration/residual/convergence；
- health/errors；
- assertion status。
```

逐条显示的优先级：

```text
- runtime_error；
- component_error；
- channel_drop / reject / deadline_miss / stale_drop；
- async_reject / async_drop；
- loop_error / budget_overrun / max_iterations_hit；
- assertion_fail；
- observer_drop_summary；
- health_event。
```

### 3.4 先本地，后扩展

第一版只绑定 `127.0.0.1`，服务本地开发和 CI artifact replay。不要在 MVP 阶段设计远程访问、认证体系、多用户会话或生产部署。

### 3.5 可回放

每一次 live run 必须可以落盘，并能离线重放。否则 CI 失败无法复盘，实时工具价值会大幅下降。

---

## 4. 推荐总体架构

```text
┌──────────────────────────────────────────────────────┐
│ Runtime hot path                                      │
│ - compile/runtime gate                               │
│ - fixed-size numeric LiveEvent                       │
│ - no JSON, no strings, no socket/file I/O             │
│ - no payload copy                                     │
│ - try_push only                                       │
└──────────────────────────────┬───────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────┐
│ Per-lane / per-worker bounded ring buffers            │
│ - SPSC preferred                                      │
│ - local_seq per stream                                │
│ - drop/coalesce on overflow                           │
│ - drop counters                                       │
└──────────────────────────────┬───────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────┐
│ Collector                                             │
│ - merge stream_id + local_seq + mono_ns               │
│ - resolve numeric IDs through symbol table            │
│ - aggregate high-frequency events                     │
│ - run live assertions                                 │
│ - produce NDJSON / artifact manifest                  │
└──────────────────────────────┬───────────────────────┘
                               │
                               ▼
┌──────────────────────────────────────────────────────┐
│ Presentation / tooling                                │
│ - topoexec graph observe                              │
│ - tools/topoexec_live_server.py                       │
│ - SSE /events with UI-frame batching                  │
│ - static dashboard                                    │
│ - record/replay                                       │
└──────────────────────────────────────────────────────┘
```

---

## 5. Runtime hot path 设计

### 5.1 默认关闭

支持两层关闭：

```text
Compile-time:
  -DTOPOEXEC_ENABLE_LIVE_OBSERVE=OFF

Runtime:
  topoexec graph run ...
  # no observer path unless graph observe explicitly enables it
```

默认 `graph run`、`graph metrics`、`graph trace` 不应承担 live observe 额外成本。

### 5.2 热路径只投递 fixed-size event

建议新增内部结构：

```cpp
namespace topoexec::runtime_observe {

enum class LiveEventKind : std::uint16_t {
  kRunStarted,
  kRunFinished,
  kSchedulerEpochBegin,
  kSchedulerEpochEnd,
  kComponentBegin,
  kComponentEnd,
  kComponentError,
  kChannelPublishSummary,
  kChannelDrop,
  kChannelReject,
  kChannelOverwrite,
  kTriggerReadySummary,
  kTriggerSuppressedSummary,
  kAsyncAdmission,
  kAsyncReject,
  kAsyncDrop,
  kLoopIterationBegin,
  kLoopIterationEnd,
  kLoopConverged,
  kLoopBudgetOverrun,
  kLoopMaxIterationsHit,
  kLoopError,
  kHealthEvent,
  kRuntimeError,
  kObserverDropSummary
};

struct alignas(64) LiveEvent {
  std::uint64_t local_seq;
  std::uint64_t mono_ns;
  std::uint32_t stream_id;
  std::uint32_t epoch_id;
  std::uint32_t kind;
  std::uint32_t flags;

  std::uint32_t lane_id;
  std::uint32_t worker_id;
  std::uint32_t component_id;
  std::uint32_t channel_id;

  std::uint32_t loop_id;
  std::uint32_t policy_id;
  std::uint32_t reason_id;
  std::uint32_t reserved0;

  std::uint64_t value0;
  std::uint64_t value1;
  std::uint64_t value2;
  std::uint64_t value3;
};

}
```

约束：

```text
- 不含 std::string；
- 不含 std::map；
- 不含 vector；
- 不拥有 payload；
- 不做 heap allocation；
- 不做 JSON；
- 不做 printf/log；
- 不做锁等待。
```

### 5.3 使用 numeric ID，不在热路径解析字符串

启动时 collector 生成一次 symbol table：

```json
{
  "observe_schema_version": "1",
  "kind": "symbol_table",
  "run_id": "run-...",
  "components": {
    "17": "source"
  },
  "channels": {
    "42": "source_transform"
  },
  "lanes": {
    "0": "main"
  },
  "loops": {
    "3": "solver_loop"
  },
  "policies": {
    "8": "rate_limit"
  },
  "reasons": {
    "5": "min_interval_ms"
  }
}
```

runtime 内部事件只写 ID。collector/dashboard 再转换为字符串。

### 5.4 不强制全局 seq

避免每条事件都执行全局 atomic increment。建议：

```text
- 每个 stream 维护 local_seq；
- stream 通常对应 lane 或 worker；
- collector 输出时再生成 display_seq；
- dashboard 以 display_seq 展示，以 stream_id/local_seq 标记真实来源。
```

输出 envelope：

```json
{
  "observe_schema_version": "1",
  "run_id": "run-20260507-001",
  "display_seq": 9012,
  "stream_id": "lane:main",
  "local_seq": 128,
  "kind": "component_end",
  "mono_ns": 123456789
}
```

### 5.5 时间戳分级

不是所有事件都要高精度 timestamp。建议根据 observe level 控制：

| Level | 行为 |
|---|---|
| `off` | 不发 live event，不采 timestamp。 |
| `summary` | 低频事件带 timestamp；高频事件按窗口聚合。 |
| `detailed` | component begin/end、error、channel drop/reject、loop、async 等逐条；publish/trigger 高频事件仍可聚合。 |
| `debug` | 选定 component/channel/policy 的更细事件；允许 payload preview，但显式声明 intrusive。 |

### 5.6 payload 禁止默认拷贝

默认 observe 只允许：

```text
payload_type_id
payload_size_bytes
payload_sequence
payload_timestamp
payload_hash_optional
```

不允许默认输出 payload body。需要 preview 时必须显式开启：

```bash
topoexec graph observe graph.yaml \
  --observe-level debug \
  --include-channel sensor_raw \
  --payload-preview-bytes 64
```

约束：

```text
- preview byte 数有硬上限；
- 只能对指定 component/channel 生效；
- 不延长 payload 生命周期；
- 不改变 ownership；
- 不强制深拷贝；
- UI 必须标注 preview 是调试模式，可能有额外开销。
```

---

## 6. Bounded event transport

### 6.1 队列策略

推荐 per-lane / per-worker SPSC ring buffer：

```text
runtime producer -> try_push(event)
collector consumer -> drain()
```

满队列处理：

```cpp
if (!ring.try_push(event)) {
  ++stream.dropped_event_count;
  stream.mark_drop_summary_pending();
}
```

禁止：

```text
- blocking push；
- 等待 UI；
- 等待 collector；
- socket write；
- file flush；
- JSON dump；
- unbounded vector push。
```

### 6.2 overflow 策略

默认：

```text
- normal high-frequency event：drop 或 coalesce；
- error / health / assertion failure：尽量保留，但仍不得阻塞；
- overflow summary：周期性输出 observer_drop_summary；
- exactness flag：dashboard 显示当前视图是否完整。
```

示例：

```json
{
  "observe_schema_version": "1",
  "kind": "observer_drop_summary",
  "stream_id": "lane:main",
  "dropped_event_count": 1842,
  "window_start_mono_ns": 120000000,
  "window_end_mono_ns": 170000000,
  "affected_kind": "channel_publish"
}
```

### 6.3 memory ordering

建议最低要求：

```text
- producer 写 event 后 release；
- consumer acquire 读取；
- counters 使用 relaxed；
- 不在 hot path 使用 contended mutex；
- 不在 hot path 使用全局 shared map。
```

如果现有代码结构无法一次性实现 lock-free SPSC，可以第一版用 bounded non-blocking queue，但必须通过 benchmark 证明 summary mode 满足性能预算，并在文档中标出后续替换为 SPSC ring 的计划。

---

## 7. observe levels 与 event 策略

### 7.1 observe level

CLI：

```bash
topoexec graph observe graph.yaml --observe-level summary
topoexec graph observe graph.yaml --observe-level detailed
topoexec graph observe graph.yaml --observe-level debug
```

默认：

```text
summary
```

### 7.2 事件精度分类

| 事件类型 | Summary | Detailed | Debug |
|---|---:|---:|---:|
| `run_started` / `run_finished` | exact | exact | exact |
| `runtime_error` | exact | exact | exact |
| `component_error` | exact | exact | exact |
| `component_begin/end` | aggregate latency + optional latest | exact | exact |
| `scheduler_epoch_begin/end` | aggregate | exact-ish | exact-ish |
| `channel_publish` | aggregate | sampled or filtered | exact for selected channels |
| `channel_commit` | aggregate | sampled or filtered | exact for selected channels |
| `channel_drop/reject/overwrite` | exact | exact | exact |
| `trigger_ready` | aggregate | sampled or filtered | exact for selected components |
| `trigger_suppressed` | aggregate | sampled or filtered | exact for selected components |
| `async_admission` | aggregate | exact | exact |
| `async_reject/drop` | exact | exact | exact |
| `loop_iteration` | aggregate + latest residual | exact | exact |
| `loop_converged/error/budget/max_iter` | exact | exact | exact |
| `health_event` | exact within bounded retention | exact | exact |
| `observer_drop_summary` | exact | exact | exact |
| `assertion_fail` | exact | exact | exact |
| `assertion_pass` | aggregate | exact | exact |

### 7.3 选择性观察

必须支持局部放大：

```bash
--include-component solver
--include-channel sensor_to_filter
--include-event channel_drop
--exclude-event channel_publish
--sample-event trigger_suppressed:0.01
```

默认策略：

```text
- exceptions-always-on；
- high-frequency normal path summary；
- user-selected component/channel detailed；
- debug mode explicit only。
```

---

## 8. observe schema v1

### 8.1 独立 schema

新增文档：

```text
docs/62-schemas-protocols/live-observe-events.md
```

新增 schema 名称：

```text
observe_schema_version = "1"
```

不要改变：

```text
metric_schema_version = "1"
trace_schema_version = "1"
graph schema v1
```

### 8.2 输出 envelope

NDJSON 每行一个 envelope：

```json
{
  "observe_schema_version": "1",
  "run_id": "run-20260507-001",
  "display_seq": 1024,
  "stream_id": "lane:main",
  "local_seq": 88,
  "kind": "component_end",
  "severity": "info",
  "exactness": "exact",
  "mono_ns": 123456789,
  "epoch_id": "7",
  "lane": "main",
  "worker_id": "",
  "component_id": "transform",
  "channel_id": "",
  "loop_id": "",
  "trace_id": "trace-...",
  "transaction_id": "source_transform#7",
  "correlation_id": "source_transform#7",
  "causation_id": "source_transform#7",
  "attributes": {
    "duration_ns": 9200,
    "status": "ok"
  }
}
```

### 8.3 exactness 字段

每个 event/batch/summary 都要声明：

```text
exact
aggregated
sampled
lossy
partial
```

dashboard 必须显示 observer drops 和 exactness，不允许把 sampled/aggregated 数据伪装成完整事件流。

### 8.4 UI batch event

SSE 不逐条推 hot event，collector 每 50ms 左右推一帧：

```json
{
  "observe_schema_version": "1",
  "kind": "ui_frame",
  "run_id": "run-20260507-001",
  "frame_seq": 120,
  "window_ms": 50,
  "event_count": 1400,
  "dropped_event_count_delta": 0,
  "events": [
    {
      "kind": "component_error",
      "component_id": "filter",
      "severity": "error"
    }
  ],
  "summaries": {
    "components": {
      "filter": {
        "execution_count_delta": 10,
        "last_duration_ns": 9000,
        "max_duration_ns": 12000
      }
    },
    "channels": {
      "source_filter": {
        "publish_count_delta": 1000,
        "delivery_count_delta": 998,
        "drop_count_delta": 0,
        "max_depth": 4
      }
    }
  }
}
```

---

## 9. CLI 设计

### 9.1 新增命令

```bash
./build/topoexec graph observe examples/minimal.yaml \
  --steps 100 \
  --observe-level summary \
  --format ndjson \
  --assert tests/live/minimal.assert.yaml \
  --record artifacts/live/minimal
```

### 9.2 参数

```text
Execution:
  --steps N
  --duration-ms N
  --until-idle

Observe:
  --observe-level off|summary|detailed|debug
  --event-buffer-capacity N
  --ui-frame-ms N
  --include-component ID
  --include-channel ID
  --include-event KIND
  --exclude-event KIND
  --sample-event KIND:RATIO
  --exceptions-always-on / --no-exceptions-always-on
  --payload-preview-bytes N
  --record DIR
  --record-max-mb N
  --record-rotation-mb N

Assertions:
  --assert FILE
  --fail-on-assertion-fail
  --fail-on-observer-drop

Output:
  --format ndjson|json-summary
  --quiet
```

### 9.3 退出码语义

区分 runtime 与 validation：

```text
0  runtime ok and live assertions ok
1  runtime failed
2  validation/parse/CLI config failed
3  live assertion failed
4  observe collector failed
```

默认：

```text
- observer drop 不导致非零退出码；
- assertion fail 导致非零退出码；
- runtime fail 仍按 runtime fail 处理；
- --fail-on-observer-drop 可让 observer drop 变成非零退出码。
```

---

## 10. Live assertion DSL

### 10.1 原则

assertion 不进入 graph schema v1。第一版作为测试工具侧 YAML：

```yaml
assertion_schema_version: "1"
assertions:
  - id: no_runtime_errors
    type: counter_equals
    source: runtime_errors
    value: 0

  - id: no_channel_drops
    type: metric_equals
    metric: runtime.channel.drop_count
    value: 0

  - id: sink_receives_by_epoch_3
    type: eventually
    event: component_end
    where:
      component_id: sink
      status: ok
    within_epochs: 3

  - id: async_inflight_bounded
    type: metric_lte
    metric: runtime.async.max_in_flight_count
    value: 4

  - id: loop_converges
    type: eventually
    event: loop_converged
    where:
      loop_id: solver_loop
    within_events: 1000

  - id: no_observer_drops
    type: metric_equals
    metric: runtime.observer.dropped_event_count
    value: 0
```

### 10.2 支持的 assertion type

第一版只做：

```text
always
never
eventually
within_epochs
within_events
counter_equals
metric_equals
metric_lte
metric_gte
sequence
```

暂不做：

```text
- arbitrary script；
- Python expression eval；
- JS expression eval；
- user-defined function；
- graph schema inline assertions；
- runtime hook assertion。
```

### 10.3 assertion 输出

```json
{
  "assertion_schema_version": "1",
  "ok": false,
  "passed": 11,
  "failed": 1,
  "pending": 0,
  "failures": [
    {
      "id": "sink_receives_by_epoch_3",
      "reason": "eventually condition not satisfied",
      "last_epoch": "5",
      "evidence_event_seq": 84,
      "last_related_event_seq": 79
    }
  ]
}
```

### 10.4 实时事件

collector 可输出：

```json
{"kind":"assertion_registered","assertion_id":"no_channel_drops"}
{"kind":"assertion_pass","assertion_id":"no_runtime_errors"}
{"kind":"assertion_fail","assertion_id":"sink_receives_by_epoch_3","reason":"eventually condition not satisfied"}
{"kind":"assertion_pending","assertion_id":"loop_converges"}
```

---

## 11. SSE dashboard 设计

### 11.1 为什么 MVP 用 SSE

SSE 适合 server 向浏览器持续推送新数据，符合 live dashboard 的单向观察模型。MDN 说明 server-sent events 允许 server 在任意时间把新数据推送给网页。
来源：<https://developer.mozilla.org/en-US/docs/Web/API/Server-sent_events>

WebSocket 适合双向通信，但 MVP 不做 runtime 控制；并且标准 `WebSocket` API 没有 backpressure，如果消息到达快于应用处理速度，可能导致内存缓冲、CPU 高占用或无响应。
来源：<https://developer.mozilla.org/en-US/docs/Web/API/WebSocket>

因此：

```text
G73: SSE, observe-only
G74/G75: 如需 pause/resume/step/fault injection，再评估 WebSocket 或 WebSocketStream
```

### 11.2 新增工具

```text
tools/topoexec_live_server.py
tools/topoexec_live_dashboard/index.html
tools/topoexec_live_dashboard/app.js
tools/topoexec_live_dashboard/style.css
```

### 11.3 server 模式

```bash
# 运行 graph 并实时显示
python tools/topoexec_live_server.py run examples/minimal.yaml \
  --steps 100 \
  --assert tests/live/minimal.assert.yaml \
  --open

# 回放已有 artifact
python tools/topoexec_live_server.py replay artifacts/live/run-001 \
  --open
```

### 11.4 安全边界

默认：

```text
- bind 127.0.0.1；
- 生成一次性 token；
- 不自动暴露到局域网；
- 不接受 runtime 控制命令；
- 不上传数据；
- static assets 无 CDN；
- 无 Node/npm 依赖。
```

示例：

```text
http://127.0.0.1:8765/?token=...
```

### 11.5 Dashboard 面板

第一版面板：

```text
Overview
  - runtime status
  - current epoch
  - observe level
  - exactness / dropped events
  - assertion pass/fail/pending
  - runtime errors / health events

Topology
  - 初始拓扑可复用 graph render mermaid
  - active component highlight
  - channel drop/reject badges
  - trigger suppression badges
  - loop region iteration/residual badge

Timeline
  - component execution lanes
  - thread_pool worker lanes
  - loop iteration spans
  - error markers
  - bounded latest history only

Channels
  - publish/delivery/drop/reject/overwrite
  - depth/capacity/max_depth
  - deadline/stale events
  - payload copy count summary

Triggers
  - ready/suppressed/coalesced/pending_drop
  - reason summary
  - policy-specific status

CompositeLoops
  - iteration count
  - residual
  - stop reason
  - output_discarded
  - budget/max-iteration/error

Assertions
  - pass/fail/pending
  - failure evidence
  - related event seq
  - last N events around failure

Health / Errors
  - runtime errors
  - health events
  - observer drops
  - collector errors

Raw Events
  - latest 1000 events by default
  - filter by kind/component/channel
  - never infinite in-memory retention
```

### 11.6 UI refresh

推荐：

```text
collector ingest: 尽快
SSE push: 10–30 Hz batch
default ui-frame-ms: 50
browser render: requestAnimationFrame
raw event retention: latest 1000
```

---

## 12. Record / replay artifact

每次 `--record DIR` 生成：

```text
artifacts/live/<run_id>/
  manifest.json
  graph.yaml
  graph.normalized.json
  plan.json
  render.mmd
  observe.ndjson
  observe.summary.json
  assertions.yaml
  assertion_result.json
  metrics.final.json
  trace.final.json
  trace.chrome.json
  health.final.json
  dashboard.html
```

### 12.1 manifest.json

```json
{
  "artifact_schema_version": "1",
  "run_id": "run-20260507-001",
  "graph_name": "minimal",
  "observe_schema_version": "1",
  "assertion_schema_version": "1",
  "observe_level": "summary",
  "recorded_at": "2026-05-07T00:00:00Z",
  "files": {
    "observe": "observe.ndjson",
    "plan": "plan.json",
    "metrics": "metrics.final.json",
    "trace": "trace.final.json",
    "chrome_trace": "trace.chrome.json",
    "assertions": "assertions.yaml",
    "assertion_result": "assertion_result.json"
  },
  "summary": {
    "runtime_ok": true,
    "assertions_ok": false,
    "observer_dropped_event_count": 0
  }
}
```

### 12.2 Replay

```bash
python tools/topoexec_live_server.py replay artifacts/live/run-001 --open
```

或后续新增：

```bash
topoexec graph observe-replay artifacts/live/run-001/observe.ndjson
```

第一版优先 Python server replay，不必扩展 C++ CLI replay。

---

## 13. 性能预算

必须把“低扰动”写成可执行验收标准。

### 13.1 目标预算

| 模式 | 目标 overhead |
|---|---:|
| observe disabled | `< 0.5%` |
| observe summary | `< 2%` |
| observe detailed | `< 5%` |
| observe debug | 不设硬阈值，但必须显式开启并标注 intrusive |

说明：

```text
- 阈值是 controlled per-machine baseline，不做跨机器绝对比较；
- 如果 CI 环境波动太大，CI 只做 output-contract 与 smoke；
- 本地 live-perf gate 使用重复 runs、median/p95 比较；
- threshold 可通过环境变量调整，但默认文档要保持保守。
```

### 13.2 benchmark 场景

新增或复用：

```text
benchmarks/live_observe_minimal.yaml
benchmarks/live_observe_high_frequency_channels.yaml
benchmarks/live_observe_trigger_stress.yaml
benchmarks/live_observe_thread_pool.yaml
benchmarks/live_observe_composite_loop.yaml
```

比较：

```bash
topoexec graph bench benchmarks/live_observe_high_frequency_channels.yaml \
  --steps 1000 --runs 20 --format json

topoexec graph observe benchmarks/live_observe_high_frequency_channels.yaml \
  --steps 1000 --observe-level summary --record /tmp/topoexec-live-summary

topoexec graph observe benchmarks/live_observe_high_frequency_channels.yaml \
  --steps 1000 --observe-level detailed --record /tmp/topoexec-live-detailed
```

### 13.3 focused gate

新增：

```bash
./scripts/goal_check.sh live
./scripts/goal_check.sh live-perf
```

`live`：

```text
- observe schema smoke；
- CLI output NDJSON smoke；
- assertion pass/fail smoke；
- record artifact smoke；
- replay smoke；
- dashboard static asset smoke；
- observer drop summary smoke。
```

`live-perf`：

```text
- disabled baseline；
- summary overhead；
- detailed overhead；
- high-frequency stress；
- queue overflow 不阻塞；
- observer drop 可观察。
```

---

## 14. 文件规划

### 14.1 C++ runtime/core

建议新增：

```text
include/topoexec/runtime/live_observe.hpp
include/topoexec/runtime/live_event.hpp
src/live_observe.cpp
```

可能修改：

```text
include/topoexec/runtime/runtime_runner.hpp
src/runtime_runner.cpp
src/event_runtime.cpp
src/channel.cpp
src/trigger_policy.cpp
src/metrics.cpp
src/trace.cpp
CMakeLists.txt
```

原则：

```text
- 不把 Python/UI/SSE 依赖引入 topoexec::runtime；
- live observe 内部 API 可以 preview，不作为 stable embedder API；
- public headers 只暴露必要开关和结果；
- 热路径代码必须局部、可审计、可禁用。
```

### 14.2 CLI

可能修改：

```text
tools/topoexec/main.cpp
tools/topoexec/graph_commands.cpp
tools/topoexec/json_output.cpp
```

实际路径以当前仓库为准。Codex 必须先 `find tools src include -maxdepth ...` 确认文件布局。

### 14.3 Python/tooling

新增：

```text
tools/topoexec_live_server.py
tools/topoexec_live_assert.py
tools/topoexec_live_dashboard/index.html
tools/topoexec_live_dashboard/app.js
tools/topoexec_live_dashboard/style.css
```

### 14.4 Tests

新增：

```text
tests/live/
  check_observe_schema.py
  check_live_assertions.py
  check_live_artifact.py
  check_live_replay.py
  minimal_pass.assert.yaml
  expected_fail.assert.yaml

tests/test_live_observe.cpp
tests/test_live_observe_queue.cpp
```

### 14.5 Scripts

新增或修改：

```text
scripts/live_smoke.sh
scripts/live_perf_check.py
scripts/goal_check.sh
scripts/bench_baseline.py
```

### 14.6 Docs

新增：

```text
docs/41-development-tools/live-runtime-validation.md
docs/62-schemas-protocols/live-observe-events.md
docs/24-testing/live-observe-performance.md
```

修改：

```text
docs/24-testing/testing-strategy.md
docs/21-architecture/runtime-architecture.md
docs/21-architecture/runtime-invariants.md
docs/31-planning-roadmap/goals/status.md
docs/31-planning-roadmap/goals/backlog.md
docs/README.md
README.md
CHANGELOG.md
```

---

## 15. 实施里程碑

### M0 — Baseline and scope lock

先执行：

```bash
git status --short
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
ctest --test-dir build --output-on-failure
cmake --build build --target topoexec_format_check
./scripts/agent_check.sh
./scripts/goal_check.sh docs
./scripts/goal_check.sh golden
./scripts/goal_check.sh bench
```

记录：

```text
- 当前 commit；
- 当前测试数量；
- 当前 benchmark output contract；
- 当前 docs/golden 状态；
- 当前 active goal 从 None 改为 G73。
```

### M1 — Spec and docs first

先加文档，不先写 runtime：

```text
docs/62-schemas-protocols/live-observe-events.md
docs/41-development-tools/live-runtime-validation.md
docs/24-testing/live-observe-performance.md
```

写清：

```text
- observe_schema_version；
- exact/aggregated/sampled/lossy 定义；
- observe levels；
- performance budget；
- event list；
- assertion schema；
- record/replay artifact；
- live observer invariant；
- non-goals。
```

验收：

```bash
./scripts/goal_check.sh docs
```

### M2 — Low-overhead internal event transport

实现：

```text
- fixed-size LiveEvent；
- numeric ID symbol table；
- per-stream local_seq；
- bounded non-blocking queue；
- drop/coalesce counters；
- observer_drop_summary；
- compile/runtime enable switch；
- no JSON/string/file/socket in hot path。
```

测试：

```text
- queue capacity exact；
- try_push never blocks；
- overflow increments counter；
- drop summary is produced；
- observer disabled path produces no events；
- observer failure remains non-fatal。
```

验收：

```bash
ctest --test-dir build --output-on-failure -R 'LiveObserve|Observer'
```

### M3 — Runtime instrumentation points

逐步加事件源，不一次性铺满：

Priority 1：

```text
run_started
run_finished
runtime_error
component_begin/end/error
channel_drop/reject/overwrite
loop_converged/error/budget/max_iter
observer_drop_summary
```

Priority 2：

```text
channel publish/commit summary
trigger ready/suppressed summary
async admission/reject/drop
loop iteration/residual summary
health_event
```

Priority 3：

```text
debug-level selected channel/component details
payload preview metadata
causal fields
```

验收：

```text
- observer enabled/disabled runtime result identical；
- existing metrics/trace golden 不被无意改变；
- high-frequency events summary mode 不逐条爆炸。
```

### M4 — `topoexec graph observe`

实现 CLI：

```bash
topoexec graph observe <graph.yaml> \
  --steps N \
  --duration-ms N \
  --until-idle \
  --observe-level summary|detailed|debug \
  --format ndjson \
  --record DIR \
  --assert FILE
```

输出：

```text
- symbol_table；
- plan_ready / graph_validated；
- live events / ui_frame；
- final summary；
- assertion_result。
```

验收：

```bash
./build/topoexec graph observe examples/minimal.yaml \
  --steps 3 \
  --observe-level summary \
  --format ndjson \
  > /tmp/topoexec-observe.ndjson

python tests/live/check_observe_schema.py /tmp/topoexec-observe.ndjson
```

### M5 — Assertion engine

第一版可用 Python 实现，运行在 collector 或 post-processing：

```text
tools/topoexec_live_assert.py
tests/live/check_live_assertions.py
```

支持：

```text
always
never
eventually
within_epochs
within_events
counter_equals
metric_equals
metric_lte
metric_gte
sequence
```

验收：

```bash
topoexec graph observe examples/minimal.yaml \
  --steps 3 \
  --assert tests/live/minimal_pass.assert.yaml \
  --format ndjson

topoexec graph observe examples/diagnostic_warnings.yaml \
  --steps 3 \
  --assert tests/live/expected_fail.assert.yaml \
  --format ndjson
```

### M6 — Record/replay

实现 artifact：

```text
manifest.json
observe.ndjson
assertion_result.json
metrics.final.json
trace.final.json
trace.chrome.json
dashboard.html
```

验收：

```bash
topoexec graph observe examples/minimal.yaml \
  --steps 10 \
  --record /tmp/topoexec-live-run

python tools/topoexec_live_server.py replay /tmp/topoexec-live-run --smoke
python tests/live/check_live_artifact.py /tmp/topoexec-live-run
```

### M7 — Local SSE dashboard

实现本地 server：

```bash
python tools/topoexec_live_server.py run examples/minimal.yaml \
  --steps 100 \
  --observe-level summary \
  --open

python tools/topoexec_live_server.py replay artifacts/live/run-001 \
  --open
```

要求：

```text
- bind 127.0.0.1；
- one-time token；
- static assets no CDN；
- UI frame batching；
- raw events capped；
- observer drop/exactness visible；
- no runtime control endpoint。
```

验收：

```bash
python tools/topoexec_live_server.py replay /tmp/topoexec-live-run --smoke
```

可选：

```bash
./scripts/goal_check.sh live-ui
```

`live-ui` 可后续引入 Playwright，但不要进入默认 runtime gate。

### M8 — Performance gate

新增：

```text
scripts/live_perf_check.py
scripts/goal_check.sh live-perf
```

检查：

```text
- disabled overhead target；
- summary overhead target；
- detailed overhead target；
- high-frequency overflow no blocking；
- observer drop visible；
- no unbounded memory growth。
```

验收：

```bash
./scripts/goal_check.sh live-perf
```

如果环境波动较大，CI 可只跑 smoke，本地 release evidence 跑 threshold。

### M9 — Docs, goal ledger, release alignment

更新：

```text
docs/24-testing/testing-strategy.md
docs/21-architecture/runtime-invariants.md
docs/31-planning-roadmap/goals/status.md
docs/31-planning-roadmap/goals/backlog.md
README.md
CHANGELOG.md
```

说明：

```text
- G73 active/complete 状态；
- live observe invariant；
- focused gates；
- performance budget；
- non-goals/deferred scope。
```

### M10 — Final validation

最终运行：

```bash
./scripts/agent_check.sh
./scripts/goal_check.sh docs
./scripts/goal_check.sh golden
./scripts/goal_check.sh live
./scripts/goal_check.sh bench
./scripts/goal_check.sh live-perf
./scripts/goal_check.sh stress
./scripts/goal_check.sh fuzz
./scripts/goal_check.sh sanitizer
git diff --check
```

如果本地无法支持某些 gate，必须在 status ledger 中记录：

```text
- 未执行命令；
- 原因；
- 替代证据；
- 是否阻塞合并。
```

---

## 16. 验收标准

G73 complete 需要满足：

```text
1. `graph observe` 可实时输出 observe_schema_version=1 的 NDJSON。
2. observe disabled 时 runtime 行为和性能接近 baseline。
3. observe summary mode 使用 bounded non-blocking event transport。
4. runtime 热路径不做 JSON/string/file/socket/UI 工作。
5. observer queue overflow 不阻塞 runtime，并产生 drop summary。
6. dashboard 可实时显示 overview/topology/timeline/channels/triggers/loops/assertions/errors。
7. live assertions 可在运行中 pass/fail/pending，并输出 assertion_result.json。
8. record artifact 可 replay。
9. sampled/aggregated/lossy 数据在 schema 和 UI 中明确标注。
10. 现有 metrics/trace/golden/schema/docs tests 不发生非预期变化。
11. 新增 live 和 live-perf focused gates。
12. docs/status/backlog 更新完整。
```

---

## 17. 风险与缓解

### 17.1 性能扰动

风险：

```text
- atomic contention；
- timestamp overhead；
- string/JSON allocation；
- queue mutex contention；
- file/socket I/O 回压；
- UI 事件过量导致 collector 积压。
```

缓解：

```text
- local_seq 替代全局 seq；
- summary mode 默认聚合；
- numeric ID；
- SPSC ring；
- no JSON in hot path；
- UI frame batching；
- record rotation；
- live-perf gate。
```

### 17.2 语义污染

风险：

```text
- assertion 逻辑进入 runtime；
- observer failure 改变 runtime ok；
- UI 控制 runtime。
```

缓解：

```text
- assertion 在 collector 层；
- observer failure/drop 只记录；
- MVP 无控制 endpoint；
- runtime-invariants 增加 live observer 不改变语义条目。
```

### 17.3 数据完整性误解

风险：

```text
dashboard 把 aggregated/sampled/lossy 数据显示成 exact。
```

缓解：

```text
- exactness 字段强制；
- UI 显示 observe level；
- observer drop visible；
- raw events capped；
- docs 明确各事件精度。
```

### 17.4 安全与隐私

风险：

```text
- payload preview 泄露；
- server 暴露到局域网；
- record artifact 包含敏感 graph/config。
```

缓解：

```text
- payload body 默认不输出；
- server 默认 127.0.0.1；
- one-time token；
- record docs 警告；
- preview 显式 component/channel whitelist。
```

---

## 18. Codex `/goal`

```text
/goal G73-low-overhead-live-runtime-validation

Implement a local-first live runtime validation workbench for TopoExec with strict low-overhead constraints.

Primary design principle:
Live visualization must not materially perturb runtime execution. Runtime hot paths must remain bounded, non-blocking, allocation-light, and independent from UI/server/file/JSON work.

Context:
- G71-post-alpha-hardening-docs-pages is complete.
- The current active goal is None.
- TopoExec is a compact C++20 in-process graph runtime, not a GUI editor, distributed runtime, production exporter, or plugin ecosystem.
- Existing CLI already supports validate, plan, render, run, metrics, trace, chrome trace, explain, diff-plan, and bench.
- Existing runtime observability surfaces include metrics, trace, health events, diagnostics, runtime errors, and bounded observer failure/drop accounting.
- Existing default repository gate is ./scripts/agent_check.sh.

Hard boundaries:
- Do not implement a full GUI editor.
- Do not add Electron/Tauri/Qt.
- Do not add React/Vite/Node as default dependencies.
- Do not implement production OpenTelemetry/Prometheus exporters.
- Do not add runtime control endpoints in MVP.
- Do not add WebSocket control in MVP.
- Do not change graph schema v1 for assertions.
- Do not stream full payload bodies by default.
- Do not pull Python/UI/SSE dependencies into topoexec::runtime.
- Do not change existing runtime scheduling/channel/trigger/CompositeLoop semantics.
- Do not let observer failure/drop change runtime ok/fail semantics.
- Do not serialize JSON, write files/sockets, allocate strings, or copy payloads in runtime hot paths.

M0 baseline:
- Run and record baseline:
  - git status --short
  - cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
  - cmake --build build -j
  - ctest --test-dir build --output-on-failure
  - cmake --build build --target topoexec_format_check
  - ./scripts/agent_check.sh
  - ./scripts/goal_check.sh docs
  - ./scripts/goal_check.sh golden
  - ./scripts/goal_check.sh bench
- Mark G73 as active in docs/31-planning-roadmap/goals/status.md and backlog.md.

M1 specs and docs first:
- Add docs/62-schemas-protocols/live-observe-events.md.
- Add docs/41-development-tools/live-runtime-validation.md.
- Add docs/24-testing/live-observe-performance.md.
- Define observe_schema_version = "1".
- Define observe levels: off, summary, detailed, debug.
- Define exactness values: exact, aggregated, sampled, lossy, partial.
- Define event classes and which classes are exact/aggregated/sampled/lossy per level.
- Define record/replay artifact manifest.
- Define assertion_schema_version = "1".
- Document performance budget:
  - disabled target < 0.5%
  - summary target < 2%
  - detailed target < 5%
  - debug explicitly intrusive
- Validate docs with ./scripts/goal_check.sh docs.

M2 low-overhead event transport:
- Add internal fixed-size LiveEvent records using numeric IDs.
- Add per-lane or per-worker bounded non-blocking ring buffers.
- Prefer stream-local sequence numbers over a global atomic sequence in the hot path.
- Add drop/coalesce counters and observer_drop_summary.
- Add compile/runtime switches so live observe is disabled by default.
- Ensure runtime hot path does not perform JSON serialization, file/socket writes, string allocation, map lookup, payload body copy, or blocking queue operations.
- Add focused C++ tests for:
  - queue capacity
  - overflow/drop summary
  - non-blocking try_push behavior
  - observer disabled path
  - observer failure/drop non-fatal semantics

M3 runtime instrumentation:
- Add event points in priority order:
  1. run_started, run_finished, runtime_error, component_begin/end/error, channel_drop/reject/overwrite, loop_converged/error/budget/max_iter, observer_drop_summary
  2. channel publish/commit summary, trigger ready/suppressed summary, async admission/reject/drop, loop iteration/residual summary, health_event
  3. selected debug details and bounded payload metadata preview
- Preserve existing metrics and trace schema contracts.
- Add semantic tests proving observer enabled/disabled produces the same graph result.

M4 CLI graph observe:
- Add `topoexec graph observe <graph.yaml>`.
- Support:
  - --steps
  - --duration-ms
  - --until-idle
  - --observe-level off|summary|detailed|debug
  - --format ndjson|json-summary
  - --record DIR
  - --assert FILE
  - --event-buffer-capacity
  - --ui-frame-ms
  - --include-component
  - --include-channel
  - --include-event
  - --exclude-event
  - --sample-event KIND:RATIO
  - --payload-preview-bytes
  - --fail-on-assertion-fail
  - --fail-on-observer-drop
- Emit symbol_table, graph_validated/plan_ready, live events or ui_frame batches, and final summary.
- Add tests/live/check_observe_schema.py and CLI smoke tests.

M5 live assertion engine:
- Implement assertion YAML outside graph schema v1.
- Support:
  - always
  - never
  - eventually
  - within_epochs
  - within_events
  - counter_equals
  - metric_equals
  - metric_lte
  - metric_gte
  - sequence
- Do not support arbitrary code execution.
- Run assertions in collector/tooling layer, not scheduler/channel/trigger runtime internals.
- Emit assertion_registered, assertion_pass, assertion_fail, assertion_pending events.
- Write assertion_result.json.
- Add pass/fail/pending tests.

M6 record/replay:
- With --record DIR, write:
  - manifest.json
  - graph.yaml
  - graph.normalized.json
  - plan.json
  - render.mmd
  - observe.ndjson
  - observe.summary.json
  - assertions.yaml
  - assertion_result.json
  - metrics.final.json
  - trace.final.json
  - trace.chrome.json
  - health.final.json
  - dashboard.html
- Add replay support through tools/topoexec_live_server.py replay DIR.
- Add artifact and replay tests.

M7 local SSE dashboard:
- Add tools/topoexec_live_server.py.
- Add static dashboard assets under tools/topoexec_live_dashboard/.
- Support:
  - run mode
  - replay mode
  - /events SSE endpoint
  - /snapshot endpoint
  - /bundle or static artifact access
- Bind to 127.0.0.1 by default.
- Use one-time token in the local URL.
- Use no CDN and no default Node/npm dependency.
- Batch SSE UI frames; default --ui-frame-ms 50.
- Dashboard panels:
  - Overview
  - Topology
  - Timeline
  - Channels
  - Triggers
  - CompositeLoops
  - Assertions
  - Health/Errors
  - Raw Events
- Raw events must be bounded by default.
- UI must show observe level, exactness, and observer drop summaries.

M8 focused gates and performance:
- Add ./scripts/goal_check.sh live.
- Add ./scripts/goal_check.sh live-perf.
- Add scripts/live_smoke.sh.
- Add scripts/live_perf_check.py.
- Add benchmark cases for high-frequency channels, trigger stress, thread-pool, and CompositeLoop.
- live gate covers schema, CLI, assertion, record, replay, dashboard smoke, drop summary.
- live-perf gate compares disabled, summary, detailed, and debug modes against local baseline.
- Do not enforce cross-machine absolute timing thresholds; use controlled per-machine baseline policy.

M9 docs and ledgers:
- Update:
  - docs/24-testing/testing-strategy.md
  - docs/21-architecture/runtime-architecture.md
  - docs/21-architecture/runtime-invariants.md
  - docs/31-planning-roadmap/goals/status.md
  - docs/31-planning-roadmap/goals/backlog.md
  - docs/README.md
  - README.md
  - CHANGELOG.md
- Document live observe as an observability/test-validation tool, not a runtime control path.
- Document non-goals and deferred scope.

M10 final validation:
- Run:
  - ./scripts/agent_check.sh
  - ./scripts/goal_check.sh docs
  - ./scripts/goal_check.sh golden
  - ./scripts/goal_check.sh live
  - ./scripts/goal_check.sh bench
  - ./scripts/goal_check.sh live-perf
  - ./scripts/goal_check.sh stress
  - ./scripts/goal_check.sh fuzz
  - ./scripts/goal_check.sh sanitizer
  - git diff --check
- If any locally unsupported gate is skipped, record command, reason, replacement evidence, and whether it blocks completion in the goal ledger.

Acceptance:
- A developer can run `topoexec graph observe examples/minimal.yaml --steps 100 --record artifacts/live/minimal` and get deterministic observe NDJSON plus replayable artifacts.
- A developer can run the local SSE dashboard and see live overview, topology, timeline, channel, trigger, CompositeLoop, assertion, health, and raw-event panels.
- A failing assertion is visible during execution and persisted in assertion_result.json.
- Runtime hot paths remain bounded, non-blocking, and free of JSON/file/socket/UI work.
- Observe disabled overhead is targeted below 0.5%.
- Observe summary overhead is targeted below 2%.
- Observe detailed overhead is targeted below 5% on normal benchmark examples.
- Observer drops are counted and reported without blocking or changing runtime semantics.
- Existing runtime semantics, metrics, trace, docs, schema, and golden tests remain stable unless intentionally updated and documented.
```

---

## 19. 推荐执行顺序

实际给 Codex 执行时，不建议一次性要求完成全部 UI 美化。推荐顺序：

```text
1. M0 baseline；
2. M1 schema/docs；
3. M2 fixed-size event + bounded queue；
4. M3 priority-1 instrumentation；
5. M4 graph observe NDJSON；
6. M5 assertion；
7. M6 record/replay；
8. M7 dashboard MVP；
9. M8 perf gate；
10. M9/M10 docs and validation。
```

如果实现过程中必须缩小范围，保留优先级应是：

```text
低扰动 event transport
> graph observe NDJSON
> assertion engine
> record/replay
> dashboard
```

UI 可以晚一点，但低扰动观察通道必须先正确。
