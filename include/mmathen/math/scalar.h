#pragma once

// Phase 0 — scalar numeric policy. Foundation uses double; no fast-math in
// correctness builds (see CMakeLists + NUMERICS.md).

#include <cmath>
#include <stdexcept>

namespace mmathen {

using Real = double;

[[nodiscard]] inline bool isFiniteReal(Real x) noexcept { return std::isfinite(x); }

inline void requireFiniteReal(Real x, const char* what) {
  if (!std::isfinite(x)) throw std::domain_error(what);
}

}  // namespace mmathen
