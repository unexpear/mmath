// Phase 1 — continuous-function tests (Catch2, test-only dependency).

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <numbers>

#include <mmathen/math/comparison.h>
#include <mmathen/time/factories.h>
#include <mmathen/time/function.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

TEST_CASE("constant functions return the value at any time", "[time]") {
  const auto f = constant(FrequencyHz{440.0});
  CHECK(f.at(TimePointSeconds{0.0}) == FrequencyHz{440.0});
  CHECK(f.at(TimePointSeconds{0.000021}) == FrequencyHz{440.0});
  CHECK(f.at(TimePointSeconds{14.239813}) == FrequencyHz{440.0});
  CHECK(f.at(TimePointSeconds{-5.0}) == FrequencyHz{440.0});  // no clamping
  CHECK(f.formula() == "const");
  CHECK(f.tryAt(TimePointSeconds{1.0}).ok());
}

TEST_CASE("linear sweep matches its closed form", "[time]") {
  // f(t) = 220 + 220*t, sampled against the definitional expression.
  const auto sweep = wrap<FrequencyHz>(
      [](TimePointSeconds t) {
        return FrequencyHz{220.0 + 220.0 * t.value()};
      },
      "220 + 220*t");
  CHECK(sweep.formula() == "220 + 220*t");
  for (const double t : {0.0, 0.25, 1.0, 3.7}) {
    CHECK(nearlyEqual(sweep.at(TimePointSeconds{t}),
                      FrequencyHz{220.0 + 220.0 * t}, FrequencyHz{1e-9}, 0.0));
  }
}

TEST_CASE("vibrato-shaped frequency stays an inspectable object", "[time]") {
  // f(t) = 440 + 10*sin(2*pi*5*t): independent anchors, not self-reference.
  const auto fm = wrap<FrequencyHz>(
      [](TimePointSeconds t) {
        return FrequencyHz{440.0 +
                           10.0 * std::sin(2.0 * std::numbers::pi * 5.0 *
                                           t.value())};
      },
      "440 + 10*sin(2*pi*5*t)");
  CHECK(fm.at(TimePointSeconds{0.0}) == FrequencyHz{440.0});
  CHECK(nearlyEqual(fm.at(TimePointSeconds{0.05}), FrequencyHz{450.0},
                    FrequencyHz{1e-9}, 0.0));  // sin(pi/2) = 1
  CHECK(nearlyEqual(fm.at(TimePointSeconds{0.15}), FrequencyHz{430.0},
                    FrequencyHz{1e-9}, 0.0));  // sin(3pi/2) = -1
  CHECK(fm.formula() == "440 + 10*sin(2*pi*5*t)");
}

TEST_CASE("tryAt rejects non-finite outputs at the boundary", "[time]") {
  const auto bad = wrap<FrequencyHz>(
      [](TimePointSeconds) {
        return FrequencyHz{std::numeric_limits<double>::quiet_NaN()};
      },
      "nan");
  CHECK(bad.tryAt(TimePointSeconds{0.0}).error() ==
        QuantityError::InvalidArgument);
  const auto good = constant(FrequencyHz{440.0});
  CHECK(good.tryAt(TimePointSeconds{0.0}).ok());
}

TEST_CASE("evaluation is repeatable and order-independent", "[time]") {
  const auto sweep = wrap<FrequencyHz>(
      [](TimePointSeconds t) {
        return FrequencyHz{220.0 + 220.0 * t.value()};
      },
      "220 + 220*t");
  // Seeking backward then forward yields identical values (pure function,
  // no hidden state to reset).
  CHECK(sweep.at(TimePointSeconds{3.0}) == sweep.at(TimePointSeconds{3.0}));
  const auto late = sweep.at(TimePointSeconds{10.0});
  const auto early = sweep.at(TimePointSeconds{0.0});
  CHECK(early == FrequencyHz{220.0});
  CHECK(late == FrequencyHz{2420.0});
}
