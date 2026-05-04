#include "topoexec/runtime/channel.hpp"
#include "topoexec/runtime/event_runtime.hpp"
#include "topoexec/runtime/runtime_runner.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <map>
#include <thread>
#include <utility>

namespace {

struct RuntimeRecord {
  std::uint64_t sequence{0};
  std::string component_id;
  topoexec::EventKind event{topoexec::EventKind::kManual};
  topoexec::TriggerKind trigger{topoexec::TriggerKind::kManual};
  std::vector<std::string> ready_inputs;
  std::map<std::string, std::string> payloads_by_port;
  std::vector<std::string> batch_payloads;
};

std::vector<RuntimeRecord>& runtime_records() {
  static std::vector<RuntimeRecord> records;
  return records;
}

void reset_runtime_records() {
  runtime_records().clear();
}

int& throwing_deactivate_count() {
  static int count = 0;
  return count;
}

void reset_throwing_component_state() {
  throwing_deactivate_count() = 0;
}

void record_invocation(const topoexec::Invocation& invocation, const topoexec::GraphContext& context) {
  RuntimeRecord record;
  record.sequence = invocation.sequence;
  record.component_id = context.component_id;
  record.event = invocation.event;
  record.trigger = invocation.trigger;
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

bool has_trace_event(const topoexec::RuntimeRunnerResult& result, const std::string& name) {
  return std::find(result.trace_events.begin(), result.trace_events.end(), name) != result.trace_events.end();
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
  registry.register_component({"topoexec.test.LoopEstimator"},
                              []() { return std::make_unique<LoopEstimatorComponent>(); });
  registry.register_component({"topoexec.test.SlowLoopEstimator"},
                              []() { return std::make_unique<SlowLoopEstimatorComponent>(); });
  registry.register_component({"topoexec.test.LoopController"},
                              []() { return std::make_unique<LoopControllerComponent>(); });
  registry.register_component({"topoexec.test.Throwing"}, []() { return std::make_unique<ThrowingComponent>(); });
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

} // namespace

TEST(Runtime, StaticRegistryValidationAndDryRunPass) {
  const auto reg = registry();
  const auto spec = graph();
  const auto validation = topoexec::validate_graph(spec, reg);
  ASSERT_TRUE(validation.ok) << validation.errors.front();

  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kDryRun;
  options.tick_iterations = 2;
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_EQ(result.instantiated_components, 3u);
  EXPECT_EQ(result.tick_calls, 6u);
}

TEST(Runtime, RunModeExecutesEventRuntimeAndRoutesChannels) {
  const auto reg = registry();
  const auto spec = graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_GE(result.channel_publish_count, 2u);
  EXPECT_GE(result.channel_delivery_count, 2u);
  EXPECT_TRUE(has_metric(result, "runtime.scheduler.completed_count"));
  EXPECT_TRUE(has_metric(result, "runtime.channel.publish_count", "source_echo"));
  EXPECT_TRUE(has_metric(result, "runtime.channel.delivery_count", "echo_sink"));
  EXPECT_TRUE(has_metric(result, "runtime.channel.max_depth", "source_echo"));
  EXPECT_TRUE(has_trace_event(result, "scheduler_iteration_begin"));
  EXPECT_TRUE(has_trace_event(result, "component_execute_begin"));
  EXPECT_TRUE(has_trace_event(result, "component_execute_end"));
  EXPECT_TRUE(has_trace_event(result, "channel_publish"));
  EXPECT_TRUE(has_trace_event(result, "channel_commit"));
  EXPECT_TRUE(has_metric(result, "runtime.trace.event_count"));
  EXPECT_NE(std::find(result.ticked_components.begin(), result.ticked_components.end(), "sink"),
            result.ticked_components.end());
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
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_TRUE(has_record(1, "target", "main", "tick-1"));
  EXPECT_FALSE(has_record(1, "target", "delayed"));
  EXPECT_EQ(result.delayed_publication_count, 1u);
  EXPECT_EQ(result.committed_publication_count, 3u);

  reset_runtime_records();
  options.tick_iterations = 2;
  result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_FALSE(has_record(1, "target", "delayed"));
  EXPECT_TRUE(has_record(2, "target", "delayed", "tick-1"));
  EXPECT_TRUE(has_record(2, "target", "main", "tick-2"));
  EXPECT_EQ(result.delayed_publication_count, 2u);
  EXPECT_EQ(result.committed_publication_count, 7u);
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
    ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
    EXPECT_TRUE(has_record(1, "target", "main", "tick-1"));
    EXPECT_FALSE(has_record(1, "target", "delayed"));

    reset_runtime_records();
    options.tick_iterations = 2;
    result = runner.run(spec, options);
    ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
    EXPECT_FALSE(has_record(1, "target", "delayed"));
    EXPECT_TRUE(has_record(2, "target", "delayed", "tick-1"));
    if (kind == topoexec::EdgeKind::kState) {
      EXPECT_EQ(result.state_publication_count, 2u);
      EXPECT_EQ(result.async_publication_count, 0u);
    } else {
      EXPECT_EQ(result.state_publication_count, 0u);
      EXPECT_EQ(result.async_publication_count, 2u);
    }
  }
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
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_FALSE(has_record(1, "join", "ready"));
  EXPECT_TRUE(has_record(2, "join", "ready", "tick-1"));
  EXPECT_TRUE(has_trigger_record(2, "join", topoexec::EventKind::kTaskReady, topoexec::TriggerKind::kTaskReady));
  EXPECT_EQ(result.async_publication_count, 2u);
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
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
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
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_TRUE(has_record(1, "service", "request", "tick-1"));
  EXPECT_TRUE(has_trigger_record(1, "service", topoexec::EventKind::kRequest, topoexec::TriggerKind::kRequest));
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
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
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
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
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
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_FALSE(has_record(1, "join", "main"));
  EXPECT_FALSE(has_record(1, "join", "delayed"));
  EXPECT_TRUE(has_record(2, "join", "main", "left-aligned"));
  EXPECT_TRUE(has_record(2, "join", "delayed", "right"));
  EXPECT_TRUE(has_trigger_record(2, "join", topoexec::EventKind::kMessage, topoexec::TriggerKind::kTimeSync));
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
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_FALSE(has_component_record(1, "batch"));
  EXPECT_FALSE(has_component_record(2, "batch"));

  reset_runtime_records();
  options.tick_iterations = 3;
  result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
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

    ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
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

    ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
    EXPECT_TRUE(has_batch_record(1, "batch", {"one", "two"}));
    EXPECT_TRUE(has_trigger_record(1, "batch", topoexec::EventKind::kMessage, topoexec::TriggerKind::kBatch));
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
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_TRUE(has_component_record(1, "timer"));
  EXPECT_TRUE(has_component_record(2, "timer"));
  EXPECT_TRUE(has_component_record(3, "timer"));
  EXPECT_EQ(result.tick_calls, 3u);
}

TEST(Runtime, CoalesceMergesMultiplePendingUpdatesIntoOneInvocation) {
  const auto reg = delay_registry();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  auto result = runner.run(coalesce_graph(false), options);
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_TRUE(has_record(1, "target", "in", "burst-1-1"));
  EXPECT_TRUE(has_record(1, "target", "in", "burst-1-2"));
  EXPECT_TRUE(has_record(1, "target", "in", "burst-1-3"));

  reset_runtime_records();
  result = runner.run(coalesce_graph(true), options);
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
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
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_TRUE(has_record(1, "target", "in", "burst-1-1"));
  EXPECT_FALSE(has_record(1, "target", "in", "burst-1-2"));
  EXPECT_FALSE(has_record(1, "target", "in", "burst-1-3"));
  EXPECT_FALSE(has_record(2, "target", "in"));
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
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
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

TEST(Runtime, CompositeLoopConvergenceStopsBeforeMaxIterations) {
  const auto reg = delay_registry();
  const auto spec = converging_composite_loop_runtime_graph();
  topoexec::RuntimeRunner runner(reg);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 1;

  reset_runtime_records();
  const auto result = runner.run(spec, options);
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_EQ(result.loop_iteration_count, 1u);
  EXPECT_EQ(result.loop_converged_count, 1u);
  EXPECT_EQ(result.loop_max_iteration_hit_count, 0u);
  EXPECT_TRUE(has_metric(result, "runtime.loop.converged"));
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
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
  EXPECT_EQ(result.loop_iteration_count, 1u);
  EXPECT_EQ(result.loop_budget_overrun_count, 1u);
  EXPECT_EQ(result.loop_max_iteration_hit_count, 0u);
  EXPECT_TRUE(has_metric(result, "runtime.loop.budget_overrun"));
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

  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
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
  ASSERT_TRUE(result.ok) << (result.errors.empty() ? "" : result.errors.front());
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
  ASSERT_FALSE(result.errors.empty());
  EXPECT_NE(result.errors.front().find("component throwing failed: boom"), std::string::npos);
  EXPECT_EQ(result.scheduler_stop_reason, topoexec::SchedulerStopReason::kError);
  EXPECT_EQ(result.started_components, 1u);
  EXPECT_EQ(result.stopped_components, 1u);
  EXPECT_EQ(throwing_deactivate_count(), 1);
}
