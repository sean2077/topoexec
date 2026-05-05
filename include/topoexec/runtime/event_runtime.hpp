#pragma once

// Public API category: experimental low-level event runtime surface.

#include "topoexec/runtime/channel.hpp"
#include "topoexec/runtime/component.hpp"
#include "topoexec/runtime/graph.hpp"
#include "topoexec/runtime/scheduler.hpp"

#include <string>
#include <vector>

namespace topoexec {

class TraceCollector;

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
  SchedulerRunResult run(const SchedulerRunOptions& options);
  std::size_t component_count() const;

private:
  RuntimeChannelBus* channels_{nullptr};
  RuntimePublicationRouter* publications_{nullptr};
  TraceCollector* trace_{nullptr};
  GraphCompiledPlan compiled_plan_;
  std::vector<EventRuntimeComponent> components_;
};

} // namespace topoexec
