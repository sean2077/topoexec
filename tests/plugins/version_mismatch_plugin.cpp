#include "topoexec/plugins/loader.hpp"

namespace {

constexpr topoexec::plugins::PluginComponentDescriptorView kComponents[] = {
    {"topoexec.plugins.VersionMismatch", "1", false, true},
};

constexpr topoexec::plugins::PluginManifestView kManifest{
    "topoexec.version_mismatch_plugin", "999", topoexec::plugins::kPluginLoaderSchemaVersion, 1, kComponents,
};

} // namespace

extern "C" const topoexec::plugins::PluginManifestView* topoexec_plugin_manifest_v0() {
  return &kManifest;
}

extern "C" bool topoexec_plugin_register_v0(topoexec::ComponentRegistry* registry) {
  (void)registry;
  return true;
}
