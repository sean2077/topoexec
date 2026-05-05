#pragma once

// Public API category: stable 0.x diagnostic code registry. The graph compiler
// may add new codes, but existing code meanings should remain stable.

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
