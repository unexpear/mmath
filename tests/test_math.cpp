// Phase 0 — math tests (Catch2, test-only dependency).
// Comparison expectations below are independently specified literals, never
// produced by the helper under test.

#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <numbers>
#include <stdexcept>

#include <mmathen/math/comparison.h>
#include <mmathen/math/constants.h>
#include <mmathen/math/scalar.h>

using namespace mmathen;

TEST_CASE("constants match the standard library", "[math]") {
  CHECK(constants::pi == std::numbers::pi);
  CHECK(constants::twoPi == 2.0 * std::numbers::pi);
  CHECK(constants::sqrt2 == std::numbers::sqrt2);
  CHECK(constants::invSqrt2 == 1.0 / std::numbers::sqrt2);
}

TEST_CASE("nearlyEqual follows the max-criterion", "[math]") {
  // |a-b| <= max(eps_abs, eps_rel*max(|a|,|b|)).
  CHECK(nearlyEqual(1.0, 1.0, 0.0, 0.0));           // exact equality
  CHECK(nearlyEqual(1.0, 1.0 + 5e-9, 1e-8, 0.0));   // within abs tol
  CHECK_FALSE(nearlyEqual(1.0, 1.0 + 5e-8, 1e-8, 0.0));
  CHECK(nearlyEqual(1000.0, 1000.5, 0.0, 1e-3));    // within rel tol
  CHECK_FALSE(nearlyEqual(1000.0, 1001.5, 0.0, 1e-3));
  // Boundary: diff exactly at max(eps_abs, eps_rel*scale) holds ...
  CHECK(nearlyEqual(10.0, 11.0, 1.0, 0.0));
  // ... just above it does not.
  CHECK_FALSE(nearlyEqual(10.0, 11.0 + 1e-9, 1.0, 0.0));
}

TEST_CASE("nearlyEqual defines non-finite behavior explicitly", "[math]") {
  const double inf = std::numeric_limits<double>::infinity();
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK_FALSE(nearlyEqual(nan, nan, 1e-9, 1e-9));  // NaN never equal
  CHECK_FALSE(nearlyEqual(inf, 1.0, 1e-9, 1e-9));
  CHECK_FALSE(nearlyEqual(1.0, inf, 1e-9, 1e-9));
  CHECK(nearlyEqual(inf, inf, 1e-9, 1e-9));  // exactly equal infinities
}

TEST_CASE("nearlyEqual rejects invalid tolerances", "[math]") {
  CHECK_THROWS_AS(nearlyEqual(1.0, 1.0, -1e-9, 0.0), std::invalid_argument);
  CHECK_THROWS_AS(nearlyEqual(1.0, 1.0, 1e-9, -1.0), std::invalid_argument);
  CHECK_THROWS_AS(
      nearlyEqual(1.0, 1.0, std::numeric_limits<double>::quiet_NaN(), 0.0),
      std::invalid_argument);
  CHECK_THROWS_AS(
      nearlyEqual(1.0, 1.0, 1e-9,
                  std::numeric_limits<double>::infinity()),
      std::invalid_argument);
}

TEST_CASE("scalar finite validation", "[math]") {
  CHECK(isFiniteReal(440.0));
  CHECK_FALSE(isFiniteReal(std::numeric_limits<double>::infinity()));
  CHECK_FALSE(isFiniteReal(std::numeric_limits<double>::quiet_NaN()));
  CHECK_NOTHROW(requireFiniteReal(1.0, "ok"));
  CHECK_THROWS_AS(
      requireFiniteReal(std::numeric_limits<double>::quiet_NaN(), "nan"),
      std::domain_error);
  CHECK_THROWS_AS(
      requireFiniteReal(std::numeric_limits<double>::infinity(), "inf"),
      std::domain_error);
}
