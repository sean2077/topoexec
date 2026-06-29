#include "topoexec/plugins/loader.hpp"

#include <algorithm>
#include <set>
#include <utility>

#if defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

namespace topoexec::plugins {
namespace {

PluginLoadError make_error(std::string code, std::string message) {
  return PluginLoadError{std::move(code), std::move(message)};
}

const char* safe_c_string(const char* value) {
  return value == nullptr ? "" : value;
}

std::set<std::string> type_set(const ComponentRegistry& registry) {
  const auto types = registry.types();
  return {types.begin(), types.end()};
}

bool contains_type(const std::vector<ComponentRegistration>& registrations, const std::string& type) {
  return std::any_of(registrations.begin(), registrations.end(),
                     [&](const auto& registration) { return registration.type == type; });
}

PluginManifest copy_manifest(const PluginManifestView& view, PluginLoadResult& result) {
  PluginManifest manifest;
  manifest.plugin_id = safe_c_string(view.plugin_id);
  manifest.plugin_api_version = safe_c_string(view.plugin_api_version);
  manifest.schema_version = safe_c_string(view.schema_version);
  if (view.component_count > 0 && view.components == nullptr) {
    result.errors.push_back(make_error("manifest.components", "component descriptors pointer is required"));
    return manifest;
  }
  for (std::size_t index = 0; index < view.component_count; ++index) {
    const auto& component = view.components[index];
    ComponentRegistration registration;
    registration.type = safe_c_string(component.type);
    registration.component_api_version = safe_c_string(component.component_api_version);
    if (registration.component_api_version.empty()) {
      registration.component_api_version = "1";
    }
    registration.runtime_min_version = "0.1.0";
    registration.static_registration = false;
    registration.realtime_allowed = component.realtime_allowed;
    registration.dry_run_safe = component.dry_run_safe;
    manifest.components.push_back(std::move(registration));
  }
  return manifest;
}

void validate_manifest_shape(const PluginManifest& manifest, PluginLoadResult& result) {
  if (manifest.plugin_id.empty()) {
    result.errors.push_back(make_error("manifest.plugin_id", "plugin_id is required"));
  }
  if (manifest.plugin_api_version.empty()) {
    result.errors.push_back(make_error("manifest.plugin_api_version", "plugin_api_version is required"));
  }
  if (manifest.schema_version.empty()) {
    result.errors.push_back(make_error("manifest.schema_version", "schema_version is required"));
  }
  if (manifest.components.empty()) {
    result.errors.push_back(make_error("manifest.components", "at least one component must be declared"));
  }
  for (const auto& component : manifest.components) {
    if (component.type.empty()) {
      result.errors.push_back(make_error("manifest.component.type", "component type is required"));
    }
  }
}

void close_handle(void* handle) noexcept {
#if defined(__unix__) || defined(__APPLE__)
  if (handle != nullptr) {
    dlclose(handle);
  }
#else
  (void)handle;
#endif
}

} // namespace

LoadedPlugin::LoadedPlugin(void* handle, PluginLoadResult result, bool close_on_destroy)
    : handle_(handle), result_(std::move(result)), close_on_destroy_(close_on_destroy) {}

LoadedPlugin::~LoadedPlugin() {
  reset();
}

LoadedPlugin::LoadedPlugin(LoadedPlugin&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)), result_(std::move(other.result_)),
      close_on_destroy_(std::exchange(other.close_on_destroy_, false)) {}

LoadedPlugin& LoadedPlugin::operator=(LoadedPlugin&& other) noexcept {
  if (this != &other) {
    reset();
    handle_ = std::exchange(other.handle_, nullptr);
    result_ = std::move(other.result_);
    close_on_destroy_ = std::exchange(other.close_on_destroy_, false);
  }
  return *this;
}

void LoadedPlugin::reset() noexcept {
  if (close_on_destroy_) {
    close_handle(handle_);
  }
  handle_ = nullptr;
  close_on_destroy_ = false;
}

LoadedPlugin load_plugin(const std::string& path, ComponentRegistry& registry, PluginLoadOptions options) {
  PluginLoadResult result;

#if !(defined(__unix__) || defined(__APPLE__))
  result.errors.push_back(make_error("platform.unsupported", "plugin loader preview currently requires POSIX dlopen"));
  return LoadedPlugin(nullptr, std::move(result), false);
#else
  void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (handle == nullptr) {
    const char* open_error = dlerror();
    result.errors.push_back(make_error("dlopen", open_error == nullptr ? "failed to open plugin" : open_error));
    return LoadedPlugin(nullptr, std::move(result), false);
  }

  dlerror();
  auto* manifest_symbol = dlsym(handle, kPluginManifestSymbol);
  const char* manifest_error = dlerror();
  if (manifest_error != nullptr || manifest_symbol == nullptr) {
    result.errors.push_back(
        make_error("symbol.manifest", manifest_error == nullptr ? "missing manifest symbol" : manifest_error));
    close_handle(handle);
    return LoadedPlugin(nullptr, std::move(result), false);
  }

  auto manifest_function = reinterpret_cast<PluginManifestFunction>(manifest_symbol);
  const PluginManifestView* manifest_view = manifest_function();
  if (manifest_view == nullptr) {
    result.errors.push_back(make_error("manifest.null", "plugin manifest function returned null"));
    close_handle(handle);
    return LoadedPlugin(nullptr, std::move(result), false);
  }

  result.manifest = copy_manifest(*manifest_view, result);
  validate_manifest_shape(result.manifest, result);
  if (result.manifest.plugin_api_version != options.required_plugin_api_version) {
    result.errors.push_back(make_error("version.plugin_api", "plugin_api_version mismatch"));
  }
  if (result.manifest.schema_version != options.required_schema_version) {
    result.errors.push_back(make_error("version.schema", "schema_version mismatch"));
  }
  if (!result.errors.empty()) {
    close_handle(handle);
    return LoadedPlugin(nullptr, std::move(result), false);
  }

  dlerror();
  auto* register_symbol = dlsym(handle, kPluginRegisterSymbol);
  const char* register_error = dlerror();
  if (register_error != nullptr || register_symbol == nullptr) {
    result.errors.push_back(
        make_error("symbol.register", register_error == nullptr ? "missing register symbol" : register_error));
    close_handle(handle);
    return LoadedPlugin(nullptr, std::move(result), false);
  }

  const auto before_types = type_set(registry);
  auto register_function = reinterpret_cast<PluginRegisterFunction>(register_symbol);

  // Roll back any components the plugin added. Used on failure paths so a failed load never leaves
  // half-registered factories behind (which would dangle once the library handle is closed).
  const auto rollback_registered = [&]() {
    for (const auto& type : type_set(registry)) {
      if (before_types.count(type) == 0u) {
        registry.unregister(type);
      }
    }
  };

  bool register_ok = false;
  try {
    register_ok = register_function(&registry);
  } catch (const std::exception& error) {
    // A C++ exception must never propagate across the C plugin ABI boundary (undefined behavior); contain it.
    result.errors.push_back(
        make_error("register.threw", std::string("plugin register function threw: ") + error.what()));
    rollback_registered();
    close_handle(handle);
    return LoadedPlugin(nullptr, std::move(result), false);
  } catch (...) {
    result.errors.push_back(make_error("register.threw", "plugin register function threw a non-standard exception"));
    rollback_registered();
    close_handle(handle);
    return LoadedPlugin(nullptr, std::move(result), false);
  }
  if (!register_ok) {
    result.errors.push_back(make_error("register.failed", "plugin register function returned false"));
    rollback_registered();
    close_handle(handle);
    return LoadedPlugin(nullptr, std::move(result), false);
  }

  const auto after_types = type_set(registry);
  for (const auto& type : after_types) {
    if (before_types.count(type) == 0u) {
      result.registered_components.push_back(type);
      if (options.require_declared_components && !contains_type(result.manifest.components, type)) {
        result.errors.push_back(
            make_error("register.undeclared_component", "plugin registered undeclared component " + type));
      }
    }
  }
  for (const auto& component : result.manifest.components) {
    if (!registry.contains(component.type)) {
      result.errors.push_back(
          make_error("register.missing_component", "plugin did not register declared component " + component.type));
    }
  }

  result.ok = result.errors.empty();
  return LoadedPlugin(handle, std::move(result), options.close_on_destroy);
#endif
}

} // namespace topoexec::plugins
