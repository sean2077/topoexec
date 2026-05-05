#pragma once

// Public API category: stable 0.x component factory registry surface.

#include "topoexec/runtime/component.hpp"

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace topoexec {

struct ComponentRegistration {
  std::string type;
  std::string component_api_version{"1"};
  std::string runtime_min_version{"0.1.0"};
  bool static_registration{true};
  bool realtime_allowed{false};
  bool dry_run_safe{true};
};

using ComponentFactory = std::function<std::unique_ptr<Component>()>;

class ComponentRegistry {
public:
  bool register_component(ComponentRegistration registration, ComponentFactory factory);
  bool contains(const std::string& type) const;
  std::optional<ComponentRegistration> metadata(const std::string& type) const;
  std::unique_ptr<Component> create(const std::string& type) const;
  std::vector<std::string> types() const;

private:
  std::map<std::string, ComponentRegistration> registrations_;
  std::map<std::string, ComponentFactory> factories_;
};

} // namespace topoexec
