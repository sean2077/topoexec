#include "topoexec/runtime/graph_builder.hpp"
#include "topoexec/runtime/runtime_runner.hpp"
#include "topoexec/runtime/state.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

struct PilotRecord {
  std::uint64_t epoch{0};
  std::string component;
  std::string port;
  std::string payload;
};

struct PilotState {
  std::vector<PilotRecord> records;
  std::vector<int> detected_frame_ids;
  std::vector<std::string> commands;
  std::vector<std::string> config_apply_events;
  std::vector<std::string> config_stage_events;
  std::map<int, const void*> camera_frame_addresses;
  std::map<int, const void*> detector_frame_addresses;
  std::size_t pool_detached_count{0};
  std::size_t pool_high_watermark_bytes{0};
};

PilotState& pilot_state() {
  static PilotState state;
  return state;
}

void reset_pilot_state() {
  pilot_state() = {};
}

std::string format_speed(double value) {
  std::ostringstream out;
  out << std::fixed << std::setprecision(2) << value;
  return out.str();
}

double parse_speed_value(const std::string& value) {
  std::size_t parsed = 0;
  const auto speed = std::stod(value, &parsed);
  if (parsed != value.size()) {
    throw std::invalid_argument("trailing characters");
  }
  return speed;
}

topoexec::Status validate_speed_config(const topoexec::ConfigView& config) {
  const auto found = config.values.find("speed");
  if (found == config.values.end()) {
    return topoexec::Status::success();
  }
  try {
    const auto speed = parse_speed_value(found->second);
    if (speed < 0.0 || speed > 1.0) {
      return topoexec::Status::error("speed must be between 0.0 and 1.0");
    }
  } catch (const std::exception&) {
    return topoexec::Status::error("speed must be numeric");
  }
  return topoexec::Status::success();
}

void record_payloads(const topoexec::Invocation& invocation, const std::string& component) {
  for (const auto& [port, payload] : invocation.payloads_by_port) {
    pilot_state().records.push_back(
        PilotRecord{invocation.sequence, component, port, payload == nullptr ? "" : payload->text()});
  }
}

void publish_or_throw(topoexec::GraphContext& context, const std::string& port, topoexec::RuntimePayload payload) {
  const auto published = context.publish(port, std::move(payload));
  if (!published.accepted) {
    throw std::runtime_error(published.reason);
  }
}

void publish_text_or_throw(topoexec::GraphContext& context, const std::string& port, std::string payload) {
  publish_or_throw(context, port, topoexec::make_text_payload(std::move(payload)));
}

class CameraComponent final : public topoexec::Component {
public:
  CameraComponent() {
    topoexec::BufferPoolConfig config;
    config.fixed_block_size = 64;
    config.alignment = 8;
    config.max_bytes = 512;
    pool_ = topoexec::BufferPool(config);
  }

  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.RobotCellCamera";
    descriptor.name = "robot_cell_camera";
    descriptor.role = topoexec::ComponentRole::kInputBoundary;
    descriptor.outputs = {{"frame", topoexec::kFrameViewPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation&, topoexec::GraphContext& context) override {
    // Publish a two-frame burst every epoch. The async edge downstream is bounded
    // to one in-flight frame and intentionally drops the older burst member.
    for (int burst = 0; burst < 2; ++burst) {
      const auto frame_id = ++frame_id_;
      auto loan = pool_.loan_frame(64, 8, 8, 8, "gray8");
      if (!loan.valid()) {
        throw std::runtime_error("camera buffer pool exhausted");
      }
      auto& view = loan.view();
      if (view.data() == nullptr || view.size == 0u) {
        throw std::runtime_error("camera loaned invalid frame");
      }
      view.buffer->data()[0] = static_cast<std::uint8_t>(frame_id);
      pilot_state().camera_frame_addresses[frame_id] = view.payload_address();
      publish_or_throw(context, "frame", topoexec::make_frame_payload(loan.detach()));
      const auto stats = pool_.stats();
      pilot_state().pool_detached_count = stats.detached_count;
      pilot_state().pool_high_watermark_bytes = stats.high_watermark_bytes;
    }
  }

private:
  topoexec::BufferPool pool_;
  int frame_id_{0};
};

class ConfigTunerComponent final : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.RobotCellConfigTuner";
    descriptor.name = "robot_cell_config_tuner";
    descriptor.role = topoexec::ComponentRole::kInputBoundary;
    topoexec::ConfigFieldSpec target;
    target.name = "target";
    target.kind = topoexec::ConfigValueKind::kString;
    target.required = true;
    topoexec::ConfigFieldSpec speed;
    speed.name = "speed";
    speed.kind = topoexec::ConfigValueKind::kString;
    speed.required = true;
    descriptor.config_fields = {target, speed};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView& config) override {
    target_ = config.values.contains("target") ? config.values.at("target") : "controller";
    speed_ = config.values.contains("speed") ? config.values.at("speed") : "0.40";
  }

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    if (context.config_store == nullptr) {
      throw std::runtime_error("config tuner requires ConfigSnapshotStore");
    }
    topoexec::ConfigView update;
    update.values["speed"] = speed_;
    const auto staged = context.config_store->stage_component_config_update(target_, update);
    if (!staged.accepted) {
      throw std::runtime_error(staged.reason);
    }
    pilot_state().config_stage_events.push_back("epoch=" + std::to_string(invocation.sequence) + ":" + target_ +
                                                ".speed=" + speed_);
  }

private:
  std::string target_{"controller"};
  std::string speed_{"0.40"};
};

class DetectorComponent final : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.RobotCellDetector";
    descriptor.name = "robot_cell_detector";
    descriptor.inputs = {{"frame", topoexec::kFrameViewPayloadSchema, {}, topoexec::PortMultiplicity::kSingle, true}};
    descriptor.outputs = {{"target", topoexec::kTextPayloadSchema}, {"obstacle_state", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    if (invocation.payload == nullptr) {
      return;
    }
    const auto* frame = topoexec::try_payload_as<topoexec::FrameView>(*invocation.payload);
    if (frame == nullptr || !frame->valid() || frame->data() == nullptr) {
      throw std::runtime_error("detector received invalid frame");
    }
    const auto frame_id = static_cast<int>(frame->data()[0]);
    pilot_state().detected_frame_ids.push_back(frame_id);
    pilot_state().detector_frame_addresses[frame_id] = frame->payload_address();
    if (const auto camera = pilot_state().camera_frame_addresses.find(frame_id);
        camera == pilot_state().camera_frame_addresses.end() || camera->second != frame->payload_address()) {
      throw std::runtime_error("frame payload address was not preserved from camera to detector");
    }
    publish_text_or_throw(context, "target", "target:frame-" + std::to_string(frame_id));
    publish_text_or_throw(context, "obstacle_state", "obstacle:clear:frame-" + std::to_string(frame_id));
  }
};

class PlannerComponent final : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.RobotCellPlanner";
    descriptor.name = "robot_cell_planner";
    descriptor.inputs = {{"target", topoexec::kTextPayloadSchema},
                         {"obstacle_state", topoexec::kTextPayloadSchema},
                         {"last_command", topoexec::kTextPayloadSchema}};
    descriptor.outputs = {{"plan", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    record_payloads(invocation, "planner");
    std::optional<std::string> target;
    std::optional<std::string> obstacle_state;
    std::optional<std::string> last_command;
    for (const auto& [port, payload] : invocation.payloads_by_port) {
      if (payload == nullptr) {
        continue;
      }
      if (port == "target") {
        target = payload->text();
      } else if (port == "obstacle_state") {
        obstacle_state = payload->text();
      } else if (port == "last_command") {
        last_command = payload->text();
      }
    }
    if (!target && !obstacle_state && !last_command) {
      return;
    }
    std::string plan = target ? "pick:" + *target : "hold";
    if (obstacle_state) {
      plan += ":" + *obstacle_state;
    }
    if (last_command) {
      plan += ":feedback=" + *last_command;
    }
    publish_text_or_throw(context, "plan", std::move(plan));
  }
};

class ControllerComponent final : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.RobotCellController";
    descriptor.name = "robot_cell_controller";
    descriptor.inputs = {{"plan", topoexec::kTextPayloadSchema}};
    descriptor.outputs = {{"command", topoexec::kTextPayloadSchema}, {"last_command", topoexec::kTextPayloadSchema}};
    topoexec::ConfigFieldSpec speed;
    speed.name = "speed";
    speed.kind = topoexec::ConfigValueKind::kDouble;
    speed.min_value = 0.0;
    speed.max_value = 1.0;
    descriptor.config_fields = {speed};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView& config) override {
    const auto found = config.values.find("speed");
    if (found != config.values.end()) {
      speed_ = parse_speed_value(found->second);
    }
  }

  topoexec::Status validate_config(const topoexec::ConfigView& config) const override {
    return validate_speed_config(config);
  }

  topoexec::Status apply_config(topoexec::GraphContext& context, const topoexec::ConfigView& config) override {
    auto validation = validate_config(config);
    if (!validation.ok()) {
      return validation;
    }
    const auto found = config.values.find("speed");
    if (found != config.values.end()) {
      speed_ = parse_speed_value(found->second);
      pilot_state().config_apply_events.push_back(context.component_id + ".speed=" + format_speed(speed_));
    }
    return topoexec::Status::success();
  }

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext& context) override {
    record_payloads(invocation, "controller");
    if (invocation.payload == nullptr) {
      return;
    }
    ++command_count_;
    const auto command = "move:" + invocation.payload->text() + ":speed=" + format_speed(speed_);
    pilot_state().commands.push_back(command);
    publish_text_or_throw(context, "command", command);
    publish_text_or_throw(context, "last_command", command);
  }

  topoexec::Result<topoexec::ComponentStateSnapshot> snapshot_state() const override {
    const auto text = "speed=" + format_speed(speed_) + ";commands=" + std::to_string(command_count_);
    topoexec::ComponentStateSnapshot snapshot;
    snapshot.component_type = "topoexec.app.RobotCellController";
    snapshot.version = "robot-cell-controller.v1";
    snapshot.payload = topoexec::make_shared_payload(topoexec::make_text_payload(text));
    snapshot.size_bytes = text.size();
    return snapshot;
  }

private:
  double speed_{0.25};
  int command_count_{0};
};

class ActuatorComponent final : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.app.RobotCellActuator";
    descriptor.name = "robot_cell_actuator";
    descriptor.role = topoexec::ComponentRole::kOutputBoundary;
    descriptor.inputs = {{"command", topoexec::kTextPayloadSchema}};
    return descriptor;
  }

  void configure(topoexec::GraphContext&, const topoexec::ConfigView&) override {}

  void execute(const topoexec::Invocation& invocation, topoexec::GraphContext&) override {
    record_payloads(invocation, "actuator");
  }
};

topoexec::ComponentRegistry registry() {
  topoexec::ComponentRegistry registry;
  registry.register_component({"topoexec.app.RobotCellCamera"}, []() { return std::make_unique<CameraComponent>(); });
  registry.register_component({"topoexec.app.RobotCellConfigTuner"},
                              []() { return std::make_unique<ConfigTunerComponent>(); });
  registry.register_component({"topoexec.app.RobotCellDetector"},
                              []() { return std::make_unique<DetectorComponent>(); });
  registry.register_component({"topoexec.app.RobotCellPlanner"}, []() { return std::make_unique<PlannerComponent>(); });
  registry.register_component({"topoexec.app.RobotCellController"},
                              []() { return std::make_unique<ControllerComponent>(); });
  registry.register_component({"topoexec.app.RobotCellActuator"},
                              []() { return std::make_unique<ActuatorComponent>(); });
  return registry;
}

topoexec::LaneSpec lane(std::string id, std::string type) {
  topoexec::LaneSpec lane;
  lane.id = std::move(id);
  lane.type = std::move(type);
  return lane;
}

topoexec::EventSourceSpec event_source(std::string type, std::vector<std::string> inputs = {}) {
  topoexec::EventSourceSpec source;
  source.type = std::move(type);
  source.inputs = std::move(inputs);
  return source;
}

topoexec::TriggerPolicySpec trigger(std::string type, std::vector<std::string> inputs = {}) {
  topoexec::TriggerPolicySpec trigger;
  trigger.type = std::move(type);
  trigger.inputs = std::move(inputs);
  return trigger;
}

topoexec::ComponentNodeSpec component(std::string id, std::string type, std::vector<topoexec::EventSourceSpec> sources,
                                      topoexec::TriggerPolicySpec trigger, std::string lane_id,
                                      topoexec::ComponentRole role = topoexec::ComponentRole::kProcessing) {
  topoexec::ExecutionSpec execution;
  execution.lane = std::move(lane_id);
  topoexec::BoundaryDescriptor boundary;
  boundary.role = role;
  boundary.descriptor = "robot-cell-pilot";
  return topoexec::component_node(std::move(id), std::move(type), std::move(sources), std::move(trigger),
                                  std::move(execution), std::move(boundary));
}

topoexec::EdgePolicySpec policy(std::string mode, int capacity, std::string overflow, std::string copy_policy,
                                int max_inflight = 0) {
  topoexec::EdgePolicySpec policy;
  policy.mode = std::move(mode);
  policy.capacity = capacity;
  policy.overflow = std::move(overflow);
  policy.copy_policy = std::move(copy_policy);
  policy.max_inflight = max_inflight;
  return policy;
}

topoexec::EdgeSpec edge(std::string id, topoexec::EdgeKind kind, std::string from, std::string to,
                        topoexec::EdgePolicySpec policy) {
  topoexec::EdgeSpec edge;
  edge.id = std::move(id);
  edge.kind = kind;
  edge.has_kind = true;
  edge.from = std::move(from);
  edge.to = std::move(to);
  edge.policy = std::move(policy);
  return edge;
}

topoexec::GraphSpec graph(std::string tuner_speed) {
  auto acquisition = lane("acquisition", "event_loop");
  auto perception = lane("perception", "thread_pool");
  perception.max_threads = 2;
  perception.queue_capacity = 4;
  perception.overflow = "reject";
  auto control = lane("control", "fixed_rate");
  control.period_ms = 10;
  control.tick_budget_ms = 5;
  control.overrun_policy = "drop_tick";
  auto supervision = lane("supervision", "event_loop");

  auto tuner = component("config_tuner", "topoexec.app.RobotCellConfigTuner", {event_source("manual")},
                         trigger("manual"), "supervision", topoexec::ComponentRole::kInputBoundary);
  tuner.config.values["target"] = "controller";
  tuner.config.values["speed"] = std::move(tuner_speed);

  auto controller = component("controller", "topoexec.app.RobotCellController", {event_source("message", {"plan"})},
                              trigger("any_input", {"plan"}), "control");
  controller.config.values["speed"] = "0.25";

  return topoexec::GraphBuilder("app_robot_cell_pilot")
      .lane(std::move(acquisition))
      .lane(std::move(perception))
      .lane(std::move(control))
      .lane(std::move(supervision))
      .component(component("camera", "topoexec.app.RobotCellCamera", {event_source("manual")}, trigger("manual"),
                           "acquisition", topoexec::ComponentRole::kInputBoundary))
      .component(std::move(tuner))
      .component(component("detector", "topoexec.app.RobotCellDetector", {event_source("task_ready", {"frame"})},
                           trigger("task_ready", {"frame"}), "perception"))
      .component(component("planner", "topoexec.app.RobotCellPlanner",
                           {event_source("message", {"target", "obstacle_state", "last_command"})},
                           trigger("any_input", {"target", "obstacle_state", "last_command"}), "control"))
      .component(std::move(controller))
      .component(component("actuator", "topoexec.app.RobotCellActuator", {event_source("message", {"command"})},
                           trigger("any_input", {"command"}), "supervision", topoexec::ComponentRole::kOutputBoundary))
      .edge(edge("camera_detector_async", topoexec::EdgeKind::kAsync, "camera.frame", "detector.frame",
                 policy("queue", 1, "drop_oldest", "loaned_view", 1)))
      .edge(edge("detector_planner_target", topoexec::EdgeKind::kImmediate, "detector.target", "planner.target",
                 policy("latest", 1, "overwrite", "shared_view")))
      .edge(edge("detector_planner_state", topoexec::EdgeKind::kState, "detector.obstacle_state",
                 "planner.obstacle_state", policy("latest", 1, "overwrite", "shared_view")))
      .edge(edge("planner_controller_plan", topoexec::EdgeKind::kImmediate, "planner.plan", "controller.plan",
                 policy("latest", 1, "overwrite", "shared_view")))
      .edge(edge("controller_actuator_command", topoexec::EdgeKind::kImmediate, "controller.command",
                 "actuator.command", policy("latest", 1, "overwrite", "shared_view")))
      .edge(edge("controller_planner_delay", topoexec::EdgeKind::kDelay, "controller.last_command",
                 "planner.last_command", policy("queue", 2, "drop_oldest", "shared_view")))
      .build();
}

std::size_t metric_value(const topoexec::RuntimeRunnerResult& result, const std::string& name) {
  for (const auto& sample : result.runtime_metrics) {
    if (sample.name == name && sample.channel_id.empty() && sample.component_id.empty() && sample.lane.empty()) {
      return static_cast<std::size_t>(sample.value);
    }
  }
  return 0u;
}

bool has_record(std::uint64_t epoch, const std::string& component, const std::string& port) {
  return std::any_of(pilot_state().records.begin(), pilot_state().records.end(), [&](const auto& record) {
    return record.epoch == epoch && record.component == component && record.port == port;
  });
}

bool has_trace_event(const topoexec::RuntimeRunnerResult& result, const std::string& name, const std::string& key,
                     const std::string& value) {
  return std::any_of(result.trace.begin(), result.trace.end(), [&](const auto& event) {
    const auto found = event.attributes.find(key);
    return event.name == name && found != event.attributes.end() && found->second == value;
  });
}

std::string controller_snapshot_text(const topoexec::RuntimeRunnerResult& result) {
  const auto found = result.component_state_snapshots.find("controller");
  if (found == result.component_state_snapshots.end() || found->second.payload == nullptr) {
    return {};
  }
  return topoexec::require_text_payload(*found->second.payload, "controller snapshot");
}

bool contains(const std::vector<std::string>& values, const std::string& needle) {
  return std::any_of(values.begin(), values.end(),
                     [&](const auto& value) { return value.find(needle) != std::string::npos; });
}

int run_happy_path(const topoexec::ComponentRegistry& components) {
  reset_pilot_state();
  topoexec::RuntimeRunner runner(components);
  topoexec::InMemoryRuntimeObserver observer(128);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 3;
  options.capture_component_state_snapshots = true;
  options.observers = {&observer};

  const auto result = runner.run(graph("0.40"), options);
  if (!result.ok) {
    for (const auto& error : result.runtime_errors) {
      std::cerr << "error: " << error.message << "\n";
    }
    return 1;
  }

  const auto& state = pilot_state();
  if (state.detected_frame_ids.size() != 2u || state.detected_frame_ids.back() != 4) {
    std::cerr << "error: detector did not consume the latest bounded async frames\n";
    return 2;
  }
  if (state.pool_detached_count < 6u || state.pool_high_watermark_bytes < 64u) {
    std::cerr << "error: payload pool did not loan/detach expected frames\n";
    return 3;
  }
  if (!contains(state.commands, "speed=0.40")) {
    std::cerr << "error: controller did not use applied config speed\n";
    return 4;
  }
  if (!has_record(3, "planner", "obstacle_state") || !has_record(3, "planner", "last_command")) {
    std::cerr << "error: planner did not observe state and delay feedback in epoch 3\n";
    return 5;
  }
  const auto async_overwrites = metric_value(result, "runtime.async.overwrite_count");
  if (result.async_publication_count < 6u || async_overwrites < 3u) {
    std::cerr << "error: bounded async overwrite metrics were not emitted; async_publication_count="
              << result.async_publication_count << " async_overwrites=" << async_overwrites
              << " channel_drop_count=" << result.channel_drop_count
              << " channel_overwrite_count=" << result.channel_overwrite_count << "\n";
    return 6;
  }
  if (result.state_publication_count < 2u || result.delayed_publication_count < 2u) {
    std::cerr << "error: state/delay publication metrics were not emitted\n";
    return 7;
  }
  if (result.runtime_metrics.empty() || result.trace.empty() || observer.metrics().empty() ||
      observer.trace_events().empty()) {
    std::cerr << "error: metrics/trace observer evidence is missing\n";
    return 8;
  }
  if (!has_trace_event(result, "config_transaction_apply", "component_id", "controller")) {
    std::cerr << "error: config transaction apply trace is missing\n";
    return 9;
  }
  const auto snapshot = controller_snapshot_text(result);
  if (snapshot.find("speed=0.40") == std::string::npos) {
    std::cerr << "error: controller snapshot did not capture applied config\n";
    return 10;
  }

  std::cout << "pilot=robot_cell_pilot\n";
  std::cout << "pilot_value=explicit_feedback_bounded_observable_cpp\n";
  std::cout << "lanes=acquisition,perception,control,supervision\n";
  std::cout << "detector_latest_frame=" << state.detected_frame_ids.back() << "\n";
  std::cout << "payload_pool_detached_count=" << state.pool_detached_count << "\n";
  std::cout << "payload_address_preserved=true\n";
  std::cout << "async_publication_count=" << result.async_publication_count << "\n";
  std::cout << "bounded_async_overwrite_count=" << async_overwrites << "\n";
  std::cout << "channel_overwrite_count=" << result.channel_overwrite_count << "\n";
  std::cout << "state_feedback_epoch=3\n";
  std::cout << "delay_feedback_epoch=3\n";
  std::cout << "controller_command=" << state.commands.back() << "\n";
  std::cout << "config_apply=" << state.config_apply_events.back() << "\n";
  std::cout << "controller_snapshot=" << snapshot << "\n";
  std::cout << "runtime_metric_samples=" << result.runtime_metrics.size() << "\n";
  std::cout << "trace_event_count=" << result.trace.size() << "\n";
  return 0;
}

int run_error_path(const topoexec::ComponentRegistry& components) {
  reset_pilot_state();
  topoexec::RuntimeRunner runner(components);
  topoexec::RuntimeRunnerOptions options;
  options.mode = topoexec::RuntimeRunMode::kRun;
  options.tick_iterations = 2;
  const auto result = runner.run(graph("invalid"), options);
  if (result.ok) {
    std::cerr << "error: invalid controller config was accepted\n";
    return 20;
  }
  const auto rejected = std::any_of(result.runtime_errors.begin(), result.runtime_errors.end(), [](const auto& error) {
    return error.message.find("config transaction validation failed for component controller") != std::string::npos;
  });
  if (!rejected) {
    std::cerr << "error: invalid config failed through an unexpected path\n";
    for (const auto& error : result.runtime_errors) {
      std::cerr << "error-detail: " << error.message << "\n";
    }
    return 21;
  }
  std::cout << "error_path=invalid_config_rejected\n";
  return 0;
}

} // namespace

int main() {
  const auto components = registry();
  if (const auto code = run_happy_path(components); code != 0) {
    return code;
  }
  return run_error_path(components);
}
