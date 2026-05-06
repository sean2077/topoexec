#include "topoexec/runtime/channel.hpp"
#include "topoexec/runtime/task_executor.hpp"

#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

topoexec::EdgeSpec edge(std::string id, std::string from, std::string to, topoexec::EdgeKind kind) {
  topoexec::EdgeSpec spec;
  spec.id = std::move(id);
  spec.from = std::move(from);
  spec.to = std::move(to);
  spec.has_kind = true;
  spec.kind = kind;
  spec.policy.mode = "queue";
  spec.policy.capacity = 2;
  spec.policy.overflow = "drop_oldest";
  spec.policy.copy_policy = "shared_view";
  return spec;
}

std::string expect_one_text(const std::vector<topoexec::RuntimeChannelMessage>& messages, const std::string& stage) {
  if (messages.size() != 1u || messages.front().payload == nullptr) {
    throw std::runtime_error(stage + " expected exactly one payload");
  }
  return messages.front().payload->text();
}

void publish_or_throw(topoexec::RuntimeChannelBus& bus, const std::string& endpoint, std::string payload) {
  const auto result = bus.publish_from(endpoint, topoexec::make_text_payload(std::move(payload)));
  if (!result.accepted) {
    throw std::runtime_error(result.reason);
  }
}

} // namespace

int main() {
  topoexec::RuntimeChannelBus bus({
      edge("request_to_validator", "request_boundary.out", "validator.request", topoexec::EdgeKind::kImmediate),
      edge("validator_to_response", "validator.accepted", "response_boundary.ready", topoexec::EdgeKind::kAsync),
  });

  topoexec::TaskExecutor executor;
  topoexec::GraphContext validator_context;
  validator_context.channels = &bus;
  validator_context.task_executor = &executor;
  validator_context.component_id = "validator";

  try {
    publish_or_throw(bus, "request_boundary.out", "req-1");
    const auto request = expect_one_text(bus.consume_for_component("validator"), "validator");
    if (request.rfind("req-", 0u) != 0u) {
      std::cerr << "error: validator rejected malformed request " << request << "\n";
      return 2;
    }

    const auto submitted = validator_context.submit_task("accepted", [request]() {
      return topoexec::make_text_payload("accepted:" + request);
    });
    if (!submitted.accepted) {
      std::cerr << "error: task submission failed: " << submitted.reason << "\n";
      return 3;
    }

    const auto completions = executor.run_ready();
    if (completions.size() != 1u || !completions.front().ok) {
      std::cerr << "error: validator task did not complete exactly once\n";
      return 4;
    }

    const auto response = expect_one_text(bus.consume_for_component("response_boundary"), "response_boundary");
    if (response != "accepted:req-1") {
      std::cerr << "error: unexpected response payload: " << response << "\n";
      return 5;
    }

    const auto metrics = executor.metrics();
    std::cout << "request_payload=" << request << "\n";
    std::cout << "response_payload=" << response << "\n";
    std::cout << "executor_completed_count=" << metrics.completed_count << "\n";
    std::cout << "response_payload_copy_count=" << bus.metrics("validator_to_response").payload_copy_count << "\n";
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << "\n";
    return 1;
  }
  return 0;
}
