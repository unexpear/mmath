#pragma once

// Phase 0 — tolerance comparison, independent of the quantities it checks.
//
//   operator== : exact equality of the represented values.
//   nearlyEqual: |a-b| <= max(eps_abs, eps_rel * max(|a|,|b|)).
//   FrameCount / FrameIndex: exact comparison only (no nearlyEqual overload).
//
// For quantity comparisons the absolute tolerance carries the appropriate
// unit (for two TimePoints it is a Duration, not another TimePoint); the
// relative tolerance is a dimensionless double. Negative or non-finite
// tolerances are rejected with std::invalid_argument. Non-finite operands
// compare true only when exactly equal (a == b, so +inf == +inf holds);
// NaN operands always compare false. Tests below pin these rules with
// independently specified expectations, never via the helper itself.

#include <algorithm>
#include <cmath>
#include <concepts>
#include <stdexcept>
#include <type_traits>

namespace mmathen {

[[nodiscard]] inline bool nearlyEqual(double a, double b, double absTol,
                                      double relTol) {
  if (!(absTol >= 0.0) || !(relTol >= 0.0) || !std::isfinite(absTol) ||
      !std::isfinite(relTol))
    throw std::invalid_argument(
        "nearlyEqual: tolerances must be finite and >= 0");
  if (a == b) return true;
  if (!std::isfinite(a) || !std::isfinite(b)) return false;
  const double scale = (std::max)(std::fabs(a), std::fabs(b));
  return std::fabs(a - b) <= (std::max)(absTol, relTol * scale);
}

template <typename Q>
  requires std::is_floating_point_v<typename Q::value_type>
[[nodiscard]] bool nearlyEqual(Q a, Q b, Q absTol, double relTol) {
  return nearlyEqual(a.value(), b.value(), absTol.value(), relTol);
}

}  // namespace mmathen
