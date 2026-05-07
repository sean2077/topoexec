#include "topoexec/runtime/graph.hpp"
#include "topoexec/runtime/runtime_runner.hpp"
#include "topoexec/runtime/task_executor.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

topoexec::ComponentDescriptor stress_descriptor(std::string type, topoexec::ComponentRole role,
                                                std::vector<topoexec::PortDescriptor> inputs,
                                                std::vector<topoexec::PortDescriptor> outputs) {
  topoexec::ComponentDescriptor descriptor;
  descriptor.type = std::move(type);
  descriptor.name = descriptor.type;
  descriptor.role = role;
  descriptor.inputs = std::move(inputs);
  descriptor.outputs = std::move(outputs);
  return descriptor;
}

class BurstSourceComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    return stress_descriptor("topoexec.stress.BurstSource", topoexec::ComponentRole::kInputBoundary, {},
                             {{"out", topoexec::kTextPayloadSchema}});
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation&, topoexec::GraphContext& ctx) override {
    for (std::size_t index = 0; index < kBurstCount; ++index) {
      const auto published = ctx.publish("out", topoexec::make_text_payload("burst-" + std::to_string(index)));
      if (!published.accepted) {
        throw std::runtime_error("burst publish failed: " + published.reason);
      }
    }
  }

  static constexpr std::size_t kBurstCount = 16;
};

class BlockingWorkerComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    return stress_descriptor("topoexec.stress.BlockingWorker", topoexec::ComponentRole::kProcessing,
                             {{"in", topoexec::kTextPayloadSchema}}, {{"out", topoexec::kTextPayloadSchema}});
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& ctx) override {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const std::string payload = invocation.payload == nullptr ? "missing" : invocation.payload->text();
    const auto published = ctx.publish("out", topoexec::make_text_payload(payload + ":done"));
    if (!published.accepted) {
      throw std::runtime_error("worker publish failed: " + published.reason);
    }
  }
};

class StressSinkComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    return stress_descriptor("topoexec.stress.Sink", topoexec::ComponentRole::kOutputBoundary,
                             {{"in", topoexec::kTextPayloadSchema}}, {});
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation&, topoexec::GraphContext&) override {}
};

topoexec::ComponentRegistry stress_registry() {
  topoexec::ComponentRegistry registry;
  registry.register_component({"topoexec.stress.BurstSource"},
                              []() { return std::make_unique<BurstSourceComponent>(); });
  registry.register_component({"topoexec.stress.BlockingWorker"},
                              []() { return std::make_unique<BlockingWorkerComponent>(); });
  registry.register_component({"topoexec.stress.Sink"}, []() { return std::make_unique<StressSinkComponent>(); });
  return registry;
}

bool has_metric_at_least(const topoexec::RuntimeRunnerResult& result, const std::string& name, const std::string& lane,
                         double minimum) {
  return std::any_of(result.runtime_metrics.begin(), result.runtime_metrics.end(), [&](const auto& metric) {
    return metric.name == name && metric.lane == lane && metric.value >= minimum;
  });
}

} // namespace

TEST(Stress, ThreadedTaskExecutorOverloadRejectsExcessWithoutDeadlock) {
  topoexec::ThreadedTaskExecutorConfig config;
  config.max_workers = 2;
  config.max_inflight = 2;
  config.queue_capacity = 3;
  config.overflow = "reject";
  topoexec::ThreadedTaskExecutor executor(config);

  std::atomic_bool release_tasks{false};
  std::atomic_size_t started_count{0};
  std::size_t accepted_count = 0;
  std::size_t rejected_count = 0;
  constexpr std::size_t kSubmissionCount = 64;

  for (std::size_t index = 0; index < kSubmissionCount; ++index) {
    auto result = executor.submit([&, index]() {
      started_count.fetch_add(1, std::memory_order_relaxed);
      while (!release_tasks.load(std::memory_order_acquire)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      return topoexec::make_text_payload("task-" + std::to_string(index));
    });
    if (result.accepted) {
      ++accepted_count;
    } else {
      ++rejected_count;
    }
  }

  EXPECT_EQ(accepted_count, config.max_inflight + config.queue_capacity);
  EXPECT_EQ(rejected_count, kSubmissionCount - accepted_count);
  EXPECT_GT(rejected_count, 0u);

  for (int attempt = 0; attempt < 100 && started_count.load(std::memory_order_relaxed) < config.max_workers;
       ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_EQ(started_count.load(std::memory_order_relaxed), config.max_workers);

  release_tasks.store(true, std::memory_order_release);
  ASSERT_TRUE(executor.wait_for_idle(std::chrono::seconds(2)));

  const auto completions = executor.run_ready();
  ASSERT_EQ(completions.size(), accepted_count);
  for (const auto& completion : completions) {
    EXPECT_TRUE(completion.ok) << completion.error;
    ASSERT_NE(completion.payload, nullptr);
    EXPECT_NE(completion.payload->text().find("task-"), std::string::npos);
  }

  const auto metrics = executor.metrics();
  EXPECT_EQ(metrics.submitted_count, accepted_count);
  EXPECT_EQ(metrics.completed_count, accepted_count);
  EXPECT_EQ(metrics.rejected_count, rejected_count);
  EXPECT_EQ(metrics.failed_count, 0u);
  EXPECT_EQ(metrics.queue_depth, 0u);
  EXPECT_LE(metrics.max_inflight_count, config.max_inflight + config.queue_capacity);
}

TEST(Stress, ThreadPoolGraphOverloadRejectsExcessInvocationsWithoutErrors) {
  const auto spec = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: stress_thread_pool_overload, kind: runnable}
lanes:
  main: {type: event_loop}
  pool: {type: thread_pool, max_threads: 1, queue_capacity: 2, overflow: drop_newest}
components:
  - id: source
    type: topoexec.stress.BurstSource
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: worker
    type: topoexec.stress.BlockingWorker
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: pool, reentrant: true, priority: low}
  - id: sink
    type: topoexec.stress.Sink
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: main}
edges:
  - {id: source_worker, kind: immediate, from: source.out, to: worker.in, policy: {mode: queue, capacity: 32, overflow: drop_oldest, copy_policy: shared_view}}
  - {id: worker_sink, kind: immediate, from: worker.out, to: sink.in, policy: {mode: queue, capacity: 32, overflow: drop_oldest, copy_policy: shared_view}}
)");
  const auto registry = stress_registry();
  topoexec::RuntimeRunner runner(registry);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 4;

  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << (result.runtime_errors.empty() ? "" : result.runtime_errors.front().message);
  EXPECT_TRUE(result.runtime_errors.empty());
  EXPECT_TRUE(result.runtime_errors.empty());
  EXPECT_EQ(result.channel_drop_count, 0u);
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.rejected_count", "pool", 4.0 * 13.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.low_priority_rejected_count", "pool", 4.0 * 13.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.queue_capacity", "pool", 2.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.queue_depth", "pool", 2.0));
}
