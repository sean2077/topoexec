#include "topoexec/runtime/runtime_runner.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct Record {
  std::uint64_t epoch{0};
  std::string component;
  std::string port;
  std::string payload;
};

std::vector<Record>& records() {
  static std::vector<Record> values;
  return values;
}

void record(const topoexec::Invocation& invocation, const std::string& component) {
  for (const auto& [port, payload] : invocation.payloads_by_port) {
    records().push_back(Record{invocation.sequence, component, port, payload == nullptr ? "" : payload->text()});
  }
}

void publish_or_throw(topoexec::GraphContext& context, const std::string& port, std::string payload) {
  const auto result = context.publish(port, topoexec::make_text_payload(std::move(payload)));
  if (!result.accepted) {
    throw std::runtime_error(result.reason);
  }
}

class SourceComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.AsyncSource";
    descriptor.role = topoexec::ComponentRole::kInputBoundary;
    descriptor.outputs = {{"out", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    publish_or_throw(context, "out", "job-" + std::to_string(invocation.sequence));
  }
};

class AsyncWorkerComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.AsyncWorker";
    descriptor.inputs = {{"in", topoexec::kTextPayloadSchema}};
    descriptor.outputs = {{"done", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    record(invocation, "worker");
    if (invocation.payload != nullptr) {
      publish_or_throw(context, "done", "done:" + invocation.payload->text() + ":older");
      publish_or_throw(context, "done", "done:" + invocation.payload->text() + ":latest");
    }
  }
};

class JoinComponent : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.AsyncJoin";
    descriptor.role = topoexec::ComponentRole::kOutputBoundary;
    descriptor.inputs = {{"ready", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext&) override {
    record(invocation, "join");
  }
};

topoexec::ComponentRegistry registry() {
  topoexec::ComponentRegistry registry;
  registry.register_component({"topoexec.app.AsyncSource"}, []() { return std::make_unique<SourceComponent>(); });
  registry.register_component({"topoexec.app.AsyncWorker"}, []() { return std::make_unique<AsyncWorkerComponent>(); });
  registry.register_component({"topoexec.app.AsyncJoin"}, []() { return std::make_unique<JoinComponent>(); });
  return registry;
}

topoexec::GraphSpec graph() {
  return topoexec::load_graph_text(R"(
schema_version: 1
graph: {name: app_async_worker, kind: runnable}
lanes: {main: {type: event_loop}}
components:
  - {id: source, type: topoexec.app.AsyncSource, event_sources: [{type: manual}], trigger_policy: {type: manual}, execution: {lane: main}}
  - {id: worker, type: topoexec.app.AsyncWorker, event_sources: [{type: message, inputs: [in]}], trigger_policy: {type: any_input, inputs: [in]}, execution: {lane: main}}
  - {id: join, type: topoexec.app.AsyncJoin, event_sources: [{type: task_ready, inputs: [ready]}], trigger_policy: {type: task_ready, inputs: [ready]}, execution: {lane: main}}
edges:
  - {id: source_worker, kind: immediate, from: source.out, to: worker.in, policy: {mode: latest, copy_policy: shared_view}}
  - {id: worker_join_async, kind: async, from: worker.done, to: join.ready, policy: {mode: queue, capacity: 1, overflow: drop_oldest, max_inflight: 1, copy_policy: shared_view}}
)");
}

bool saw_join_in_second_epoch_only() {
  bool saw_second_epoch = false;
  for (const auto& record : records()) {
    if (record.component != "join") {
      continue;
    }
    if (record.epoch == 1) {
      return false;
    }
    saw_second_epoch =
        saw_second_epoch || (record.epoch == 2 && record.port == "ready" && record.payload == "done:job-1:latest");
  }
  return saw_second_epoch;
}

std::size_t async_overwrite_count(const topoexec::RuntimeRunnerResult& result) {
  for (const auto& sample : result.runtime_metrics) {
    if (sample.name == "runtime.async.overwrite_count") {
      return static_cast<std::size_t>(sample.value);
    }
  }
  return 0u;
}

} // namespace

int main() {
  records().clear();
  const auto components = registry();
  topoexec::RuntimeRunner runner(components);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;
  const auto result = runner.run(graph(), options);
  if (!result.ok) {
    for (const auto& error : result.runtime_errors) {
      std::cerr << "error: " << error.message << "\n";
    }
    return 1;
  }
  if (!saw_join_in_second_epoch_only()) {
    std::cerr << "error: task_ready join did not run in epoch 2 only\n";
    return 2;
  }
  const auto async_overwrites = async_overwrite_count(result);
  if (async_overwrites != 2u) {
    std::cerr << "error: async drop_oldest policy did not overwrite exactly two pending completions; got "
              << async_overwrites << "\n";
    return 3;
  }
  std::cout << "task_ready_epoch=2\n";
  std::cout << "async_publication_count=" << result.async_publication_count << "\n";
  std::cout << "async_overwrite_count=" << async_overwrites << "\n";
  std::cout << "channel_drop_count=" << result.channel_drop_count << "\n";
  std::cout << "channel_overwrite_count=" << result.channel_overwrite_count << "\n";
  return 0;
}
