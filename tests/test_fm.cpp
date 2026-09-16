// D5 — FM synthesis and Bessel sideband tests (Catch2).
//
// Bessel anchors are mpmath-verified literals; the FM trajectory is checked
// against the textbook closed form written inline (independent code path
// from the sineFM adapter under test).

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/math/comparison.h>
#include <mmathen/signal/fm.h>
#include <mmathen/signal/oscillator.h>
#include <mmathen/signal/phase.h>
#include <mmathen/signal/signal.h>
#include <mmathen/time/factories.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
}  // namespace

TEST_CASE("Bessel sideband amplitudes match mpmath", "[d5fm]") {
  CHECK(nearlyEqual(std::cyl_bessel_j(0, 0.0), 1.0, 1e-12, 0.0));
  CHECK(nearlyEqual(std::cyl_bessel_j(0, 2.0), 0.2238907791412357, 1e-12,
                    1e-12));
  CHECK(nearlyEqual(std::cyl_bessel_j(1, 2.0), 0.5767248077568734, 1e-12,
                    1e-12));
  CHECK(nearlyEqual(std::cyl_bessel_j(2, 2.0), 0.3528340286156377, 1e-12,
                    1e-12));
  CHECK(nearlyEqual(std::cyl_bessel_j(3, 2.0), 0.12894324947440206, 1e-12,
                    1e-12));
  CHECK(nearlyEqual(std::cyl_bessel_j(4, 2.0), 0.033995719807568436, 1e-12,
                    1e-12));
  CHECK(nearlyEqual(std::cyl_bessel_j(1, 4.0), -0.06604332802354913, 1e-12,
                    1e-12));
  CHECK(std::fabs(std::cyl_bessel_j(0, 2.404825557695773)) < 1e-12);
}

TEST_CASE("sideband table follows Chowning's structure", "[d5fm]") {
  // fc = 220, fm = 110, I = 2: k = 0..4 per Carson (ceil(2)+2).
  const auto table =
      unwrap(sidebandTable(FrequencyHz{220.0}, FrequencyHz{110.0}, 2.0));
  CHECK(table.size() == 9);
  CHECK(table[0].frequency == FrequencyHz{220.0});
  CHECK(nearlyEqual(table[0].amplitude, 0.2238907791412357, 1e-12, 1e-12));
  // Upper k=1 at 330 with +J1; lower k=1 at 110 with -J1 (phase flip).
  CHECK(table[1].frequency == FrequencyHz{330.0});
  CHECK(nearlyEqual(table[1].amplitude, 0.5767248077568734, 1e-12, 1e-12));
  CHECK(table[2].frequency == FrequencyHz{110.0});
  CHECK(nearlyEqual(table[2].amplitude, -0.5767248077568734, 1e-12, 1e-12));
  // k=2 lower lands on 0 Hz: listed raw, unfolding deferred.
  CHECK(table[4].frequency == FrequencyHz{0.0});
  CHECK(nearlyEqual(table[4].amplitude, 0.3528340286156377, 1e-12, 1e-12));
  CHECK(unwrap(carsonBandwidth(FrequencyHz{110.0}, 2.0)) ==
        FrequencyHz{660.0});
  CHECK(sidebandTable(FrequencyHz{0.0}, FrequencyHz{110.0}, 2.0).error() ==
        QuantityError::DomainError);
  CHECK(sidebandTable(FrequencyHz{220.0}, FrequencyHz{110.0}, -1.0).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("FM tone matches the textbook closed form", "[d5fm]") {
  // x(t) = 0.25*sin(2*pi*440*t + 2*(1-cos(2*pi*5*t))): the integrated-phase
  // form of Chowning's equation (modulator phase convention documented).
  const auto sig = unwrap(fmTone(FrequencyHz{440.0}, FrequencyHz{5.0}, 2.0,
                                 constant(AmplitudeLinear{0.25})));
  for (const double t : {0.0, 0.037, 0.11, 0.73}) {
    const double expected =
        0.25 * std::sin(2.0 * std::numbers::pi * 440.0 * t +
                        2.0 * (1.0 - std::cos(2.0 * std::numbers::pi * 5.0 * t)));
    CHECK(nearlyEqual(sig.at(TimePointSeconds{t}),
                      SignedAudioValue{expected}, SignedAudioValue{1e-9},
                      1e-9));
  }
  // Deviation wiring: I=2, fm=5 -> d=10 Hz, same trajectory as sineFM.
  const auto direct =
      sineFM(FrequencyHz{440.0}, FrequencyHz{10.0}, FrequencyHz{5.0},
             constant(AmplitudeLinear{0.25}));
  CHECK(nearlyEqual(sig.at(TimePointSeconds{0.37}).value(),
                    direct.at(TimePointSeconds{0.37}).value(), 1e-12, 1e-12));
  CHECK(fmTone(FrequencyHz{440.0}, FrequencyHz{5.0}, -1.0).error() ==
        QuantityError::InvalidArgument);
  CHECK(fmTone(FrequencyHz{0.0}, FrequencyHz{5.0}, 2.0).error() ==
        QuantityError::DomainError);
}
