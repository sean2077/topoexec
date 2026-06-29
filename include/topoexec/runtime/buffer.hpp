#pragma once

// API stability: mixed. SharedBuffer/FrameView are stable-v0.2; BufferPool/LoanedFrame are experimental before beta.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace topoexec {

class SharedBuffer {
public:
  explicit SharedBuffer(std::size_t size = 0);

  std::uint8_t* data();
  const std::uint8_t* data() const;
  std::size_t size() const;
  void resize(std::size_t size);

private:
  std::vector<std::uint8_t> bytes_;
};

struct FrameView {
  std::shared_ptr<SharedBuffer> buffer;
  std::size_t offset{0};
  std::size_t size{0};
  std::uint32_t width{0};
  std::uint32_t height{0};
  std::uint32_t stride{0};
  std::string format;

  const std::uint8_t* data() const;
  bool valid() const;
  const void* payload_address() const;
};

struct BufferPoolStats {
  std::size_t alloc_count{0};
  std::size_t reuse_count{0};
  std::size_t loan_count{0};
  std::size_t release_count{0};
  std::size_t detached_count{0};
  std::size_t exhausted_count{0};
  std::size_t bytes_allocated{0};
  std::size_t bytes_owned{0};
  std::size_t active_bytes{0};
  std::size_t high_watermark_bytes{0};
  std::size_t bytes_available{0};
  std::size_t active_count{0};
  std::size_t available_count{0};
  std::size_t max_bytes{0};
};

struct BufferPoolConfig {
  std::size_t fixed_block_size{0};
  std::vector<std::size_t> bucket_sizes;
  std::size_t alignment{1};
  std::size_t max_bytes{0};
};

class BufferPool;

class LoanedFrame {
public:
  LoanedFrame() = default;
  LoanedFrame(BufferPool* pool, FrameView view);
  LoanedFrame(LoanedFrame&& other) noexcept;
  LoanedFrame& operator=(LoanedFrame&& other) noexcept;
  ~LoanedFrame();

  LoanedFrame(const LoanedFrame&) = delete;
  LoanedFrame& operator=(const LoanedFrame&) = delete;

  FrameView& view();
  const FrameView& view() const;
  FrameView detach();
  void release();
  bool valid() const;

private:
  BufferPool* pool_{nullptr};
  FrameView view_;
};

class BufferPool {
public:
  BufferPool() = default;
  explicit BufferPool(BufferPoolConfig config);

  // Movable (the mutex member otherwise deletes these). Moving a pool with outstanding loans is unsupported,
  // since LoanedFrame holds a raw pointer back to its pool; callers must move only an idle pool.
  BufferPool(BufferPool&& other) noexcept;
  BufferPool& operator=(BufferPool&& other) noexcept;
  BufferPool(const BufferPool&) = delete;
  BufferPool& operator=(const BufferPool&) = delete;

  LoanedFrame loan_frame(std::size_t size, std::uint32_t width, std::uint32_t height, std::uint32_t stride,
                         std::string format);
  const BufferPoolConfig& config() const;
  BufferPoolStats stats() const;
  bool has_outstanding_loans() const;

private:
  friend class LoanedFrame;

  std::size_t allocation_size(std::size_t requested) const;
  bool can_allocate(std::size_t allocation_size) const;
  void mark_detached(const std::shared_ptr<SharedBuffer>& buffer);
  void return_buffer(std::shared_ptr<SharedBuffer> buffer);
  // Private helpers below assume mutex_ is already held by the calling public method.
  void refresh_available_stats();

  // Loaned frames are designed to flow across threads (e.g. thread_pool lanes) and are returned/detached from
  // whichever thread releases them, so every access to the pool's mutable state is serialized by this mutex.
  mutable std::mutex mutex_;
  BufferPoolConfig config_;
  std::vector<std::shared_ptr<SharedBuffer>> available_;
  BufferPoolStats stats_;
};

} // namespace topoexec
