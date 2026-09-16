#pragma once

// Phase 1 — continuous time function: f: R -> X.
//
// Function1D<Output> is the TIME-FUNCTION contract (FOUNDATION.md), not a
// processor: at(t) is pure, repeatable, and seekable to any TimePoint with
// no hidden mutation. Stateful captures (mutable lambdas, shared mutable
// state) are a contract violation: callers must supply pure callables.
// Piecewise functions and declared discontinuities are allowed; nothing here
// promises differentiability or integrability.
//
// Errors follow the boundary policy: at() is the unchecked hot path for
// known-valid functions; tryAt() is the checked boundary and reports a
// non-finite floating output as InvalidArgument instead of propagating it.

#include <cmath>
#include <functional>
#include <string>
#include <type_traits>

#include "../core/result.h"
#include "../units/physical_units.h"

namespace mmathen {

template <typename Output>
class Function1D {
 public:
  using output_type = Output;

  Function1D(std::function<Output(TimePointSeconds)> fn, std::string formula)
      : fn_(std::move(fn)), formula_(std::move(formula)) {}

  // Pure evaluation at any time point. No hidden state is changed.
  [[nodiscard]] Output at(TimePointSeconds t) const { return fn_(t); }

  // Checked evaluation: rejects non-finite floating-point outputs.
  [[nodiscard]] Result<Output, QuantityError> tryAt(TimePointSeconds t) const {
    Output o = fn_(t);
    if constexpr (requires { typename Output::value_type; }) {
      if constexpr (std::is_floating_point_v<typename Output::value_type>) {
        if (!std::isfinite(o.value()))
          return Result<Output, QuantityError>::failure(
              QuantityError::InvalidArgument);
      }
    }
    return Result<Output, QuantityError>::success(std::move(o));
  }

  [[nodiscard]] const std::string& formula() const noexcept { return formula_; }

 private:
  std::function<Output(TimePointSeconds)> fn_;
  std::string formula_;
};

}  // namespace mmathen
