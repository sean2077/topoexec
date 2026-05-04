#include "topoexec/runtime/buffer.hpp"

#include <algorithm>
#include <utility>

namespace topoexec {

SharedBuffer::SharedBuffer(std::size_t size) : bytes_(size) {}

std::uint8_t* SharedBuffer::data() {
  return bytes_.data();
}

const std::uint8_t* SharedBuffer::data() const {
  return bytes_.data();
}

std::size_t SharedBuffer::size() const {
  return bytes_.size();
}

void SharedBuffer::resize(std::size_t size) {
  bytes_.resize(size);
}

const std::uint8_t* FrameView::data() const {
  if (!valid()) {
    return nullptr;
  }
  return buffer->data() + offset;
}

bool FrameView::valid() const {
  return buffer != nullptr && offset <= buffer->size() && size <= buffer->size() - offset;
}

const void* FrameView::payload_address() const {
  return static_cast<const void*>(data());
}

LoanedFrame::LoanedFrame(BufferPool* pool, FrameView view) : pool_(pool), view_(std::move(view)) {}

LoanedFrame::LoanedFrame(LoanedFrame&& other) noexcept : pool_(other.pool_), view_(std::move(other.view_)) {
  other.pool_ = nullptr;
}

LoanedFrame& LoanedFrame::operator=(LoanedFrame&& other) noexcept {
  if (this != &other) {
    release();
    pool_ = other.pool_;
    view_ = std::move(other.view_);
    other.pool_ = nullptr;
  }
  return *this;
}

LoanedFrame::~LoanedFrame() {
  release();
}

FrameView& LoanedFrame::view() {
  return view_;
}

const FrameView& LoanedFrame::view() const {
  return view_;
}

FrameView LoanedFrame::detach() {
  pool_ = nullptr;
  return std::move(view_);
}

void LoanedFrame::release() {
  if (pool_ != nullptr && view_.buffer != nullptr) {
    pool_->return_buffer(std::move(view_.buffer));
  }
  pool_ = nullptr;
  view_ = {};
}

bool LoanedFrame::valid() const {
  return view_.valid();
}

LoanedFrame BufferPool::loan_frame(std::size_t size, std::uint32_t width, std::uint32_t height, std::uint32_t stride,
                                   std::string format) {
  std::shared_ptr<SharedBuffer> buffer;
  auto reusable = std::find_if(available_.begin(), available_.end(),
                               [size](const auto& candidate) { return candidate->size() >= size; });
  if (reusable == available_.end()) {
    buffer = std::make_shared<SharedBuffer>(size);
    ++stats_.alloc_count;
  } else {
    buffer = *reusable;
    available_.erase(reusable);
    ++stats_.reuse_count;
  }
  stats_.available_count = available_.size();
  return LoanedFrame(this, FrameView{buffer, 0, size, width, height, stride, std::move(format)});
}

BufferPoolStats BufferPool::stats() const {
  return stats_;
}

void BufferPool::return_buffer(std::shared_ptr<SharedBuffer> buffer) {
  available_.push_back(std::move(buffer));
  stats_.available_count = available_.size();
}

} // namespace topoexec
