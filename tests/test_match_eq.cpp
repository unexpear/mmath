// D25 — graphic-EQ gain solver and match EQ (Catch2).
//
// The solver removes the D21 band interaction: centre responses reach the
// targets to < 0.01 dB (octave and third-octave), where plain gains were off
// by 1.6-2.8 dB. Match EQ: white noise matched to the same noise through a
// known tone-shaping EQ comes out within 0.25 dB of the reference in every
// third-octave band from 50 Hz to 16 kHz (it was up to ~6 dB away before),
// after closed-loop refinement on predicted band levels;
// identical signals give an all-zero EQ; amount and normalize act as stated.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <mmathen/analysis/spectrum.h>
#include <mmathen/dsp/equalizer.h>
#include <mmathen/dsp/match_eq.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
const SampleRateHz kRate{48000.0};

// Deterministic white noise (64-bit LCG, top 53 bits, uniform in [-1, 1)).
std::vector<double> noise(std::size_t n, std::uint64_t seed) {
  std::vector<double> x(n);
  std::uint64_t s = seed;
  for (auto& v : x) {
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    v = static_cast<double>(s >> 11) * (2.0 / 9007199254740992.0) - 1.0;
    v *= 0.25;
  }
  return x;
}

Spectrum ltas(const std::vector<double>& x) {
  return unwrap(longTermSpectrum(x, kRate, 8192));
}
std::vector<BandLevel> ltasBands(const std::vector<double>& x,
                                 const std::vector<FrequencyBand>& bands) {
  return bandLevels(ltas(x), bands);
}
}  // namespace

TEST_CASE("gain solver removes the graphic-EQ interaction", "[d25match]") {
  for (const int b : {1, 3}) {
    const auto bands = unwrap(fractionalOctaveBands(
        b, FrequencyHz{25.0}, FrequencyHz{16000.0}));
    std::vector<double> target(bands.size());
    for (std::size_t i = 0; i < target.size(); ++i)
      target[i] = 8.0 * std::sin(0.45 * static_cast<double>(i));  // smooth
    // Plain gains miss the targets because neighbours add up...
    const auto plain = unwrap(makeGraphicEq(bands, target, kRate));
    const auto plainErr = unwrap(graphicEqCentreErrorDb(plain, bands, target));
    double plainWorst = 0.0;
    for (double e : plainErr) plainWorst = std::max(plainWorst, std::fabs(e));
    CHECK(plainWorst > 1.0);
    // ...the solved gains hit them.
    const auto sol = unwrap(solveGraphicEqGains(bands, target, kRate));
    CHECK(sol.residualDb < 1e-3);
    CHECK(sol.iterations < 100);
    const auto solved = unwrap(makeGraphicEq(bands, sol.gainsDb, kRate));
    for (std::size_t i = 0; i < bands.size(); ++i)
      CHECK(std::fabs(unwrap(solved.responseDb(bands[i].center)) - target[i]) <
            1e-3);
  }
  // All +6 dB octave bands: the D21 case (1.6-2.8 dB interaction) solved.
  const auto oct = unwrap(fractionalOctaveBands(1, FrequencyHz{30.0},
                                                FrequencyHz{17000.0}));
  const auto six = unwrap(
      solveGraphicEqGains(oct, std::vector<double>(oct.size(), 6.0), kRate));
  CHECK(six.residualDb < 1e-3);
}

TEST_CASE("match EQ brings noise to a shaped reference", "[d25match]") {
  const auto src = noise(std::size_t{1} << 18, 12345);
  // Reference: the same noise through a known tone-shaping EQ.
  auto shape = unwrap(ParametricEq::create(
      {{EqShape::LowShelf, FrequencyHz{200.0}, 6.0, 0.7071067811865476},
       {EqShape::Peaking, FrequencyHz{2000.0}, -5.0, 1.2},
       {EqShape::HighShelf, FrequencyHz{8000.0}, 4.0, 0.7071067811865476}},
      kRate));
  std::vector<double> ref(src.size());
  shape.processBlock(src.data(), ref.data(),
                     FrameCount{static_cast<std::uint64_t>(src.size())});

  const auto bands = unwrap(fractionalOctaveBands(3, FrequencyHz{25.0},
                                                  FrequencyHz{16000.0}));
  const auto ls = ltasBands(src, bands);
  const auto lr = ltasBands(ref, bands);
  MatchEqOptions o;
  o.normalize = false;  // absolute match (the reference is not re-levelled)
  const auto d = unwrap(designMatchEq(ltas(src), ltas(ref), bands, kRate, o));
  CHECK(d.solution.residualDb < 1e-3);
  CHECK(d.predictedErrorDb < 0.1);  // closed loop converged on prediction

  auto eq = unwrap(makeGraphicEq(d.bands, d.solution.gainsDb, kRate));
  std::vector<double> out(src.size());
  eq.processBlock(src.data(), out.data(),
                  FrameCount{static_cast<std::uint64_t>(src.size())});
  const auto lo = ltasBands(out, bands);
  double before = 0.0, after = 0.0;
  for (std::size_t i = 0; i < bands.size(); ++i) {
    const double f = bands[i].center.value();
    if (f < 50.0) continue;
    before = std::max(before, std::fabs(ls[i].levelDb - lr[i].levelDb));
    after = std::max(after, std::fabs(lo[i].levelDb - lr[i].levelDb));
  }
  CHECK(before > 4.0);
  CHECK(after < 0.25);  // measured 0.064 dB (5.98 dB before)
}

TEST_CASE("match EQ: identity, amount, normalize, refusals", "[d25match]") {
  const auto x = noise(std::size_t{1} << 16, 99);
  const auto bands = unwrap(fractionalOctaveBands(1, FrequencyHz{30.0},
                                                  FrequencyHz{16000.0}));
  const auto l = ltas(x);
  const auto same = unwrap(designMatchEq(l, l, bands, kRate));
  for (double g : same.solution.gainsDb) CHECK(std::fabs(g) < 1e-9);
  CHECK(same.predictedErrorDb < 1e-9);

  // A pure level difference (+5 dB in every bin) is removed by normalize
  // and kept without it; amount scales it.
  auto louder = l;
  for (auto& p : louder.power) p *= std::pow(10.0, 0.5);
  const auto norm = unwrap(designMatchEq(l, louder, bands, kRate));
  for (double t : norm.correctionDb) CHECK(std::fabs(t) < 1e-9);
  MatchEqOptions raw;
  raw.normalize = false;
  raw.amount = 0.5;
  const auto half = unwrap(designMatchEq(l, louder, bands, kRate, raw));
  for (double t : half.correctionDb) CHECK(std::fabs(t - 2.5) < 1e-9);
  CHECK(half.predictedErrorDb < 0.1);

  // Bands with no reference energy are not matched.
  auto dead = l;
  for (std::size_t k = 0; k < dead.power.size(); ++k)
    if (static_cast<double>(k) * dead.binHz < bands[1].lower.value())
      dead.power[k] = 0.0;
  const auto part = unwrap(designMatchEq(l, dead, bands, kRate));
  CHECK_FALSE(part.matched[0]);
  CHECK(part.correctionDb[0] == 0.0);

  // Refusals.
  CHECK(designMatchEq(l, Spectrum{}, bands, kRate).error() ==
        QuantityError::InvalidArgument);
  MatchEqOptions bad;
  bad.amount = 1.5;
  CHECK(designMatchEq(l, l, bands, kRate, bad).error() ==
        QuantityError::InvalidArgument);
  CHECK(solveGraphicEqGains(bands, std::vector<double>(bands.size(), 30.0),
                            kRate)
            .error() == QuantityError::InvalidArgument);
  CHECK(longTermSpectrum(std::vector<double>(100, 0.0), kRate, 8192).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("match EQ to a reference at another sample rate", "[d25match]") {
  // Source: white noise at 48 kHz. Reference: white noise at 44.1 kHz
  // through a tone-shaping EQ designed at 44.1 kHz. White noise levels per
  // band differ between rates by a constant (density sigma^2 / (fs/2)),
  // which normalize removes; the tonal shape must match.
  const SampleRateHz r441{44100.0};
  const auto src = noise(std::size_t{1} << 18, 4242);
  auto refNoise = noise(std::size_t{1} << 18, 777);
  auto shape = unwrap(ParametricEq::create(
      {{EqShape::LowShelf, FrequencyHz{150.0}, -6.0, 0.7071067811865476},
       {EqShape::Peaking, FrequencyHz{3000.0}, 5.0, 1.0}},
      r441));
  shape.processBlock(refNoise.data(), refNoise.data(),
                     FrameCount{static_cast<std::uint64_t>(refNoise.size())});
  const auto bands = unwrap(fractionalOctaveBands(3, FrequencyHz{25.0},
                                                  FrequencyHz{16000.0}));
  const auto refLevels =
      bandLevels(unwrap(longTermSpectrum(refNoise, r441, 8192)), bands);
  const auto d = unwrap(designMatchEq(ltas(src), refLevels, bands, kRate));
  // Worst prediction error is in the 31.6 Hz band, which holds 2 FFT bins:
  // a graphic EQ cannot shape inside it (measured 0.12 dB).
  CHECK(d.predictedErrorDb < 0.2);
  auto eq = unwrap(makeGraphicEq(d.bands, d.solution.gainsDb, kRate));
  std::vector<double> out(src.size());
  eq.processBlock(src.data(), out.data(),
                  FrameCount{static_cast<std::uint64_t>(src.size())});
  const auto lo = ltasBands(out, bands);
  // Compare shapes over the bands holding >= 3 bins (63 Hz up): below,
  // one or two bins of two independent noise signals differ by chance
  // (measured up to 1.4 dB at 31.6 Hz), which no EQ design can remove.
  double mean = 0.0;
  std::size_t n = 0;
  for (std::size_t i = 0; i < bands.size(); ++i)
    if (bands[i].center.value() >= 60.0) {
      mean += lo[i].levelDb - refLevels[i].levelDb;
      ++n;
    }
  mean /= static_cast<double>(n);
  double worst = 0.0;
  for (std::size_t i = 0; i < bands.size(); ++i)
    if (bands[i].center.value() >= 60.0)
      worst = std::max(worst,
                       std::fabs(lo[i].levelDb - refLevels[i].levelDb - mean));
  CHECK(worst < 0.25);  // measured 0.157 dB, at 63 Hz (3 bins)
  // Mismatched band lists are refused.
  auto wrong = refLevels;
  wrong.pop_back();
  CHECK(designMatchEq(ltas(src), wrong, bands, kRate).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("match EQ ignores bands far below the signal's own peak",
          "[d25match]") {
  // A tonal source and reference: a 1 kHz sine each (different levels),
  // plus nothing else. Bands away from 1 kHz hold only window leakage;
  // with the relative floor they are left alone instead of boosted.
  std::vector<double> a(1 << 16), b(1 << 16);
  for (std::size_t i = 0; i < a.size(); ++i) {
    const double ph = 2.0 * 3.141592653589793 * 1000.0 * static_cast<double>(i) / 48000.0;
    a[i] = 0.5 * std::sin(ph);
    b[i] = 0.1 * std::sin(ph + 0.3);
  }
  const auto bands = unwrap(fractionalOctaveBands(3, FrequencyHz{25.0},
                                                  FrequencyHz{16000.0}));
  const auto d = unwrap(designMatchEq(ltas(a), ltas(b), bands, kRate));
  std::size_t matched = 0;
  for (std::size_t i = 0; i < bands.size(); ++i)
    if (d.matched[i]) {
      ++matched;
      CHECK(std::fabs(bands[i].center.value() - 1000.0) < 400.0);
    }
  CHECK(matched >= 1);
  for (double g : d.solution.gainsDb) CHECK(std::fabs(g) < 3.0);
  MatchEqOptions bad;
  bad.relativeFloorDb = 0.0;
  CHECK(designMatchEq(ltas(a), ltas(b), bands, kRate, bad).error() ==
        QuantityError::InvalidArgument);
}
