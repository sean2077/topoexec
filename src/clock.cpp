#include "topoexec/runtime/clock.hpp"

namespace topoexec {

std::optional<TimestampDomain> parse_timestamp_domain_value(const std::string& value) {
  if (value == "steady") {
    return TimestampDomain::kSteady;
  }
  if (value == "system") {
    return TimestampDomain::kSystem;
  }
  if (value == "device") {
    return TimestampDomain::kDevice;
  }
  if (value == "external") {
    return TimestampDomain::kExternal;
  }
  return std::nullopt;
}

bool is_timestamp_domain_value(const std::string& value) {
  return parse_timestamp_domain_value(value).has_value();
}

std::string timestamp_domain_name(TimestampDomain domain) {
  switch (domain) {
  case TimestampDomain::kSteady:
    return "steady";
  case TimestampDomain::kSystem:
    return "system";
  case TimestampDomain::kDevice:
    return "device";
  case TimestampDomain::kExternal:
    return "external";
  }
  return "unknown";
}

}  // namespace topoexec

