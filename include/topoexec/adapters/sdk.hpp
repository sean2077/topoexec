#pragma once

// API stability: experimental. Adapter SDK v0 is a dependency-free preview boundary before beta.

#include "topoexec/runtime/component.hpp"
#include "topoexec/runtime/component_registry.hpp"
#include "topoexec/runtime/payload.hpp"
#include "topoexec/runtime/runtime_runner.hpp"
#include "topoexec/runtime/status.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace topoexec::adapters {

inline constexpr const char* kAdapterSdkContractVersion = "0";

using ResultSink = topoexec::ResultSink;
using MetricSink = topoexec::MetricSink;
using TraceSink = topoexec::TraceSink;
using RuntimeObserver = topoexec::RuntimeObserver;
using NoopRuntimeObserver = topoexec::NoopRuntimeObserver;
using InMemoryRuntimeObserver = topoexec::InMemoryRuntimeObserver;
using RuntimeObserverStatus = topoexec::RuntimeObserverStatus;

struct BoundaryMessage {
  std::string boundary_id;
  std::string port;
  RuntimePayloadPtr payload;
  std::optional<EventTimestamp> event_timestamp;
  InvocationMetadata metadata;
};

struct BoundaryPollResult {
  bool ready{false};
  BoundaryMessage message;
  std::string reason;

  static BoundaryPollResult no_input() {
    return {};
  }

  static BoundaryPollResult input(BoundaryMessage value) {
    BoundaryPollResult result;
    result.ready = true;
    result.message = std::move(value);
    return result;
  }

  static BoundaryPollResult error(std::string message) {
    BoundaryPollResult result;
    result.ready = false;
    result.reason = std::move(message);
    return result;
  }

  bool ok() const {
    return reason.empty();
  }
};

struct BoundaryBridgeStatus {
  std::size_t pending_input_count{0};
  std::size_t accepted_input_count{0};
  std::size_t dropped_input_count{0};
  std::size_t published_output_count{0};
  std::size_t failed_output_count{0};
};

class BoundaryBridge {
public:
  virtual ~BoundaryBridge() = default;

  virtual BoundaryPollResult poll_input() = 0;
  virtual Status publish_output(const BoundaryMessage& message) = 0;
  virtual BoundaryBridgeStatus status() const {
    return {};
  }
};

class ComponentFactoryProvider {
public:
  virtual ~ComponentFactoryProvider() = default;

  virtual std::string provider_name() const {
    return {};
  }

  virtual Status register_components(ComponentRegistry& registry) = 0;
};

} // namespace topoexec::adapters
