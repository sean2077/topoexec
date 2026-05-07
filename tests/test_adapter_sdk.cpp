#include "topoexec/adapters/sdk.hpp"
#include "topoexec/runtime/graph_builder.hpp"

#include <gtest/gtest.h>

#include <deque>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

class AdapterProbeComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.AdapterProbe";
    descriptor.role = topoexec::ComponentRole::kInputOutputBoundary;
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}
  void execute(const topoexec::Invocation&, topoexec::GraphContext&) override {}
};

class ProbeProvider : public topoexec::adapters::ComponentFactoryProvider {
public:
  std::string provider_name() const override {
    return "probe-provider";
  }

  topoexec::Status register_components(topoexec::ComponentRegistry& registry) override {
    registry.register_component({"topoexec.test.AdapterProbe"},
                                []() { return std::make_unique<AdapterProbeComponent>(); });
    ++register_count;
    return topoexec::Status::success();
  }

  std::size_t register_count{0};
};

class CountingResultSink : public topoexec::adapters::ResultSink {
public:
  topoexec::Status on_result(const topoexec::RuntimeRunnerResult& result) override {
    ++result_count;
    last_graph_name = result.graph_name;
    return topoexec::Status::success();
  }

  std::size_t result_count{0};
  std::string last_graph_name;
};

class FailingAdapterObserver : public topoexec::adapters::RuntimeObserver {
public:
  topoexec::Status on_metric(const topoexec::RuntimeMetricSample&) override {
    ++metric_count;
    return topoexec::Status::error("adapter metric sink failed");
  }

  topoexec::Status on_result(const topoexec::RuntimeRunnerResult&) override {
    ++result_count;
    return topoexec::Status::error("adapter result sink failed");
  }

  std::size_t metric_count{0};
  std::size_t result_count{0};
};

class QueueBoundaryBridge : public topoexec::adapters::BoundaryBridge {
public:
  explicit QueueBoundaryBridge(std::size_t capacity) : capacity_(capacity) {}

  topoexec::Status enqueue_input(topoexec::adapters::BoundaryMessage message) {
    if (pending_inputs_.size() >= capacity_) {
      ++status_.dropped_input_count;
      return topoexec::Status::error("boundary input queue full");
    }
    pending_inputs_.push_back(std::move(message));
    ++status_.accepted_input_count;
    status_.pending_input_count = pending_inputs_.size();
    return topoexec::Status::success();
  }

  topoexec::adapters::BoundaryPollResult poll_input() override {
    if (pending_inputs_.empty()) {
      status_.pending_input_count = 0;
      return topoexec::adapters::BoundaryPollResult::no_input();
    }
    auto message = std::move(pending_inputs_.front());
    pending_inputs_.pop_front();
    status_.pending_input_count = pending_inputs_.size();
    return topoexec::adapters::BoundaryPollResult::input(std::move(message));
  }

  topoexec::Status publish_output(const topoexec::adapters::BoundaryMessage& message) override {
    if (message.payload == nullptr) {
      ++status_.failed_output_count;
      return topoexec::Status::error("boundary output payload is null");
    }
    published_outputs_.push_back(message);
    ++status_.published_output_count;
    return topoexec::Status::success();
  }

  topoexec::adapters::BoundaryBridgeStatus status() const override {
    return status_;
  }

  const std::vector<topoexec::adapters::BoundaryMessage>& published_outputs() const {
    return published_outputs_;
  }

private:
  std::size_t capacity_{0};
  std::deque<topoexec::adapters::BoundaryMessage> pending_inputs_;
  std::vector<topoexec::adapters::BoundaryMessage> published_outputs_;
  topoexec::adapters::BoundaryBridgeStatus status_;
};

topoexec::GraphSpec adapter_probe_graph() {
  return topoexec::GraphBuilder("adapter_sdk_probe")
      .event_loop_lane("main")
      .component(topoexec::component_node("probe", "topoexec.test.AdapterProbe", {topoexec::manual_event_source()},
                                          topoexec::manual_trigger(), topoexec::lane_execution("main")))
      .build();
}

topoexec::RuntimeRunnerResult run_probe_graph(topoexec::RuntimeObserver* observer = nullptr) {
  topoexec::ComponentRegistry registry;
  ProbeProvider provider;
  const auto registered = provider.register_components(registry);
  EXPECT_TRUE(registered.ok()) << registered.message();
  EXPECT_EQ(provider.provider_name(), "probe-provider");
  EXPECT_EQ(provider.register_count, 1u);

  topoexec::RuntimeRunner runner(registry);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;
  if (observer != nullptr) {
    options.observers.push_back(observer);
  }
  return runner.run(adapter_probe_graph(), options);
}

} // namespace

static_assert(std::is_base_of_v<topoexec::ResultSink, topoexec::adapters::ResultSink>);
static_assert(std::is_base_of_v<topoexec::RuntimeObserver, topoexec::adapters::RuntimeObserver>);

TEST(AdapterSdk, ExposesContractVersionAndResultSinkAlias) {
  EXPECT_STREQ(topoexec::adapters::kAdapterSdkContractVersion, "0");

  CountingResultSink sink;
  topoexec::RuntimeRunnerResult result;
  result.graph_name = "adapter_sdk_probe";
  const auto status = sink.on_result(result);

  EXPECT_TRUE(status.ok()) << status.message();
  EXPECT_EQ(sink.result_count, 1u);
  EXPECT_EQ(sink.last_graph_name, "adapter_sdk_probe");
}

TEST(AdapterSdk, ComponentFactoryProviderRegistersWithoutRuntimeInternals) {
  const auto result = run_probe_graph();

  ASSERT_TRUE(result.ok) << (result.runtime_errors.empty() ? "" : result.runtime_errors.front().message);
  EXPECT_EQ(result.graph_name, "adapter_sdk_probe");
  EXPECT_EQ(result.instantiated_components, 1u);
  EXPECT_EQ(result.tick_calls, 1u);
}

TEST(AdapterSdk, BoundaryBridgeIsBoundedAndBestEffort) {
  QueueBoundaryBridge bridge(1);
  topoexec::adapters::BoundaryMessage input;
  input.boundary_id = "external_input";
  input.port = "out";
  input.payload = topoexec::make_shared_payload(topoexec::make_text_payload("request-1"));

  EXPECT_TRUE(bridge.enqueue_input(input).ok());
  EXPECT_FALSE(bridge.enqueue_input(input).ok());
  EXPECT_EQ(bridge.status().dropped_input_count, 1u);

  const auto first = bridge.poll_input();
  ASSERT_TRUE(first.ok()) << first.reason;
  ASSERT_TRUE(first.ready);
  ASSERT_NE(first.message.payload, nullptr);
  EXPECT_EQ(first.message.payload->text(), "request-1");

  const auto empty = bridge.poll_input();
  EXPECT_TRUE(empty.ok()) << empty.reason;
  EXPECT_FALSE(empty.ready);

  topoexec::adapters::BoundaryMessage output;
  output.boundary_id = "external_output";
  output.port = "in";
  output.payload = topoexec::make_shared_payload(topoexec::make_text_payload("response-1"));
  EXPECT_TRUE(bridge.publish_output(output).ok());
  EXPECT_EQ(bridge.status().published_output_count, 1u);
  ASSERT_EQ(bridge.published_outputs().size(), 1u);
  EXPECT_EQ(bridge.published_outputs().front().payload->text(), "response-1");
}

TEST(AdapterSdk, ObserverFailureIsNonFatalRuntimeEvidence) {
  FailingAdapterObserver observer;
  const auto result = run_probe_graph(&observer);

  ASSERT_TRUE(result.ok) << (result.runtime_errors.empty() ? "" : result.runtime_errors.front().message);
  EXPECT_GT(observer.metric_count, 0u);
  EXPECT_EQ(observer.result_count, 1u);
  EXPECT_GT(result.observer_failure_count, 0u);
  EXPECT_FALSE(result.runtime_errors.empty());
}
