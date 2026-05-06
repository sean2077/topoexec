#include "topoexec/runtime/graph.hpp"

#include <iostream>

#ifndef TOPOEXEC_SMOKE_GRAPH
#error "TOPOEXEC_SMOKE_GRAPH must be provided by CMake"
#endif

int main() {
  const auto graph = topoexec::load_graph_file(TOPOEXEC_SMOKE_GRAPH);
  if (graph.name != "minimal") {
    std::cerr << "unexpected graph name: " << graph.name << "\n";
    return 1;
  }
  if (graph.schema_version != topoexec::kTopoExecSchemaVersion) {
    std::cerr << "unexpected schema version\n";
    return 2;
  }
  if (graph.components.size() != 3u || graph.edges.size() != 2u) {
    std::cerr << "unexpected graph shape\n";
    return 3;
  }
  std::cout << "yaml_smoke_graph=" << graph.name << "\n";
  return 0;
}
