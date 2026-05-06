#pragma once

// API stability: stable-v0.2. Runtime metric descriptors define the exporter-safe schema contract.

#include "topoexec/runtime/graph.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace topoexec {

inline constexpr std::string_view kRuntimeMetricSchemaVersion{"1"};

struct RuntimeMetricDescriptor {
  std::string name;
  std::string kind;
  std::string unit;
  std::vector<std::string> labels;
  std::string cardinality;
  std::string stability;
  std::string description;
};

struct RuntimeMetricSchemaValidationResult {
  bool ok{true};
  std::vector<std::string> errors;
};

const std::vector<RuntimeMetricDescriptor>& runtime_metric_descriptors();
const RuntimeMetricDescriptor* find_runtime_metric_descriptor(std::string_view name);
RuntimeMetricSchemaValidationResult validate_runtime_metric_samples(const std::vector<RuntimeMetricSample>& samples);

} // namespace topoexec
