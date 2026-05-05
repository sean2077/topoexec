#pragma once

// API stability: stable-v0.2. Timestamp value types are intended embedder API.

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace topoexec {

enum class TimestampDomain {
  kSteady,
  kSystem,
  kDevice,
  kExternal,
};

std::optional<TimestampDomain> parse_timestamp_domain_value(const std::string& value);
bool is_timestamp_domain_value(const std::string& value);
std::string timestamp_domain_name(TimestampDomain domain);

struct EventTimestamp {
  TimestampDomain domain{TimestampDomain::kSteady};
  std::int64_t nanoseconds{0};
  std::string source;
};

inline EventTimestamp make_event_timestamp(TimestampDomain domain, std::int64_t nanoseconds, std::string source = {}) {
  return EventTimestamp{domain, nanoseconds, std::move(source)};
}

} // namespace topoexec
