#include "topoexec/runtime/graph.hpp"

#include <CLI/CLI.hpp>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace {

int print_validation(const topoexec::GraphValidationResult& result, const std::string& format) {
  if (format == "json") {
    nlohmann::json value;
    value["ok"] = result.ok;
    value["errors"] = result.errors;
    value["region_order"] = result.compiled_plan.region_order;
    std::cout << value.dump(2) << "\n";
  } else {
    std::cout << (result.ok ? "ok" : "error") << "\n";
    for (const auto& error : result.errors) {
      std::cout << "- " << error << "\n";
    }
  }
  return result.ok ? 0 : 1;
}

topoexec::GraphValidationResult load_and_validate(const std::string& path, topoexec::GraphSpec& graph) {
  graph = topoexec::load_graph_file(path);
  return topoexec::validate_graph_structure(graph);
}

}  // namespace

int main(int argc, char** argv) {
  CLI::App app{"TopoExec graph tooling"};
  app.require_subcommand(1);

  auto* graph_cmd = app.add_subcommand("graph", "Graph inspection commands");
  graph_cmd->require_subcommand(1);

  std::string validate_path;
  std::string validate_format{"text"};
  auto* validate = graph_cmd->add_subcommand("validate", "Validate a TopoExec graph");
  validate->add_option("file", validate_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  validate->add_option("--format", validate_format, "Output format")->check(CLI::IsMember({"text", "json"}));

  std::string plan_path;
  std::string plan_format{"text"};
  auto* plan = graph_cmd->add_subcommand("plan", "Print compiled graph plan");
  plan->add_option("file", plan_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  plan->add_option("--format", plan_format, "Output format")->check(CLI::IsMember({"text", "json"}));

  std::string render_path;
  std::string render_format{"mermaid"};
  auto* render = graph_cmd->add_subcommand("render", "Render a graph");
  render->add_option("file", render_path, "Graph YAML file")->required()->check(CLI::ExistingFile);
  render->add_option("--format", render_format, "Output format")->check(CLI::IsMember({"mermaid", "text", "json"}));

  try {
    app.parse(argc, argv);
    if (*validate) {
      topoexec::GraphSpec graph;
      const auto result = load_and_validate(validate_path, graph);
      return print_validation(result, validate_format);
    }
    if (*plan) {
      topoexec::GraphSpec graph;
      const auto result = load_and_validate(plan_path, graph);
      if (!result.ok) {
        return print_validation(result, plan_format == "json" ? "json" : "text");
      }
      if (plan_format == "json") {
        std::cout << topoexec::graph_plan_json(graph, result.compiled_plan) << "\n";
      } else {
        std::cout << topoexec::graph_plan_text(graph, result.compiled_plan);
      }
      return 0;
    }
    if (*render) {
      topoexec::GraphSpec graph;
      const auto result = load_and_validate(render_path, graph);
      if (!result.ok) {
        return print_validation(result, render_format == "json" ? "json" : "text");
      }
      if (render_format == "json") {
        std::cout << topoexec::graph_plan_json(graph, result.compiled_plan) << "\n";
      } else if (render_format == "text") {
        std::cout << topoexec::graph_plan_text(graph, result.compiled_plan);
      } else {
        std::cout << topoexec::graph_mermaid(graph, result.compiled_plan);
      }
      return 0;
    }
  } catch (const CLI::ParseError& error) {
    return app.exit(error);
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << "\n";
    return 1;
  }
  return 0;
}

