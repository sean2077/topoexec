#pragma once

// API stability: stable-v0.2. Status/Result helpers are intended embedder API.

#include <optional>
#include <stdexcept>
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
    if (!value_.has_value()) {
      throw std::logic_error("topoexec::Result has no value");
    }
    return value_.value();
  }

  T& value() {
    if (!value_.has_value()) {
      throw std::logic_error("topoexec::Result has no value");
    }
    return value_.value();
  }

private:
  Status status_;
  std::optional<T> value_;
};

} // namespace topoexec
