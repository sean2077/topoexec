#include "topoexec/runtime/component_registry.hpp"

#include <stdexcept>
#include <utility>

namespace topoexec {

bool ComponentRegistry::register_component(ComponentRegistration registration, ComponentFactory factory) {
  if (registration.type.empty()) {
    throw std::invalid_argument("component type must not be empty");
  }
  if (!factory) {
    throw std::invalid_argument("component factory must not be empty");
  }
  const auto type = registration.type;
  if (registrations_.count(type) != 0u) {
    return false;
  }
  registrations_[type] = std::move(registration);
  factories_[type] = std::move(factory);
  return true;
}

bool ComponentRegistry::contains(const std::string& type) const {
  return registrations_.count(type) != 0u;
}

std::optional<ComponentRegistration> ComponentRegistry::metadata(const std::string& type) const {
  const auto found = registrations_.find(type);
  if (found == registrations_.end()) {
    return std::nullopt;
  }
  return found->second;
}

std::unique_ptr<Component> ComponentRegistry::create(const std::string& type) const {
  const auto found = factories_.find(type);
  if (found == factories_.end()) {
    throw std::out_of_range("component type is not registered: " + type);
  }
  return found->second();
}

std::vector<std::string> ComponentRegistry::types() const {
  std::vector<std::string> values;
  values.reserve(registrations_.size());
  for (const auto& [type, unused] : registrations_) {
    (void)unused;
    values.push_back(type);
  }
  return values;
}

} // namespace topoexec
