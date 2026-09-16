#pragma once

// Phase 0 — mathematical constants from the standard library.

#include <numbers>

namespace mmathen::constants {

inline constexpr double pi = std::numbers::pi;
inline constexpr double twoPi = 2.0 * std::numbers::pi;
inline constexpr double sqrt2 = std::numbers::sqrt2;
inline constexpr double ln2 = std::numbers::ln2;
inline constexpr double ln10 = std::numbers::ln10;
inline constexpr double invSqrt2 = 1.0 / std::numbers::sqrt2;

}  // namespace mmathen::constants
