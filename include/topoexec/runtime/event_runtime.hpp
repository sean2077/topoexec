#pragma once

#include "topoexec/runtime/channel.hpp"
#include "topoexec/runtime/component.hpp"
#include "topoexec/runtime/graph.hpp"
#include "topoexec/runtime/scheduler.hpp"

#include <string>
#include <vector>

namespace topoexec {

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

  void add_component(EventRuntimeComponent component);
  void set_compiled_plan(GraphCompiledPlan compiled_plan);
  SchedulerRunResult run(const SchedulerRunOptions& options);
  std::size_t component_count() const;

private:
  RuntimeChannelBus* channels_{nullptr};
  GraphCompiledPlan compiled_plan_;
  std::vector<EventRuntimeComponent> components_;
};

}  // namespace topoexec

