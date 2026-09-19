// D21 — equalizer (Catch2).
//
// Exact cookbook properties: peaking |H(f0)| = gain, unity at DC and
// Nyquist, +g/-g sections cancel exactly; shelves reach the gain at their
// own end and unity at the other; 0 dB is an identity. Rendered sines
// through a parametric EQ come out at the level responseDb predicts
// (measured with the D11 estimator). Graphic EQ: flat gains are an exact
// passthrough to roundoff, a single band hits its gain exactly, and the band-overlap
// interaction is reported, not hidden.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/analysis/spectrum.h>
#include <mmathen/dsp/biquad.h>
#include <mmathen/dsp/equalizer.h>
#include <mmathen/math/comparison.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

const SampleRateHz kRate{48000.0};

double magDb(const BiquadCoeffs& k, double f) {
  return 20.0 * std::log10(unwrap(transferMagnitude(k, FrequencyHz{f}, kRate)));
}
}  // namespace

TEST_CASE("peaking section: exact gain at f0, unity at the ends",
          "[d21eq]") {
  for (const double g : {-12.0, -3.0, 6.0, 15.0}) {
    const auto k = unwrap(makePeaking(FrequencyHz{1000.0}, g, 1.4, kRate));
    CHECK(isStable(k));
    CHECK(nearlyEqual(magDb(k, 1000.0), g, 1e-10, 0.0));
    CHECK(std::fabs(magDb(k, 0.0)) < 1e-10);
    CHECK(std::fabs(magDb(k, 24000.0)) < 1e-10);
    // Boost and cut of the same size are exact inverses.
    const auto inv = unwrap(makePeaking(FrequencyHz{1000.0}, -g, 1.4, kRate));
    for (const double f : {50.0, 700.0, 1000.0, 3000.0, 15000.0})
      CHECK(std::fabs(magDb(k, f) + magDb(inv, f)) < 1e-10);
  }
  // 0 dB is an identity section: numerator == denominator, exactly.
  const auto flat = unwrap(makePeaking(FrequencyHz{1000.0}, 0.0, 1.4, kRate));
  CHECK(flat.b1 == flat.a1);
  CHECK(flat.b2 == flat.a2);
  CHECK(flat.b0 == 1.0);
}

TEST_CASE("shelves reach their gain at their own end", "[d21eq]") {
  for (const double g : {-9.0, 6.0}) {
    const auto lo = unwrap(makeLowShelf(FrequencyHz{300.0}, g, 0.7071, kRate));
    const auto hi = unwrap(makeHighShelf(FrequencyHz{4000.0}, g, 0.7071, kRate));
    CHECK(isStable(lo));
    CHECK(isStable(hi));
    CHECK(nearlyEqual(magDb(lo, 0.0), g, 1e-10, 0.0));
    CHECK(std::fabs(magDb(lo, 24000.0)) < 1e-10);
    CHECK(std::fabs(magDb(hi, 0.0)) < 1e-10);
    CHECK(nearlyEqual(magDb(hi, 24000.0), g, 1e-10, 0.0));
    // Half the gain at the corner frequency (cookbook midpoint property).
    CHECK(nearlyEqual(magDb(lo, 300.0), g / 2.0, 1e-9, 0.0));
    CHECK(nearlyEqual(magDb(hi, 4000.0), g / 2.0, 1e-9, 0.0));
  }
}

TEST_CASE("rendered sines match the cascade response", "[d21eq]") {
  auto eq = unwrap(ParametricEq::create(
      {EqBand{EqShape::LowShelf, FrequencyHz{120.0}, 4.0, 0.7071},
       EqBand{EqShape::Peaking, FrequencyHz{900.0}, -7.5, 2.0},
       EqBand{EqShape::HighShelf, FrequencyHz{6000.0}, 5.0, 0.7071}},
      kRate));
  for (const double f : {80.0, 900.0, 2500.0, 11000.0}) {
    eq.reset();
    const std::size_t n = 65536;
    std::vector<double> x(n), y(n);
    for (std::size_t i = 0; i < n; ++i)
      x[i] = 0.25 * std::sin(2.0 * std::numbers::pi * f * static_cast<double>(i) /
                             48000.0);
    eq.processBlock(x.data(), y.data(), FrameCount{static_cast<std::uint64_t>(n)});
    // Measure the steady-state second half (transient long gone).
    std::vector<double> tail(y.begin() + n / 2, y.end());
    const auto est = unwrap(estimateSinusoid(
        tail, kRate, Window::BlackmanHarris, FrequencyHz{f * 0.95},
        FrequencyHz{f * 1.05}));
    const double measured = 20.0 * std::log10(est.amplitude / 0.25);
    CHECK(nearlyEqual(measured, unwrap(eq.responseDb(FrequencyHz{f})), 1e-4,
                      0.0));
  }
}

TEST_CASE("graphic EQ: flat passthrough, single band exact, interaction "
          "reported",
          "[d21eq]") {
  const auto bands = unwrap(fractionalOctaveBands(1, FrequencyHz{30.0},
                                                  FrequencyHz{17000.0}));
  REQUIRE(bands.size() == 10);
  // Flat: every section is an identity (response exactly 0 dB), so the
  // output equals the input to roundoff. Not bit-for-bit: the difference
  // equation's terms cancel mathematically, not in floating point (a first
  // version of this test asserted x == y and was wrong).
  auto flat = unwrap(makeGraphicEq(bands, std::vector<double>(10, 0.0), kRate));
  std::vector<double> x(4096), y(4096);
  for (std::size_t i = 0; i < x.size(); ++i)
    x[i] = std::sin(0.37 * static_cast<double>(i)) * 0.3;
  flat.processBlock(x.data(), y.data(), FrameCount{4096});
  double worst = 0.0;
  for (std::size_t i = 0; i < x.size(); ++i)
    worst = (std::max)(worst, std::fabs(y[i] - x[i]));
  // Measured 2.4e-13: ten cascaded sections, the lowest with poles very
  // near the unit circle, recirculate roundoff. Asserted < 1e-11.
  CHECK(worst < 1e-11);
  for (const auto& b : bands)
    CHECK(unwrap(flat.responseDb(b.center)) == 0.0);
  // One band at +9 dB, the rest 0 dB (identities): exact at its centre.
  std::vector<double> one(10, 0.0);
  one[5] = 9.0;
  const auto single = unwrap(makeGraphicEq(bands, one, kRate));
  CHECK(nearlyEqual(unwrap(single.responseDb(bands[5].center)), 9.0, 1e-10, 0.0));
  // All bands +6 dB: neighbours' skirts add up at every centre. Stated.
  const std::vector<double> all(10, 6.0);
  const auto boosted = unwrap(makeGraphicEq(bands, all, kRate));
  const auto err = unwrap(graphicEqCentreErrorDb(boosted, bands, all));
  for (std::size_t i = 1; i + 1 < err.size(); ++i) {
    CHECK(err[i] > 0.5);  // interaction is real and positive
    CHECK(err[i] < 4.0);  // and bounded (measured 1.6 .. 2.8 dB interior)
  }
  // Band-edge Q for an octave: r = 10^0.3 -> Q = sqrt(r)/(r-1) ~= 1.4142.
  CHECK(nearlyEqual(bandEdgeQ(bands[5]),
                    std::sqrt(std::pow(10.0, 0.3)) / (std::pow(10.0, 0.3) - 1.0),
                    0.0, 1e-14));
}

TEST_CASE("equalizers reject invalid input", "[d21eq]") {
  CHECK(makePeaking(FrequencyHz{30000.0}, 3.0, 1.0, kRate).error() ==
        QuantityError::InvalidArgument);  // above Nyquist
  CHECK(makePeaking(FrequencyHz{1000.0}, 3.0, 0.0, kRate).error() ==
        QuantityError::InvalidArgument);
  CHECK(makeLowShelf(FrequencyHz{100.0}, 80.0, 0.7, kRate).error() ==
        QuantityError::InvalidArgument);  // outside the stated gain range
  CHECK(ParametricEq::create({}, kRate).error() ==
        QuantityError::InvalidArgument);
  const auto bands = unwrap(fractionalOctaveBands(1, FrequencyHz{30.0},
                                                  FrequencyHz{17000.0}));
  CHECK(makeGraphicEq(bands, std::vector<double>(3, 0.0), kRate).error() ==
        QuantityError::InvalidArgument);
  CHECK(makeGraphicEq(bands, std::vector<double>(10, 0.0), SampleRateHz{22050.0})
            .error() == QuantityError::DomainError);  // 15.8k > 0.45 fs
}

TEST_CASE("setBand retunes in place: same response as a fresh EQ, state "
          "kept, unchanged on error",
          "[d21eq]") {
  const std::vector<EqBand> start = {
      {EqShape::LowShelf, FrequencyHz{120.0}, 4.0, 0.7071067811865476},
      {EqShape::Peaking, FrequencyHz{1000.0}, -3.0, 1.4},
      {EqShape::HighShelf, FrequencyHz{6000.0}, 2.0, 0.7071067811865476}};
  auto eq = unwrap(ParametricEq::create(start, kRate));

  // Retuned response equals a freshly built EQ with the same bands, exactly.
  EqBand changed = start[1];
  changed.gainDb = 7.5;
  changed.frequency = FrequencyHz{2500.0};
  REQUIRE(eq.setBand(1, changed).ok());
  auto moved = start;
  moved[1] = changed;
  const auto fresh = unwrap(ParametricEq::create(moved, kRate));
  for (double f : {30.0, 250.0, 2500.0, 9000.0, 20000.0})
    CHECK(unwrap(eq.responseDb(FrequencyHz{f})) ==
          unwrap(fresh.responseDb(FrequencyHz{f})));

  // State is kept: splitting a run with a no-op retune in the middle is
  // bit-identical to one uninterrupted run.
  std::vector<double> x(2000), whole(2000), split(2000);
  for (std::size_t i = 0; i < x.size(); ++i)
    x[i] = std::sin(0.037 * static_cast<double>(i)) +
           0.3 * std::sin(0.61 * static_cast<double>(i));
  auto a = unwrap(ParametricEq::create(start, kRate));
  auto b = unwrap(ParametricEq::create(start, kRate));
  a.processBlock(x.data(), whole.data(), FrameCount{2000});
  b.processBlock(x.data(), split.data(), FrameCount{1000});
  REQUIRE(b.setBand(0, start[0]).ok());
  b.processBlock(x.data() + 1000, split.data() + 1000, FrameCount{1000});
  CHECK(whole == split);

  // Errors leave the EQ untouched.
  const double before = unwrap(eq.responseDb(FrequencyHz{2500.0}));
  CHECK(eq.setBand(3, changed).error() == QuantityError::InvalidArgument);
  EqBand bad = changed;
  bad.q = 0.0;
  CHECK(eq.setBand(1, bad).error() == QuantityError::InvalidArgument);
  CHECK(unwrap(eq.responseDb(FrequencyHz{2500.0})) == before);
  CHECK(eq.bands()[1].gainDb == 7.5);
}
