// D18 — hearing: loudness, scales, masking (Catch2).
//
// Anchors and where each comes from:
// - Bark/ERB/A-weighting: closed-form definitions (Traunmüller 1990,
//   Glasberg & Moore 1990, IEC 61672); values below re-derived in
//   C:\Users\micha\AppData\Local\Temp\opencode\audit_d18.py.
// - ISO 226: table nodes are exact by construction; off-node values pin the
//   log-frequency interpolation against the same script. The 40-phon/100 Hz
//   point additionally agrees within 0.2 dB with an INDEPENDENT lineage
//   (PyDSM's ISO 226:2003 A_f/B_f formulation: 64.37 dB vs ours 64.24) —
//   different equations, different project, same physics.
// - MPEG-1 spreading: S(0) ~ 0 by construction; asymmetry pinned at +/-2.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <utility>

#include <mmathen/hearing/loudness.h>
#include <mmathen/hearing/masking.h>
#include <mmathen/hearing/scales.h>
#include <mmathen/math/comparison.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
}  // namespace

TEST_CASE("Bark anchors in low, mid, and corrected-high regimes",
          "[hearing]") {
  CHECK(nearlyEqual(unwrap(barkRate(FrequencyHz{100.0})), 0.955738, 1e-6, 0.0));
  CHECK(nearlyEqual(unwrap(barkRate(FrequencyHz{1000.0})), 8.5274324, 1e-6, 0.0));
  CHECK(nearlyEqual(unwrap(barkRate(FrequencyHz{8000.0})), 21.20305, 1e-4, 0.0));
  // Monotone across the audio band (the corrections preserve order).
  double prev = unwrap(barkRate(FrequencyHz{20.0}));
  for (double f = 30.0; f <= 15000.0; f *= 1.5) {
    const double b = unwrap(barkRate(FrequencyHz{f}));
    CHECK(b > prev);
    prev = b;
  }
  CHECK(!barkRate(FrequencyHz{0.0}).ok());
  CHECK(!barkRate(FrequencyHz{-440.0}).ok());
}

TEST_CASE("ERB bandwidth and ERB-number anchors", "[hearing]") {
  // 24.7 * 5.37 = 132.639, the ~132 Hz PMC anchor at 1 kHz.
  CHECK(nearlyEqual(unwrap(erbBandwidth(FrequencyHz{1000.0})), 132.639, 1e-9, 0.0));
  CHECK(nearlyEqual(unwrap(erbNumber(FrequencyHz{1000.0})), 15.6214497, 1e-6, 0.0));
  CHECK(unwrap(erbBandwidth(FrequencyHz{2000.0})) >
        unwrap(erbBandwidth(FrequencyHz{1000.0})));
  CHECK(!erbBandwidth(FrequencyHz{0.0}).ok());
  CHECK(!erbNumber(FrequencyHz{-1.0}).ok());
}

TEST_CASE("A-weighting textbook anchors", "[hearing]") {
  CHECK(nearlyEqual(unwrap(aWeightingDb(FrequencyHz{100.0})), -19.14510, 1e-4, 0.0));
  // Exact 0 at 1 kHz by construction (same expression subtracted).
  CHECK(nearlyEqual(unwrap(aWeightingDb(FrequencyHz{1000.0})), 0.0, 1e-12, 0.0));
  CHECK(nearlyEqual(unwrap(aWeightingDb(FrequencyHz{10000.0})), -2.49171, 1e-4, 0.0));
  CHECK(!aWeightingDb(FrequencyHz{0.0}).ok());
}

TEST_CASE("MPEG-1 spreading shape and asymmetry", "[hearing]") {
  // No self-masking offset at zero separation.
  CHECK(nearlyEqual(unwrap(mpegSpreadingDb(0.0)), 0.0, 1e-2, 0.0));
  CHECK(nearlyEqual(unwrap(mpegSpreadingDb(2.0)), -12.333, 1e-3, 0.0));
  CHECK(nearlyEqual(unwrap(mpegSpreadingDb(-2.0)), -27.563, 1e-3, 0.0));
  // Upward spread (probe above masker) decays slower: the asymmetry is in
  // the formula, asserted here, not beside it.
  CHECK(unwrap(mpegSpreadingDb(1.0)) > unwrap(mpegSpreadingDb(-1.0)));
  CHECK(!mpegSpreadingDb(std::numeric_limits<double>::quiet_NaN()).ok());
}

TEST_CASE("ISO 226: 1 kHz identity both directions", "[hearing]") {
  CHECK(nearlyEqual(unwrap(splFromPhon(FrequencyHz{1000.0}, Phons{60.0})).value(),
                    60.0, 1e-6, 0.0));
  CHECK(nearlyEqual(unwrap(phonFromSpl(FrequencyHz{1000.0}, Decibels{60.0})).value(),
                    60.0, 1e-6, 0.0));
}

TEST_CASE("ISO 226: off-node contours", "[hearing]") {
  // 40 phon at 100 Hz: 64.24 dB here vs 64.37 from the independent 2003
  // lineage — agreement, not identity (revised alpha table).
  CHECK(nearlyEqual(unwrap(splFromPhon(FrequencyHz{100.0}, Phons{40.0})).value(),
                    64.2397, 1e-3, 0.0));
  CHECK(nearlyEqual(unwrap(phonFromSpl(FrequencyHz{100.0}, Decibels{70.0})).value(),
                    47.978, 1e-3, 0.0));
  // Round-trip away from any table node.
  const double back = unwrap(
      phonFromSpl(FrequencyHz{440.0},
                  unwrap(splFromPhon(FrequencyHz{440.0}, Phons{55.0}))))
                          .value();
  CHECK(nearlyEqual(back, 55.0, 1e-6, 0.0));
}

TEST_CASE("ISO 226: domain errors", "[hearing]") {
  CHECK(!splFromPhon(FrequencyHz{0.0}, Phons{40.0}).ok());
  CHECK(!splFromPhon(FrequencyHz{20000.0}, Phons{40.0}).ok());
  // Below the 1 kHz threshold (2.4 dB): no real phon value exists.
  CHECK(!phonFromSpl(FrequencyHz{1000.0}, Decibels{0.0}).ok());
  // Above the tabled 12.5 kHz edge: extrapolation refused.
  CHECK(!splFromPhon(FrequencyHz{15000.0}, Phons{40.0}).ok());
}

TEST_CASE("Sones doubling and domain", "[hearing]") {
  CHECK(nearlyEqual(unwrap(sonesFromPhon(Phons{40.0})).value(), 1.0, 1e-12, 0.0));
  CHECK(nearlyEqual(unwrap(sonesFromPhon(Phons{60.0})).value(), 4.0, 1e-12, 0.0));
  CHECK(nearlyEqual(unwrap(sonesFromPhon(Phons{70.0})).value(), 8.0, 1e-12, 0.0));
  // The power law is total on finite inputs: 30 phon = half a sone.
  CHECK(nearlyEqual(unwrap(sonesFromPhon(Phons{30.0})).value(), 0.5, 1e-12, 0.0));
  CHECK(!sonesFromPhon(Phons{std::numeric_limits<double>::quiet_NaN()}).ok());
}

TEST_CASE("Quiet threshold reads the T_f table", "[hearing]") {
  CHECK(nearlyEqual(unwrap(quietThresholdSpl(FrequencyHz{1000.0})).value(),
                    2.4, 1e-9, 0.0));
  CHECK(nearlyEqual(unwrap(quietThresholdSpl(FrequencyHz{4000.0})).value(),
                    -5.4, 1e-9, 0.0));
  CHECK(!quietThresholdSpl(FrequencyHz{10.0}).ok());
  CHECK(!quietThresholdSpl(FrequencyHz{20000.0}).ok());
}

TEST_CASE("Masking: self, floor, and genuine masking", "[hearing]") {
  // A tone barely masks itself at its own frequency.
  const double selfMask = unwrap(maskingThresholdDb(
      FrequencyHz{1000.0}, Decibels{70.0}, FrequencyHz{1000.0})).value();
  CHECK(nearlyEqual(selfMask, 70.0, 5e-2, 0.0));
  // Seven Bark below, the masker is irrelevant: exactly the quiet floor.
  const double floored = unwrap(maskingThresholdDb(
      FrequencyHz{1000.0}, Decibels{70.0}, FrequencyHz{100.0})).value();
  CHECK(nearlyEqual(floored, 26.5, 1e-9, 0.0));
  // An octave above 1 kHz at 70 dB SPL: masked well above quiet (-1.3 dB)
  // yet below the masker level — structural, since the spread is pinned.
  const double masked = unwrap(maskingThresholdDb(
      FrequencyHz{1000.0}, Decibels{70.0}, FrequencyHz{2000.0})).value();
  CHECK(masked > 8.7);
  CHECK(masked < 70.0);
  CHECK(!maskingThresholdDb(FrequencyHz{0.0}, Decibels{70.0},
                            FrequencyHz{1000.0}).ok());
  CHECK(!maskingThresholdDb(FrequencyHz{1000.0}, Decibels{70.0},
                            FrequencyHz{0.0}).ok());
}
