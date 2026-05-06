#include "topoexec/runtime/graph.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <string>

#ifdef TOPOEXEC_STANDALONE_FUZZER
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
#endif

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string text(reinterpret_cast<const char*>(data), size);
  try {
    const auto graph = topoexec::load_graph_text(text);
    const auto validation = topoexec::validate_graph_structure(graph);
    if (validation.ok) {
      (void)topoexec::graph_plan_json(graph, validation.compiled_plan);
      (void)topoexec::graph_mermaid(graph, validation.compiled_plan);
      (void)topoexec::component_lifecycle_order(graph);
    }
  } catch (const std::exception&) {
  }
  return 0;
}

#ifdef TOPOEXEC_STANDALONE_FUZZER
namespace {

std::string read_file(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("failed to open corpus file: " + path.string());
  }
  return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::vector<std::filesystem::path> corpus_files(const std::filesystem::path& root) {
  std::vector<std::filesystem::path> files;
  if (std::filesystem::is_regular_file(root)) {
    files.push_back(root);
  } else {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
      if (entry.is_regular_file()) {
        files.push_back(entry.path());
      }
    }
  }
  std::sort(files.begin(), files.end());
  return files;
}

} // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "usage: fuzz_graph_inputs <corpus-file-or-directory>...\n";
    return 2;
  }
  std::size_t cases = 0u;
  try {
    for (int index = 1; index < argc; ++index) {
      for (const auto& path : corpus_files(argv[index])) {
        const auto input = read_file(path);
        (void)LLVMFuzzerTestOneInput(reinterpret_cast<const std::uint8_t*>(input.data()), input.size());
        ++cases;
      }
    }
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << "\n";
    return 1;
  }
  std::cout << "ok fuzz corpus cases=" << cases << "\n";
  return cases == 0u ? 1 : 0;
}
#endif
