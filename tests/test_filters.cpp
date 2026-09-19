// D23 — digital filters (Catch2).
//
// Butterworth cascades of every order 1..8 match the closed-form bilinear
// magnitude at every tested frequency; RBJ band-pass/notch/all-pass hit
// their exact anchors; Linkwitz-Riley LP + HP is an all-pass; every design
// over a frequency grid is stable; BiquadCascade processes exactly like the
// sections in series, keeps state across setSections, and refuses what it
// cannot hold.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/dsp/biquad.h>
#include <mmathen/dsp/equalizer.h>
#include <mmathen/dsp/filters.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
const SampleRateHz kRate{48000.0};

double mag(const std::vector<BiquadCoeffs>& s, double f) {
  return std::abs(unwrap(cascadeResponse(s, FrequencyHz{f}, kRate)));
}
}  // namespace

TEST_CASE("Butterworth cascades equal the closed-form bilinear magnitude",
          "[d23filt]") {
  const double fs = kRate.value();
  for (const bool hp : {false, true}) {
    for (int n = 1; n <= 8; ++n) {
      for (const double fc : {80.0, 1000.0, 9000.0}) {
        const auto s = unwrap(designButterworth(hp, n, FrequencyHz{fc}, kRate));
        CHECK(s.size() == static_cast<std::size_t>((n + 1) / 2));
        for (const auto& k : s) CHECK(isStable(k));
        const double tc = std::tan(std::numbers::pi * fc / fs);
        for (const double f : {10.0, 50.0, 200.0, 999.0, 1000.0, 4000.0,
                               15000.0, 23000.0}) {
          const double t = std::tan(std::numbers::pi * f / fs);
          const double ratio = hp ? tc / t : t / tc;
          const double expected =
              1.0 / std::sqrt(1.0 + std::pow(ratio, 2.0 * n));
          const double got = mag(s, f);
          // Relative agreement; tiny stop-band values compared in absolute.
          CHECK(std::fabs(got - expected) <= 1e-9 * expected + 1e-13);
        }
        // -3.0103 dB at the cutoff, every order.
        CHECK(std::fabs(mag(s, fc) - 1.0 / std::sqrt(2.0)) < 1e-12);
      }
    }
  }
}

TEST_CASE("RBJ band-pass, notch and all-pass anchors", "[d23filt]") {
  for (const double q : {0.5, 2.0, 10.0}) {
    const FrequencyHz f0{1500.0};
    const auto bp = unwrap(makeBandpass(f0, q, kRate));
    const auto no = unwrap(makeNotch(f0, q, kRate));
    const auto ap = unwrap(makeAllpass(f0, q, kRate));
    CHECK((isStable(bp) && isStable(no) && isStable(ap)));
    CHECK(std::fabs(mag({bp}, 1500.0) - 1.0) < 1e-12);
    CHECK(mag({bp}, 0.0) < 1e-15);
    CHECK(mag({bp}, 24000.0) < 1e-12);
    CHECK(mag({no}, 1500.0) < 1e-12);
    CHECK(std::fabs(mag({no}, 0.0) - 1.0) < 1e-12);
    CHECK(std::fabs(mag({no}, 24000.0) - 1.0) < 1e-12);
    for (const double f : {0.0, 20.0, 700.0, 1500.0, 3000.0, 20000.0, 24000.0})
      CHECK(std::fabs(mag({ap}, f) - 1.0) < 1e-12);
    const auto h = unwrap(transferResponse(ap, f0, kRate));
    CHECK(std::fabs(std::fabs(std::arg(h)) - std::numbers::pi) < 1e-9);
  }
}

TEST_CASE("Linkwitz-Riley crossovers sum flat", "[d23filt]") {
  for (const int order : {2, 4, 8}) {
    const FrequencyHz fc{2000.0};
    const auto lp = unwrap(designLinkwitzRiley(false, order, fc, kRate));
    const auto hp = unwrap(designLinkwitzRiley(true, order, fc, kRate));
    CHECK(std::fabs(20.0 * std::log10(mag(lp, 2000.0)) +
                    20.0 * std::log10(2.0)) < 1e-9);  // -6.02 dB
    CHECK(std::fabs(20.0 * std::log10(mag(hp, 2000.0)) +
                    20.0 * std::log10(2.0)) < 1e-9);
    for (const double f : {20.0, 300.0, 1500.0, 2000.0, 2600.0, 9000.0,
                           20000.0}) {
      const auto a = unwrap(cascadeResponse(lp, FrequencyHz{f}, kRate));
      const auto b = unwrap(cascadeResponse(hp, FrequencyHz{f}, kRate));
      // LR4, LR8: bands in phase; LR2: HP polarity inverted.
      const double sum = order == 2 ? std::abs(a - b) : std::abs(a + b);
      CHECK(std::fabs(sum - 1.0) < 1e-10);
    }
  }
}

TEST_CASE("every design is stable across the band", "[d23filt]") {
  for (double f = 20.0; f < 0.49 * kRate.value(); f *= 1.25) {
    const FrequencyHz fr{f};
    for (const double q : {0.3, 0.7071, 5.0, 30.0}) {
      CHECK(isStable(unwrap(makeBandpass(fr, q, kRate))));
      CHECK(isStable(unwrap(makeNotch(fr, q, kRate))));
      CHECK(isStable(unwrap(makeAllpass(fr, q, kRate))));
    }
    CHECK(isStable(unwrap(makeFirstOrderLowpass(fr, kRate))));
    CHECK(isStable(unwrap(makeFirstOrderHighpass(fr, kRate))));
    for (const auto& k : unwrap(designButterworth(true, 8, fr, kRate)))
      CHECK(isStable(k));
  }
}

TEST_CASE("BiquadCascade: series-exact, keeps state, bounded", "[d23filt]") {
  const auto secs =
      unwrap(designButterworth(false, 5, FrequencyHz{3000.0}, kRate));
  std::vector<double> x(3000), ref(3000), got(3000);
  for (std::size_t i = 0; i < x.size(); ++i)
    x[i] = std::sin(0.05 * static_cast<double>(i)) +
           0.5 * std::sin(1.9 * static_cast<double>(i));
  // Reference: the sections one after another.
  ref = x;
  for (const auto& k : secs) {
    Biquad b{k};
    b.processBlock(ref.data(), ref.data(), FrameCount{3000});
  }
  auto c = unwrap(BiquadCascade::create(8));
  REQUIRE(c.setSections(secs.data(), secs.size()).ok());
  CHECK(c.size() == 3);
  // Processed in two blocks with a no-op reload between: state is kept.
  c.processBlock(x.data(), got.data(), FrameCount{1234});
  REQUIRE(c.setSections(secs.data(), secs.size()).ok());
  c.processBlock(x.data() + 1234, got.data() + 1234, FrameCount{3000 - 1234});
  CHECK(got == ref);

  // Empty cascade copies; oversize and unstable loads are refused, unchanged.
  REQUIRE(c.setSections(nullptr, 0).ok());
  std::vector<double> copy(3000);
  c.processBlock(x.data(), copy.data(), FrameCount{3000});
  CHECK(copy == x);
  std::vector<BiquadCoeffs> nine(9);
  CHECK(c.setSections(nine.data(), 9).error() == QuantityError::InvalidArgument);
  const BiquadCoeffs unstable{1.0, 0.0, 0.0, 0.0, 1.5};
  CHECK(c.setSections(&unstable, 1).error() == QuantityError::DomainError);
  CHECK(c.size() == 0);
  CHECK(BiquadCascade::create(0).error() == QuantityError::InvalidArgument);
}

TEST_CASE("filter designs reject invalid input", "[d23filt]") {
  CHECK(makeBandpass(FrequencyHz{24000.0}, 1.0, kRate).error() ==
        QuantityError::InvalidArgument);
  CHECK(makeNotch(FrequencyHz{1000.0}, 0.0, kRate).error() ==
        QuantityError::InvalidArgument);
  CHECK(makeFirstOrderLowpass(FrequencyHz{-5.0}, kRate).error() ==
        QuantityError::InvalidArgument);
  CHECK(designButterworth(false, 9, FrequencyHz{1000.0}, kRate).error() ==
        QuantityError::InvalidArgument);
  CHECK(designLinkwitzRiley(true, 6, FrequencyHz{1000.0}, kRate).error() ==
        QuantityError::InvalidArgument);
  // EqShape filter types route through the same designers.
  EqBand notch{EqShape::Notch, FrequencyHz{1000.0}, 0.0, 4.0};
  const auto k = unwrap(designEqSection(notch, kRate));
  CHECK(k == unwrap(makeNotch(FrequencyHz{1000.0}, 4.0, kRate)));
  notch.gainDb = 90.0;
  CHECK(designEqSection(notch, kRate).error() == QuantityError::InvalidArgument);
}
