#include "topoexec/runtime/buffer.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace topoexec {
namespace {

std::size_t align_up(std::size_t value, std::size_t alignment) {
  if (alignment <= 1u || value == 0u) {
    return value;
  }
  const auto remainder = value % alignment;
  if (remainder == 0u) {
    return value;
  }
  const auto increment = alignment - remainder;
  if (value > std::numeric_limits<std::size_t>::max() - increment) {
    return value;
  }
  return value + increment;
}

} // namespace

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
  if (pool_ != nullptr && view_.buffer != nullptr) {
    pool_->mark_detached(view_.buffer);
  }
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

BufferPool::BufferPool(BufferPoolConfig config) : config_(std::move(config)) {
  if (config_.alignment == 0u) {
    config_.alignment = 1u;
  }
  stats_.max_bytes = config_.max_bytes;
}

LoanedFrame BufferPool::loan_frame(std::size_t size, std::uint32_t width, std::uint32_t height, std::uint32_t stride,
                                   std::string format) {
  std::shared_ptr<SharedBuffer> buffer;
  auto reusable = std::find_if(available_.begin(), available_.end(),
                               [size](const auto& candidate) { return candidate->size() >= size; });
  if (reusable == available_.end()) {
    const auto allocated_size = allocation_size(size);
    if (!can_allocate(allocated_size)) {
      ++stats_.exhausted_count;
      refresh_available_stats();
      return {};
    }
    buffer = std::make_shared<SharedBuffer>(allocated_size);
    ++stats_.alloc_count;
    stats_.bytes_allocated += allocated_size;
    stats_.bytes_owned += allocated_size;
  } else {
    buffer = *reusable;
    available_.erase(reusable);
    ++stats_.reuse_count;
  }
  ++stats_.loan_count;
  ++stats_.active_count;
  stats_.active_bytes += buffer->size();
  stats_.high_watermark_bytes = std::max(stats_.high_watermark_bytes, stats_.active_bytes);
  refresh_available_stats();
  return LoanedFrame(this, FrameView{buffer, 0, size, width, height, stride, std::move(format)});
}

const BufferPoolConfig& BufferPool::config() const {
  return config_;
}

BufferPoolStats BufferPool::stats() const {
  return stats_;
}

bool BufferPool::has_outstanding_loans() const {
  return stats_.active_count != 0u;
}

std::size_t BufferPool::allocation_size(std::size_t requested) const {
  std::size_t chosen = requested;
  if (!config_.bucket_sizes.empty()) {
    auto bucket = std::min_element(config_.bucket_sizes.begin(), config_.bucket_sizes.end(),
                                   [requested](std::size_t lhs, std::size_t rhs) {
                                     const auto lhs_fits = lhs >= requested;
                                     const auto rhs_fits = rhs >= requested;
                                     if (lhs_fits != rhs_fits) {
                                       return lhs_fits;
                                     }
                                     return lhs < rhs;
                                   });
    if (bucket != config_.bucket_sizes.end() && *bucket >= requested) {
      chosen = *bucket;
    }
  } else if (config_.fixed_block_size > 0u) {
    chosen = std::max(config_.fixed_block_size, requested);
  }
  return align_up(chosen, config_.alignment);
}

bool BufferPool::can_allocate(std::size_t allocation_size) const {
  return config_.max_bytes == 0u ||
         (stats_.bytes_owned <= config_.max_bytes && allocation_size <= config_.max_bytes - stats_.bytes_owned);
}

void BufferPool::mark_detached(const std::shared_ptr<SharedBuffer>& buffer) {
  if (buffer == nullptr) {
    return;
  }
  ++stats_.detached_count;
  if (stats_.active_count > 0u) {
    --stats_.active_count;
  }
  if (stats_.active_bytes >= buffer->size()) {
    stats_.active_bytes -= buffer->size();
  } else {
    stats_.active_bytes = 0u;
  }
  if (stats_.bytes_owned >= buffer->size()) {
    stats_.bytes_owned -= buffer->size();
  } else {
    stats_.bytes_owned = 0u;
  }
  refresh_available_stats();
}

void BufferPool::return_buffer(std::shared_ptr<SharedBuffer> buffer) {
  const auto size = buffer == nullptr ? 0u : buffer->size();
  available_.push_back(std::move(buffer));
  ++stats_.release_count;
  if (stats_.active_count > 0u) {
    --stats_.active_count;
  }
  if (stats_.active_bytes >= size) {
    stats_.active_bytes -= size;
  } else {
    stats_.active_bytes = 0u;
  }
  refresh_available_stats();
}

void BufferPool::refresh_available_stats() {
  stats_.available_count = available_.size();
  stats_.bytes_available = 0;
  for (const auto& available : available_) {
    if (available != nullptr) {
      stats_.bytes_available += available->size();
    }
  }
}

} // namespace topoexec
