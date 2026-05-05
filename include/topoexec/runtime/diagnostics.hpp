#pragma once

// API stability: stable-v0.2. Diagnostic code registry is stable for tooling integrations.

#include <optional>
#include <string>
#include <vector>

namespace topoexec {

struct GraphDiagnosticDescriptor {
  std::string code;
  std::string severity;
  std::string summary;
  std::string suggested_fix;
};

const std::vector<GraphDiagnosticDescriptor>& graph_diagnostic_registry();
std::optional<GraphDiagnosticDescriptor> graph_diagnostic_descriptor(const std::string& code);

} // namespace topoexec
