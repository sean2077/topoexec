#pragma once

// API stability: experimental. Low-level EventRuntime APIs may change before beta.

#include "topoexec/runtime/channel.hpp"
#include "topoexec/runtime/component.hpp"
#include "topoexec/runtime/graph.hpp"
#include "topoexec/runtime/scheduler.hpp"

#include <string>
#include <vector>

namespace topoexec {

class TraceCollector;
class RuntimeStateStore;
class ConfigSnapshotStore;

struct EventRuntimeComponent {
  std::string id;
  Component* component{nullptr};
  GraphContext* context{nullptr};
  ComponentNodeSpec spec;
  SchedulerGroupConfig lane;
};

class EventRuntime {
public:
  explicit EventRuntime(RuntimeChannelBus* channels);
  EventRuntime(RuntimeChannelBus* channels, GraphCompiledPlan compiled_plan);
  EventRuntime(RuntimeChannelBus* channels, GraphCompiledPlan compiled_plan, RuntimePublicationRouter* publications);

  void add_component(EventRuntimeComponent component);
  void set_compiled_plan(GraphCompiledPlan compiled_plan);
  void set_publication_router(RuntimePublicationRouter* publications);
  void set_trace_collector(TraceCollector* trace);
  void set_health_event_sink(HealthEventSink* sink);
  void set_state_store(RuntimeStateStore* state_store);
  void set_config_store(ConfigSnapshotStore* config_store);
  SchedulerRunResult run(const SchedulerRunOptions& options);
  std::size_t component_count() const;

private:
  RuntimeChannelBus* channels_{nullptr};
  RuntimePublicationRouter* publications_{nullptr};
  TraceCollector* trace_{nullptr};
  HealthEventSink* health_events_{nullptr};
  RuntimeStateStore* state_store_{nullptr};
  ConfigSnapshotStore* config_store_{nullptr};
  GraphCompiledPlan compiled_plan_;
  std::vector<EventRuntimeComponent> components_;
};

} // namespace topoexec
