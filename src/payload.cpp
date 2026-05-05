#include "topoexec/runtime/payload.hpp"

namespace topoexec {

const std::uint8_t* BinaryBlobPayload::data() const {
  if (!valid()) {
    return nullptr;
  }
  return buffer->data() + offset;
}

bool BinaryBlobPayload::valid() const {
  return buffer != nullptr && offset <= buffer->size() && size <= buffer->size() - offset;
}

const void* BinaryBlobPayload::payload_address() const {
  return static_cast<const void*>(data());
}

bool OpaquePayload::valid() const {
  return object != nullptr;
}

const void* OpaquePayload::payload_address() const {
  return object.get();
}

bool RuntimePayload::is_text() const {
  return std::holds_alternative<TextPayload>(value);
}

bool RuntimePayload::is_large_payload() const {
  return !is_text();
}

const std::string& RuntimePayload::text() const {
  return require_text_payload(*this);
}

bool RuntimePayload::empty() const {
  return text().empty();
}

RuntimePayload::operator const std::string&() const {
  return text();
}

RuntimePayload make_text_payload(std::string text, std::string schema) {
  RuntimePayload payload;
  payload.schema = std::move(schema);
  payload.value = TextPayload{std::move(text)};
  return payload;
}

RuntimePayload make_frame_payload(FrameView frame, std::string schema) {
  RuntimePayload payload;
  payload.schema = std::move(schema);
  payload.value = std::move(frame);
  return payload;
}

RuntimePayload make_binary_blob_payload(std::shared_ptr<const SharedBuffer> buffer, std::size_t offset,
                                        std::size_t size, std::string format, std::string schema) {
  RuntimePayload payload;
  payload.schema = std::move(schema);
  payload.value = BinaryBlobPayload{std::move(buffer), offset, size, std::move(format)};
  return payload;
}

RuntimePayload make_opaque_payload(std::shared_ptr<const void> object, std::string schema, std::size_t size_bytes,
                                   std::string debug_summary) {
  RuntimePayload payload;
  payload.schema = std::move(schema);
  payload.value = OpaquePayload{std::move(object), size_bytes, std::move(debug_summary)};
  return payload;
}

RuntimePayloadPtr make_shared_payload(RuntimePayload payload) {
  return std::make_shared<const RuntimePayload>(std::move(payload));
}

const std::string& require_text_payload(const RuntimePayload& payload, const std::string& context) {
  if (!std::holds_alternative<TextPayload>(payload.value)) {
    const auto prefix = context.empty() ? std::string{} : context + ": ";
    throw std::runtime_error(prefix + "expected Text payload, got schema " + payload.schema);
  }
  return std::get<TextPayload>(payload.value).text;
}

const FrameView& require_frame_payload(const RuntimePayload& payload, const std::string& context) {
  if (!std::holds_alternative<FrameView>(payload.value)) {
    const auto prefix = context.empty() ? std::string{} : context + ": ";
    throw std::runtime_error(prefix + "expected FrameView payload, got schema " + payload.schema);
  }
  return std::get<FrameView>(payload.value);
}

const BinaryBlobPayload& require_binary_blob_payload(const RuntimePayload& payload, const std::string& context) {
  if (!std::holds_alternative<BinaryBlobPayload>(payload.value)) {
    const auto prefix = context.empty() ? std::string{} : context + ": ";
    throw std::runtime_error(prefix + "expected BinaryBlob payload, got schema " + payload.schema);
  }
  return std::get<BinaryBlobPayload>(payload.value);
}

const OpaquePayload& require_opaque_payload(const RuntimePayload& payload, const std::string& context) {
  if (!std::holds_alternative<OpaquePayload>(payload.value)) {
    const auto prefix = context.empty() ? std::string{} : context + ": ";
    throw std::runtime_error(prefix + "expected Opaque payload, got schema " + payload.schema);
  }
  return std::get<OpaquePayload>(payload.value);
}

const void* payload_address(const RuntimePayload& payload) {
  if (std::holds_alternative<TextPayload>(payload.value)) {
    return static_cast<const void*>(std::get<TextPayload>(payload.value).text.data());
  }
  if (std::holds_alternative<FrameView>(payload.value)) {
    return std::get<FrameView>(payload.value).payload_address();
  }
  if (std::holds_alternative<BinaryBlobPayload>(payload.value)) {
    return std::get<BinaryBlobPayload>(payload.value).payload_address();
  }
  return std::get<OpaquePayload>(payload.value).payload_address();
}

RuntimePayload copy_text_payload(const RuntimePayload& payload) {
  return make_text_payload(require_text_payload(payload), payload.schema);
}

bool operator==(const RuntimePayload& lhs, const std::string& rhs) {
  return lhs.text() == rhs;
}

bool operator==(const std::string& lhs, const RuntimePayload& rhs) {
  return lhs == rhs.text();
}

bool operator!=(const RuntimePayload& lhs, const std::string& rhs) {
  return !(lhs == rhs);
}

bool operator!=(const std::string& lhs, const RuntimePayload& rhs) {
  return !(lhs == rhs);
}

std::ostream& operator<<(std::ostream& out, const RuntimePayload& payload) {
  if (payload.is_text()) {
    return out << payload.text();
  }
  if (std::holds_alternative<OpaquePayload>(payload.value)) {
    const auto& opaque = std::get<OpaquePayload>(payload.value);
    if (!opaque.debug_summary.empty()) {
      return out << payload.schema << ":" << opaque.debug_summary;
    }
  }
  return out << payload.schema;
}

} // namespace topoexec
