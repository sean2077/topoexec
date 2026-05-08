#include "topoexec/runtime/channel.hpp"
#include "topoexec/runtime/event_runtime.hpp"
#include "topoexec/runtime/metric_schema.hpp"
#include "topoexec/runtime/runtime_runner.hpp"
#include "topoexec/runtime/scheduler.hpp"
#include "topoexec/runtime/state.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <thread>
#include <utility>

namespace {

struct RuntimeRecord {
  std::uint64_t sequence{0};
  std::string component_id;
  std::string loop_id;
  std::size_t loop_iteration_index{0};
  std::size_t loop_iteration_number{0};
  topoexec::EventKind event{topoexec::EventKind::kManual};
  topoexec::TriggerKind trigger{topoexec::TriggerKind::kManual};
  std::string correlation_id;
  std::string causation_id;
  std::uint64_t epoch_id{0};
  std::string transaction_id;
  std::string source_component;
  std::string source_port;
  std::string trigger_kind;
  std::vector<std::string> ready_inputs;
  std::map<std::string, std::string> payloads_by_port;
  std::vector<std::string> batch_payloads;
};

struct PublicationProbeState {
  bool publisher_active{false};
  bool publish_accepted{false};
  bool sink_seen_before_publish_return{false};
  bool sink_ran_while_publisher_active{false};
  std::string publish_reason;
  std::vector<std::string> events;
};

std::vector<RuntimeRecord>& runtime_records() {
  static std::vector<RuntimeRecord> records;
  return records;
}

std::vector<std::string>& lifecycle_events() {
  static std::vector<std::string> events;
  return events;
}

std::vector<std::string>& config_observations() {
  static std::vector<std::string> values;
  return values;
}

std::vector<std::string>& config_apply_events() {
  static std::vector<std::string> values;
  return values;
}

std::mutex& runtime_records_mutex() {
  static std::mutex mutex;
  return mutex;
}

PublicationProbeState& publication_probe_state() {
  static PublicationProbeState state;
  return state;
}

void reset_runtime_records() {
  std::lock_guard lock(runtime_records_mutex());
  runtime_records().clear();
}

void reset_lifecycle_events() {
  lifecycle_events().clear();
}

void reset_config_observations() {
  config_observations().clear();
  config_apply_events().clear();
}

void reset_publication_probe_state() {
  publication_probe_state() = PublicationProbeState{};
}

int& throwing_deactivate_count() {
  static int count = 0;
  return count;
}

int& status_failure_deactivate_count() {
  static int count = 0;
  return count;
}

std::atomic_int& thread_pool_active_invocations() {
  static std::atomic_int count{0};
  return count;
}

std::atomic_int& thread_pool_max_invocations() {
  static std::atomic_int count{0};
  return count;
}

std::set<std::thread::id>& thread_pool_thread_ids() {
  static std::set<std::thread::id> ids;
  return ids;
}

std::mutex& thread_pool_probe_mutex() {
  static std::mutex mutex;
  return mutex;
}

std::function<void()>& cancellation_request_hook() {
  static std::function<void()> hook;
  return hook;
}

void reset_throwing_component_state() {
  throwing_deactivate_count() = 0;
  status_failure_deactivate_count() = 0;
}

void reset_thread_pool_probe_state() {
  thread_pool_active_invocations().store(0);
  thread_pool_max_invocations().store(0);
  std::lock_guard lock(thread_pool_probe_mutex());
  thread_pool_thread_ids().clear();
}

void reset_cancellation_request_hook() {
  cancellation_request_hook() = {};
}

void observe_thread_pool_invocation_begin() {
  const auto active = thread_pool_active_invocations().fetch_add(1) + 1;
  auto observed = thread_pool_max_invocations().load();
  while (active > observed && !thread_pool_max_invocations().compare_exchange_weak(observed, active)) {
  }
  std::lock_guard lock(thread_pool_probe_mutex());
  thread_pool_thread_ids().insert(std::this_thread::get_id());
}

void observe_thread_pool_invocation_end() {
  thread_pool_active_invocations().fetch_sub(1);
}

void record_invocation(const topoexec::Invocation& invocation, const topoexec::GraphContext& context) {
  RuntimeRecord record;
  record.sequence = invocation.sequence;
  record.component_id = context.component_id;
  record.loop_id = context.loop_iteration.loop_id;
  record.loop_iteration_index = context.loop_iteration.iteration_index;
  record.loop_iteration_number = context.loop_iteration.iteration_number;
  record.event = invocation.event;
  record.trigger = invocation.trigger;
  record.correlation_id = invocation.correlation_id;
  record.causation_id = invocation.metadata.causation_id;
  record.epoch_id = invocation.metadata.epoch_id;
  record.transaction_id = invocation.metadata.transaction_id;
  record.source_component = invocation.metadata.source_component;
  record.source_port = invocation.metadata.source_port;
  record.trigger_kind = invocation.metadata.trigger_kind;
  record.ready_inputs = invocation.ready_inputs;
  for (const auto& [port, payload] : invocation.payloads_by_port) {
    if (payload != nullptr) {
      record.payloads_by_port[port] = payload->text();
    }
  }
  for (const auto& payload : invocation.batch_payloads) {
    if (payload != nullptr) {
      record.batch_payloads.push_back(payload->text());
    }
  }
  std::lock_guard lock(runtime_records_mutex());
  runtime_records().push_back(std::move(record));
}

bool has_record(std::uint64_t sequence, const std::string& component_id, const std::string& ready_input,
                const std::string& payload = {}) {
  return std::any_of(runtime_records().begin(), runtime_records().end(), [&](const RuntimeRecord& record) {
    if (record.sequence != sequence || record.component_id != component_id) {
      return false;
    }
    if (std::find(record.ready_inputs.begin(), record.ready_inputs.end(), ready_input) == record.ready_inputs.end()) {
      return false;
    }
    if (payload.empty()) {
      return true;
    }
    const auto found = record.payloads_by_port.find(ready_input);
    return found != record.payloads_by_port.end() && found->second == payload;
  });
}

bool has_component_record(std::uint64_t sequence, const std::string& component_id) {
  return std::any_of(runtime_records().begin(), runtime_records().end(), [&](const RuntimeRecord& record) {
    return record.sequence == sequence && record.component_id == component_id;
  });
}

std::optional<std::size_t> first_record_index(std::uint64_t sequence, const std::string& component_id) {
  const auto found = std::find_if(runtime_records().begin(), runtime_records().end(), [&](const RuntimeRecord& record) {
    return record.sequence == sequence && record.component_id == component_id;
  });
  if (found == runtime_records().end()) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(std::distance(runtime_records().begin(), found));
}

bool has_batch_record(std::uint64_t sequence, const std::string& component_id,
                      const std::vector<std::string>& payloads) {
  return std::any_of(runtime_records().begin(), runtime_records().end(), [&](const RuntimeRecord& record) {
    return record.sequence == sequence && record.component_id == component_id && record.batch_payloads == payloads;
  });
}

bool has_metric(const topoexec::RuntimeRunnerResult& result, const std::string& name,
                const std::string& channel_id = {}) {
  return std::any_of(result.runtime_metrics.begin(), result.runtime_metrics.end(), [&](const auto& metric) {
    return metric.name == name && (channel_id.empty() || metric.channel_id == channel_id);
  });
}

bool has_component_metric(const topoexec::RuntimeRunnerResult& result, const std::string& name,
                          const std::string& component_id) {
  return std::any_of(result.runtime_metrics.begin(), result.runtime_metrics.end(),
                     [&](const auto& metric) { return metric.name == name && metric.component_id == component_id; });
}

bool has_component_metric_at_least(const topoexec::RuntimeRunnerResult& result, const std::string& name,
                                   const std::string& component_id, double value) {
  return std::any_of(result.runtime_metrics.begin(), result.runtime_metrics.end(), [&](const auto& metric) {
    return metric.name == name && metric.component_id == component_id && metric.value >= value;
  });
}

bool has_metric_at_least(const topoexec::RuntimeRunnerResult& result, const std::string& name, double value) {
  return std::any_of(result.runtime_metrics.begin(), result.runtime_metrics.end(),
                     [&](const auto& metric) { return metric.name == name && metric.value >= value; });
}

std::string first_runtime_error_message(const topoexec::RuntimeRunnerResult& result) {
  return result.runtime_errors.empty() ? std::string{} : result.runtime_errors.front().message;
}

std::string first_error_message(const topoexec::RuntimeRunnerResult& result) {
  return first_runtime_error_message(result);
}

template <typename Result> std::string first_error_message(const Result& result) {
  return result.errors.empty() ? std::string{} : result.errors.front();
}

bool has_result_errors(const topoexec::RuntimeRunnerResult& result) {
  return !result.runtime_errors.empty();
}

template <typename Result> bool has_result_errors(const Result& result) {
  return !result.errors.empty();
}

std::optional<double> metric_value(const topoexec::RuntimeRunnerResult& result, const std::string& name) {
  const auto found = std::find_if(result.runtime_metrics.begin(), result.runtime_metrics.end(),
                                  [&](const auto& metric) { return metric.name == name; });
  if (found == result.runtime_metrics.end()) {
    return std::nullopt;
  }
  return found->value;
}

std::optional<double> component_metric_value(const topoexec::RuntimeRunnerResult& result, const std::string& name,
                                             const std::string& component_id) {
  const auto found =
      std::find_if(result.runtime_metrics.begin(), result.runtime_metrics.end(),
                   [&](const auto& metric) { return metric.name == name && metric.component_id == component_id; });
  if (found == result.runtime_metrics.end()) {
    return std::nullopt;
  }
  return found->value;
}

bool has_trace_event(const topoexec::RuntimeRunnerResult& result, const std::string& name) {
  return std::any_of(result.trace.begin(), result.trace.end(), [&](const auto& event) { return event.name == name; });
}

std::vector<std::string> trace_event_names(const topoexec::RuntimeRunnerResult& result) {
  std::vector<std::string> names;
  names.reserve(result.trace.size());
  for (const auto& event : result.trace) {
    names.push_back(event.name);
  }
  return names;
}

bool has_live_event(const topoexec::RuntimeRunnerResult& result, topoexec::runtime_observe::LiveEventKind kind) {
  const auto encoded = topoexec::runtime_observe::encode_kind(kind);
  return std::any_of(result.live_events.begin(), result.live_events.end(),
                     [&](const auto& event) { return event.kind == encoded; });
}

bool has_trace_event_attribute(const topoexec::RuntimeRunnerResult& result, const std::string& name,
                               const std::string& key, const std::string& value) {
  return std::any_of(result.trace.begin(), result.trace.end(), [&](const auto& event) {
    const auto found = event.attributes.find(key);
    return event.name == name && found != event.attributes.end() && found->second == value;
  });
}

bool has_trace_event_attribute_key(const topoexec::RuntimeRunnerResult& result, const std::string& name,
                                   const std::string& key) {
  return std::any_of(result.trace.begin(), result.trace.end(), [&](const auto& event) {
    return event.name == name && event.attributes.find(key) != event.attributes.end();
  });
}

const topoexec::RuntimeTraceEvent* first_trace_event(const topoexec::RuntimeRunnerResult& result,
                                                     const std::string& name) {
  const auto found =
      std::find_if(result.trace.begin(), result.trace.end(), [&](const auto& event) { return event.name == name; });
  return found == result.trace.end() ? nullptr : &*found;
}

class FailingRuntimeObserver : public topoexec::RuntimeObserver {
public:
  topoexec::Status on_metric(const topoexec::RuntimeMetricSample&) override {
    ++metric_calls;
    return topoexec::Status::error("metric observer failed");
  }

  topoexec::Status on_result(const topoexec::RuntimeRunnerResult&) override {
    ++result_calls;
    return topoexec::Status::error("result observer failed");
  }

  std::size_t metric_calls{0};
  std::size_t result_calls{0};
};

bool has_health_event(const topoexec::RuntimeRunnerResult& result, topoexec::HealthEventKind kind,
                      const std::string& channel_id = {}) {
  return std::any_of(result.health_events.begin(), result.health_events.end(), [&](const auto& event) {
    return event.kind == kind && (channel_id.empty() || event.channel_id == channel_id);
  });
}

std::optional<RuntimeRecord> find_record_snapshot(std::uint64_t sequence, const std::string& component_id,
                                                  const std::string& ready_input = {}) {
  std::lock_guard lock(runtime_records_mutex());
  for (const auto& record : runtime_records()) {
    if ((sequence != 0u && record.sequence != sequence) || record.component_id != component_id) {
      continue;
    }
    if (!ready_input.empty() &&
        std::find(record.ready_inputs.begin(), record.ready_inputs.end(), ready_input) == record.ready_inputs.end()) {
      continue;
    }
    return record;
  }
  return std::nullopt;
}
bool has_correlation_record(std::uint64_t sequence, const std::string& component_id,
                            const std::string& correlation_id) {
  return std::any_of(runtime_records().begin(), runtime_records().end(), [&](const RuntimeRecord& record) {
    return record.sequence == sequence && record.component_id == component_id &&
           record.correlation_id == correlation_id;
  });
}

bool has_trigger_record(std::uint64_t sequence, const std::string& component_id, topoexec::EventKind event,
                        topoexec::TriggerKind trigger) {
  return std::any_of(runtime_records().begin(), runtime_records().end(), [&](const RuntimeRecord& record) {
    return record.sequence == sequence && record.component_id == component_id && record.event == event &&
           record.trigger == trigger;
  });
}

topoexec::EdgeSpec runtime_edge(std::string id, std::string from, std::string to) {
  topoexec::EdgeSpec edge;
  edge.id = std::move(id);
  edge.from = std::move(from);
  edge.to = std::move(to);
  edge.has_kind = true;
  edge.kind = topoexec::EdgeKind::kImmediate;
  edge.policy.mode = "queue";
  edge.policy.capacity = 4;
  edge.policy.overflow = "drop_oldest";
  edge.policy.copy_policy = "shared_view";
  return edge;
}

class SourceComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.Source";
    descriptor.name = "source";
    descriptor.role = topoexec::ComponentRole::kInputBoundary;
    descriptor.outputs = {{"out", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation&, topoexec::GraphContext& context) override {
    const auto result = context.publish("out", topoexec::make_text_payload("payload"));
    if (!result.accepted) {
      throw std::runtime_error(result.reason);
    }
  }
};

class EchoComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.Echo";
    descriptor.name = "echo";
    descriptor.inputs = {{"in", topoexec::kTextPayloadSchema}};
    descriptor.outputs = {{"out", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation&, topoexec::GraphContext& context) override {
    auto payload = context.inputs().peek_latest("in");
    if (payload == nullptr) {
      return;
    }
    const auto result = context.publish("out", topoexec::make_text_payload(payload->text()));
    if (!result.accepted) {
      throw std::runtime_error(result.reason);
    }
  }
};

class SinkComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.Sink";
    descriptor.name = "sink";
    descriptor.role = topoexec::ComponentRole::kOutputBoundary;
    descriptor.inputs = {{"in", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation&, topoexec::GraphContext& context) override {
    last = context.inputs().peek_latest("in");
  }

  topoexec::RuntimePayloadPtr last;
};

class StagedPublisherComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.StagedPublisher";
    descriptor.name = "staged_publisher";
    descriptor.role = topoexec::ComponentRole::kInputBoundary;
    descriptor.outputs = {{"out", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation&, topoexec::GraphContext& context) override {
    auto& state = publication_probe_state();
    state.events.push_back("publisher_begin");
    state.publisher_active = true;

    const auto result = context.publish("out", topoexec::make_text_payload("staged"));
    state.publish_accepted = result.accepted;
    state.publish_reason = result.reason;
    state.sink_seen_before_publish_return =
        std::find(state.events.begin(), state.events.end(), "sink_execute") != state.events.end();
    state.events.push_back("publish_return");

    state.publisher_active = false;
    state.events.push_back("publisher_end");
    if (!result.accepted) {
      throw std::runtime_error(result.reason);
    }
  }
};

class PublicationProbeSinkComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.PublicationProbeSink";
    descriptor.name = "publication_probe_sink";
    descriptor.role = topoexec::ComponentRole::kOutputBoundary;
    descriptor.inputs = {{"in", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation&, topoexec::GraphContext&) override {
    auto& state = publication_probe_state();
    if (state.publisher_active) {
      state.sink_ran_while_publisher_active = true;
    }
    state.events.push_back("sink_execute");
  }
};

class TickSourceComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.TickSource";
    descriptor.name = "tick_source";
    descriptor.role = topoexec::ComponentRole::kInputBoundary;
    descriptor.outputs = {{"out", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    record_invocation(invocation, context);
    const auto result =
        context.publish("out", topoexec::make_text_payload("tick-" + std::to_string(invocation.sequence)));
    if (!result.accepted) {
      throw std::runtime_error(result.reason);
    }
  }
};

class ForwardComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.Forward";
    descriptor.name = "forward";
    descriptor.inputs = {{"in", topoexec::kTextPayloadSchema}};
    descriptor.outputs = {{"out", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    record_invocation(invocation, context);
    if (invocation.payload == nullptr) {
      return;
    }
    const auto result = context.publish("out", topoexec::make_text_payload(invocation.payload->text()));
    if (!result.accepted) {
      throw std::runtime_error(result.reason);
    }
  }
};

class DelayTargetComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.DelayTarget";
    descriptor.name = "delay_target";
    descriptor.role = topoexec::ComponentRole::kOutputBoundary;
    descriptor.inputs = {{"main", topoexec::kTextPayloadSchema}, {"delayed", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    record_invocation(invocation, context);
  }
};

class BatchTargetComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.BatchTarget";
    descriptor.name = "batch_target";
    descriptor.role = topoexec::ComponentRole::kOutputBoundary;
    descriptor.inputs = {{"in", topoexec::kTextPayloadSchema},
                         {"ready", topoexec::kTextPayloadSchema},
                         {"request", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    record_invocation(invocation, context);
  }
};

class TimerRecordComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.TimerRecord";
    descriptor.name = "timer_record";
    descriptor.role = topoexec::ComponentRole::kInputOutputBoundary;
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    record_invocation(invocation, context);
  }
};

class BurstSourceComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.BurstSource";
    descriptor.name = "burst_source";
    descriptor.role = topoexec::ComponentRole::kInputBoundary;
    descriptor.outputs = {{"out", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    record_invocation(invocation, context);
    for (int index = 1; index <= 3; ++index) {
      const auto result =
          context.publish("out", topoexec::make_text_payload("burst-" + std::to_string(invocation.sequence) + "-" +
                                                             std::to_string(index)));
      if (!result.accepted) {
        throw std::runtime_error(result.reason);
      }
    }
  }
};

class LenientBurstSourceComponent : public BurstSourceComponent {
public:
  topoexec::ComponentDescriptor describe() const override {
    auto descriptor = BurstSourceComponent::describe();
    descriptor.type = "topoexec.test.LenientBurstSource";
    return descriptor;
  }

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    record_invocation(invocation, context);
    for (int index = 1; index <= 3; ++index) {
      (void)context.publish("out", topoexec::make_text_payload("burst-" + std::to_string(invocation.sequence) + "-" +
                                                               std::to_string(index)));
    }
  }
};

class ThreadPoolProbeComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.ThreadPoolProbe";
    descriptor.name = "thread_pool_probe";
    descriptor.role = topoexec::ComponentRole::kOutputBoundary;
    descriptor.inputs = {{"in", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    observe_thread_pool_invocation_begin();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    record_invocation(invocation, context);
    observe_thread_pool_invocation_end();
  }
};

class SlowManualComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.SlowManual";
    descriptor.name = "slow_manual";
    descriptor.role = topoexec::ComponentRole::kInputOutputBoundary;
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    record_invocation(invocation, context);
  }
};

class CancellationProbeComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.CancellationProbe";
    descriptor.name = "cancellation_probe";
    descriptor.role = topoexec::ComponentRole::kInputOutputBoundary;
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    if (cancellation_request_hook()) {
      cancellation_request_hook()();
    }
    if (!invocation.cancel_requested() || !context.cancel_requested()) {
      throw std::runtime_error("cancellation was not observable");
    }
    record_invocation(invocation, context);
  }
};

class CancellationIgnoringSlowComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.CancellationIgnoringSlow";
    descriptor.name = "cancellation_ignoring_slow";
    descriptor.role = topoexec::ComponentRole::kInputOutputBoundary;
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    if (cancellation_request_hook()) {
      cancellation_request_hook()();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    record_invocation(invocation, context);
  }
};

class LoopEstimatorComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.LoopEstimator";
    descriptor.name = "loop_estimator";
    descriptor.inputs = {{"measurement", topoexec::kTextPayloadSchema}, {"correction", topoexec::kTextPayloadSchema}};
    descriptor.outputs = {{"state", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    record_invocation(invocation, context);
    if (invocation.payload == nullptr) {
      return;
    }
    const auto result = context.publish("state", topoexec::make_text_payload("state:" + invocation.payload->text()));
    if (!result.accepted) {
      throw std::runtime_error(result.reason);
    }
  }
};

class SlowLoopEstimatorComponent : public LoopEstimatorComponent {
public:
  topoexec::ComponentDescriptor describe() const override {
    auto descriptor = LoopEstimatorComponent::describe();
    descriptor.type = "topoexec.test.SlowLoopEstimator";
    return descriptor;
  }

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    LoopEstimatorComponent::execute(invocation, context);
  }
};

class LoopControllerComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.LoopController";
    descriptor.name = "loop_controller";
    descriptor.inputs = {{"state", topoexec::kTextPayloadSchema}};
    descriptor.outputs = {{"command", topoexec::kTextPayloadSchema}, {"correction", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    record_invocation(invocation, context);
    if (invocation.payload == nullptr) {
      return;
    }
    const auto command =
        context.publish("command", topoexec::make_text_payload("command:" + invocation.payload->text()));
    if (!command.accepted) {
      throw std::runtime_error(command.reason);
    }
    const auto correction =
        context.publish("correction", topoexec::make_text_payload("correction:" + invocation.payload->text()));
    if (!correction.accepted) {
      throw std::runtime_error(correction.reason);
    }
  }
};

class ResidualLoopControllerComponent : public LoopControllerComponent {
public:
  topoexec::ComponentDescriptor describe() const override {
    auto descriptor = LoopControllerComponent::describe();
    descriptor.type = "topoexec.test.ResidualLoopController";
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView& config) override {
    const auto found = config.values.find("report_converged");
    report_converged_ = found == config.values.end() || found->second != "false";
  }

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    record_invocation(invocation, context);
    const auto residual = context.loop_iteration.iteration_number >= 2u ? 0.05 : 0.5;
    const auto converged = report_converged_ && residual <= 0.05;
    context.report_loop_convergence(
        topoexec::LoopConvergenceReport{converged, residual, converged ? "controller_residual" : "residual_pending"});
    if (invocation.payload == nullptr) {
      return;
    }
    const auto command =
        context.publish("command", topoexec::make_text_payload("command:" + invocation.payload->text()));
    if (!command.accepted) {
      throw std::runtime_error(command.reason);
    }
    const auto correction =
        context.publish("correction", topoexec::make_text_payload("correction:" + invocation.payload->text()));
    if (!correction.accepted) {
      throw std::runtime_error(correction.reason);
    }
  }

private:
  bool report_converged_{true};
};

class FailingLoopControllerComponent : public LoopControllerComponent {
public:
  topoexec::ComponentDescriptor describe() const override {
    auto descriptor = LoopControllerComponent::describe();
    descriptor.type = "topoexec.test.FailingLoopController";
    return descriptor;
  }

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    record_invocation(invocation, context);
    throw std::runtime_error("loop controller failed");
  }
};

class CancellingLoopControllerComponent : public LoopControllerComponent {
public:
  topoexec::ComponentDescriptor describe() const override {
    auto descriptor = LoopControllerComponent::describe();
    descriptor.type = "topoexec.test.CancellingLoopController";
    return descriptor;
  }

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    if (cancellation_request_hook()) {
      cancellation_request_hook()();
    }
    LoopControllerComponent::execute(invocation, context);
  }
};

class ConfigUpdaterComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.ConfigUpdater";
    descriptor.name = "config_updater";
    descriptor.outputs = {{"out", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView& config) override {
    target_id_ = config.values.contains("target") ? config.values.at("target") : "observer";
    fixed_value_.reset();
    if (const auto found = config.values.find("value"); found != config.values.end()) {
      fixed_value_ = found->second;
    }
    value_prefix_ = config.values.contains("value_prefix") ? config.values.at("value_prefix") : "updated-";
    publish_ = !config.values.contains("publish") || config.values.at("publish") != "false";
  }

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    topoexec::ConfigView update;
    update.values["value"] = fixed_value_.value_or(value_prefix_ + std::to_string(invocation.sequence));
    const auto staged = context.config_store->stage_component_config_update(target_id_, update);
    if (!staged.accepted) {
      throw std::runtime_error(staged.reason);
    }
    if (!publish_) {
      return;
    }
    const auto result = context.publish("out", topoexec::make_text_payload("config-ready"));
    if (!result.accepted) {
      throw std::runtime_error(result.reason);
    }
  }

private:
  std::string target_id_{"observer"};
  std::string value_prefix_{"updated-"};
  std::optional<std::string> fixed_value_;
  bool publish_{true};
};

class ConfigObserverComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.ConfigObserver";
    descriptor.name = "config_observer";
    descriptor.inputs = {{"in", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  topoexec::Status validate_config(const topoexec::ConfigView& config) const override {
    const auto found = config.values.find("value");
    if (found != config.values.end() && found->second == "invalid") {
      return topoexec::Status::error("invalid config value");
    }
    return topoexec::Status::success();
  }

  topoexec::Status apply_config(topoexec::GraphContext& context, const topoexec::ConfigView& config) override {
    const auto found = config.values.find("value");
    const auto value = found == config.values.end() ? std::string{"<missing>"} : found->second;
    config_apply_events().push_back(context.component_id + ".apply." + value);
    if (value == "apply-fail") {
      return topoexec::Status::error("apply config failed");
    }
    return topoexec::Status::success();
  }

  void execute(const topoexec::Invocation&, topoexec::GraphContext& context) override {
    const auto config = context.config_store->component_config(context.component_id);
    const auto found = config.values.find("value");
    config_observations().push_back(found == config.values.end() ? "<missing>" : found->second);
  }
};

class ThrowingComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.Throwing";
    descriptor.name = "throwing";
    descriptor.role = topoexec::ComponentRole::kInputOutputBoundary;
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void deactivate() override {
    ++throwing_deactivate_count();
  }

  void execute(const topoexec::Invocation&, topoexec::GraphContext&) override {
    throw std::runtime_error("boom");
  }
};

class ConfigureStatusFailureComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.ConfigureStatusFailure";
    descriptor.name = "configure_status_failure";
    descriptor.role = topoexec::ComponentRole::kInputOutputBoundary;
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  topoexec::Status configure_status(topoexec::GraphContext&, const topoexec::ConfigView&) override {
    return topoexec::Status::error("configure status failed");
  }

  void execute(const topoexec::Invocation&, topoexec::GraphContext&) override {}
};

class ExecuteStatusFailureComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.test.ExecuteStatusFailure";
    descriptor.name = "execute_status_failure";
    descriptor.role = topoexec::ComponentRole::kInputOutputBoundary;
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void deactivate() override {
    ++status_failure_deactivate_count();
  }

  void execute(const topoexec::Invocation&, topoexec::GraphContext&) override {}

  topoexec::Status execute_status(const topoexec::Invocation&, topoexec::GraphContext&) override {
    return topoexec::Status::error("execute status failed");
  }
};

class LifecycleProbeComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = type_name();
    descriptor.name = "lifecycle_probe";
    descriptor.role = topoexec::ComponentRole::kInputOutputBoundary;
    return descriptor;
  }

  void configure(topoexec::GraphContext& context, const topoexec::ConfigView&) override {
    id_ = context.component_id;
    lifecycle_events().push_back(id_ + ".configure");
  }

  topoexec::Status activate_status() override {
    lifecycle_events().push_back(id_ + ".activate");
    return activate_result();
  }

  topoexec::Status deactivate_status() override {
    lifecycle_events().push_back(id_ + ".deactivate");
    return deactivate_result();
  }

  void execute(const topoexec::Invocation&, topoexec::GraphContext&) override {
    lifecycle_events().push_back(id_ + ".execute");
  }

protected:
  virtual std::string type_name() const {
    return "topoexec.test.LifecycleProbe";
  }

  virtual topoexec::Status activate_result() {
    return topoexec::Status::success();
  }

  virtual topoexec::Status deactivate_result() {
    return topoexec::Status::success();
  }

private:
  std::string id_;
};

class ActivateStatusFailureComponent : public LifecycleProbeComponent {
protected:
  std::string type_name() const override {
    return "topoexec.test.ActivateStatusFailure";
  }

  topoexec::Status activate_result() override {
    return topoexec::Status::error("activate status failed");
  }
};

class DeactivateStatusFailureComponent : public LifecycleProbeComponent {
protected:
  std::string type_name() const override {
    return "topoexec.test.DeactivateStatusFailure";
  }

  topoexec::Status deactivate_result() override {
    return topoexec::Status::error("deactivate status failed");
  }
};

class StatefulLifecycleComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = type_name();
    descriptor.name = "stateful_lifecycle";
    descriptor.role = topoexec::ComponentRole::kInputOutputBoundary;
    return descriptor;
  }

  void configure(topoexec::GraphContext& context, const topoexec::ConfigView&) override {
    id_ = context.component_id;
    count_ = 5;
    lifecycle_events().push_back(id_ + ".configure");
  }

  topoexec::Status activate_status() override {
    lifecycle_events().push_back(id_ + ".activate");
    return topoexec::Status::success();
  }

  topoexec::Status deactivate_status() override {
    lifecycle_events().push_back(id_ + ".deactivate");
    return topoexec::Status::success();
  }

  void reset(topoexec::GraphContext&) override {
    lifecycle_events().push_back(id_ + ".reset");
    count_ = 0;
  }

  void execute(const topoexec::Invocation&, topoexec::GraphContext&) override {
    ++count_;
    lifecycle_events().push_back(id_ + ".execute." + std::to_string(count_));
  }

  topoexec::Result<topoexec::ComponentStateSnapshot> snapshot_state() const override {
    lifecycle_events().push_back(id_ + ".snapshot." + std::to_string(count_));
    topoexec::ComponentStateSnapshot snapshot;
    snapshot.component_type = type_name();
    snapshot.version = "counter.v1";
    snapshot.payload = topoexec::make_shared_payload(
        topoexec::make_text_payload(std::to_string(count_), "topoexec.test.CounterState"));
    snapshot.size_bytes = std::to_string(count_).size();
    return snapshot;
  }

  topoexec::Status restore_state(const topoexec::ComponentStateSnapshot& snapshot) override {
    lifecycle_events().push_back(id_ + ".restore." + snapshot.version);
    if (snapshot.version != "counter.v1") {
      return topoexec::Status::error("unsupported snapshot version " + snapshot.version);
    }
    if (snapshot.payload == nullptr) {
      return topoexec::Status::error("snapshot payload must not be null");
    }
    count_ = std::stoi(topoexec::require_text_payload(*snapshot.payload, "counter snapshot"));
    return topoexec::Status::success();
  }

protected:
  virtual std::string type_name() const {
    return "topoexec.test.StatefulLifecycle";
  }

  std::string id_;
  int count_{0};
};

class ResetFailureComponent : public StatefulLifecycleComponent {
public:
  void reset(topoexec::GraphContext&) override {
    lifecycle_events().push_back(id_ + ".reset");
    throw std::runtime_error("reset failed");
  }

protected:
  std::string type_name() const override {
    return "topoexec.test.ResetFailure";
  }
};

topoexec::ComponentRegistry registry() {
  topoexec::ComponentRegistry registry;
  registry.register_component({"topoexec.test.Source"}, []() { return std::make_unique<SourceComponent>(); });
  registry.register_component({"topoexec.test.Echo"}, []() { return std::make_unique<EchoComponent>(); });
  registry.register_component({"topoexec.test.Sink"}, []() { return std::make_unique<SinkComponent>(); });
  return registry;
}

topoexec::ComponentRegistry delay_registry() {
  topoexec::ComponentRegistry registry;
  registry.register_component({"topoexec.test.TickSource"}, []() { return std::make_unique<TickSourceComponent>(); });
  registry.register_component({"topoexec.test.Forward"}, []() { return std::make_unique<ForwardComponent>(); });
  registry.register_component({"topoexec.test.DelayTarget"}, []() { return std::make_unique<DelayTargetComponent>(); });
  registry.register_component({"topoexec.test.BatchTarget"}, []() { return std::make_unique<BatchTargetComponent>(); });
  registry.register_component({"topoexec.test.TimerRecord"}, []() { return std::make_unique<TimerRecordComponent>(); });
  registry.register_component({"topoexec.test.BurstSource"}, []() { return std::make_unique<BurstSourceComponent>(); });
  registry.register_component({"topoexec.test.LenientBurstSource"},
                              []() { return std::make_unique<LenientBurstSourceComponent>(); });
  registry.register_component({"topoexec.test.ThreadPoolProbe"},
                              []() { return std::make_unique<ThreadPoolProbeComponent>(); });
  registry.register_component({"topoexec.test.SlowManual"}, []() { return std::make_unique<SlowManualComponent>(); });
  registry.register_component({"topoexec.test.CancellationProbe"},
                              []() { return std::make_unique<CancellationProbeComponent>(); });
  registry.register_component({"topoexec.test.CancellationIgnoringSlow"},
                              []() { return std::make_unique<CancellationIgnoringSlowComponent>(); });
  registry.register_component({"topoexec.test.LoopEstimator"},
                              []() { return std::make_unique<LoopEstimatorComponent>(); });
  registry.register_component({"topoexec.test.SlowLoopEstimator"},
                              []() { return std::make_unique<SlowLoopEstimatorComponent>(); });
  registry.register_component({"topoexec.test.LoopController"},
                              []() { return std::make_unique<LoopControllerComponent>(); });
  registry.register_component({"topoexec.test.ResidualLoopController"},
                              []() { return std::make_unique<ResidualLoopControllerComponent>(); });
  registry.register_component({"topoexec.test.FailingLoopController"},
                              []() { return std::make_unique<FailingLoopControllerComponent>(); });
  registry.register_component({"topoexec.test.CancellingLoopController"},
                              []() { return std::make_unique<CancellingLoopControllerComponent>(); });
  registry.register_component({"topoexec.test.Throwing"}, []() { return std::make_unique<ThrowingComponent>(); });
  registry.register_component({"topoexec.test.ConfigureStatusFailure"},
                              []() { return std::make_unique<ConfigureStatusFailureComponent>(); });
  registry.register_component({"topoexec.test.ExecuteStatusFailure"},
                              []() { return std::make_unique<ExecuteStatusFailureComponent>(); });
  registry.register_component({"topoexec.test.LifecycleProbe"},
                              []() { return std::make_unique<LifecycleProbeComponent>(); });
  registry.register_component({"topoexec.test.ActivateStatusFailure"},
                              []() { return std::make_unique<ActivateStatusFailureComponent>(); });
  registry.register_component({"topoexec.test.DeactivateStatusFailure"},
                              []() { return std::make_unique<DeactivateStatusFailureComponent>(); });
  registry.register_component({"topoexec.test.StatefulLifecycle"},
                              []() { return std::make_unique<StatefulLifecycleComponent>(); });
  registry.register_component({"topoexec.test.ResetFailure"},
                              []() { return std::make_unique<ResetFailureComponent>(); });
  registry.register_component({"topoexec.test.ConfigUpdater"},
                              []() { return std::make_unique<ConfigUpdaterComponent>(); });
  registry.register_component({"topoexec.test.ConfigObserver"},
                              []() { return std::make_unique<ConfigObserverComponent>(); });
  return registry;
}

topoexec::ComponentRegistry publication_probe_registry() {
  topoexec::ComponentRegistry registry;
  registry.register_component({"topoexec.test.StagedPublisher"},
                              []() { return std::make_unique<StagedPublisherComponent>(); });
  registry.register_component({"topoexec.test.PublicationProbeSink"},
                              []() { return std::make_unique<PublicationProbeSinkComponent>(); });
  return registry;
}

topoexec::GraphSpec graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: runtime, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - id: source
    type: topoexec.test.Source
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: echo
    type: topoexec.test.Echo
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: main}
  - id: sink
    type: topoexec.test.Sink
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: main}
edges:
  - {id: source_echo, kind: immediate, from: source.out, to: echo.in, policy: {mode: latest, copy_policy: shared_view}}
  - {id: echo_sink, kind: immediate, from: echo.out, to: sink.in, policy: {mode: latest, copy_policy: shared_view}}
)");
}

topoexec::GraphSpec hierarchical_runtime_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: hierarchical_runtime, kind: runnable}
lanes: {main: {type: event_loop}}
components: []
edges: []
subgraphs:
  - id: cell
    components:
      - id: source
        type: topoexec.test.Source
        event_sources: [{type: manual}]
        trigger_policy: {type: manual}
        execution: {lane: main}
      - id: echo
        type: topoexec.test.Echo
        event_sources: [{type: message, inputs: [in]}]
        trigger_policy: {type: any_input, inputs: [in]}
        execution: {lane: main}
      - id: sink
        type: topoexec.test.Sink
        event_sources: [{type: message, inputs: [in]}]
        trigger_policy: {type: any_input, inputs: [in]}
        execution: {lane: main}
    edges:
      - {id: source_echo, kind: immediate, from: source.out, to: echo.in, policy: {mode: latest, copy_policy: shared_view}}
      - {id: echo_sink, kind: immediate, from: echo.out, to: sink.in, policy: {mode: latest, copy_policy: shared_view}}
)");
}

topoexec::GraphSpec publication_probe_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: publication_probe, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - id: publisher
    type: topoexec.test.StagedPublisher
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: sink
    type: topoexec.test.PublicationProbeSink
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: main}
edges:
  - {id: publisher_sink, kind: immediate, from: publisher.out, to: sink.in, policy: {mode: latest, copy_policy: shared_view}}
)");
}

topoexec::GraphSpec delay_visibility_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: delay_visibility, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - id: source
    type: topoexec.test.TickSource
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: publisher
    type: topoexec.test.Forward
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: main}
  - id: gate
    type: topoexec.test.Forward
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: main}
  - id: target
    type: topoexec.test.DelayTarget
    event_sources: [{type: message, inputs: [main, delayed]}]
    trigger_policy: {type: any_input, inputs: [main, delayed]}
    execution: {lane: main}
edges:
  - {id: source_publisher, kind: immediate, from: source.out, to: publisher.in, policy: {mode: latest, copy_policy: shared_view}}
  - {id: publisher_gate, kind: immediate, from: publisher.out, to: gate.in, policy: {mode: latest, copy_policy: shared_view}}
  - {id: publisher_target_delay, kind: delay, from: publisher.out, to: target.delayed, policy: {mode: queue, capacity: 4, overflow: drop_oldest, copy_policy: shared_view}}
  - {id: gate_target, kind: immediate, from: gate.out, to: target.main, policy: {mode: latest, copy_policy: shared_view}}
)");
}

topoexec::GraphSpec deferred_visibility_graph(topoexec::EdgeKind kind) {
  auto graph = delay_visibility_graph();
  for (auto& edge : graph.edges) {
    if (edge.id != "publisher_target_delay") {
      continue;
    }
    edge.id = "publisher_target_" + topoexec::to_string(kind);
    edge.kind = kind;
    if (kind == topoexec::EdgeKind::kState) {
      edge.policy.mode = "latest";
      edge.policy.overflow = "overwrite";
    }
  }
  return graph;
}

topoexec::GraphSpec config_snapshot_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph:
  name: config_snapshot
  kind: internal_test
  config: {profile: alpha}
lanes: {main: {type: event_loop}}
components:
  - id: updater
    type: topoexec.test.ConfigUpdater
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: observer
    type: topoexec.test.ConfigObserver
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: main}
    config: {value: initial}
edges:
  - {id: updater_observer, kind: immediate, from: updater.out, to: observer.in, policy: {mode: latest, copy_policy: shared_view}}
)");
}

topoexec::GraphSpec invalid_config_transaction_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph:
  name: invalid_config_transaction
  kind: internal_test
lanes: {main: {type: event_loop}}
components:
  - id: updater
    type: topoexec.test.ConfigUpdater
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
    config: {target: observer, value: invalid, publish: "false"}
  - id: observer
    type: topoexec.test.ConfigObserver
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
    config: {value: initial}
edges: []
)");
}

topoexec::GraphSpec apply_failure_config_transaction_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph:
  name: apply_failure_config_transaction
  kind: internal_test
lanes: {main: {type: event_loop}}
components:
  - id: updater_a
    type: topoexec.test.ConfigUpdater
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
    config: {target: observer_a, value: updated-a, publish: "false"}
  - id: updater_b
    type: topoexec.test.ConfigUpdater
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
    config: {target: observer_b, value: apply-fail, publish: "false"}
  - id: observer_a
    type: topoexec.test.ConfigObserver
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
    config: {value: initial-a}
  - id: observer_b
    type: topoexec.test.ConfigObserver
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
    config: {value: initial-b}
edges: []
)");
}

topoexec::GraphSpec task_ready_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: task_ready, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - id: source
    type: topoexec.test.TickSource
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: worker
    type: topoexec.test.Forward
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: main}
  - id: join
    type: topoexec.test.BatchTarget
    event_sources: [{type: task_ready, inputs: [ready]}]
    trigger_policy: {type: task_ready, inputs: [ready]}
    execution: {lane: main}
edges:
  - {id: source_worker, kind: immediate, from: source.out, to: worker.in, policy: {mode: latest, copy_policy: shared_view}}
  - {id: worker_join_async, kind: async, from: worker.out, to: join.ready, policy: {mode: queue, capacity: 4, overflow: drop_oldest, copy_policy: shared_view}}
)");
}

topoexec::GraphSpec future_ready_graph() {
  auto graph = task_ready_graph();
  graph.name = "future_ready";
  graph.components.back().event_sources = {topoexec::EventSourceSpec{}};
  graph.components.back().event_sources.front().type = "future_ready";
  graph.components.back().event_sources.front().inputs = {"ready"};
  graph.components.back().trigger_policy.type = "any_input";
  graph.components.back().trigger_policy.inputs = {"ready"};
  return graph;
}

topoexec::GraphSpec request_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: request, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - id: source
    type: topoexec.test.TickSource
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: service
    type: topoexec.test.BatchTarget
    event_sources: [{type: request, inputs: [request]}]
    trigger_policy: {type: request, inputs: [request]}
    execution: {lane: main}
edges:
  - {id: source_service, kind: immediate, from: source.out, to: service.request, policy: {mode: latest, copy_policy: shared_view}}
)");
}

topoexec::GraphSpec all_inputs_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: all_inputs, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - {id: left_source, type: topoexec.test.TickSource, event_sources: [{type: manual}], trigger_policy: {type: manual}, execution: {lane: main}}
  - {id: right_source, type: topoexec.test.TickSource, event_sources: [{type: manual}], trigger_policy: {type: manual}, execution: {lane: main}}
  - id: join
    type: topoexec.test.DelayTarget
    event_sources: [{type: message, inputs: [main, delayed]}]
    trigger_policy: {type: all_inputs, inputs: [main, delayed]}
    execution: {lane: main}
edges:
  - {id: left_join, kind: immediate, from: left_source.out, to: join.main, policy: {mode: latest, copy_policy: shared_view}}
  - {id: right_join, kind: immediate, from: right_source.out, to: join.delayed, policy: {mode: latest, copy_policy: shared_view}}
)");
}

topoexec::GraphSpec time_sync_graph() {
  auto graph = all_inputs_graph();
  graph.name = "time_sync";
  graph.components.back().trigger_policy.type = "time_sync";
  graph.components.back().trigger_policy.sync_slop_ms = 5;
  return graph;
}

topoexec::GraphSpec batch_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: batch, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - id: source
    type: topoexec.test.TickSource
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: batch
    type: topoexec.test.BatchTarget
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: batch, inputs: [in], batch_size: 3}
    execution: {lane: main}
edges:
  - {id: source_batch, kind: immediate, from: source.out, to: batch.in, policy: {mode: queue, capacity: 8, overflow: drop_oldest, copy_policy: shared_view}}
)");
}

topoexec::GraphSpec timer_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: timer, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - id: timer
    type: topoexec.test.TimerRecord
    event_sources: [{type: timer, period_ms: 10}]
    trigger_policy: {type: manual}
    execution: {lane: main}
edges: []
)");
}

topoexec::GraphSpec coalesce_graph(bool coalesce) {
  auto graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: coalesce, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - id: source
    type: topoexec.test.BurstSource
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: target
    type: topoexec.test.BatchTarget
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: main}
edges:
  - {id: source_target, kind: immediate, from: source.out, to: target.in, policy: {mode: queue, capacity: 8, overflow: drop_oldest, copy_policy: shared_view}}
)");
  graph.components.back().trigger_policy.coalesce = coalesce;
  return graph;
}

topoexec::GraphSpec min_interval_graph() {
  auto graph = coalesce_graph(false);
  graph.name = "min_interval";
  graph.components.back().trigger_policy.min_interval_ms = 1000;
  return graph;
}

topoexec::GraphSpec composite_loop_runtime_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: composite_loop_runtime, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - id: source
    type: topoexec.test.TickSource
    boundary: {role: input, descriptor: test}
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: estimator
    type: topoexec.test.LoopEstimator
    event_sources: [{type: message, inputs: [measurement, correction]}]
    trigger_policy: {type: any_input, inputs: [measurement, correction]}
    execution: {lane: main}
  - id: controller
    type: topoexec.test.LoopController
    event_sources: [{type: message, inputs: [state]}]
    trigger_policy: {type: any_input, inputs: [state]}
    execution: {lane: main}
  - id: sink
    type: topoexec.test.BatchTarget
    boundary: {role: output, descriptor: test}
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: main}
edges:
  - {id: source_estimator, kind: immediate, from: source.out, to: estimator.measurement, policy: {mode: latest, copy_policy: shared_view}}
  - {id: estimator_controller, kind: immediate, from: estimator.state, to: controller.state, policy: {mode: latest, copy_policy: shared_view}}
  - {id: controller_estimator, kind: immediate, from: controller.correction, to: estimator.correction, policy: {mode: latest, copy_policy: shared_view}}
  - {id: controller_sink, kind: immediate, from: controller.command, to: sink.in, policy: {mode: latest, copy_policy: shared_view}}
composite_loops:
  - id: estimator_controller_loop
    components: [estimator, controller]
    loop_policy: {type: fixed_point, max_iterations: 2}
)");
}

topoexec::GraphSpec converging_composite_loop_runtime_graph() {
  auto graph = composite_loop_runtime_graph();
  graph.composite_loops.front().loop_policy.max_iterations = 5;
  graph.composite_loops.front().loop_policy.convergence = "single_pass";
  return graph;
}

topoexec::GraphSpec budget_overrun_composite_loop_runtime_graph() {
  auto graph = composite_loop_runtime_graph();
  graph.components[1].type = "topoexec.test.SlowLoopEstimator";
  graph.composite_loops.front().loop_policy.max_iterations = 5;
  graph.composite_loops.front().loop_policy.budget_ms = 1;
  return graph;
}

topoexec::GraphSpec failing_composite_loop_runtime_graph() {
  auto graph = composite_loop_runtime_graph();
  graph.components[2].type = "topoexec.test.FailingLoopController";
  return graph;
}

topoexec::GraphSpec cancelling_composite_loop_runtime_graph() {
  auto graph = composite_loop_runtime_graph();
  graph.components[2].type = "topoexec.test.CancellingLoopController";
  graph.composite_loops.front().loop_policy.max_iterations = 5;
  return graph;
}

topoexec::GraphSpec solver_iteration_composite_loop_runtime_graph(bool report_converged = true) {
  auto graph = composite_loop_runtime_graph();
  graph.components[2].type = "topoexec.test.ResidualLoopController";
  if (!report_converged) {
    graph.components[2].config.values["report_converged"] = "false";
  }
  auto& policy = graph.composite_loops.front().loop_policy;
  policy.type = "solver_iteration";
  policy.max_iterations = 5;
  policy.partial_success.clear();
  return graph;
}

topoexec::GraphSpec throwing_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: throwing, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - id: throwing
    type: topoexec.test.Throwing
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
edges: []
)");
}

topoexec::GraphSpec status_failure_graph(std::string type) {
  topoexec::GraphSpec graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: status_failure, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - id: failing
    type: topoexec.test.ConfigureStatusFailure
    boundary: {role: input_output, descriptor: test}
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
edges: []
)");
  graph.components.front().type = std::move(type);
  return graph;
}

topoexec::GraphSpec fixed_rate_overrun_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: fixed_rate_overrun, kind: runnable}
lanes: {main: {type: fixed_rate, hz: 1000}}
components:
  - id: slow
    type: topoexec.test.SlowManual
    boundary: {role: input_output, descriptor: test}
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
edges: []
)");
}

topoexec::GraphSpec fixed_rate_wall_clock_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: fixed_rate_wall_clock, kind: runnable}
lanes:
  main:
    type: fixed_rate
    wall_clock_enabled: true
    period_ms: 10
    overrun_policy: drop_tick
components:
  - id: slow
    type: topoexec.test.SlowManual
    boundary: {role: input_output, descriptor: test}
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
edges: []
)");
}

topoexec::GraphSpec priority_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: priority_order, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - id: low
    type: topoexec.test.SlowManual
    boundary: {role: input_output, descriptor: test}
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main, priority: low}
  - id: high
    type: topoexec.test.SlowManual
    boundary: {role: input_output, descriptor: test}
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main, priority: high}
edges: []
)");
}

topoexec::GraphSpec cancellation_probe_graph(const std::string& component_type) {
  auto graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: cancellation_probe, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - id: worker
    type: topoexec.test.CancellationProbe
    boundary: {role: input_output, descriptor: test}
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
edges: []
)");
  graph.components.front().type = component_type;
  return graph;
}

topoexec::GraphSpec lifecycle_graph(std::vector<std::pair<std::string, std::string>> components) {
  topoexec::GraphSpec graph;
  graph.schema_version = 1;
  graph.name = "lifecycle";
  graph.kind = "runnable";
  topoexec::LaneSpec lane;
  lane.id = "main";
  lane.type = "event_loop";
  graph.lanes.push_back(std::move(lane));
  for (const auto& [id, type] : components) {
    topoexec::ComponentNodeSpec component;
    component.id = id;
    component.type = type;
    component.event_sources = {topoexec::EventSourceSpec{}};
    component.event_sources.front().type = "manual";
    component.trigger_policy.type = "manual";
    component.execution.lane = "main";
    graph.components.push_back(std::move(component));
  }
  return graph;
}

topoexec::GraphSpec thread_pool_graph(bool reentrant) {
  auto graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: thread_pool_runtime, kind: runnable}
lanes:
  main: {type: event_loop}
  pool: {type: thread_pool, max_threads: 3}
components:
  - id: source
    type: topoexec.test.BurstSource
    boundary: {role: input, descriptor: test}
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: worker
    type: topoexec.test.ThreadPoolProbe
    boundary: {role: output, descriptor: test}
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: pool}
edges:
  - {id: source_worker, kind: immediate, from: source.out, to: worker.in, policy: {mode: queue, capacity: 8, overflow: drop_oldest, copy_policy: shared_view}}
)");
  graph.components.back().execution.reentrant = reentrant;
  return graph;
}

topoexec::GraphSpec health_event_graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph:
  name: health_event_runtime
  kind: runnable
lanes:
  main: {type: event_loop}
components:
  - id: source
    type: topoexec.test.LenientBurstSource
    boundary: {role: input, descriptor: test}
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: target
    type: topoexec.test.BatchTarget
    boundary: {role: output, descriptor: test}
    event_sources: [{type: message, inputs: [in]}]
    trigger_policy: {type: any_input, inputs: [in]}
    execution: {lane: main}
edges:
  - {id: source_target, kind: immediate, from: source.out, to: target.in, policy: {mode: queue, capacity: 1, overflow: drop_oldest, copy_policy: shared_view}}
)");
}

topoexec::GraphSpec async_max_inflight_graph(std::string overflow = "drop_oldest", int max_inflight = 2,
                                             std::string source_type = "topoexec.test.BurstSource") {
  auto graph = topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: async_max_inflight, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - id: source
    type: topoexec.test.BurstSource
    boundary: {role: input, descriptor: test}
    event_sources: [{type: manual}]
    trigger_policy: {type: manual}
    execution: {lane: main}
  - id: join
    type: topoexec.test.BatchTarget
    boundary: {role: output, descriptor: test}
    event_sources: [{type: task_ready, inputs: [ready]}]
    trigger_policy: {type: task_ready, inputs: [ready]}
    execution: {lane: main}
edges:
  - {id: source_join_async, kind: async, from: source.out, to: join.ready, policy: {mode: queue, capacity: 4, overflow: drop_oldest, max_inflight: 2, copy_policy: shared_view}}
)");
  graph.components.front().type = std::move(source_type);
  graph.edges.front().policy.overflow = std::move(overflow);
  graph.edges.front().policy.max_inflight = max_inflight;
  return graph;
}

} // namespace

TEST(Runtime, SchedulerMetricsTrackerCountsObservedTicks) {
  topoexec::SchedulerMetricsTracker tracker;
  topoexec::SchedulerGroupConfig group;
  group.id = "main";
  const auto scheduled_at = std::chrono::steady_clock::now();
  const auto started_at = scheduled_at + std::chrono::milliseconds(2);

  tracker.observe_tick(group, scheduled_at, started_at, std::chrono::milliseconds(3));
  tracker.observe_tick(group, scheduled_at, started_at, std::chrono::milliseconds(0));

  const auto& metrics = tracker.metrics();
  EXPECT_EQ(metrics.tick_count, 2u);
  EXPECT_EQ(metrics.completed_count, 1u);
  EXPECT_GT(metrics.tick_jitter_ms, 0.0);
}

TEST(Runtime, StaticRegistryValidationAndDryRunPass) {
  const auto reg = registry();
  const auto spec = graph();
  const auto validation = topoexec::validate_graph(spec, reg);
  ASSERT_TRUE(validation.ok) << first_error_message(validation);

  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kDryRun;
  options.tick_iterations = 2;
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.instantiated_components, 3u);
  EXPECT_EQ(result.tick_calls, 6u);
}

TEST(Metrics, RuntimeMetricDescriptorRegistryIsStableAndUnique) {
  EXPECT_EQ(std::string(topoexec::kRuntimeMetricSchemaVersion), "1");
  const auto& descriptors = topoexec::runtime_metric_descriptors();
  ASSERT_FALSE(descriptors.empty());

  std::set<std::string> names;
  for (const auto& descriptor : descriptors) {
    EXPECT_FALSE(descriptor.name.empty());
    EXPECT_FALSE(descriptor.kind.empty());
    EXPECT_FALSE(descriptor.unit.empty());
    EXPECT_TRUE(names.insert(descriptor.name).second) << descriptor.name;
  }

  const auto* scheduler = topoexec::find_runtime_metric_descriptor("runtime.scheduler.completed_count");
  ASSERT_NE(scheduler, nullptr);
  EXPECT_NE(std::find(scheduler->labels.begin(), scheduler->labels.end(), "lane"), scheduler->labels.end());
  EXPECT_EQ(topoexec::find_runtime_metric_descriptor("runtime.missing"), nullptr);
}

TEST(Metrics, RuntimeMetricSchemaRejectsHighCardinalityDefaultTags) {
  topoexec::RuntimeMetricSample sample;
  sample.name = "runtime.trace.event_count";
  sample.value = 1.0;
  sample.tags = {"correlation_id"};

  const auto validation = topoexec::validate_runtime_metric_samples({sample});

  EXPECT_FALSE(validation.ok);
  ASSERT_TRUE(has_result_errors(validation));
  EXPECT_NE(first_error_message(validation).find("forbidden default tag correlation_id"), std::string::npos);
}

TEST(Metrics, ExportedRuntimeMetricsMatchDescriptorSchema) {
  const auto reg = registry();
  const auto spec = graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  const auto validation = topoexec::validate_runtime_metric_samples(result.runtime_metrics);

  EXPECT_TRUE(validation.ok) << first_error_message(validation);
}

TEST(Runtime, RunModeExecutesEventRuntimeAndRoutesChannels) {
  const auto reg = registry();
  const auto spec = graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_GE(result.channel_publish_count, 2u);
  EXPECT_GE(result.channel_delivery_count, 2u);
  EXPECT_EQ(result.channel_drop_count, 0u);
  EXPECT_EQ(result.channel_overwrite_count, 2u);
  EXPECT_EQ(result.channel_reject_count, 0u);
  EXPECT_EQ(result.channel_stale_drop_count, 0u);
  EXPECT_EQ(result.channel_deadline_miss_count, 0u);
  EXPECT_TRUE(has_metric(result, "runtime.scheduler.completed_count"));
  EXPECT_TRUE(has_metric(result, "runtime.channel.publish_count", "source_echo"));
  EXPECT_TRUE(has_metric(result, "runtime.channel.delivery_count", "echo_sink"));
  EXPECT_TRUE(has_metric(result, "runtime.channel.max_depth", "source_echo"));
  EXPECT_TRUE(has_trace_event(result, "scheduler_iteration_begin"));
  EXPECT_TRUE(has_trace_event(result, "component_execute_begin"));
  EXPECT_TRUE(has_trace_event(result, "component_execute"));
  EXPECT_TRUE(has_trace_event(result, "component_execute_end"));
  EXPECT_TRUE(has_trace_event(result, "channel_publish"));
  EXPECT_TRUE(has_trace_event(result, "channel_commit"));
  EXPECT_TRUE(has_trace_event_attribute(result, "component_execute_begin", "component_id", "source"));
  EXPECT_TRUE(has_trace_event_attribute(result, "channel_publish", "channel_id", "source_echo"));
  EXPECT_TRUE(has_trace_event_attribute(result, "channel_publish", "edge_kind", "immediate"));
  EXPECT_TRUE(has_trace_event_attribute(result, "channel_commit", "channel_id", "echo_sink"));
  EXPECT_TRUE(has_trace_event_attribute(result, "channel_commit", "edge_kind", "immediate"));
  EXPECT_TRUE(has_component_metric(result, "runtime.component.execution_count", "source"));
  EXPECT_TRUE(has_component_metric(result, "runtime.component.last_duration_ns", "source"));
  EXPECT_TRUE(has_component_metric(result, "runtime.component.max_in_flight_count", "source"));
  EXPECT_TRUE(has_component_metric(result, "runtime.trigger.ready_count", "sink"));
  EXPECT_TRUE(has_metric(result, "runtime.trace.event_count"));
  EXPECT_NE(std::find(result.ticked_components.begin(), result.ticked_components.end(), "sink"),
            result.ticked_components.end());
}

TEST(Runtime, HierarchicalGraphMetricsPreserveExpandedComponentPath) {
  const auto reg = registry();
  const auto spec = hierarchical_runtime_graph();
  const auto validation = topoexec::validate_graph(spec, reg);
  ASSERT_TRUE(validation.ok) << first_error_message(validation);

  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_metric(result, "runtime.channel.publish_count", "cell.source_echo"));
  EXPECT_TRUE(has_metric(result, "runtime.channel.delivery_count", "cell.echo_sink"));
  EXPECT_TRUE(has_component_metric(result, "runtime.component.execution_count", "cell.source"));
  EXPECT_TRUE(has_component_metric(result, "runtime.component.execution_count", "cell.echo"));
  EXPECT_TRUE(has_component_metric(result, "runtime.trigger.ready_count", "cell.sink"));
  EXPECT_TRUE(has_trace_event_attribute(result, "component_execute_begin", "component_id", "cell.source"));
  EXPECT_TRUE(has_trace_event_attribute(result, "channel_publish", "channel_id", "cell.source_echo"));
  EXPECT_NE(std::find(result.ticked_components.begin(), result.ticked_components.end(), "cell.sink"),
            result.ticked_components.end());
}

TEST(Runtime, TraceSchemaAddsTimelineAndCausalityFields) {
  EXPECT_EQ(std::string(topoexec::kRuntimeTraceSchemaVersion), "1");
  const auto reg = delay_registry();
  const auto spec = delay_visibility_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  const auto execute =
      std::find_if(result.trace.begin(), result.trace.end(), [](const topoexec::RuntimeTraceEvent& event) {
        return event.name == "component_execute" && event.component_id == "publisher";
      });
  ASSERT_NE(execute, result.trace.end());
  EXPECT_EQ(execute->phase, "component");
  EXPECT_EQ(execute->lane, "main");
  EXPECT_FALSE(execute->correlation_id.empty());
  EXPECT_EQ(execute->causation_id, "source_publisher#1");

  const auto* publish = first_trace_event(result, "channel_publish");
  ASSERT_NE(publish, nullptr);
  EXPECT_EQ(publish->phase, "channel");
  EXPECT_EQ(publish->channel_id, "source_publisher");
  EXPECT_EQ(publish->component_id, "source");
}

TEST(Runtime, TraceEventsAreOrderedAndDurationsAreLegal) {
  const auto reg = registry();
  const auto spec = graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  ASSERT_FALSE(result.trace.empty());
  std::uint64_t previous_offset = 0;
  for (const auto& event : result.trace) {
    EXPECT_GE(event.start_offset_ns, previous_offset);
    previous_offset = event.start_offset_ns;
    EXPECT_FALSE(event.phase.empty());
    EXPECT_FALSE(event.trace_id.empty());
  }
  const auto* execute = first_trace_event(result, "component_execute");
  ASSERT_NE(execute, nullptr);
  EXPECT_GT(execute->duration_ns, 0u);
}

TEST(Runtime, InMemoryObserverReceivesResultMetricTraceAndError) {
  const auto reg = delay_registry();
  const auto spec = throwing_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::InMemoryRuntimeObserver observer(128);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;
  options.observers.push_back(&observer);

  reset_throwing_component_state();
  const auto result = runner.run(spec, options);

  ASSERT_FALSE(result.ok);
  EXPECT_EQ(observer.results().size(), 1u);
  EXPECT_FALSE(observer.metrics().empty());
  EXPECT_FALSE(observer.trace_events().empty());
  ASSERT_FALSE(observer.runtime_errors().empty());
  EXPECT_EQ(observer.runtime_errors().front().component_id, "throwing");
  EXPECT_EQ(result.observer_failure_count, 0u);
  EXPECT_EQ(result.observer_dropped_event_count, 0u);
}

TEST(Runtime, InMemoryObserverReceivesHealthEvents) {
  const auto reg = delay_registry();
  const auto spec = health_event_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::InMemoryRuntimeObserver observer(128);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;
  options.observers.push_back(&observer);

  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_FALSE(observer.health_events().empty());
  EXPECT_TRUE(has_metric_at_least(result, "runtime.health.event_count", 1.0));
}

TEST(Runtime, ObserverFailureIsRecordedButDoesNotChangeRuntimeSemantics) {
  const auto reg = registry();
  const auto spec = graph();
  topoexec::RuntimeRunner runner(reg);
  FailingRuntimeObserver observer;
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;
  options.observers.push_back(&observer);

  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_GT(observer.metric_calls, 0u);
  EXPECT_EQ(observer.result_calls, 1u);
  EXPECT_GT(result.observer_failure_count, 0u);
  EXPECT_TRUE(has_metric_at_least(result, "runtime.observer.failure_count", 1.0));
  EXPECT_TRUE(std::any_of(result.runtime_errors.begin(), result.runtime_errors.end(), [](const auto& error) {
    return error.phase == "observer" && error.code == "observer_failure" && !error.fatal;
  }));
}

TEST(Runtime, InMemoryObserverDropsBoundedRecordsAndReportsDrops) {
  const auto reg = registry();
  const auto spec = graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::InMemoryRuntimeObserver observer(1);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;
  options.observers.push_back(&observer);

  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  const auto observer_status = observer.status();
  EXPECT_GT(observer_status.dropped_event_count, 0u);
  EXPECT_EQ(result.observer_dropped_event_count, observer_status.dropped_event_count);
  EXPECT_TRUE(has_metric_at_least(result, "runtime.observer.dropped_event_count", 1.0));
  EXPECT_LE(observer.metrics().size(), 1u);
  EXPECT_LE(observer.trace_events().size(), 1u);
  EXPECT_LE(observer.results().size(), 1u);
}

TEST(Runtime, LiveObserveDisabledByDefaultProducesNoEvents) {
  const auto reg = registry();
  const auto spec = graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(result.live_events.empty());
  EXPECT_EQ(result.live_observe_dropped_event_count, 0u);
}

TEST(Runtime, LiveObserveEnabledEmitsLifecycleAndRuntimeEventsWithoutChangingSemantics) {
  const auto reg = registry();
  const auto spec = graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions baseline_options;
  baseline_options.mode = topoexec::RuntimeRunMode::kRun;
  baseline_options.tick_iterations = 1;
  const auto baseline = runner.run(spec, baseline_options);

  topoexec::RuntimeRunnerOptions live_options = baseline_options;
  live_options.live_observe.level = topoexec::runtime_observe::LiveObserveLevel::kSummary;
  live_options.live_observe.event_buffer_capacity = 128;
  live_options.live_observe.stream_id = 4;
  const auto observed = runner.run(spec, live_options);

  ASSERT_TRUE(baseline.ok) << first_error_message(baseline);
  ASSERT_TRUE(observed.ok) << first_error_message(observed);
  EXPECT_EQ(observed.tick_calls, baseline.tick_calls);
  EXPECT_EQ(observed.channel_publish_count, baseline.channel_publish_count);
  EXPECT_EQ(observed.channel_delivery_count, baseline.channel_delivery_count);
  EXPECT_EQ(observed.channel_drop_count, baseline.channel_drop_count);
  EXPECT_EQ(trace_event_names(observed), trace_event_names(baseline));
  EXPECT_EQ(observed.live_observe_dropped_event_count, 0u);
  EXPECT_TRUE(has_live_event(observed, topoexec::runtime_observe::LiveEventKind::kRunStarted));
  EXPECT_TRUE(has_live_event(observed, topoexec::runtime_observe::LiveEventKind::kRunFinished));
  EXPECT_TRUE(has_live_event(observed, topoexec::runtime_observe::LiveEventKind::kComponentBegin));
  EXPECT_TRUE(has_live_event(observed, topoexec::runtime_observe::LiveEventKind::kComponentEnd));
  ASSERT_FALSE(observed.live_events.empty());
  EXPECT_EQ(observed.live_events.front().stream_id, 4u);
}

TEST(Runtime, LiveObserveOverflowReportsDropSummaryWithoutChangingRuntimeResult) {
  const auto reg = registry();
  const auto spec = graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;
  options.live_observe.level = topoexec::runtime_observe::LiveObserveLevel::kDetailed;
  options.live_observe.event_buffer_capacity = 1;

  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_GT(result.live_observe_dropped_event_count, 0u);
  EXPECT_TRUE(has_live_event(result, topoexec::runtime_observe::LiveEventKind::kRunStarted));
  EXPECT_TRUE(has_live_event(result, topoexec::runtime_observe::LiveEventKind::kObserverDropSummary));
}

TEST(Runtime, CorrelationMetadataStaysStableThroughImmediateChain) {
  const auto reg = delay_registry();
  const auto spec = delay_visibility_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  const auto publisher = find_record_snapshot(1, "publisher", "in");
  const auto gate = find_record_snapshot(1, "gate", "in");
  const auto target = find_record_snapshot(1, "target", "main");
  ASSERT_TRUE(publisher.has_value());
  ASSERT_TRUE(gate.has_value());
  ASSERT_TRUE(target.has_value());

  EXPECT_FALSE(publisher->correlation_id.empty());
  EXPECT_EQ(publisher->correlation_id, gate->correlation_id);
  EXPECT_EQ(publisher->correlation_id, target->correlation_id);
  EXPECT_EQ(publisher->transaction_id, publisher->correlation_id);
  EXPECT_EQ(publisher->causation_id, "source_publisher#1");
  EXPECT_EQ(publisher->source_component, "source");
  EXPECT_EQ(publisher->source_port, "out");
  EXPECT_EQ(gate->causation_id, "publisher_gate#1");
  EXPECT_EQ(gate->source_component, "publisher");
  EXPECT_EQ(gate->source_port, "out");
  EXPECT_EQ(target->causation_id, "gate_target#1");
  EXPECT_EQ(target->source_component, "gate");
  EXPECT_EQ(target->source_port, "out");
  EXPECT_EQ(target->trigger_kind, "any_input");
  EXPECT_TRUE(has_trace_event_attribute(result, "component_execute", "correlation_id", publisher->correlation_id));
  EXPECT_TRUE(has_trace_event_attribute(result, "component_execute", "causation_id", "source_publisher#1"));
}

TEST(Runtime, PublishStagesWithoutRecursiveDownstreamExecute) {
  const auto reg = publication_probe_registry();
  const auto spec = publication_probe_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_publication_probe_state();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  const auto& state = publication_probe_state();
  EXPECT_TRUE(state.publish_accepted) << state.publish_reason;
  EXPECT_FALSE(state.sink_seen_before_publish_return);
  EXPECT_FALSE(state.sink_ran_while_publisher_active);
  EXPECT_EQ(state.events,
            std::vector<std::string>({"publisher_begin", "publish_return", "publisher_end", "sink_execute"}));
  EXPECT_EQ(result.committed_publication_count, 1u);
  EXPECT_EQ(result.channel_publish_count, 1u);
  EXPECT_EQ(result.channel_delivery_count, 1u);
}

TEST(Runtime, ImmediateFeedForwardIsVisibleInSameEpochAndMetricsMatch) {
  const auto reg = delay_registry();
  const auto spec = delay_visibility_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_record(1, "publisher", "in", "tick-1"));
  EXPECT_TRUE(has_record(1, "gate", "in", "tick-1"));
  EXPECT_TRUE(has_record(1, "target", "main", "tick-1"));
  EXPECT_FALSE(has_record(1, "target", "delayed"));
  EXPECT_EQ(result.staged_publication_count, 4u);
  EXPECT_EQ(result.committed_publication_count, 3u);
  EXPECT_EQ(result.delayed_publication_count, 1u);
  EXPECT_EQ(result.payload_copy_count, 0u);

  const auto staged = metric_value(result, "runtime.publication.staged");
  const auto committed = metric_value(result, "runtime.publication.committed");
  const auto delayed = metric_value(result, "runtime.publication.delayed");
  ASSERT_TRUE(staged.has_value());
  ASSERT_TRUE(committed.has_value());
  ASSERT_TRUE(delayed.has_value());
  EXPECT_DOUBLE_EQ(*staged, 4.0);
  EXPECT_DOUBLE_EQ(*committed, 3.0);
  EXPECT_DOUBLE_EQ(*delayed, 1.0);
}

TEST(Runtime, DelayEdgeCommitsAtNextEpochBoundary) {
  const auto reg = delay_registry();
  const auto spec = delay_visibility_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;

  reset_runtime_records();
  options.tick_iterations = 1;
  auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_record(1, "target", "main", "tick-1"));
  EXPECT_FALSE(has_record(1, "target", "delayed"));
  EXPECT_EQ(result.staged_publication_count, 4u);
  EXPECT_EQ(result.delayed_publication_count, 1u);
  EXPECT_EQ(result.committed_publication_count, 3u);

  reset_runtime_records();
  options.tick_iterations = 2;
  result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_FALSE(has_record(1, "target", "delayed"));
  EXPECT_TRUE(has_record(2, "target", "delayed", "tick-1"));
  EXPECT_TRUE(has_record(2, "target", "main", "tick-2"));
  EXPECT_EQ(result.staged_publication_count, 8u);
  EXPECT_EQ(result.delayed_publication_count, 2u);
  EXPECT_EQ(result.committed_publication_count, 7u);
}

TEST(Runtime, DelayEdgeCarriesCausationAcrossEpoch) {
  const auto reg = delay_registry();
  const auto spec = delay_visibility_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;

  reset_runtime_records();
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  const auto delayed = find_record_snapshot(2, "target", "delayed");
  ASSERT_TRUE(delayed.has_value());
  EXPECT_EQ(delayed->causation_id, "publisher_target_delay#1");
  EXPECT_EQ(delayed->source_component, "publisher");
  EXPECT_EQ(delayed->source_port, "out");
  EXPECT_EQ(delayed->epoch_id, 2u);
  EXPECT_TRUE(has_trace_event_attribute(result, "component_execute", "causation_id", "publisher_target_delay#1"));
}

TEST(Runtime, HealthEventsExposeChannelBackpressureInRunnerResultAndTrace) {
  const auto reg = delay_registry();
  const auto spec = health_event_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.channel_drop_count, 0u);
  EXPECT_EQ(result.channel_overwrite_count, 2u);
  EXPECT_EQ(result.channel_reject_count, 0u);
  EXPECT_EQ(result.channel_stale_drop_count, 0u);
  EXPECT_TRUE(has_health_event(result, topoexec::HealthEventKind::kBackpressureHighWatermark, "source_target"));
  EXPECT_TRUE(has_health_event(result, topoexec::HealthEventKind::kChannelOverflow, "source_target"));
  EXPECT_GE(result.health_event_count, 2u);
  EXPECT_TRUE(has_metric_at_least(result, "runtime.health.event_count", 2.0));
  EXPECT_TRUE(has_trace_event_attribute(result, "health_event", "kind", "channel_overflow"));
  const auto overflow = std::find_if(result.health_events.begin(), result.health_events.end(), [](const auto& event) {
    return event.kind == topoexec::HealthEventKind::kChannelOverflow;
  });
  ASSERT_NE(overflow, result.health_events.end());
  EXPECT_EQ(overflow->edge_id, "source_target");
  EXPECT_EQ(overflow->policy, "drop_oldest");
  EXPECT_EQ(overflow->occurrence_count, 2u);
}

TEST(Runtime, ChannelDropAggregateBreaksOutRejectedPublications) {
  const auto reg = delay_registry();
  auto spec = health_event_graph();
  spec.edges.front().policy.overflow = "drop_newest";
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  const auto result = runner.run(spec, options);
  ASSERT_FALSE(result.ok);
  ASSERT_TRUE(has_result_errors(result));
  EXPECT_NE(first_error_message(result).find("dropped newest payload"), std::string::npos);
  EXPECT_EQ(result.channel_drop_count, 1u);
  EXPECT_EQ(result.channel_overwrite_count, 0u);
  EXPECT_EQ(result.channel_reject_count, 1u);
  EXPECT_EQ(result.channel_stale_drop_count, 0u);
}

TEST(Runtime, HealthEventsCanBeDisabledByGraphConfig) {
  const auto reg = delay_registry();
  auto spec = health_event_graph();
  spec.config.values["emit_health_events"] = "false";
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.health_event_count, 0u);
  EXPECT_TRUE(result.health_events.empty());
  EXPECT_FALSE(has_trace_event_attribute(result, "health_event", "kind", "channel_overflow"));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.channel.health_event_count", 1.0));
}

TEST(Runtime, HealthEventCapacityBoundsRunnerResult) {
  const auto reg = delay_registry();
  auto spec = health_event_graph();
  spec.config.values["health_event_capacity"] = "1";
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_LE(result.health_events.size(), 1u);
  EXPECT_EQ(result.health_event_count, result.health_events.size());
  EXPECT_GE(result.health_event_dropped_count, 1u);
}

TEST(Runtime, StateAndAsyncEdgesCommitAfterCurrentEpoch) {
  const auto reg = delay_registry();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;

  for (const auto kind : {topoexec::EdgeKind::kState, topoexec::EdgeKind::kAsync}) {
    SCOPED_TRACE(topoexec::to_string(kind));
    const auto spec = deferred_visibility_graph(kind);

    reset_runtime_records();
    options.tick_iterations = 1;
    auto result = runner.run(spec, options);
    ASSERT_TRUE(result.ok) << first_error_message(result);
    EXPECT_TRUE(has_record(1, "target", "main", "tick-1"));
    EXPECT_FALSE(has_record(1, "target", "delayed"));
    EXPECT_EQ(result.staged_publication_count, 4u);
    EXPECT_EQ(result.committed_publication_count, 3u);

    reset_runtime_records();
    options.tick_iterations = 2;
    result = runner.run(spec, options);
    ASSERT_TRUE(result.ok) << first_error_message(result);
    EXPECT_FALSE(has_record(1, "target", "delayed"));
    EXPECT_TRUE(has_record(2, "target", "delayed", "tick-1"));
    EXPECT_EQ(result.staged_publication_count, 8u);
    EXPECT_EQ(result.committed_publication_count, 7u);
    if (kind == topoexec::EdgeKind::kState) {
      EXPECT_EQ(result.state_publication_count, 2u);
      EXPECT_EQ(result.async_publication_count, 0u);
      const auto state = metric_value(result, "runtime.publication.state");
      ASSERT_TRUE(state.has_value());
      EXPECT_DOUBLE_EQ(*state, 2.0);
    } else {
      EXPECT_EQ(result.state_publication_count, 0u);
      EXPECT_EQ(result.async_publication_count, 2u);
      const auto async = metric_value(result, "runtime.publication.async");
      ASSERT_TRUE(async.has_value());
      EXPECT_DOUBLE_EQ(*async, 2.0);
    }
  }
}

TEST(Runtime, StateEdgeKeepsCommittedSnapshotIsolatedUntilNextEpoch) {
  topoexec::EdgeSpec edge;
  edge.id = "state_edge";
  edge.from = "producer.out";
  edge.to = "consumer.state";
  edge.kind = topoexec::EdgeKind::kState;
  edge.has_kind = true;
  edge.policy.mode = "latest";
  edge.policy.overflow = "overwrite";
  edge.policy.copy_policy = "shared_view";

  topoexec::RuntimeChannelBus bus({edge});
  topoexec::RuntimePublicationRouter router(&bus, {edge});

  ASSERT_TRUE(bus.publish("state_edge", topoexec::make_text_payload("old")).accepted);
  ASSERT_TRUE(router.publish_from("producer.out", topoexec::make_text_payload("new")).accepted);

  auto during_epoch = bus.peek_latest_for_component_port("consumer", "state");
  ASSERT_TRUE(during_epoch.ok);
  ASSERT_TRUE(during_epoch.message.has_value());
  EXPECT_EQ(*during_epoch.message->payload, "old");

  router.end_epoch();
  ASSERT_TRUE(router.begin_epoch().accepted);

  auto next_epoch = bus.peek_latest_for_component_port("consumer", "state");
  ASSERT_TRUE(next_epoch.ok);
  ASSERT_TRUE(next_epoch.message.has_value());
  EXPECT_EQ(*next_epoch.message->payload, "new");

  const auto metrics = router.metrics();
  EXPECT_EQ(metrics.state_staged_count, 1u);
  EXPECT_EQ(metrics.state_commit_count, 1u);
}

TEST(Runtime, ComponentConfigUpdatesApplyOnEpochBoundary) {
  const auto reg = delay_registry();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;

  reset_config_observations();
  const auto result = runner.run(config_snapshot_graph(), options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  ASSERT_EQ(config_observations().size(), 2u);
  EXPECT_EQ(config_observations()[0], "initial");
  EXPECT_EQ(config_observations()[1], "updated-1");
  EXPECT_EQ(config_apply_events(), std::vector<std::string>({"observer.apply.updated-1"}));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.config.version", 1.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.config.last_transaction_id", 1.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.config.staged_update_count", 2.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.config.committed_update_count", 1.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.config.snapshot_read_count", 2.0));
}

TEST(Runtime, InvalidComponentConfigTransactionIsRejectedBeforeApply) {
  const auto reg = delay_registry();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;

  reset_config_observations();
  const auto result = runner.run(invalid_config_transaction_graph(), options);

  ASSERT_FALSE(result.ok);
  ASSERT_TRUE(has_result_errors(result));
  EXPECT_NE(first_error_message(result).find("config transaction validation failed for component observer"),
            std::string::npos);
  EXPECT_EQ(config_observations(), std::vector<std::string>({"initial"}));
  EXPECT_TRUE(config_apply_events().empty());
  EXPECT_EQ(metric_value(result, "runtime.config.version").value_or(-1.0), 0.0);
  EXPECT_EQ(metric_value(result, "runtime.config.committed_update_count").value_or(-1.0), 0.0);
  EXPECT_EQ(metric_value(result, "runtime.config.rolled_back_update_count").value_or(-1.0), 1.0);
  EXPECT_EQ(metric_value(result, "runtime.config.rejected_update_count").value_or(-1.0), 1.0);
}

TEST(Runtime, ComponentConfigApplyFailureRollsBackAppliedComponents) {
  const auto reg = delay_registry();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;

  reset_config_observations();
  const auto result = runner.run(apply_failure_config_transaction_graph(), options);

  ASSERT_FALSE(result.ok);
  ASSERT_TRUE(has_result_errors(result));
  EXPECT_NE(first_error_message(result).find("config transaction apply failed for component observer_b"),
            std::string::npos);
  EXPECT_EQ(config_observations(), (std::vector<std::string>{"initial-a", "initial-b"}));
  EXPECT_EQ(config_apply_events(),
            (std::vector<std::string>{"observer_a.apply.updated-a", "observer_b.apply.apply-fail",
                                      "observer_b.apply.initial-b", "observer_a.apply.initial-a"}));
  EXPECT_EQ(metric_value(result, "runtime.config.version").value_or(-1.0), 0.0);
  EXPECT_EQ(metric_value(result, "runtime.config.committed_update_count").value_or(-1.0), 0.0);
  EXPECT_EQ(metric_value(result, "runtime.config.rolled_back_update_count").value_or(-1.0), 2.0);
  EXPECT_EQ(metric_value(result, "runtime.config.rejected_update_count").value_or(-1.0), 2.0);
}

TEST(Runtime, DeterministicTaskExecutorCompletesTasksInOrder) {
  topoexec::TaskExecutorConfig config;
  config.max_inflight = 2;
  topoexec::DeterministicTaskExecutor executor(config);

  const auto first = executor.submit([]() { return topoexec::make_text_payload("one"); });
  const auto second = executor.submit([]() { return topoexec::make_text_payload("two"); });
  const auto completions = executor.run_ready();

  ASSERT_TRUE(first.accepted) << first.reason;
  ASSERT_TRUE(second.accepted) << second.reason;
  ASSERT_EQ(completions.size(), 2u);
  ASSERT_TRUE(completions[0].ok);
  ASSERT_TRUE(completions[1].ok);
  EXPECT_EQ(*completions[0].payload, "one");
  EXPECT_EQ(*completions[1].payload, "two");
  const auto metrics = executor.metrics();
  EXPECT_EQ(metrics.submitted_count, 2u);
  EXPECT_EQ(metrics.completed_count, 2u);
  EXPECT_EQ(metrics.max_inflight_count, 2u);
}

TEST(Runtime, DeterministicTaskExecutorRejectsAndCancelsBoundedBacklog) {
  topoexec::TaskExecutorConfig config;
  config.max_inflight = 1;
  config.overflow = "reject";
  topoexec::DeterministicTaskExecutor executor(config);
  topoexec::HealthEventSink sink(4);
  executor.set_health_event_sink(&sink);

  EXPECT_TRUE(executor.submit([]() { return topoexec::make_text_payload("one"); }).accepted);
  const auto rejected = executor.submit([]() { return topoexec::make_text_payload("two"); });

  EXPECT_FALSE(rejected.accepted);
  EXPECT_EQ(rejected.reason, "task executor queue full");
  EXPECT_EQ(executor.cancel_pending(), 1u);
  const auto metrics = executor.metrics();
  EXPECT_EQ(metrics.rejected_count, 1u);
  EXPECT_EQ(metrics.cancelled_count, 1u);
  const auto events = sink.snapshot();
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events.front().kind, topoexec::HealthEventKind::kTaskReject);
  EXPECT_EQ(events.front().policy, "reject");
  EXPECT_EQ(events.front().reason, "task executor queue full");
}

TEST(Runtime, DeterministicTaskExecutorCancellationTokenCancelsPendingTasks) {
  topoexec::TaskExecutorConfig config;
  config.max_inflight = 1;
  config.queue_capacity = 2;
  topoexec::DeterministicTaskExecutor executor(config);
  topoexec::CancellationSource cancellation;

  EXPECT_TRUE(executor.submit([]() { return topoexec::make_text_payload("one"); }).accepted);
  EXPECT_TRUE(executor.submit([]() { return topoexec::make_text_payload("two"); }).accepted);
  cancellation.request_cancel();
  const auto completions = executor.run_ready(0, cancellation.token());

  EXPECT_TRUE(completions.empty());
  const auto metrics = executor.metrics();
  EXPECT_EQ(metrics.cancelled_count, 2u);
  EXPECT_EQ(metrics.completed_count, 0u);
  EXPECT_EQ(metrics.cancellation_requested_count, 1u);
  EXPECT_EQ(metrics.cancellation_observed_count, 1u);
}

TEST(Runtime, DeterministicTaskExecutorBudgetExceededIsReportedWithoutPreemption) {
  topoexec::TaskExecutorConfig config;
  config.max_inflight = 1;
  config.task_budget = std::chrono::milliseconds(1);
  topoexec::DeterministicTaskExecutor executor(config);

  ASSERT_TRUE(executor
                  .submit([]() {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    return topoexec::make_text_payload("slow");
                  })
                  .accepted);
  const auto completions = executor.run_ready();

  ASSERT_EQ(completions.size(), 1u);
  EXPECT_TRUE(completions.front().ok);
  const auto metrics = executor.metrics();
  EXPECT_EQ(metrics.completed_count, 1u);
  EXPECT_EQ(metrics.timeout_budget_exceeded_count, 1u);
}

TEST(Runtime, ThreadedTaskExecutorRunsBoundedWorkAndReportsCompletions) {
  topoexec::ThreadedTaskExecutorConfig config;
  config.max_workers = 2;
  config.max_inflight = 2;
  config.queue_capacity = 2;
  topoexec::ThreadedTaskExecutor executor(config);

  ASSERT_TRUE(executor.submit([]() { return topoexec::make_text_payload("one"); }).accepted);
  ASSERT_TRUE(executor.submit([]() { return topoexec::make_text_payload("two"); }).accepted);

  ASSERT_TRUE(executor.wait_for_idle(std::chrono::seconds(1)));
  auto completions = executor.run_ready();
  ASSERT_EQ(completions.size(), 2u);
  std::set<std::string> payloads;
  for (const auto& completion : completions) {
    EXPECT_TRUE(completion.ok) << completion.error;
    ASSERT_NE(completion.payload, nullptr);
    payloads.insert(completion.payload->text());
  }
  EXPECT_EQ(payloads, (std::set<std::string>{"one", "two"}));

  const auto metrics = executor.metrics();
  EXPECT_EQ(metrics.submitted_count, 2u);
  EXPECT_EQ(metrics.queued_count, 2u);
  EXPECT_EQ(metrics.completed_count, 2u);
  EXPECT_EQ(metrics.failed_count, 0u);
  EXPECT_EQ(metrics.queue_depth, 0u);
  EXPECT_GE(metrics.max_inflight_count, 1u);
}

TEST(Runtime, ThreadedTaskExecutorCompletedBacklogCountsTowardAdmission) {
  topoexec::ThreadedTaskExecutorConfig config;
  config.max_workers = 1;
  config.max_inflight = 1;
  config.queue_capacity = 0;
  config.overflow = "reject";
  topoexec::ThreadedTaskExecutor executor(config);
  topoexec::HealthEventSink sink(4);
  executor.set_health_event_sink(&sink);

  ASSERT_TRUE(executor.submit([]() { return topoexec::make_text_payload("one"); }).accepted);
  ASSERT_TRUE(executor.wait_for_idle(std::chrono::seconds(1)));
  EXPECT_EQ(executor.metrics().completed_backlog_depth, 1u);

  const auto rejected = executor.submit([]() { return topoexec::make_text_payload("two"); });

  EXPECT_FALSE(rejected.accepted);
  EXPECT_EQ(rejected.reason, "task executor queue full");
  const auto events = sink.snapshot();
  ASSERT_EQ(events.size(), 1u);
  EXPECT_EQ(events.front().kind, topoexec::HealthEventKind::kTaskReject);
  EXPECT_EQ(events.front().depth, 1u);

  const auto completions = executor.run_ready();
  ASSERT_EQ(completions.size(), 1u);
  EXPECT_EQ(executor.metrics().completed_backlog_depth, 0u);
  EXPECT_TRUE(executor.submit([]() { return topoexec::make_text_payload("three"); }).accepted);
}

TEST(Runtime, ThreadedTaskExecutorReportsFailuresWithoutThrowingFromRunReady) {
  topoexec::ThreadedTaskExecutor executor;
  ASSERT_TRUE(
      executor.submit([]() -> topoexec::RuntimePayload { throw std::runtime_error("threaded task boom"); }).accepted);

  ASSERT_TRUE(executor.wait_for_idle(std::chrono::seconds(1)));
  const auto completions = executor.run_ready();

  ASSERT_EQ(completions.size(), 1u);
  EXPECT_FALSE(completions.front().ok);
  EXPECT_EQ(completions.front().error, "threaded task boom");
  EXPECT_EQ(executor.metrics().failed_count, 1u);
}

TEST(Runtime, ThreadedTaskExecutorCancellationRemovesOnlyPendingTasks) {
  topoexec::ThreadedTaskExecutorConfig config;
  config.max_workers = 1;
  config.max_inflight = 1;
  config.queue_capacity = 2;
  topoexec::ThreadedTaskExecutor executor(config);
  std::atomic_bool first_started{false};
  std::atomic_bool release_first{false};

  ASSERT_TRUE(executor
                  .submit([&]() {
                    first_started.store(true);
                    while (!release_first.load()) {
                      std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    return topoexec::make_text_payload("active");
                  })
                  .accepted);
  ASSERT_TRUE(executor.submit([]() { return topoexec::make_text_payload("pending-1"); }).accepted);
  ASSERT_TRUE(executor.submit([]() { return topoexec::make_text_payload("pending-2"); }).accepted);

  for (int attempt = 0; attempt < 100 && !first_started.load(); ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  ASSERT_TRUE(first_started.load());

  EXPECT_EQ(executor.cancel_pending(), 2u);
  release_first.store(true);
  ASSERT_TRUE(executor.wait_for_idle(std::chrono::seconds(1)));

  const auto completions = executor.run_ready();
  ASSERT_EQ(completions.size(), 1u);
  ASSERT_NE(completions.front().payload, nullptr);
  EXPECT_EQ(completions.front().payload->text(), "active");
  const auto metrics = executor.metrics();
  EXPECT_EQ(metrics.cancelled_count, 2u);
  EXPECT_EQ(metrics.completed_count, 1u);
  EXPECT_EQ(metrics.queue_depth, 0u);
}

TEST(Runtime, ThreadedTaskExecutorShutdownDrainsQueuedTasksByDefault) {
  topoexec::ThreadedTaskExecutorConfig config;
  config.max_workers = 1;
  config.max_inflight = 1;
  config.queue_capacity = 2;
  topoexec::ThreadedTaskExecutor executor(config);

  ASSERT_TRUE(executor.submit([]() { return topoexec::make_text_payload("one"); }).accepted);
  ASSERT_TRUE(executor.submit([]() { return topoexec::make_text_payload("two"); }).accepted);
  executor.shutdown();

  const auto completions = executor.run_ready();
  ASSERT_EQ(completions.size(), 2u);
  EXPECT_EQ(executor.metrics().completed_count, 2u);
}

TEST(Runtime, ThreadedTaskExecutorGraphContextPublishesCompletionExactlyOnce) {
  topoexec::ThreadedTaskExecutor executor;
  topoexec::RuntimeChannelBus channels({runtime_edge("worker_join", "worker.done", "join.ready")});
  topoexec::GraphContext context;
  context.channels = &channels;
  context.component_id = "worker";
  context.task_executor = &executor;

  const auto submitted = context.submit_task("done", []() { return topoexec::make_text_payload("complete"); });
  ASSERT_TRUE(submitted.accepted) << submitted.reason;
  ASSERT_TRUE(executor.wait_for_idle(std::chrono::seconds(1)));
  const auto completed = executor.run_ready();
  ASSERT_EQ(completed.size(), 1u);

  const auto messages = channels.consume_for_component("join");
  ASSERT_EQ(messages.size(), 1u);
  EXPECT_EQ(*messages.front().payload, "complete");
  EXPECT_TRUE(channels.consume_for_component("join").empty());
}

TEST(Runtime, DeterministicTaskExecutorReportsFailureAndGraphContextPublishesCompletion) {
  topoexec::DeterministicTaskExecutor executor;
  ASSERT_TRUE(executor.submit([]() -> topoexec::RuntimePayload { throw std::runtime_error("task boom"); }).accepted);
  const auto failed = executor.run_ready();
  ASSERT_EQ(failed.size(), 1u);
  EXPECT_FALSE(failed.front().ok);
  EXPECT_EQ(failed.front().error, "task boom");
  EXPECT_EQ(executor.metrics().failed_count, 1u);

  topoexec::RuntimeChannelBus channels({runtime_edge("worker_join", "worker.done", "join.ready")});
  topoexec::GraphContext context;
  context.channels = &channels;
  context.component_id = "worker";
  context.task_executor = &executor;

  const auto submitted = context.submit_task("done", []() { return topoexec::make_text_payload("complete"); });
  ASSERT_TRUE(submitted.accepted) << submitted.reason;
  const auto completed = executor.run_ready();
  ASSERT_EQ(completed.size(), 1u);
  const auto messages = channels.consume_for_component("join");
  ASSERT_EQ(messages.size(), 1u);
  EXPECT_EQ(*messages.front().payload, "complete");
}

TEST(Runtime, AsyncTaskReadyTriggersDownstreamOnLaterEpoch) {
  const auto reg = delay_registry();
  const auto spec = task_ready_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;

  reset_runtime_records();
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_FALSE(has_record(1, "join", "ready"));
  EXPECT_TRUE(has_record(2, "join", "ready", "tick-1"));
  EXPECT_TRUE(has_trigger_record(2, "join", topoexec::EventKind::kTaskReady, topoexec::TriggerKind::kTaskReady));
  EXPECT_EQ(result.async_publication_count, 2u);
}

TEST(Runtime, AsyncCompletionMaintainsOriginalRequestCorrelation) {
  const auto reg = delay_registry();
  const auto spec = task_ready_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;

  reset_runtime_records();
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  const auto worker = find_record_snapshot(1, "worker", "in");
  const auto join = find_record_snapshot(2, "join", "ready");
  ASSERT_TRUE(worker.has_value());
  ASSERT_TRUE(join.has_value());
  EXPECT_EQ(join->correlation_id, worker->correlation_id);
  EXPECT_EQ(join->causation_id, "worker_join_async#1");
  EXPECT_EQ(join->source_component, "worker");
  EXPECT_EQ(join->source_port, "out");
  EXPECT_EQ(join->trigger_kind, "task_ready");
}

TEST(Runtime, FutureReadyEventSourceUsesFutureReadyEventKind) {
  const auto reg = delay_registry();
  const auto spec = future_ready_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;

  reset_runtime_records();
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_record(2, "join", "ready", "tick-1"));
  EXPECT_TRUE(has_trigger_record(2, "join", topoexec::EventKind::kFutureReady, topoexec::TriggerKind::kAnyInput));
}

TEST(Runtime, RequestTriggerUsesRequestInvocationKind) {
  const auto reg = delay_registry();
  const auto spec = request_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_record(1, "service", "request", "tick-1"));
  EXPECT_TRUE(has_trigger_record(1, "service", topoexec::EventKind::kRequest, topoexec::TriggerKind::kRequest));
  EXPECT_TRUE(has_correlation_record(1, "service", "source_service#1"));
}

TEST(Runtime, RequestTriggerDropsTimedOutPendingMessage) {
  topoexec::RuntimeChannelBus channels({runtime_edge("source_service", "source.out", "service.request")});
  ASSERT_TRUE(channels.publish_from("source.out", topoexec::make_text_payload("expired")).accepted);
  std::this_thread::sleep_for(std::chrono::milliseconds(2));

  BatchTargetComponent service;
  topoexec::GraphContext context;
  context.channels = &channels;
  context.component_id = "service";

  topoexec::ComponentNodeSpec service_spec;
  service_spec.id = "service";
  service_spec.type = "topoexec.test.BatchTarget";
  service_spec.event_sources = {topoexec::EventSourceSpec{}};
  service_spec.event_sources.front().type = "request";
  service_spec.event_sources.front().inputs = {"request"};
  service_spec.trigger_policy.type = "request";
  service_spec.trigger_policy.inputs = {"request"};
  service_spec.trigger_policy.max_latency_ms = 1;
  service_spec.execution.lane = "main";

  topoexec::SchedulerGroupConfig lane;
  lane.id = "main";
  lane.type = "event_loop";
  topoexec::EventRuntime runtime(&channels);
  runtime.add_component({"service", &service, &context, service_spec, lane});
  topoexec::SchedulerRunOptions options;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runtime.run(options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_FALSE(has_component_record(1, "service"));
  ASSERT_NE(result.trigger_metrics.find("service"), result.trigger_metrics.end());
  EXPECT_EQ(result.trigger_metrics.at("service").timeout_drop_count, 1u);
  EXPECT_EQ(result.trigger_metrics.at("service").suppressed_count, 1u);
}

TEST(Runtime, AllInputsWaitsForEveryRequiredPort) {
  const auto reg = delay_registry();
  const auto spec = all_inputs_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_record(1, "join", "main", "tick-1"));
  EXPECT_TRUE(has_record(1, "join", "delayed", "tick-1"));
}

TEST(Runtime, TimeSyncWaitsForInputsAndUsesTimeSyncTriggerKind) {
  const auto reg = delay_registry();
  const auto spec = time_sync_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_record(1, "join", "main", "tick-1"));
  EXPECT_TRUE(has_record(1, "join", "delayed", "tick-1"));
  EXPECT_TRUE(has_trigger_record(1, "join", topoexec::EventKind::kMessage, topoexec::TriggerKind::kTimeSync));
}

TEST(Runtime, TimeSyncDropsOldestOutOfSlopSampleUntilInputsAlign) {
  reset_runtime_records();
  topoexec::RuntimeChannelBus channels(
      {runtime_edge("left_join", "left.out", "join.main"), runtime_edge("right_join", "right.out", "join.delayed")});
  ASSERT_TRUE(channels
                  .publish_from("left.out", topoexec::make_text_payload("left-old"),
                                topoexec::make_event_timestamp(topoexec::TimestampDomain::kSteady, 0))
                  .accepted);
  ASSERT_TRUE(channels
                  .publish_from("right.out", topoexec::make_text_payload("right"),
                                topoexec::make_event_timestamp(topoexec::TimestampDomain::kSteady, 10000000))
                  .accepted);

  topoexec::GraphContext context;
  context.channels = &channels;
  context.component_id = "join";
  DelayTargetComponent join;

  topoexec::ComponentNodeSpec join_spec;
  join_spec.id = "join";
  join_spec.type = "topoexec.test.DelayTarget";
  join_spec.event_sources = {topoexec::EventSourceSpec{}};
  join_spec.event_sources.front().type = "message";
  join_spec.event_sources.front().inputs = {"main", "delayed"};
  join_spec.trigger_policy.type = "time_sync";
  join_spec.trigger_policy.inputs = {"main", "delayed"};
  join_spec.trigger_policy.sync_slop_ms = 5;
  join_spec.execution.lane = "main";

  topoexec::SchedulerGroupConfig lane;
  lane.id = "main";
  lane.type = "event_loop";
  topoexec::EventRuntime runtime(&channels);
  runtime.add_component({"join", &join, &context, join_spec, lane});

  topoexec::SchedulerRunOptions options;
  options.tick_iterations = 2;
  bool aligned_publish_accepted = false;
  std::string aligned_publish_reason;
  options.after_iteration = [&](std::uint64_t iteration) {
    if (iteration == 1u) {
      const auto publish =
          channels.publish_from("left.out", topoexec::make_text_payload("left-aligned"),
                                topoexec::make_event_timestamp(topoexec::TimestampDomain::kSteady, 12000000));
      aligned_publish_accepted = publish.accepted;
      aligned_publish_reason = publish.reason;
    }
  };
  const auto result = runtime.run(options);

  EXPECT_TRUE(aligned_publish_accepted) << aligned_publish_reason;
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_FALSE(has_record(1, "join", "main"));
  EXPECT_FALSE(has_record(1, "join", "delayed"));
  EXPECT_TRUE(has_record(2, "join", "main", "left-aligned"));
  EXPECT_TRUE(has_record(2, "join", "delayed", "right"));
  EXPECT_TRUE(has_trigger_record(2, "join", topoexec::EventKind::kMessage, topoexec::TriggerKind::kTimeSync));
  ASSERT_NE(result.trigger_metrics.find("join"), result.trigger_metrics.end());
  EXPECT_EQ(result.trigger_metrics.at("join").time_sync_drop_count, 1u);
}

TEST(Runtime, BatchTriggerPreservesPartialBatchUntilThreshold) {
  const auto reg = delay_registry();
  const auto spec = batch_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;

  reset_runtime_records();
  options.tick_iterations = 2;
  auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_FALSE(has_component_record(1, "batch"));
  EXPECT_FALSE(has_component_record(2, "batch"));

  reset_runtime_records();
  options.tick_iterations = 3;
  result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_batch_record(3, "batch", {"tick-1", "tick-2", "tick-3"}));
}

TEST(Runtime, BatchTriggerFlushesPartialBatchAfterWindowExpires) {
  auto make_runtime = [](topoexec::RuntimeChannelBus& channels, BatchTargetComponent& batch,
                         topoexec::GraphContext& context, int batch_window_ms) {
    context.channels = &channels;
    context.component_id = "batch";

    topoexec::ComponentNodeSpec batch_spec;
    batch_spec.id = "batch";
    batch_spec.type = "topoexec.test.BatchTarget";
    batch_spec.event_sources = {topoexec::EventSourceSpec{}};
    batch_spec.event_sources.front().type = "message";
    batch_spec.event_sources.front().inputs = {"in"};
    batch_spec.trigger_policy.type = "batch";
    batch_spec.trigger_policy.inputs = {"in"};
    batch_spec.trigger_policy.batch_size = 3;
    batch_spec.trigger_policy.batch_window_ms = batch_window_ms;
    batch_spec.execution.lane = "main";

    topoexec::SchedulerGroupConfig lane;
    lane.id = "main";
    lane.type = "event_loop";
    topoexec::EventRuntime runtime(&channels);
    runtime.add_component({"batch", &batch, &context, batch_spec, lane});
    return runtime;
  };

  {
    reset_runtime_records();
    topoexec::RuntimeChannelBus channels({runtime_edge("source_batch", "source.out", "batch.in")});
    ASSERT_TRUE(channels.publish_from("source.out", topoexec::make_text_payload("one")).accepted);
    ASSERT_TRUE(channels.publish_from("source.out", topoexec::make_text_payload("two")).accepted);
    BatchTargetComponent batch;
    topoexec::GraphContext context;
    auto runtime = make_runtime(channels, batch, context, 1000);
    topoexec::SchedulerRunOptions options;
    options.tick_iterations = 1;

    const auto result = runtime.run(options);

    ASSERT_TRUE(result.ok) << first_error_message(result);
    EXPECT_FALSE(has_component_record(1, "batch"));
  }

  {
    reset_runtime_records();
    topoexec::RuntimeChannelBus channels({runtime_edge("source_batch", "source.out", "batch.in")});
    ASSERT_TRUE(channels.publish_from("source.out", topoexec::make_text_payload("one")).accepted);
    ASSERT_TRUE(channels.publish_from("source.out", topoexec::make_text_payload("two")).accepted);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    BatchTargetComponent batch;
    topoexec::GraphContext context;
    auto runtime = make_runtime(channels, batch, context, 1);
    topoexec::SchedulerRunOptions options;
    options.tick_iterations = 1;

    const auto result = runtime.run(options);

    ASSERT_TRUE(result.ok) << first_error_message(result);
    EXPECT_TRUE(has_batch_record(1, "batch", {"one", "two"}));
    EXPECT_TRUE(has_trigger_record(1, "batch", topoexec::EventKind::kMessage, topoexec::TriggerKind::kBatch));
    ASSERT_NE(result.trigger_metrics.find("batch"), result.trigger_metrics.end());
    EXPECT_EQ(result.trigger_metrics.at("batch").batch_flush_count, 1u);
  }
}

TEST(Runtime, TimerTriggerRunsOncePerSimulatedStep) {
  const auto reg = delay_registry();
  const auto spec = timer_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 3;

  reset_runtime_records();
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_component_record(1, "timer"));
  EXPECT_TRUE(has_component_record(2, "timer"));
  EXPECT_TRUE(has_component_record(3, "timer"));
  EXPECT_EQ(result.tick_calls, 3u);
}

TEST(Runtime, FixedRateSimulatedLaneReportsOverrunMetric) {
  const auto reg = delay_registry();
  const auto spec = fixed_rate_overrun_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.tick_calls, 1u);
  EXPECT_TRUE(std::any_of(result.runtime_metrics.begin(), result.runtime_metrics.end(), [](const auto& metric) {
    return metric.name == "runtime.scheduler.tick_overrun_count" && metric.lane == "main" && metric.value >= 1.0;
  }));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.tick_count", 1.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.max_lateness_ms", 1.0));
  EXPECT_TRUE(has_trace_event(result, "fixed_rate_tick_begin"));
  EXPECT_TRUE(has_trace_event(result, "fixed_rate_tick_end"));
  EXPECT_TRUE(has_trace_event(result, "fixed_rate_overrun"));
}

TEST(Runtime, FixedRateWallClockModeSleepsBetweenTicksWhenOptedIn) {
  const auto reg = delay_registry();
  const auto spec = fixed_rate_wall_clock_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 3;

  reset_runtime_records();
  const auto started_at = std::chrono::steady_clock::now();
  const auto result = runner.run(spec, options);
  const auto elapsed = std::chrono::steady_clock::now() - started_at;

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.tick_calls, 3u);
  EXPECT_TRUE(has_component_record(1, "slow"));
  EXPECT_TRUE(has_component_record(2, "slow"));
  EXPECT_TRUE(has_component_record(3, "slow"));
  EXPECT_GE(elapsed, std::chrono::milliseconds(8));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.tick_count", 3.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.blocked_duration_ms", 1.0));
  EXPECT_TRUE(has_trace_event(result, "fixed_rate_tick_begin"));
  EXPECT_TRUE(has_trace_event(result, "fixed_rate_tick_end"));
}

TEST(Runtime, FixedRateWallClockSleepStopsPromptlyWhenTokenRequested) {
  const auto reg = delay_registry();
  auto spec = fixed_rate_wall_clock_graph();
  spec.lanes.front().period_ms = 250;
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;
  topoexec::SchedulerStopSource stop_source;
  options.stop_token = stop_source.token();
  topoexec::RuntimeRunnerResult result;
  std::atomic_bool run_started{false};

  const auto started_at = std::chrono::steady_clock::now();
  std::thread runtime_thread([&]() {
    run_started.store(true, std::memory_order_release);
    result = runner.run(spec, options);
  });
  while (!run_started.load(std::memory_order_acquire)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  stop_source.request_stop();
  runtime_thread.join();
  const auto elapsed = std::chrono::steady_clock::now() - started_at;

  EXPECT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.scheduler_stop_reason, topoexec::SchedulerStopReason::kStopRequested);
  EXPECT_LT(elapsed, std::chrono::milliseconds(200));
  EXPECT_LE(result.tick_calls, 1u);
}

TEST(Runtime, RuntimePriorityOrdersIndependentReadyComponentsAndDoesNotStarveLowPriority) {
  const auto reg = delay_registry();
  const auto spec = priority_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 3;

  reset_runtime_records();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  const auto high_first = first_record_index(1, "high");
  const auto low_first = first_record_index(1, "low");
  ASSERT_TRUE(high_first.has_value());
  ASSERT_TRUE(low_first.has_value());
  EXPECT_LT(*high_first, *low_first);
  EXPECT_TRUE(has_component_record(3, "low"));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.priority_high_count", 3.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.priority_low_count", 3.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.starvation_guard_count", 0.0));
}

TEST(Runtime, ComponentObservesCooperativeCancellationToken) {
  const auto reg = delay_registry();
  const auto spec = cancellation_probe_graph("topoexec.test.CancellationProbe");
  topoexec::SchedulerStopSource stop_source;
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;
  options.stop_token = stop_source.token();

  reset_runtime_records();
  reset_cancellation_request_hook();
  cancellation_request_hook() = [&]() { stop_source.request_stop(); };
  const auto result = runner.run(spec, options);
  reset_cancellation_request_hook();

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_component_record(1, "worker"));
  EXPECT_TRUE(has_component_metric_at_least(result, "runtime.component.cancellation_requested_count", "worker", 1.0));
  EXPECT_TRUE(has_component_metric_at_least(result, "runtime.component.cancellation_observed_count", "worker", 1.0));
  EXPECT_TRUE(has_trace_event(result, "component_cancellation_requested"));
  EXPECT_TRUE(has_trace_event(result, "component_cancellation_observed"));
}

TEST(Runtime, ComponentIgnoringCancellationReportsBudgetWithoutForcedKill) {
  const auto reg = delay_registry();
  auto spec = cancellation_probe_graph("topoexec.test.CancellationIgnoringSlow");
  spec.components.front().execution.budget_ms = 1;
  topoexec::SchedulerStopSource stop_source;
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;
  options.stop_token = stop_source.token();

  reset_runtime_records();
  reset_cancellation_request_hook();
  cancellation_request_hook() = [&]() { stop_source.request_stop(); };
  const auto result = runner.run(spec, options);
  reset_cancellation_request_hook();

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_component_record(1, "worker"));
  EXPECT_TRUE(has_component_metric_at_least(result, "runtime.component.cancellation_requested_count", "worker", 1.0));
  const auto observed = component_metric_value(result, "runtime.component.cancellation_observed_count", "worker");
  ASSERT_TRUE(observed.has_value());
  EXPECT_EQ(*observed, 0.0);
  EXPECT_TRUE(has_component_metric_at_least(result, "runtime.component.timeout_budget_exceeded_count", "worker", 1.0));
  EXPECT_TRUE(has_trace_event(result, "component_timeout_budget_exceeded"));
}

TEST(Runtime, CoalesceMergesMultiplePendingUpdatesIntoOneInvocation) {
  const auto reg = delay_registry();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  auto result = runner.run(coalesce_graph(false), options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_record(1, "target", "in", "burst-1-1"));
  EXPECT_TRUE(has_record(1, "target", "in", "burst-1-2"));
  EXPECT_TRUE(has_record(1, "target", "in", "burst-1-3"));

  reset_runtime_records();
  result = runner.run(coalesce_graph(true), options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_FALSE(has_record(1, "target", "in", "burst-1-1"));
  EXPECT_FALSE(has_record(1, "target", "in", "burst-1-2"));
  EXPECT_TRUE(has_record(1, "target", "in", "burst-1-3"));
}

TEST(Runtime, MinIntervalSuppressesRepeatedInvocationsInsideInterval) {
  const auto reg = delay_registry();
  const auto spec = min_interval_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;

  reset_runtime_records();
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_record(1, "target", "in", "burst-1-1"));
  EXPECT_FALSE(has_record(1, "target", "in", "burst-1-2"));
  EXPECT_FALSE(has_record(1, "target", "in", "burst-1-3"));
  EXPECT_FALSE(has_record(2, "target", "in"));
}

TEST(Runtime, DebounceTriggerCoalescesPendingEventsWithoutCoalesceFlag) {
  const auto reg = delay_registry();
  auto spec = coalesce_graph(false);
  spec.name = "debounce";
  spec.components.back().trigger_policy.type = "debounce";
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_FALSE(has_record(1, "target", "in", "burst-1-1"));
  EXPECT_FALSE(has_record(1, "target", "in", "burst-1-2"));
  EXPECT_TRUE(has_record(1, "target", "in", "burst-1-3"));
  EXPECT_TRUE(has_trigger_record(1, "target", topoexec::EventKind::kMessage, topoexec::TriggerKind::kDebounce));
  EXPECT_TRUE(has_component_metric_at_least(result, "runtime.trigger.coalesced_count", "target", 1.0));
}

TEST(Runtime, RateLimitTriggerSuppressesRepeatedReadyChecksWithReasonMetric) {
  const auto reg = delay_registry();
  auto spec = min_interval_graph();
  spec.name = "rate_limit";
  spec.components.back().trigger_policy.type = "rate_limit";
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;

  reset_runtime_records();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_record(1, "target", "in", "burst-1-1"));
  EXPECT_FALSE(has_record(1, "target", "in", "burst-1-2"));
  EXPECT_FALSE(has_record(2, "target", "in"));
  EXPECT_TRUE(has_trigger_record(1, "target", topoexec::EventKind::kMessage, topoexec::TriggerKind::kRateLimit));
  EXPECT_TRUE(has_component_metric_at_least(result, "runtime.trigger.rate_limit_suppressed_count", "target", 1.0));
}

TEST(Runtime, RateLimitTriggerBoundsSuppressedPendingMessages) {
  const auto reg = delay_registry();
  auto spec = min_interval_graph();
  spec.name = "rate_limit_pending_bound";
  spec.edges.front().policy.capacity = 2;
  spec.components.back().trigger_policy.type = "rate_limit";
  spec.components.back().trigger_policy.min_interval_ms = 1000;
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 4;

  reset_runtime_records();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_record(1, "target", "in", "burst-1-2"));
  EXPECT_FALSE(has_component_record(2, "target"));
  EXPECT_FALSE(has_component_record(3, "target"));
  EXPECT_FALSE(has_component_record(4, "target"));
  const auto pending_drops = component_metric_value(result, "runtime.trigger.pending_drop_count", "target");
  ASSERT_TRUE(pending_drops.has_value());
  EXPECT_EQ(*pending_drops, 4.0);
  EXPECT_TRUE(has_component_metric_at_least(result, "runtime.trigger.rate_limit_suppressed_count", "target", 1.0));
}

TEST(Runtime, WatermarkTriggerDropsLateSamplesAndReportsMetrics) {
  reset_runtime_records();
  topoexec::RuntimeChannelBus channels({runtime_edge("source_target", "source.out", "target.in")});
  ASSERT_TRUE(channels
                  .publish_from("source.out", topoexec::make_text_payload("new"),
                                topoexec::make_event_timestamp(topoexec::TimestampDomain::kSteady, 100000000))
                  .accepted);
  ASSERT_TRUE(channels
                  .publish_from("source.out", topoexec::make_text_payload("late"),
                                topoexec::make_event_timestamp(topoexec::TimestampDomain::kSteady, 90000000))
                  .accepted);
  ASSERT_TRUE(channels
                  .publish_from("source.out", topoexec::make_text_payload("newer"),
                                topoexec::make_event_timestamp(topoexec::TimestampDomain::kSteady, 103000000))
                  .accepted);

  topoexec::GraphContext context;
  context.channels = &channels;
  context.component_id = "target";
  BatchTargetComponent target;

  topoexec::ComponentNodeSpec target_spec;
  target_spec.id = "target";
  target_spec.type = "topoexec.test.BatchTarget";
  target_spec.event_sources = {topoexec::EventSourceSpec{}};
  target_spec.event_sources.front().type = "message";
  target_spec.event_sources.front().inputs = {"in"};
  target_spec.trigger_policy.type = "watermark";
  target_spec.trigger_policy.inputs = {"in"};
  target_spec.trigger_policy.watermark_lateness_ms = 5;
  target_spec.execution.lane = "main";

  topoexec::SchedulerGroupConfig lane;
  lane.id = "main";
  lane.type = "event_loop";
  topoexec::EventRuntime runtime(&channels);
  runtime.add_component({"target", &target, &context, target_spec, lane});

  topoexec::SchedulerRunOptions options;
  options.tick_iterations = 1;
  const auto result = runtime.run(options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_record(1, "target", "in", "new"));
  EXPECT_FALSE(has_record(1, "target", "in", "late"));
  EXPECT_TRUE(has_record(1, "target", "in", "newer"));
  EXPECT_TRUE(has_trigger_record(1, "target", topoexec::EventKind::kMessage, topoexec::TriggerKind::kWatermark));
  ASSERT_NE(result.trigger_metrics.find("target"), result.trigger_metrics.end());
  EXPECT_EQ(result.trigger_metrics.at("target").late_drop_count, 1u);
}

TEST(Runtime, ConditionTriggerWaitsForDeclarativeReadinessWithoutScripting) {
  reset_runtime_records();
  topoexec::RuntimeChannelBus channels(
      {runtime_edge("left_join", "left.out", "join.main"), runtime_edge("right_join", "right.out", "join.delayed")});
  ASSERT_TRUE(channels.publish_from("left.out", topoexec::make_text_payload("left")).accepted);

  topoexec::GraphContext context;
  context.channels = &channels;
  context.component_id = "join";
  DelayTargetComponent join;

  topoexec::ComponentNodeSpec join_spec;
  join_spec.id = "join";
  join_spec.type = "topoexec.test.DelayTarget";
  join_spec.event_sources = {topoexec::EventSourceSpec{}};
  join_spec.event_sources.front().type = "message";
  join_spec.event_sources.front().inputs = {"main", "delayed"};
  join_spec.trigger_policy.type = "condition";
  join_spec.trigger_policy.inputs = {"main", "delayed"};
  join_spec.trigger_policy.condition = "all_inputs_ready";
  join_spec.execution.lane = "main";

  topoexec::SchedulerGroupConfig lane;
  lane.id = "main";
  lane.type = "event_loop";
  topoexec::EventRuntime runtime(&channels);
  runtime.add_component({"join", &join, &context, join_spec, lane});

  topoexec::SchedulerRunOptions options;
  options.tick_iterations = 2;
  bool right_publish_accepted = false;
  options.after_iteration = [&](std::uint64_t iteration) {
    if (iteration == 1u) {
      right_publish_accepted = channels.publish_from("right.out", topoexec::make_text_payload("right")).accepted;
    }
  };
  const auto result = runtime.run(options);

  EXPECT_TRUE(right_publish_accepted);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_FALSE(has_record(1, "join", "main"));
  EXPECT_TRUE(has_record(2, "join", "main", "left"));
  EXPECT_TRUE(has_record(2, "join", "delayed", "right"));
  EXPECT_TRUE(has_trigger_record(2, "join", topoexec::EventKind::kMessage, topoexec::TriggerKind::kCondition));
  ASSERT_NE(result.trigger_metrics.find("join"), result.trigger_metrics.end());
  EXPECT_EQ(result.trigger_metrics.at("join").condition_suppressed_count, 1u);
}

TEST(Runtime, ConditionTimestampTriggerDropsMissingTimestampHeadItem) {
  reset_runtime_records();
  topoexec::RuntimeChannelBus channels({runtime_edge("source_target", "source.out", "target.in")});
  ASSERT_TRUE(channels.publish_from("source.out", topoexec::make_text_payload("missing-ts")).accepted);
  ASSERT_TRUE(channels
                  .publish_from("source.out", topoexec::make_text_payload("timestamped"),
                                topoexec::make_event_timestamp(topoexec::TimestampDomain::kSteady, 42))
                  .accepted);

  topoexec::GraphContext context;
  context.channels = &channels;
  context.component_id = "target";
  BatchTargetComponent target;

  topoexec::ComponentNodeSpec target_spec;
  target_spec.id = "target";
  target_spec.type = "topoexec.test.BatchTarget";
  target_spec.event_sources = {topoexec::EventSourceSpec{}};
  target_spec.event_sources.front().type = "message";
  target_spec.event_sources.front().inputs = {"in"};
  target_spec.trigger_policy.type = "condition";
  target_spec.trigger_policy.inputs = {"in"};
  target_spec.trigger_policy.condition = "event_timestamp_present";
  target_spec.execution.lane = "main";

  topoexec::SchedulerGroupConfig lane;
  lane.id = "main";
  lane.type = "event_loop";
  topoexec::EventRuntime runtime(&channels);
  runtime.add_component({"target", &target, &context, target_spec, lane});

  topoexec::SchedulerRunOptions options;
  options.tick_iterations = 1;
  const auto result = runtime.run(options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_FALSE(has_record(1, "target", "in", "missing-ts"));
  EXPECT_TRUE(has_record(1, "target", "in", "timestamped"));
  EXPECT_TRUE(has_trigger_record(1, "target", topoexec::EventKind::kMessage, topoexec::TriggerKind::kCondition));
  ASSERT_NE(result.trigger_metrics.find("target"), result.trigger_metrics.end());
  EXPECT_EQ(result.trigger_metrics.at("target").pending_drop_count, 1u);
  EXPECT_EQ(result.trigger_metrics.at("target").condition_suppressed_count, 0u);
}

TEST(Runtime, CompositeLoopRegionOwnsInternalFixedPointIterations) {
  const auto reg = delay_registry();
  const auto spec = composite_loop_runtime_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.loop_iteration_count, 2u);
  EXPECT_EQ(result.loop_max_iteration_hit_count, 1u);
  EXPECT_TRUE(has_component_record(1, "estimator"));
  EXPECT_TRUE(has_component_record(1, "controller"));
  EXPECT_TRUE(has_component_record(1, "sink"));
  EXPECT_TRUE(has_metric(result, "runtime.loop.iterations"));
  EXPECT_TRUE(has_metric(result, "runtime.loop.max_iterations_hit"));
  EXPECT_TRUE(has_trace_event(result, "loop_iteration_begin"));
  EXPECT_TRUE(has_trace_event(result, "loop_iteration_end"));
}

TEST(Runtime, CompositeLoopExternalOutputKeepsCausationMetadata) {
  const auto reg = delay_registry();
  const auto spec = composite_loop_runtime_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  const auto sink = find_record_snapshot(1, "sink", "in");
  ASSERT_TRUE(sink.has_value());
  EXPECT_EQ(sink->causation_id, "controller_sink#1");
  EXPECT_EQ(sink->source_component, "controller");
  EXPECT_EQ(sink->source_port, "command");
  EXPECT_TRUE(has_trace_event_attribute(result, "component_execute", "causation_id", "controller_sink#1"));
}

TEST(Runtime, CompositeLoopConvergenceStopsBeforeMaxIterations) {
  const auto reg = delay_registry();
  const auto spec = converging_composite_loop_runtime_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.loop_iteration_count, 1u);
  EXPECT_EQ(result.loop_converged_count, 1u);
  EXPECT_EQ(result.loop_max_iteration_hit_count, 0u);
  EXPECT_TRUE(has_metric(result, "runtime.loop.converged"));
}

TEST(Runtime, CompositeLoopSolverIterationConvergesByTypedReportAndResidualMetric) {
  const auto reg = delay_registry();
  const auto spec = solver_iteration_composite_loop_runtime_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.loop_iteration_count, 2u);
  EXPECT_EQ(result.loop_converged_count, 1u);
  EXPECT_EQ(result.loop_max_iteration_hit_count, 0u);
  ASSERT_TRUE(result.loop_last_residual.contains("estimator_controller_loop"));
  EXPECT_DOUBLE_EQ(result.loop_last_residual.at("estimator_controller_loop"), 0.05);
  ASSERT_TRUE(result.loop_stop_reason.contains("estimator_controller_loop"));
  EXPECT_EQ(result.loop_stop_reason.at("estimator_controller_loop"), "controller_residual");
  EXPECT_TRUE(has_component_metric(result, "runtime.loop.residual", "estimator_controller_loop"));
  EXPECT_TRUE(has_trace_event_attribute(result, "loop_converged", "reason", "controller_residual"));
  EXPECT_TRUE(has_trace_event_attribute(result, "loop_iteration_end", "residual", "0.05"));
  EXPECT_TRUE(has_component_record(1, "sink"));
  const auto controller = find_record_snapshot(1, "controller", "state");
  ASSERT_TRUE(controller.has_value());
  EXPECT_EQ(controller->loop_id, "estimator_controller_loop");
  EXPECT_EQ(controller->loop_iteration_index, 1u);
  EXPECT_EQ(controller->loop_iteration_number, 2u);
}

TEST(Runtime, CompositeLoopSolverIterationConvergesByResidualThreshold) {
  const auto reg = delay_registry();
  auto spec = solver_iteration_composite_loop_runtime_graph(false);
  spec.composite_loops.front().loop_policy.residual_threshold = 0.1;
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.loop_iteration_count, 2u);
  EXPECT_EQ(result.loop_converged_count, 1u);
  ASSERT_TRUE(result.loop_stop_reason.contains("estimator_controller_loop"));
  EXPECT_EQ(result.loop_stop_reason.at("estimator_controller_loop"), "residual_threshold");
  EXPECT_TRUE(has_trace_event_attribute(result, "loop_converged", "residual_threshold", "0.1"));
  EXPECT_TRUE(has_component_record(1, "sink"));
}

TEST(Runtime, CompositeLoopSolverIterationDiscardsPartialOutputsByDefault) {
  const auto reg = delay_registry();
  auto spec = solver_iteration_composite_loop_runtime_graph(false);
  spec.composite_loops.front().loop_policy.max_iterations = 2;
  spec.composite_loops.front().loop_policy.residual_threshold = 0.01;
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.loop_iteration_count, 2u);
  EXPECT_EQ(result.loop_converged_count, 0u);
  EXPECT_EQ(result.loop_max_iteration_hit_count, 1u);
  EXPECT_EQ(result.loop_output_discarded_count, 1u);
  ASSERT_TRUE(result.loop_stop_reason.contains("estimator_controller_loop"));
  EXPECT_EQ(result.loop_stop_reason.at("estimator_controller_loop"), "max_iterations");
  EXPECT_FALSE(has_component_record(1, "sink"));
  EXPECT_TRUE(has_component_metric(result, "runtime.loop.output_discarded", "estimator_controller_loop"));
  EXPECT_TRUE(has_metric(result, "runtime.publication.composite_discarded"));
  EXPECT_TRUE(has_trace_event(result, "loop_output_discarded"));
}

TEST(Runtime, CompositeLoopSolverIterationCanCommitPartialOutputsWhenPolicyAllows) {
  const auto reg = delay_registry();
  auto spec = solver_iteration_composite_loop_runtime_graph(false);
  spec.composite_loops.front().loop_policy.max_iterations = 2;
  spec.composite_loops.front().loop_policy.residual_threshold = 0.01;
  spec.composite_loops.front().loop_policy.partial_success = "commit_outputs";
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.loop_iteration_count, 2u);
  EXPECT_EQ(result.loop_converged_count, 0u);
  EXPECT_EQ(result.loop_max_iteration_hit_count, 1u);
  EXPECT_EQ(result.loop_output_discarded_count, 0u);
  ASSERT_TRUE(result.loop_stop_reason.contains("estimator_controller_loop"));
  EXPECT_EQ(result.loop_stop_reason.at("estimator_controller_loop"), "max_iterations");
  EXPECT_TRUE(has_component_record(1, "sink"));
  EXPECT_FALSE(has_trace_event(result, "loop_output_discarded"));
}

TEST(Runtime, CompositeLoopSolverIterationCanFailOnPartialSuccessPolicy) {
  const auto reg = delay_registry();
  auto spec = solver_iteration_composite_loop_runtime_graph(false);
  spec.composite_loops.front().loop_policy.max_iterations = 2;
  spec.composite_loops.front().loop_policy.residual_threshold = 0.01;
  spec.composite_loops.front().loop_policy.partial_success = "fail_run";
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);

  EXPECT_FALSE(result.ok);
  EXPECT_EQ(result.scheduler_stop_reason, topoexec::SchedulerStopReason::kError);
  ASSERT_TRUE(has_result_errors(result));
  EXPECT_NE(first_error_message(result).find("stopped without convergence"), std::string::npos);
  EXPECT_EQ(result.loop_output_discarded_count, 1u);
  EXPECT_FALSE(has_component_record(1, "sink"));
}

TEST(Runtime, CompositeLoopDiscardedAsyncOutputsReleaseInflightAccounting) {
  const auto reg = delay_registry();
  auto spec = solver_iteration_composite_loop_runtime_graph(false);
  spec.composite_loops.front().loop_policy.max_iterations = 2;
  spec.composite_loops.front().loop_policy.residual_threshold = 0.01;
  for (auto& edge : spec.edges) {
    if (edge.id == "controller_sink") {
      edge.kind = topoexec::EdgeKind::kAsync;
      edge.policy.max_inflight = 1;
      edge.policy.overflow = "overwrite";
      edge.policy.mode = "queue";
      edge.policy.capacity = 4;
    }
  }
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;

  reset_runtime_records();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.loop_output_discarded_count, 2u);
  EXPECT_FALSE(has_component_record(1, "sink"));
  EXPECT_FALSE(has_component_record(2, "sink"));
  EXPECT_EQ(*metric_value(result, "runtime.async.max_in_flight_count"), 1.0);
  EXPECT_EQ(*metric_value(result, "runtime.async.in_flight_count"), 0.0);
  EXPECT_EQ(*metric_value(result, "runtime.async.cancelled_count"), 2.0);
  EXPECT_TRUE(has_metric(result, "runtime.publication.composite_discarded"));
}

TEST(Runtime, CompositeLoopBudgetOverrunStopsLoopAndReportsMetric) {
  const auto reg = delay_registry();
  const auto spec = budget_overrun_composite_loop_runtime_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.loop_iteration_count, 1u);
  EXPECT_EQ(result.loop_budget_overrun_count, 1u);
  EXPECT_EQ(result.loop_max_iteration_hit_count, 0u);
  EXPECT_TRUE(has_metric(result, "runtime.loop.budget_overrun"));
}

TEST(Runtime, CompositeLoopCancellationStopsBetweenIterations) {
  const auto reg = delay_registry();
  const auto spec = cancelling_composite_loop_runtime_graph();
  topoexec::SchedulerStopSource stop_source;
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;
  options.stop_token = stop_source.token();

  reset_runtime_records();
  reset_cancellation_request_hook();
  cancellation_request_hook() = [&]() { stop_source.request_stop(); };
  const auto result = runner.run(spec, options);
  reset_cancellation_request_hook();

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.scheduler_stop_reason, topoexec::SchedulerStopReason::kStopRequested);
  EXPECT_LT(result.loop_iteration_count, 5u);
  EXPECT_EQ(result.loop_cancellation_requested_count, 1u);
  EXPECT_EQ(result.loop_cancellation_observed_count, 1u);
  EXPECT_EQ(result.loop_max_iteration_hit_count, 0u);
  EXPECT_TRUE(has_metric(result, "runtime.loop.cancellation_requested"));
  EXPECT_TRUE(has_metric(result, "runtime.loop.cancellation_observed"));
  EXPECT_TRUE(has_trace_event(result, "loop_cancellation_requested"));
  EXPECT_TRUE(has_trace_event(result, "loop_cancellation_observed"));
}

TEST(Runtime, CompositeLoopInternalFailureStopsLoopAndSuppressesExternalCommit) {
  const auto reg = delay_registry();
  const auto spec = failing_composite_loop_runtime_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);

  EXPECT_FALSE(result.ok);
  ASSERT_FALSE(result.runtime_errors.empty());
  EXPECT_EQ(result.runtime_errors.front().component_id, "controller");
  EXPECT_EQ(result.runtime_errors.front().phase, "execute");
  EXPECT_EQ(result.loop_error_count, 1u);
  EXPECT_FALSE(has_component_record(1, "sink"));
  EXPECT_TRUE(has_metric(result, "runtime.loop.error"));
  EXPECT_TRUE(has_trace_event(result, "loop_error"));
}

TEST(Runtime, RunUntilIdleStopsAfterMessageDrivenInputsDrain) {
  reset_runtime_records();
  topoexec::RuntimeChannelBus channels({runtime_edge("seeded_input", "source.out", "target.in")});
  const auto publish = channels.publish_from("source.out", topoexec::make_text_payload("seed"));
  ASSERT_TRUE(publish.accepted) << publish.reason;

  topoexec::GraphContext context;
  context.channels = &channels;
  context.component_id = "target";
  BatchTargetComponent target;

  topoexec::ComponentNodeSpec target_spec;
  target_spec.id = "target";
  target_spec.type = "topoexec.test.BatchTarget";
  target_spec.event_sources = {topoexec::EventSourceSpec{}};
  target_spec.event_sources.front().type = "message";
  target_spec.event_sources.front().inputs = {"in"};
  target_spec.trigger_policy.type = "any_input";
  target_spec.trigger_policy.inputs = {"in"};
  target_spec.execution.lane = "main";

  topoexec::SchedulerGroupConfig lane;
  lane.id = "main";
  lane.type = "event_loop";
  topoexec::EventRuntime runtime(&channels);
  runtime.add_component({"target", &target, &context, target_spec, lane});

  topoexec::SchedulerRunOptions options;
  options.tick_iterations = 5;
  options.run_until_idle = true;
  const auto result = runtime.run(options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.tick_calls, 1u);
  EXPECT_EQ(result.iterations, 2u);
  EXPECT_EQ(result.stop_reason, topoexec::SchedulerStopReason::kIdle);
  EXPECT_TRUE(has_record(1, "target", "in", "seed"));
}

TEST(Runtime, StopTokenStopsBeforeExecutingComponentsAndCleansUp) {
  const auto reg = registry();
  const auto spec = graph();
  topoexec::SchedulerStopSource stop_source;
  stop_source.request_stop();

  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 3;
  options.stop_token = stop_source.token();

  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.scheduler_stop_reason, topoexec::SchedulerStopReason::kStopRequested);
  EXPECT_EQ(result.tick_calls, 0u);
  EXPECT_EQ(result.started_components, 3u);
  EXPECT_EQ(result.stopped_components, 3u);
}

TEST(Runtime, ComponentErrorStopsRuntimeAndDeactivatesStartedComponents) {
  const auto reg = delay_registry();
  const auto spec = throwing_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_throwing_component_state();
  const auto result = runner.run(spec, options);
  EXPECT_FALSE(result.ok);
  ASSERT_TRUE(has_result_errors(result));
  ASSERT_FALSE(result.runtime_errors.empty());
  EXPECT_NE(first_error_message(result).find("component throwing failed: boom"), std::string::npos);
  EXPECT_EQ(result.runtime_errors.front().phase, "execute");
  EXPECT_EQ(result.runtime_errors.front().component_id, "throwing");
  EXPECT_EQ(result.runtime_errors.front().code, "component_execute");
  EXPECT_TRUE(result.runtime_errors.front().fatal);
  EXPECT_EQ(result.scheduler_stop_reason, topoexec::SchedulerStopReason::kError);
  EXPECT_EQ(result.started_components, 1u);
  EXPECT_EQ(result.stopped_components, 1u);
  EXPECT_EQ(throwing_deactivate_count(), 1);
}

TEST(Runtime, ConfigureStatusFailureIsObservableWithoutThrowing) {
  const auto reg = delay_registry();
  const auto spec = status_failure_graph("topoexec.test.ConfigureStatusFailure");
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_throwing_component_state();
  const auto result = runner.run(spec, options);

  EXPECT_FALSE(result.ok);
  ASSERT_TRUE(has_result_errors(result));
  ASSERT_FALSE(result.runtime_errors.empty());
  EXPECT_NE(first_error_message(result).find("configure status failed"), std::string::npos);
  EXPECT_EQ(result.runtime_errors.front().phase, "configure");
  EXPECT_EQ(result.runtime_errors.front().component_id, "failing");
  EXPECT_EQ(result.runtime_errors.front().code, "component_configure");
  EXPECT_EQ(result.instantiated_components, 1u);
  EXPECT_EQ(result.configured_components, 0u);
  EXPECT_EQ(result.started_components, 0u);
  EXPECT_EQ(result.stopped_components, 0u);
}

TEST(Runtime, ExecuteStatusFailureStopsRuntimeAndDeactivatesStartedComponents) {
  const auto reg = delay_registry();
  const auto spec = status_failure_graph("topoexec.test.ExecuteStatusFailure");
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_throwing_component_state();
  const auto result = runner.run(spec, options);

  EXPECT_FALSE(result.ok);
  ASSERT_TRUE(has_result_errors(result));
  ASSERT_FALSE(result.runtime_errors.empty());
  EXPECT_NE(first_error_message(result).find("component failing failed: execute status failed"), std::string::npos);
  EXPECT_EQ(result.runtime_errors.front().phase, "execute");
  EXPECT_EQ(result.runtime_errors.front().component_id, "failing");
  EXPECT_EQ(result.runtime_errors.front().code, "component_execute");
  EXPECT_EQ(result.started_components, 1u);
  EXPECT_EQ(result.stopped_components, 1u);
  EXPECT_EQ(status_failure_deactivate_count(), 1);
  EXPECT_TRUE(has_component_metric(result, "runtime.component.error_count", "failing"));
}

TEST(Runtime, ThreadPoolExecuteStatusFailureKeepsStructuredRuntimeError) {
  const auto reg = delay_registry();
  auto spec = status_failure_graph("topoexec.test.ExecuteStatusFailure");
  spec.lanes.front().type = "thread_pool";
  spec.lanes.front().max_threads = 1;
  spec.components.front().execution.reentrant = true;
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_throwing_component_state();
  const auto result = runner.run(spec, options);

  EXPECT_FALSE(result.ok);
  ASSERT_FALSE(result.runtime_errors.empty());
  EXPECT_EQ(result.runtime_errors.front().phase, "execute");
  EXPECT_EQ(result.runtime_errors.front().component_id, "failing");
  EXPECT_EQ(result.runtime_errors.front().code, "component_execute");
  EXPECT_EQ(result.started_components, 1u);
  EXPECT_EQ(result.stopped_components, 1u);
}

TEST(Runtime, ActivateStatusFailureCleansUpStartedComponentsInReverseOrder) {
  const auto reg = delay_registry();
  const auto spec = lifecycle_graph({{"first", "topoexec.test.LifecycleProbe"},
                                     {"failing", "topoexec.test.ActivateStatusFailure"},
                                     {"unreached", "topoexec.test.LifecycleProbe"}});
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_lifecycle_events();
  const auto result = runner.run(spec, options);

  EXPECT_FALSE(result.ok);
  ASSERT_TRUE(has_result_errors(result));
  ASSERT_FALSE(result.runtime_errors.empty());
  EXPECT_NE(first_error_message(result).find("activate status failed"), std::string::npos);
  EXPECT_EQ(result.runtime_errors.front().phase, "activate");
  EXPECT_EQ(result.runtime_errors.front().component_id, "failing");
  EXPECT_EQ(result.runtime_errors.front().code, "component_activate");
  EXPECT_EQ(result.scheduler_stop_reason, topoexec::SchedulerStopReason::kError);
  EXPECT_EQ(result.instantiated_components, 2u);
  EXPECT_EQ(result.configured_components, 2u);
  EXPECT_EQ(result.started_components, 1u);
  EXPECT_EQ(result.stopped_components, 1u);
  EXPECT_EQ(lifecycle_events(), std::vector<std::string>({"first.configure", "first.activate", "failing.configure",
                                                          "failing.activate", "first.deactivate"}));
}

TEST(Runtime, SuccessfulRunDeactivatesComponentsInReverseStartupOrder) {
  const auto reg = delay_registry();
  const auto spec = lifecycle_graph({{"first", "topoexec.test.LifecycleProbe"},
                                     {"second", "topoexec.test.LifecycleProbe"},
                                     {"third", "topoexec.test.LifecycleProbe"}});
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_lifecycle_events();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.started_components, 3u);
  EXPECT_EQ(result.stopped_components, 3u);
  EXPECT_EQ(lifecycle_events(),
            std::vector<std::string>({"first.configure", "first.activate", "second.configure", "second.activate",
                                      "third.configure", "third.activate", "first.execute", "second.execute",
                                      "third.execute", "third.deactivate", "second.deactivate", "first.deactivate"}));
}

TEST(Runtime, DeactivateStatusFailureIsReportedWithComponentAndPhase) {
  const auto reg = delay_registry();
  const auto spec = lifecycle_graph({{"failing", "topoexec.test.DeactivateStatusFailure"}});
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_lifecycle_events();
  const auto result = runner.run(spec, options);

  EXPECT_FALSE(result.ok);
  ASSERT_TRUE(has_result_errors(result));
  ASSERT_FALSE(result.runtime_errors.empty());
  EXPECT_NE(first_error_message(result).find("deactivate status failed"), std::string::npos);
  EXPECT_EQ(result.runtime_errors.front().phase, "deactivate");
  EXPECT_EQ(result.runtime_errors.front().component_id, "failing");
  EXPECT_EQ(result.runtime_errors.front().code, "component_deactivate");
  EXPECT_FALSE(result.runtime_errors.front().fatal);
  EXPECT_EQ(result.started_components, 1u);
  EXPECT_EQ(result.stopped_components, 1u);
  EXPECT_EQ(lifecycle_events(), std::vector<std::string>({"failing.configure", "failing.activate", "failing.execute",
                                                          "failing.deactivate"}));
}

TEST(Runtime, ResetAtEpochStartClearsStateBeforeExecution) {
  const auto reg = delay_registry();
  const auto spec = lifecycle_graph({{"counter", "topoexec.test.StatefulLifecycle"}});
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;
  options.reset_component_ids = {"counter"};
  options.capture_component_state_snapshots = true;

  reset_lifecycle_events();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  ASSERT_EQ(result.component_state_snapshots.size(), 1u);
  const auto& snapshot = result.component_state_snapshots.at("counter");
  ASSERT_NE(snapshot.payload, nullptr);
  EXPECT_EQ(topoexec::require_text_payload(*snapshot.payload), "1");
  EXPECT_EQ(snapshot.version, "counter.v1");
  EXPECT_EQ(result.lifecycle_reset_count, 1u);
  EXPECT_EQ(result.lifecycle_snapshot_count, 1u);
  EXPECT_TRUE(has_metric_at_least(result, "runtime.lifecycle.reset_count", 1.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.lifecycle.snapshot_size_bytes", 1.0));
  EXPECT_TRUE(has_trace_event_attribute(result, "component_reset", "component_id", "counter"));
  EXPECT_TRUE(has_trace_event_attribute(result, "component_snapshot", "component_id", "counter"));
  EXPECT_EQ(lifecycle_events(),
            std::vector<std::string>({"counter.configure", "counter.activate", "counter.reset", "counter.execute.1",
                                      "counter.snapshot.1", "counter.deactivate"}));
}

TEST(Runtime, SnapshotRestoreAppliesBeforeNextExecution) {
  const auto reg = delay_registry();
  const auto spec = lifecycle_graph({{"counter", "topoexec.test.StatefulLifecycle"}});
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions first_options;
  first_options.mode = topoexec::RuntimeRunMode::kRun;
  first_options.tick_iterations = 2;
  first_options.capture_component_state_snapshots = true;

  reset_lifecycle_events();
  const auto first = runner.run(spec, first_options);

  ASSERT_TRUE(first.ok) << first_error_message(first);
  ASSERT_EQ(first.component_state_snapshots.size(), 1u);
  EXPECT_EQ(topoexec::require_text_payload(*first.component_state_snapshots.at("counter").payload), "7");

  topoexec::RuntimeRunnerOptions second_options;
  second_options.mode = topoexec::RuntimeRunMode::kRun;
  second_options.tick_iterations = 1;
  second_options.restore_component_states = first.component_state_snapshots;
  second_options.capture_component_state_snapshots = true;

  reset_lifecycle_events();
  const auto second = runner.run(spec, second_options);

  ASSERT_TRUE(second.ok) << first_error_message(second);
  ASSERT_EQ(second.component_state_snapshots.size(), 1u);
  EXPECT_EQ(topoexec::require_text_payload(*second.component_state_snapshots.at("counter").payload), "8");
  EXPECT_EQ(second.lifecycle_restore_count, 1u);
  EXPECT_TRUE(has_metric_at_least(second, "runtime.lifecycle.restore_count", 1.0));
  EXPECT_TRUE(has_trace_event_attribute(second, "component_restore", "component_id", "counter"));
  EXPECT_EQ(lifecycle_events(),
            std::vector<std::string>({"counter.configure", "counter.activate", "counter.restore.counter.v1",
                                      "counter.execute.8", "counter.snapshot.8", "counter.deactivate"}));
}

TEST(Runtime, RestoreIncompatibleVersionRejectsAndCleansUp) {
  const auto reg = delay_registry();
  const auto spec = lifecycle_graph({{"counter", "topoexec.test.StatefulLifecycle"}});
  topoexec::ComponentStateSnapshot snapshot;
  snapshot.component_type = "topoexec.test.StatefulLifecycle";
  snapshot.version = "counter.v2";
  snapshot.payload = topoexec::make_shared_payload(topoexec::make_text_payload("42", "topoexec.test.CounterState"));
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;
  options.restore_component_states = {{"counter", snapshot}};

  reset_lifecycle_events();
  const auto result = runner.run(spec, options);

  EXPECT_FALSE(result.ok);
  ASSERT_FALSE(result.runtime_errors.empty());
  EXPECT_EQ(result.runtime_errors.front().phase, "restore");
  EXPECT_EQ(result.runtime_errors.front().component_id, "counter");
  EXPECT_EQ(result.runtime_errors.front().code, "component_restore");
  EXPECT_EQ(result.lifecycle_restore_failure_count, 1u);
  EXPECT_EQ(result.started_components, 1u);
  EXPECT_EQ(result.stopped_components, 1u);
  EXPECT_TRUE(has_metric_at_least(result, "runtime.lifecycle.restore_failure_count", 1.0));
  EXPECT_TRUE(has_trace_event_attribute(result, "component_restore", "status", "error"));
  EXPECT_EQ(lifecycle_events(), std::vector<std::string>({"counter.configure", "counter.activate",
                                                          "counter.restore.counter.v2", "counter.deactivate"}));
}

TEST(Runtime, ResetFailureRejectsAndCleansUpStartedComponent) {
  const auto reg = delay_registry();
  const auto spec = lifecycle_graph({{"failing", "topoexec.test.ResetFailure"}});
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;
  options.reset_component_ids = {"failing"};

  reset_lifecycle_events();
  const auto result = runner.run(spec, options);

  EXPECT_FALSE(result.ok);
  ASSERT_FALSE(result.runtime_errors.empty());
  EXPECT_EQ(result.runtime_errors.front().phase, "reset");
  EXPECT_EQ(result.runtime_errors.front().component_id, "failing");
  EXPECT_EQ(result.runtime_errors.front().code, "component_reset");
  EXPECT_EQ(result.lifecycle_reset_failure_count, 1u);
  EXPECT_EQ(result.started_components, 1u);
  EXPECT_EQ(result.stopped_components, 1u);
  EXPECT_TRUE(has_metric_at_least(result, "runtime.lifecycle.reset_failure_count", 1.0));
  EXPECT_TRUE(has_trace_event_attribute(result, "component_reset", "status", "error"));
  EXPECT_EQ(lifecycle_events(),
            std::vector<std::string>({"failing.configure", "failing.activate", "failing.reset", "failing.deactivate"}));
}

TEST(Runtime, ThreadPoolLaneExecutesReentrantInvocationsConcurrently) {
  const auto reg = delay_registry();
  const auto spec = thread_pool_graph(true);
  const auto validation = topoexec::validate_graph(spec, reg);
  ASSERT_TRUE(validation.ok) << first_error_message(validation);

  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  reset_thread_pool_probe_state();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_GE(thread_pool_max_invocations().load(), 2);
  EXPECT_LE(thread_pool_max_invocations().load(), 3);
  EXPECT_TRUE(has_component_metric_at_least(result, "runtime.component.max_in_flight_count", "worker", 2.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.active_count", 2.0));
  EXPECT_TRUE(has_record(1, "worker", "in", "burst-1-1"));
  EXPECT_TRUE(has_record(1, "worker", "in", "burst-1-2"));
  EXPECT_TRUE(has_record(1, "worker", "in", "burst-1-3"));
  EXPECT_TRUE(has_trace_event_attribute_key(result, "component_execute", "worker_id"));
  EXPECT_TRUE(has_trace_event_attribute_key(result, "thread_pool_batch", "worker_ids"));
}

TEST(Runtime, ThreadPoolWorkersPersistAcrossMultipleRuntimeSteps) {
  const auto reg = delay_registry();
  const auto spec = thread_pool_graph(true);
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;

  reset_runtime_records();
  reset_thread_pool_probe_state();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_record(1, "worker", "in", "burst-1-1"));
  EXPECT_TRUE(has_record(2, "worker", "in", "burst-2-1"));
  std::lock_guard lock(thread_pool_probe_mutex());
  EXPECT_GE(thread_pool_thread_ids().size(), 2u);
  EXPECT_LE(thread_pool_thread_ids().size(), 3u);
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.worker_count", 3.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.completed_count", 6.0));
}

TEST(Runtime, ThreadPoolLaneSerializesNonReentrantInvocations) {
  const auto reg = delay_registry();
  const auto spec = thread_pool_graph(false);
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  reset_thread_pool_probe_state();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(thread_pool_max_invocations().load(), 1);
  EXPECT_TRUE(has_component_metric_at_least(result, "runtime.component.max_in_flight_count", "worker", 1.0));
  EXPECT_TRUE(has_record(1, "worker", "in", "burst-1-1"));
  EXPECT_TRUE(has_record(1, "worker", "in", "burst-1-2"));
  EXPECT_TRUE(has_record(1, "worker", "in", "burst-1-3"));
  std::lock_guard lock(thread_pool_probe_mutex());
  EXPECT_LE(thread_pool_thread_ids().size(), 3u);
}

TEST(Runtime, ThreadPoolLaneQueueCapacityRejectsNewestWhenFull) {
  const auto reg = delay_registry();
  auto spec = thread_pool_graph(true);
  spec.lanes.back().max_threads = 1;
  spec.lanes.back().queue_capacity = 1;
  spec.lanes.back().overflow = "drop_newest";
  spec.components.back().execution.priority = "low";
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  reset_thread_pool_probe_state();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(thread_pool_max_invocations().load(), 1);
  EXPECT_TRUE(has_record(1, "worker", "in", "burst-1-1"));
  EXPECT_TRUE(has_record(1, "worker", "in", "burst-1-2"));
  EXPECT_FALSE(has_record(1, "worker", "in", "burst-1-3"));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.rejected_count", 1.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.low_priority_rejected_count", 1.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.queue_capacity", 1.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.queue_depth", 1.0));
  EXPECT_TRUE(has_health_event(result, topoexec::HealthEventKind::kSchedulerReject));
  EXPECT_TRUE(has_trace_event_attribute(result, "health_event", "kind", "scheduler_reject"));
  EXPECT_TRUE(has_trace_event(result, "thread_pool_batch"));
}

TEST(Runtime, ThreadPoolLaneRejectNewKeepsOldestAdmissionWhenFull) {
  const auto reg = delay_registry();
  auto spec = thread_pool_graph(true);
  spec.lanes.back().max_threads = 1;
  spec.lanes.back().queue_capacity = 1;
  spec.lanes.back().overflow = "reject_new";
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  reset_thread_pool_probe_state();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_record(1, "worker", "in", "burst-1-1"));
  EXPECT_TRUE(has_record(1, "worker", "in", "burst-1-2"));
  EXPECT_FALSE(has_record(1, "worker", "in", "burst-1-3"));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.rejected_count", 1.0));
  EXPECT_TRUE(std::any_of(result.health_events.begin(), result.health_events.end(), [](const auto& event) {
    return event.kind == topoexec::HealthEventKind::kSchedulerReject && event.policy == "reject_new";
  }));
}

TEST(Runtime, ThreadPoolLaneQueueCapacityDropsOldestWhenConfigured) {
  const auto reg = delay_registry();
  auto spec = thread_pool_graph(true);
  spec.lanes.back().max_threads = 1;
  spec.lanes.back().queue_capacity = 1;
  spec.lanes.back().overflow = "drop_oldest";
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  reset_thread_pool_probe_state();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_FALSE(has_record(1, "worker", "in", "burst-1-1"));
  EXPECT_TRUE(has_record(1, "worker", "in", "burst-1-2"));
  EXPECT_TRUE(has_record(1, "worker", "in", "burst-1-3"));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.rejected_count", 1.0));
}

TEST(Runtime, ThreadPoolStopWhileQueueNonEmptyDrainsAdmittedWork) {
  const auto reg = delay_registry();
  auto spec = thread_pool_graph(true);
  spec.lanes.back().max_threads = 1;
  spec.lanes.back().queue_capacity = 2;
  topoexec::SchedulerStopSource stop_source;
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;
  options.stop_token = stop_source.token();

  reset_runtime_records();
  reset_thread_pool_probe_state();
  std::atomic_bool run_done{false};
  std::thread stopper([&]() {
    while (!run_done.load() && thread_pool_active_invocations().load() == 0) {
      std::this_thread::yield();
    }
    if (!run_done.load()) {
      stop_source.request_stop();
    }
  });
  const auto result = runner.run(spec, options);
  run_done.store(true);
  stopper.join();

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_EQ(result.scheduler_stop_reason, topoexec::SchedulerStopReason::kStopRequested);
  EXPECT_TRUE(has_record(1, "worker", "in", "burst-1-1"));
  EXPECT_TRUE(has_record(1, "worker", "in", "burst-1-2"));
  EXPECT_TRUE(has_record(1, "worker", "in", "burst-1-3"));
  EXPECT_FALSE(has_component_record(2, "source"));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.scheduler.queue_depth", 2.0));
}

TEST(Runtime, AsyncMaxInflightDropsOldestBeforeChannelCapacity) {
  const auto reg = delay_registry();
  const auto spec = async_max_inflight_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;

  reset_runtime_records();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_FALSE(has_record(2, "join", "ready", "burst-1-1"));
  EXPECT_TRUE(has_record(2, "join", "ready", "burst-1-2"));
  EXPECT_TRUE(has_record(2, "join", "ready", "burst-1-3"));
  EXPECT_EQ(result.channel_publish_count, 2u);
  EXPECT_EQ(result.async_publication_count, 6u);
  EXPECT_TRUE(has_metric_at_least(result, "runtime.async.accepted_count", 6.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.async.overwrite_count", 2.0));
  EXPECT_EQ(*metric_value(result, "runtime.async.dropped_count"), 0.0);
  EXPECT_TRUE(has_metric_at_least(result, "runtime.async.completed_count", 2.0));
  EXPECT_TRUE(has_metric_at_least(result, "runtime.async.max_in_flight_count", 2.0));
  EXPECT_LE(*metric_value(result, "runtime.async.in_flight_count"), 2.0);
  EXPECT_TRUE(has_trace_event(result, "async_admission"));
}

TEST(Runtime, AsyncMaxInflightAcceptsWithinLimitBeforeChannelCapacity) {
  const auto reg = delay_registry();
  const auto spec = async_max_inflight_graph("drop_oldest", 3);
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;

  reset_runtime_records();
  const auto result = runner.run(spec, options);

  ASSERT_TRUE(result.ok) << first_error_message(result);
  EXPECT_TRUE(has_record(2, "join", "ready", "burst-1-1"));
  EXPECT_TRUE(has_record(2, "join", "ready", "burst-1-2"));
  EXPECT_TRUE(has_record(2, "join", "ready", "burst-1-3"));
  EXPECT_EQ(result.channel_publish_count, 3u);
  EXPECT_EQ(result.async_publication_count, 6u);
  EXPECT_EQ(*metric_value(result, "runtime.async.accepted_count"), 6.0);
  EXPECT_EQ(*metric_value(result, "runtime.async.rejected_count"), 0.0);
  EXPECT_EQ(*metric_value(result, "runtime.async.dropped_count"), 0.0);
  EXPECT_EQ(*metric_value(result, "runtime.async.overwrite_count"), 0.0);
  EXPECT_EQ(*metric_value(result, "runtime.async.completed_count"), 3.0);
  EXPECT_EQ(*metric_value(result, "runtime.async.max_in_flight_count"), 3.0);
  EXPECT_LE(*metric_value(result, "runtime.async.in_flight_count"), 3.0);
}

TEST(Runtime, AsyncMaxInflightRejectPoliciesDoNotCommitRejectedCompletions) {
  const auto reg = delay_registry();
  for (const auto& overflow : {"drop_newest", "reject", "fail_fast", "block"}) {
    SCOPED_TRACE(overflow);
    const auto spec = async_max_inflight_graph(overflow, 2, "topoexec.test.LenientBurstSource");
    topoexec::RuntimeRunner runner(reg);
    topoexec::RuntimeRunnerOptions options;
    options.mode = topoexec::RuntimeRunMode::kRun;
    options.tick_iterations = 2;

    reset_runtime_records();
    const auto result = runner.run(spec, options);

    ASSERT_TRUE(result.ok) << first_error_message(result);
    EXPECT_TRUE(has_record(2, "join", "ready", "burst-1-1"));
    EXPECT_TRUE(has_record(2, "join", "ready", "burst-1-2"));
    EXPECT_FALSE(has_record(2, "join", "ready", "burst-1-3"));
    EXPECT_EQ(result.channel_publish_count, 2u);
    EXPECT_EQ(result.async_publication_count, 4u);
    EXPECT_EQ(*metric_value(result, "runtime.async.accepted_count"), 4.0);
    EXPECT_EQ(*metric_value(result, "runtime.async.rejected_count"), 2.0);
    EXPECT_EQ(*metric_value(result, "runtime.async.completed_count"), 2.0);
    EXPECT_EQ(*metric_value(result, "runtime.async.max_in_flight_count"), 2.0);
    EXPECT_LE(*metric_value(result, "runtime.async.in_flight_count"), 2.0);
    if (std::string(overflow) == "drop_newest") {
      EXPECT_EQ(*metric_value(result, "runtime.async.dropped_count"), 2.0);
    } else {
      EXPECT_EQ(*metric_value(result, "runtime.async.dropped_count"), 0.0);
    }
    EXPECT_EQ(*metric_value(result, "runtime.async.overwrite_count"), 0.0);
  }
}
