// D16 — validation annex: sourced material and instrument data (Catch2).
//
// Three kinds of evidence, kept apart on purpose:
// A. SOURCE CONSISTENCY — independent publications agree with each other
//    (Chabassier 2013 E/rho vs Young 1952 Q/rho; Young's and Fletcher's
//    published constants re-derived from E/rho).
// B. DATA CONSISTENCY — the published scale data (L, d, F, T0) reproduce the
//    published f0 through the ideal-string law (D2) the solvers use.
// C. MODEL vs MEASUREMENT — inharmonicity predicted by the D10 stiff-string
//    model from that data, against Young's MEASURED six-piano trend.
// Tolerances for C are trend-level because Young's summary says "about";
// the measured ratios are stated in the D16 record, not hidden behind them.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <utility>

#include <mmathen/math/comparison.h>
#include <mmathen/physics/materials.h>
#include <mmathen/physics/wave1d.h>
#include <mmathen/signal/inharmonic.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
}  // namespace

TEST_CASE("A: sources agree on steel wire", "[d16data]") {
  const auto steel = steelPianoWireChabassier2013();
  const double eOverRho = steel.youngsModulusPa / steel.densityKgPerM3;
  // Chabassier E/rho vs Young's Q/rho: 0.09% apart.
  CHECK(nearlyEqual(eOverRho, kYoung1952SteelSpecificModulus, 0.0, 2e-3));
  // Young's cents constant = (1200 / (2 ln 2)) * pi^2 (Q/rho)_cgs / 64,
  // since cents(n) ~ (1200/(2 ln2)) B n^2 and B = pi^2 (E/rho) d^2/(64 l^4 f0^2).
  const double eOverRhoCgs = eOverRho * 1e4;
  const double centsConstant = 1200.0 / (2.0 * std::numbers::ln2) *
                               std::numbers::pi * std::numbers::pi *
                               eOverRhoCgs / 64.0;
  CHECK(nearlyEqual(centsConstant, kYoung1952CentsConstantCgs, 0.0, 1e-3));
  // Fletcher 1964's steel constant 3.95e10 (as quoted by Roy 2024): 0.53%.
  const double fletcher = std::numbers::pi * std::numbers::pi * eOverRhoCgs / 64.0;
  CHECK(nearlyEqual(fletcher, 3.95e10, 0.0, 1e-2));
}

TEST_CASE("B: published scale data reproduce published f0", "[d16data]") {
  const auto steel = steelPianoWireChabassier2013();
  for (const auto& spec : chabassier2013Strings()) {
    const auto sys = unwrap(pianoString(spec, steel));
    // The ideal-string fundamental c/2L (D2's law).
    const double f0 = sys.speed.value() / (2.0 * spec.length.value());
    // All five within 0.32% of the printed f0 (printed to 0.1 Hz; tensions
    // to 1 N). Asserted at 0.5% (8.6 cents).
    CHECK(nearlyEqual(f0, spec.publishedF0.value(), 0.0, 5e-3));
  }
}

TEST_CASE("C: modeled inharmonicity follows Young's measured trend",
          "[d16data]") {
  const auto steel = steelPianoWireChabassier2013();
  for (const auto& spec : chabassier2013Strings()) {
    if (!spec.plainWire) continue;  // Young measured plain wire only
    const auto sys = unwrap(pianoString(spec, steel));
    const double modeled = unwrap(inharmonicityCents(sys, 2));
    const double measuredTrend = young1952Mode2Cents(spec.semitonesFromC4);
    const double ratio = modeled / measuredTrend;
    // Concert grand vs six-piano trend: 0.83..0.90 measured. Asserted at
    // +-25% ("about").
    CHECK(ratio > 0.75);
    CHECK(ratio < 1.25);
    // Young: below middle C, large pianos are consistently LESS
    // inharmonic. F3 on a concert grand must sit under the trend.
    if (spec.semitonesFromC4 < 0) CHECK(ratio < 1.0);
  }
}

TEST_CASE("piano data flows into the D1 and D10 models consistently",
          "[d16data]") {
  // The same string through D10 (solver modes) and D1 (Fletcher partials).
  const auto steel = steelPianoWireChabassier2013();
  const auto spec = chabassier2013Strings()[3];  // C#5
  const auto sys = unwrap(pianoString(spec, steel));
  const double f0Ideal = sys.speed.value() / (2.0 * spec.length.value());
  const auto spectrum =
      unwrap(makeStiffSpectrum(FrequencyHz{f0Ideal}, inharmonicityB(sys), 8));
  for (int n = 1; n <= 8; ++n)
    CHECK(nearlyEqual(unwrap(modeFrequency(sys, n)).value(),
                      unwrap(partialFrequency(spectrum, n)).value(), 0.0,
                      1e-12));
  // Young's cents formula (his cgs constant) on the same data.
  const double dCm = spec.diameter.value() * 100.0;
  const double lCm = spec.length.value() * 100.0;
  const double youngCents =
      kYoung1952CentsConstantCgs * 4.0 * dCm * dCm /
      (f0Ideal * f0Ideal * lCm * lCm * lCm * lCm);
  CHECK(nearlyEqual(unwrap(inharmonicityCents(sys, 2)), youngCents, 0.0,
                    5e-3));
}

TEST_CASE("piano data helpers reject invalid input", "[d16data]") {
  auto spec = chabassier2013Strings()[2];
  auto steel = steelPianoWireChabassier2013();
  steel.youngsModulusPa = -1.0;
  CHECK(pianoString(spec, steel).error() == QuantityError::InvalidArgument);
  spec.diameter = DistanceMeters{0.0};
  CHECK(pianoString(spec, steelPianoWireChabassier2013()).error() ==
        QuantityError::InvalidArgument);
  const auto pipe = unwrap(stoppedPipe(DistanceMeters{0.7},
                                       SpeedMetersPerSecond{343.0}));
  CHECK(inharmonicityCents(pipe, 2).error() == QuantityError::InvalidArgument);
}
