#include "topoexec/plugins/loader.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string required_env(const char* name) {
  const char* value = std::getenv(name);
  if (value == nullptr || std::string(value).empty()) {
    ADD_FAILURE() << "missing required test environment variable " << name;
    return {};
  }
  return value;
}

std::vector<std::string> error_codes(const topoexec::plugins::PluginLoadResult& result) {
  std::vector<std::string> codes;
  codes.reserve(result.errors.size());
  for (const auto& error : result.errors) {
    codes.push_back(error.code);
  }
  return codes;
}

bool has_error_code(const topoexec::plugins::PluginLoadResult& result, const std::string& code) {
  const auto codes = error_codes(result);
  return std::find(codes.begin(), codes.end(), code) != codes.end();
}

std::string format_errors(const topoexec::plugins::PluginLoadResult& result) {
  std::ostringstream out;
  for (const auto& error : result.errors) {
    out << error.code << ": " << error.message << '\n';
  }
  return out.str();
}

} // namespace

TEST(PluginLoader, LoadsSamplePluginAndRegistersDeclaredComponent) {
  topoexec::ComponentRegistry registry;
  auto loaded = topoexec::plugins::load_plugin(required_env("TOPOEXEC_SAMPLE_PLUGIN"), registry);

  ASSERT_TRUE(loaded.result().ok) << format_errors(loaded.result());
  EXPECT_TRUE(loaded.has_native_handle());
  EXPECT_FALSE(loaded.close_on_destroy());
  EXPECT_EQ(loaded.result().manifest.plugin_id, "topoexec.sample_plugin");
  EXPECT_EQ(loaded.result().manifest.plugin_api_version, topoexec::plugins::kPluginLoaderPreviewApiVersion);
  EXPECT_EQ(loaded.result().manifest.schema_version, topoexec::plugins::kPluginLoaderSchemaVersion);
  EXPECT_EQ(loaded.result().registered_components, std::vector<std::string>{"topoexec.plugins.SampleEcho"});
  EXPECT_TRUE(registry.contains("topoexec.plugins.SampleEcho"));

  const auto metadata = registry.metadata("topoexec.plugins.SampleEcho");
  ASSERT_TRUE(metadata.has_value());
  EXPECT_EQ(metadata->component_api_version, "1");
  EXPECT_FALSE(metadata->static_registration);
  EXPECT_FALSE(metadata->realtime_allowed);
  EXPECT_TRUE(metadata->dry_run_safe);

  const auto component = registry.create("topoexec.plugins.SampleEcho");
  ASSERT_NE(component, nullptr);
  EXPECT_EQ(component->describe().type, "topoexec.plugins.SampleEcho");
}

TEST(PluginLoader, RejectsPluginApiVersionMismatchBeforeRegistration) {
  topoexec::ComponentRegistry registry;
  auto loaded = topoexec::plugins::load_plugin(required_env("TOPOEXEC_VERSION_MISMATCH_PLUGIN"), registry);

  EXPECT_FALSE(loaded.result().ok);
  EXPECT_FALSE(loaded.has_native_handle());
  EXPECT_TRUE(has_error_code(loaded.result(), "version.plugin_api")) << format_errors(loaded.result());
  EXPECT_FALSE(registry.contains("topoexec.plugins.VersionMismatch"));
}

TEST(PluginLoader, ReportsDescriptorMismatchAfterRegistration) {
  topoexec::ComponentRegistry registry;
  auto loaded = topoexec::plugins::load_plugin(required_env("TOPOEXEC_DESCRIPTOR_MISMATCH_PLUGIN"), registry);

  EXPECT_FALSE(loaded.result().ok);
  EXPECT_TRUE(loaded.has_native_handle());
  EXPECT_TRUE(has_error_code(loaded.result(), "register.undeclared_component")) << format_errors(loaded.result());
  EXPECT_TRUE(has_error_code(loaded.result(), "register.missing_component")) << format_errors(loaded.result());
  EXPECT_TRUE(registry.contains("topoexec.plugins.Actual"));
  EXPECT_FALSE(registry.contains("topoexec.plugins.Advertised"));
}

TEST(PluginLoader, ReportsMissingPluginPath) {
  topoexec::ComponentRegistry registry;
  auto loaded = topoexec::plugins::load_plugin("/definitely/missing/topoexec-plugin.so", registry);

  EXPECT_FALSE(loaded.result().ok);
  EXPECT_FALSE(loaded.has_native_handle());
  EXPECT_TRUE(has_error_code(loaded.result(), "dlopen")) << format_errors(loaded.result());
}

TEST(PluginLoader, UnloadIsOptInAndUnsafeByDefault) {
  auto registry = std::make_unique<topoexec::ComponentRegistry>();
  topoexec::plugins::PluginLoadOptions options;
  options.close_on_destroy = true;
  auto loaded = topoexec::plugins::load_plugin(required_env("TOPOEXEC_SAMPLE_PLUGIN"), *registry, options);

  ASSERT_TRUE(loaded.result().ok) << format_errors(loaded.result());
  EXPECT_TRUE(loaded.has_native_handle());
  EXPECT_TRUE(loaded.close_on_destroy());
  registry.reset();
}
