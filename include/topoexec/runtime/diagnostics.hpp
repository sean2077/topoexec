#pragma once

// API stability: stable-v0.2. Diagnostic code registry is stable for tooling integrations.

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace topoexec {

inline constexpr const char* kGraphDiagnosticSchemaVersion = "1";

struct GraphDiagnosticDescriptor {
  GraphDiagnosticDescriptor() = default;
  GraphDiagnosticDescriptor(std::string code_value, std::string severity_value, std::string summary_value,
                            std::string suggested_fix_value)
      : code(std::move(code_value)), severity(std::move(severity_value)), summary(std::move(summary_value)),
        suggested_fix(std::move(suggested_fix_value)) {}
  GraphDiagnosticDescriptor(std::string code_value, std::string severity_value, std::string category_value,
                            std::string summary_value, std::string suggested_fix_value)
      : code(std::move(code_value)), severity(std::move(severity_value)), category(std::move(category_value)),
        summary(std::move(summary_value)), suggested_fix(std::move(suggested_fix_value)) {}

  std::string code;
  std::string severity;
  std::string category{"graph_structure"};
  std::string summary;
  std::string suggested_fix;
};

const std::vector<GraphDiagnosticDescriptor>& graph_diagnostic_registry();
std::optional<GraphDiagnosticDescriptor> graph_diagnostic_descriptor(const std::string& code);
std::string graph_diagnostic_category(const std::string& code);

} // namespace topoexec
