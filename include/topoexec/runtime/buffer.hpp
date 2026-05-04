#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
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
  std::size_t available_count{0};
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
  LoanedFrame loan_frame(std::size_t size, std::uint32_t width, std::uint32_t height, std::uint32_t stride,
                         std::string format);
  BufferPoolStats stats() const;

private:
  friend class LoanedFrame;

  void return_buffer(std::shared_ptr<SharedBuffer> buffer);

  std::vector<std::shared_ptr<SharedBuffer>> available_;
  BufferPoolStats stats_;
};

} // namespace topoexec
