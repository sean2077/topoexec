#include "topoexec/runtime/component.hpp"

#include "topoexec/runtime/channel.hpp"

#include <exception>
#include <stdexcept>

namespace topoexec {

std::string to_string(ComponentRole role) {
  switch (role) {
  case ComponentRole::kProcessing:
    return "processing";
  case ComponentRole::kInputBoundary:
    return "input";
  case ComponentRole::kOutputBoundary:
    return "output";
  case ComponentRole::kInputOutputBoundary:
    return "input_output";
  }
  return "unknown";
}

std::optional<ComponentRole> parse_component_role(const std::string& value) {
  if (value == "processing") {
    return ComponentRole::kProcessing;
  }
  if (value == "input") {
    return ComponentRole::kInputBoundary;
  }
  if (value == "output") {
    return ComponentRole::kOutputBoundary;
  }
  if (value == "input_output") {
    return ComponentRole::kInputOutputBoundary;
  }
  return std::nullopt;
}

bool component_role_has_input(ComponentRole role) {
  return role == ComponentRole::kInputBoundary || role == ComponentRole::kInputOutputBoundary;
}

bool component_role_has_output(ComponentRole role) {
  return role == ComponentRole::kOutputBoundary || role == ComponentRole::kInputOutputBoundary;
}

RuntimePayloadPtr InputView::read_latest_update(const std::string& port) const {
  if (channels == nullptr) {
    return nullptr;
  }
  auto result = channels->read_latest_update_for_component_port(component_id, port);
  if (!result.ok || !result.message.has_value()) {
    return nullptr;
  }
  return result.message->payload;
}

RuntimePayloadPtr InputView::peek_latest(const std::string& port) const {
  if (channels == nullptr) {
    return nullptr;
  }
  auto result = channels->peek_latest_for_component_port(component_id, port);
  if (!result.ok || !result.message.has_value()) {
    return nullptr;
  }
  return result.message->payload;
}

std::vector<RuntimePayloadPtr> InputView::drain(const std::string& port, std::size_t max_batch) const {
  std::vector<RuntimePayloadPtr> payloads;
  if (channels == nullptr) {
    return payloads;
  }
  for (const auto& message : channels->drain_for_component_port(component_id, port, max_batch)) {
    payloads.push_back(message.payload);
  }
  return payloads;
}

namespace {

InvocationMetadata publication_metadata_for(const GraphContext& context, const std::string& port) {
  auto metadata = context.invocation_metadata;
  metadata.source_component = context.component_id;
  metadata.source_port = port;
  if (metadata.transaction_id.empty()) {
    metadata.transaction_id = metadata.correlation_id;
  }
  return metadata;
}

} // namespace

RuntimeChannelPublishResult GraphContext::publish(const std::string& port, RuntimePayload payload,
                                                  std::optional<EventTimestamp> event_timestamp) const {
  const auto endpoint = component_id + "." + port;
  auto metadata = publication_metadata_for(*this, port);
  if (publisher != nullptr) {
    return publisher->publish_from_with_metadata(endpoint, std::move(payload), std::move(metadata),
                                                 std::move(event_timestamp));
  }
  if (channels != nullptr) {
    return channels->publish_from_with_metadata(endpoint, std::move(payload), std::move(metadata),
                                                std::move(event_timestamp));
  }
  return {false, "graph context has no publisher"};
}

RuntimeChannelPublishResult GraphContext::publish_shared(const std::string& port, RuntimePayloadPtr payload,
                                                         std::optional<EventTimestamp> event_timestamp) const {
  const auto endpoint = component_id + "." + port;
  auto metadata = publication_metadata_for(*this, port);
  if (publisher != nullptr) {
    return publisher->publish_shared_from_with_metadata(endpoint, std::move(payload), std::move(metadata),
                                                        std::move(event_timestamp));
  }
  if (channels != nullptr) {
    return channels->publish_shared_from_with_metadata(endpoint, std::move(payload), std::move(metadata),
                                                       std::move(event_timestamp));
  }
  return {false, "graph context has no publisher"};
}

TaskSubmissionResult GraphContext::submit_task(const std::string& completion_port, ITaskExecutor::Work work) const {
  if (task_executor == nullptr) {
    return {false, 0u, "graph context has no task executor"};
  }
  const auto endpoint = component_id + "." + completion_port;
  auto metadata = publication_metadata_for(*this, completion_port);
  auto* output = publisher != nullptr ? publisher : channels;
  return task_executor->submit(std::move(work), [output, endpoint, metadata](const TaskCompletion& completion) {
    if (output == nullptr || !completion.ok || completion.payload == nullptr) {
      return;
    }
    (void)output->publish_shared_from_with_metadata(endpoint, completion.payload, metadata);
  });
}

bool GraphContext::cancel_requested() const {
  return cancel_token.cancel_requested();
}

void Component::execute(const Invocation&, GraphContext&) {
  throw std::logic_error("component does not implement execute");
}

void Component::reset(GraphContext&) {}

void Component::pause() {}

void Component::resume() {}

Status Component::configure_status(GraphContext& ctx, const ConfigView& config) {
  try {
    configure(ctx, config);
    return Status::success();
  } catch (const std::exception& error) {
    return Status::error(error.what());
  }
}

Status Component::activate_status() {
  try {
    activate();
    return Status::success();
  } catch (const std::exception& error) {
    return Status::error(error.what());
  }
}

Status Component::deactivate_status() {
  try {
    deactivate();
    return Status::success();
  } catch (const std::exception& error) {
    return Status::error(error.what());
  }
}

Status Component::reset_status(GraphContext& ctx) {
  try {
    reset(ctx);
    return Status::success();
  } catch (const std::exception& error) {
    return Status::error(error.what());
  }
}

Status Component::pause_status() {
  try {
    pause();
    return Status::success();
  } catch (const std::exception& error) {
    return Status::error(error.what());
  }
}

Status Component::resume_status() {
  try {
    resume();
    return Status::success();
  } catch (const std::exception& error) {
    return Status::error(error.what());
  }
}

Status Component::execute_status(const Invocation& invocation, GraphContext& ctx) {
  try {
    execute(invocation, ctx);
    return Status::success();
  } catch (const std::exception& error) {
    return Status::error(error.what());
  }
}

Result<ComponentStateSnapshot> Component::snapshot_state() const {
  return ComponentStateSnapshot{};
}

Status Component::restore_state(const ComponentStateSnapshot& snapshot) {
  if (snapshot.payload == nullptr && snapshot.version.empty()) {
    return Status::success();
  }
  return Status::error("component does not implement restore_state");
}

} // namespace topoexec
