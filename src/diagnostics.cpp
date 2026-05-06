#include "topoexec/runtime/diagnostics.hpp"

#include <algorithm>

namespace topoexec {

const std::vector<GraphDiagnosticDescriptor>& graph_diagnostic_registry() {
  static const std::vector<GraphDiagnosticDescriptor> registry{
      {"unknown_component", "error", "An edge, dependency, or loop references a component id that is not declared.",
       "Check component ids referenced by edges, depends_on, and CompositeLoop declarations."},
      {"unknown_port", "error", "An edge endpoint references a port that the component descriptor does not expose.",
       "Check component descriptors and endpoint port names."},
      {"payload_type_mismatch", "error", "An edge connects descriptor ports with incompatible payload contracts.",
       "Align the source output and target input schema or payload_type, or insert an explicit transform."},
      {"missing_required_input", "error", "A descriptor-declared required input has no incoming edge.",
       "Connect the required input or mark the port optional in the component descriptor."},
      {"optional_input_unconnected", "advisory", "A descriptor-declared optional input has no incoming edge.",
       "Ignore this if the component can run without the port, or connect it to silence the advisory."},
      {"boundary_role_mismatch", "error", "A graph boundary role is incompatible with the component descriptor role.",
       "Align the graph boundary role with the descriptor's input/output boundary capability."},
      {"port_multiplicity_mismatch", "error", "A single-input port has multiple incoming edges.",
       "Use one incoming edge or mark the descriptor input as multiple."},
      {"duplicate_id", "error", "A graph section contains duplicate ids.", "Use unique ids inside each graph section."},
      {"immediate_cycle_without_loop", "error", "Immediate edges form a cycle without an owning CompositeLoop.",
       "Break the cycle with delay/state/async or declare an exact CompositeLoop."},
      {"partial_composite_loop", "error", "A CompositeLoop declaration only partially matches an immediate SCC.",
       "Make the CompositeLoop component set exactly match one immediate SCC."},
      {"decorative_composite_loop", "error", "A CompositeLoop declaration does not own an immediate cycle.",
       "Remove the CompositeLoop or add the immediate feedback edges it owns."},
      {"multi_state_writer", "error", "Multiple state edges write the same target snapshot.",
       "Keep one writer per state target until an explicit merge policy exists."},
      {"invalid_move_only_multireader", "error", "A move_only payload policy was combined with multi-reader delivery.",
       "Use readers: single for move_only or switch to shared_view/copy."},
      {"invalid_channel_policy", "error", "A channel policy field is unsupported or internally inconsistent.",
       "Use a supported bounded channel mode, capacity, overflow, timestamp, owner, and copy policy."},
      {"unsupported_lane_type", "error", "A scheduler lane type is not implemented by the core runtime.",
       "Use event_loop, fixed_rate, or thread_pool."},
      {"trigger_missing_input", "error", "A trigger/event-source input is missing or has no incoming edge.",
       "Add trigger inputs and matching incoming edges."},
      {"incompatible_trigger_edge_mode", "error",
       "Trigger policy and event-source declarations are incompatible with the incoming edge shape.",
       "Align event_sources, trigger_policy, and incoming edge modes."},
      {"unsupported_error_policy", "error", "A non-fail-fast execution error policy was requested.",
       "Use fail_fast until continue/isolate policies are implemented."},
      {"advisory_lane_field_ignored", "advisory",
       "A scheduler lane field is parsed and preserved but not enforced, or only applied best-effort, by the current "
       "runtime.",
       "Treat the field as documentation only until the matching scheduler capability is implemented."},
      {"advisory_execution_field_ignored", "advisory",
       "A component execution field is parsed and preserved but not enforced by the current runtime.",
       "Treat the field as documentation only until the matching execution capability is implemented."},
      {"graph_validation_error", "error", "Generic graph validation failure.",
       "Inspect the graph path and schema reference for the invalid contract."},
  };
  return registry;
}

std::optional<GraphDiagnosticDescriptor> graph_diagnostic_descriptor(const std::string& code) {
  const auto& registry = graph_diagnostic_registry();
  const auto found =
      std::find_if(registry.begin(), registry.end(), [&](const auto& descriptor) { return descriptor.code == code; });
  if (found == registry.end()) {
    return std::nullopt;
  }
  return *found;
}

} // namespace topoexec
