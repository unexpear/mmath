#pragma once

// Phase 0 — minimal project-owned fallible-result utility.
//
// Deliberately NOT std::expected: <expected> is C++23 and this project
// compiles as strict C++20. Keep narrow: success/failure, ok(), value(),
// error(). Do not grow this into a general error-handling framework.

#include <optional>
#include <stdexcept>
#include <utility>

namespace mmathen {

enum class QuantityError {
  None = 0,
  InvalidArgument,  // rejected input: bad creation args, bad tolerances,
                    // negative/fractional counter inputs, decrement below zero
  DomainError,      // mathematically undefined: period(f<=0), cents of
                    // non-positive ratio, negative-duration frame count
  RangeError,       // valid inputs, invalid result: overflow to infinity,
                    // underflow to an invalid zero ratio, counter overflow
};

template <typename T, typename E>
class [[nodiscard]] Result {
 public:
  static Result success(T value) {
    Result r;
    r.storage_.emplace(std::move(value));
    return r;
  }
  static Result failure(E e) {
    Result r;
    r.error_ = e;
    return r;
  }

  [[nodiscard]] bool ok() const noexcept { return storage_.has_value(); }
  [[nodiscard]] E error() const noexcept { return error_; }

  const T& value() const& {
    if (!ok()) throw std::logic_error("Result: accessed value of failure");
    return *storage_;
  }
  T value() && {
    if (!ok()) throw std::logic_error("Result: accessed value of failure");
    return std::move(*storage_);
  }

 private:
  Result() = default;
  std::optional<T> storage_;
  E error_{};
};

}  // namespace mmathen
