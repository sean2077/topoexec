#pragma once

// API stability: stable-v0.2. Cooperative cancellation tokens are public embedder API.

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>

namespace topoexec {

class CancellationSource;

class CancellationToken {
public:
  using RequestCallback = std::function<bool()>;

  CancellationToken() = default;

  static CancellationToken from_callback(RequestCallback callback) {
    auto state = std::make_shared<State>();
    state->callback = std::move(callback);
    return CancellationToken(std::move(state));
  }

  bool valid() const {
    return state_ != nullptr;
  }

  bool requested() const {
    return state_ != nullptr &&
           (state_->requested.load(std::memory_order_acquire) || (state_->callback && state_->callback()));
  }

  bool cancel_requested() const {
    const auto is_requested = requested();
    if (is_requested) {
      state_->observed_count.fetch_add(1u, std::memory_order_relaxed);
    }
    return is_requested;
  }

  std::size_t observed_count() const {
    return state_ == nullptr ? 0u : state_->observed_count.load(std::memory_order_relaxed);
  }

private:
  friend class CancellationSource;

  struct State {
    RequestCallback callback;
    std::atomic_bool requested{false};
    std::atomic_size_t observed_count{0};
  };

  explicit CancellationToken(std::shared_ptr<State> state) : state_(std::move(state)) {}

  std::shared_ptr<State> state_;
};

class CancellationSource {
public:
  CancellationSource() : state_(std::make_shared<CancellationToken::State>()) {}

  CancellationToken token() const {
    return CancellationToken(state_);
  }

  void request_cancel() {
    // Release so that state published before requesting cancellation is visible to any thread that observes
    // the flag with the matching acquire load in requested()/cancel_requested().
    state_->requested.store(true, std::memory_order_release);
  }

  bool cancel_requested() const {
    return state_->requested.load(std::memory_order_acquire);
  }

private:
  std::shared_ptr<CancellationToken::State> state_;
};

} // namespace topoexec
