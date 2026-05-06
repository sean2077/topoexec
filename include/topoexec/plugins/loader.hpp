#pragma once

// API stability: experimental. Dynamic plugin loading preview v0 is optional and trusted-native-code only.

#include "topoexec/runtime/component_registry.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace topoexec::plugins {

inline constexpr const char* kPluginLoaderPreviewApiVersion = "0";
inline constexpr const char* kPluginLoaderSchemaVersion = "1";
inline constexpr const char* kPluginManifestSymbol = "topoexec_plugin_manifest_v0";
inline constexpr const char* kPluginRegisterSymbol = "topoexec_plugin_register_v0";

struct PluginComponentDescriptorView {
  const char* type;
  const char* component_api_version;
  bool realtime_allowed;
  bool dry_run_safe;
};

struct PluginManifestView {
  const char* plugin_id;
  const char* plugin_api_version;
  const char* schema_version;
  std::size_t component_count;
  const PluginComponentDescriptorView* components;
};

using PluginManifestFunction = const PluginManifestView* (*)();
using PluginRegisterFunction = bool (*)(ComponentRegistry* registry);

struct PluginManifest {
  std::string plugin_id;
  std::string plugin_api_version;
  std::string schema_version;
  std::vector<ComponentRegistration> components;
};

struct PluginLoadOptions {
  std::string required_plugin_api_version{kPluginLoaderPreviewApiVersion};
  std::string required_schema_version{kPluginLoaderSchemaVersion};
  bool require_declared_components{true};
  bool close_on_destroy{false};
};

struct PluginLoadError {
  std::string code;
  std::string message;
};

struct PluginLoadResult {
  bool ok{false};
  PluginManifest manifest;
  std::vector<std::string> registered_components;
  std::vector<PluginLoadError> errors;
};

class LoadedPlugin {
public:
  LoadedPlugin() = default;
  LoadedPlugin(void* handle, PluginLoadResult result, bool close_on_destroy);
  ~LoadedPlugin();

  LoadedPlugin(const LoadedPlugin&) = delete;
  LoadedPlugin& operator=(const LoadedPlugin&) = delete;
  LoadedPlugin(LoadedPlugin&& other) noexcept;
  LoadedPlugin& operator=(LoadedPlugin&& other) noexcept;

  const PluginLoadResult& result() const {
    return result_;
  }
  bool has_native_handle() const {
    return handle_ != nullptr;
  }
  bool close_on_destroy() const {
    return close_on_destroy_;
  }

private:
  void reset() noexcept;

  void* handle_{nullptr};
  PluginLoadResult result_{};
  bool close_on_destroy_{false};
};

LoadedPlugin load_plugin(const std::string& path, ComponentRegistry& registry,
                         PluginLoadOptions options = PluginLoadOptions{});

} // namespace topoexec::plugins
