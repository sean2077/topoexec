#pragma once

// Public API category: stable 0.x status/result helpers.

#include <optional>
#include <string>
#include <utility>

namespace topoexec {

class Status {
public:
  Status() = default;

  static Status success() {
    return Status(true, {});
  }

  static Status error(std::string message) {
    return Status(false, std::move(message));
  }

  bool ok() const {
    return ok_;
  }

  const std::string& message() const {
    return message_;
  }

  explicit operator bool() const {
    return ok();
  }

private:
  Status(bool ok, std::string message) : ok_(ok), message_(std::move(message)) {}

  bool ok_{true};
  std::string message_;
};

template <typename T> class Result {
public:
  Result(T value) : status_(Status::success()), value_(std::move(value)) {}
  Result(Status status) : status_(std::move(status)) {}

  const Status& status() const {
    return status_;
  }

  bool ok() const {
    return status_.ok();
  }

  const T& value() const {
    return *value_;
  }

  T& value() {
    return *value_;
  }

private:
  Status status_;
  std::optional<T> value_;
};

} // namespace topoexec
