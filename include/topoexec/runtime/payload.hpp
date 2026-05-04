#pragma once

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

using RuntimePayloadValue = std::variant<TextPayload, FrameView, BinaryBlobPayload>;

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
RuntimePayloadPtr make_shared_payload(RuntimePayload payload);

const std::string& require_text_payload(const RuntimePayload& payload, const std::string& context = {});
const FrameView& require_frame_payload(const RuntimePayload& payload, const std::string& context = {});
const BinaryBlobPayload& require_binary_blob_payload(const RuntimePayload& payload, const std::string& context = {});
const void* payload_address(const RuntimePayload& payload);
RuntimePayload copy_text_payload(const RuntimePayload& payload);

bool operator==(const RuntimePayload& lhs, const std::string& rhs);
bool operator==(const std::string& lhs, const RuntimePayload& rhs);
bool operator!=(const RuntimePayload& lhs, const std::string& rhs);
bool operator!=(const std::string& lhs, const RuntimePayload& rhs);
std::ostream& operator<<(std::ostream& out, const RuntimePayload& payload);

} // namespace topoexec
