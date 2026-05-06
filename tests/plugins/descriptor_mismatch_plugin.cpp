#include "topoexec/plugins/loader.hpp"

#include <memory>
#include <utility>

namespace {

class ActualComponent final : public topoexec::Component {
public:
  topoexec::ComponentDescriptor describe() const override {
    topoexec::ComponentDescriptor descriptor;
    descriptor.type = "topoexec.plugins.Actual";
    descriptor.name = "Actual";
    descriptor.role = topoexec::ComponentRole::kProcessing;
    return descriptor;
  }

  void configure(topoexec::GraphContext& ctx, const topoexec::ConfigView& config) override {
    (void)ctx;
    (void)config;
  }
};

constexpr topoexec::plugins::PluginComponentDescriptorView kComponents[] = {
    {"topoexec.plugins.Advertised", "1", false, true},
};

constexpr topoexec::plugins::PluginManifestView kManifest{
    "topoexec.descriptor_mismatch_plugin",
    topoexec::plugins::kPluginLoaderPreviewApiVersion,
    topoexec::plugins::kPluginLoaderSchemaVersion,
    1,
    kComponents,
};

} // namespace

extern "C" const topoexec::plugins::PluginManifestView* topoexec_plugin_manifest_v0() {
  return &kManifest;
}

extern "C" bool topoexec_plugin_register_v0(topoexec::ComponentRegistry* registry) {
  if (registry == nullptr) {
    return false;
  }
  topoexec::ComponentRegistration registration;
  registration.type = "topoexec.plugins.Actual";
  registration.component_api_version = "1";
  registration.runtime_min_version = "0.1.0";
  registration.static_registration = false;
  registration.realtime_allowed = false;
  registration.dry_run_safe = true;
  return registry->register_component(std::move(registration), [] { return std::make_unique<ActualComponent>(); });
}
