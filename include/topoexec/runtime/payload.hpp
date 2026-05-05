#pragma once

// API stability: stable-v0.2. Built-in payload helpers and typed access are intended embedder API.

#include "topoexec/runtime/buffer.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace topoexec {

inline constexpr const char* kTextPayloadSchema = "topoexec.runtime.Text";
inline constexpr const char* kFrameViewPayloadSchema = "topoexec.runtime.FrameView";
inline constexpr const char* kBinaryBlobPayloadSchema = "topoexec.runtime.BinaryBlob";

struct TextPayload {
  std::string text;
};

struct BinaryBlobPayload {
  std::shared_ptr<const SharedBuffer> buffer;
  std::size_t offset{0};
  std::size_t size{0};
  std::string format;

  const std::uint8_t* data() const;
  bool valid() const;
  const void* payload_address() const;
};

struct OpaquePayload {
  std::shared_ptr<const void> object;
  std::size_t size_bytes{0};
  std::string debug_summary;

  bool valid() const;
  const void* payload_address() const;
};

using RuntimePayloadValue = std::variant<TextPayload, FrameView, BinaryBlobPayload, OpaquePayload>;

struct RuntimePayload {
  std::string schema{kTextPayloadSchema};
  RuntimePayloadValue value{TextPayload{}};

  bool is_text() const;
  bool is_large_payload() const;
  const std::string& text() const;
  bool empty() const;
  operator const std::string&() const;
};

using RuntimePayloadPtr = std::shared_ptr<const RuntimePayload>;

RuntimePayload make_text_payload(std::string text, std::string schema = kTextPayloadSchema);
RuntimePayload make_frame_payload(FrameView frame, std::string schema = kFrameViewPayloadSchema);
RuntimePayload make_binary_blob_payload(std::shared_ptr<const SharedBuffer> buffer, std::size_t offset,
                                        std::size_t size, std::string format = {},
                                        std::string schema = kBinaryBlobPayloadSchema);
RuntimePayload make_opaque_payload(std::shared_ptr<const void> object, std::string schema, std::size_t size_bytes = 0,
                                   std::string debug_summary = {});

template <typename T>
RuntimePayload make_custom_payload(std::shared_ptr<const T> object, std::string schema,
                                   std::string debug_summary = {}) {
  return make_opaque_payload(std::move(object), std::move(schema), sizeof(T), std::move(debug_summary));
}
RuntimePayloadPtr make_shared_payload(RuntimePayload payload);

template <typename T> bool payload_is(const RuntimePayload& payload) {
  return std::holds_alternative<T>(payload.value);
}

template <typename T> const T* try_payload_as(const RuntimePayload& payload) {
  return std::get_if<T>(&payload.value);
}

template <typename T> const T& payload_as(const RuntimePayload& payload, const std::string& context = {}) {
  const auto* value = try_payload_as<T>(payload);
  if (value == nullptr) {
    const auto prefix = context.empty() ? std::string{} : context + ": ";
    throw std::runtime_error(prefix + "payload schema/type mismatch for schema " + payload.schema);
  }
  return *value;
}

const std::string& require_text_payload(const RuntimePayload& payload, const std::string& context = {});
const FrameView& require_frame_payload(const RuntimePayload& payload, const std::string& context = {});
const BinaryBlobPayload& require_binary_blob_payload(const RuntimePayload& payload, const std::string& context = {});
const OpaquePayload& require_opaque_payload(const RuntimePayload& payload, const std::string& context = {});
const void* payload_address(const RuntimePayload& payload);
RuntimePayload copy_text_payload(const RuntimePayload& payload);

bool operator==(const RuntimePayload& lhs, const std::string& rhs);
bool operator==(const std::string& lhs, const RuntimePayload& rhs);
bool operator!=(const RuntimePayload& lhs, const std::string& rhs);
bool operator!=(const std::string& lhs, const RuntimePayload& rhs);
std::ostream& operator<<(std::ostream& out, const RuntimePayload& payload);

} // namespace topoexec
