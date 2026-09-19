// D27 — f0 estimation: autocorrelation and YIN (Catch2).
//
// Sines from 55 Hz to 1760 Hz: both methods within 1e-4 (sub-sample
// parabolic refinement). A tone with a strong 2nd harmonic and one with a
// MISSING fundamental: both report the fundamental (the period), not an
// octave above. The plucked stiff string (D10 physics) is read within 0.2%
// of its first partial. White noise is reported unvoiced by both. The
// tracker follows a two-note sequence. Invalid searches are refused.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/analysis/pitch.h>
#include <mmathen/physics/wave1d.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
const SampleRateHz kRate{48000.0};

std::vector<double> tone(double f0, std::size_t n,
                         const std::vector<double>& harmonicAmps,
                         double phase = 0.3) {
  std::vector<double> x(n, 0.0);
  for (std::size_t h = 0; h < harmonicAmps.size(); ++h)
    for (std::size_t i = 0; i < n; ++i)
      x[i] += harmonicAmps[h] *
              std::sin(2.0 * std::numbers::pi * f0 * static_cast<double>(h + 1) *
                           static_cast<double>(i) / kRate.value() +
                       phase * static_cast<double>(h + 1));
  return x;
}
double relErr(double a, double b) { return std::fabs(a - b) / b; }
}  // namespace

TEST_CASE("sines: both methods within 1e-4", "[d27pitch]") {
  PitchSearch s;
  s.fMinHz = 40.0;
  s.fMaxHz = 2000.0;
  for (const double f : {55.0, 110.0, 220.0, 261.63, 440.0, 880.0, 1760.0}) {
    const auto x = tone(f, 4096, {0.8});
    const auto a = unwrap(autocorrelationPitch(x, kRate, s));
    const auto y = unwrap(yinPitch(x, kRate, s));
    CHECK(a.voiced);
    CHECK(y.voiced);
    CHECK(relErr(a.frequencyHz, f) < 1e-4);  // measured <= 2.4e-5
    CHECK(relErr(y.frequencyHz, f) < 1e-4);  // measured <= 2.7e-5
    CHECK(y.confidence > 0.99);
  }
}

TEST_CASE("harmonic tones: no octave errors, missing fundamental found",
          "[d27pitch]") {
  PitchSearch s;
  s.fMinHz = 60.0;
  s.fMaxHz = 1500.0;
  // Strong 2nd harmonic (twice the fundamental's amplitude).
  const auto strong2 = tone(196.0, 4096, {0.3, 0.6, 0.2, 0.1});
  // Missing fundamental: harmonics 2..6 of 150 Hz only.
  const auto missing = tone(150.0, 4096, {0.0, 0.5, 0.4, 0.3, 0.2, 0.1});
  for (const auto* x : {&strong2, &missing}) {
    const double f0 = x == &strong2 ? 196.0 : 150.0;
    const auto a = unwrap(autocorrelationPitch(*x, kRate, s));
    const auto y = unwrap(yinPitch(*x, kRate, s));
    CHECK(relErr(a.frequencyHz, f0) < 2e-3);
    CHECK(relErr(y.frequencyHz, f0) < 2e-3);
  }
}

TEST_CASE("plucked stiff string: first partial", "[d27pitch]") {
  auto base = unwrap(idealString(DistanceMeters{0.65}, TensionNewtons{97.344},
                                 LinearDensityKgPerM{0.001}));
  const double b = 4e-4;
  auto stiff = unwrap(withStiffness(base, unwrap(stiffnessFromB(base, b))));
  auto pluck = unwrap(InitialShape::pluck(stiff, DistanceMeters{0.1234}, 0.002));
  auto modal = unwrap(ModalSolution::create(pluck, 40));
  std::vector<double> x(8192);
  for (std::size_t i = 0; i < x.size(); ++i)
    x[i] = unwrap(modal.at(DistanceMeters{0.0845},
                           TimePointSeconds{static_cast<double>(i) / 48000.0}));
  // Stiff-string partial 1: f1 = f0 * sqrt(1 + B) (1 * sqrt(1 + B n^2)).
  const double f1 = 240.0 * std::sqrt(1.0 + b);
  PitchSearch s;
  s.fMinHz = 80.0;
  s.fMaxHz = 1000.0;
  CHECK(relErr(unwrap(yinPitch(x, kRate, s)).frequencyHz, f1) < 2e-3);
  CHECK(relErr(unwrap(autocorrelationPitch(x, kRate, s)).frequencyHz, f1) <
        2e-3);
}

TEST_CASE("noise is unvoiced; silence gives no pitch", "[d27pitch]") {
  std::vector<double> x(4096);
  std::uint64_t st = 777;
  for (auto& v : x) {
    st = st * 6364136223846793005ULL + 1442695040888963407ULL;
    v = static_cast<double>(st >> 11) * (2.0 / 9007199254740992.0) - 1.0;
  }
  CHECK_FALSE(unwrap(yinPitch(x, kRate)).voiced);
  CHECK_FALSE(unwrap(autocorrelationPitch(x, kRate)).voiced);
  const std::vector<double> zero(4096, 0.0);
  CHECK(unwrap(autocorrelationPitch(zero, kRate)).frequencyHz == 0.0);
  CHECK_FALSE(unwrap(yinPitch(zero, kRate)).voiced);
}

TEST_CASE("tracker follows a two-note sequence; refusals", "[d27pitch]") {
  auto x = tone(220.0, 24000, {0.6, 0.3, 0.1});
  const auto y = tone(330.0, 24000, {0.6, 0.3, 0.1});
  x.insert(x.end(), y.begin(), y.end());
  const auto track =
      unwrap(trackPitch(x, kRate, 2048, 1024, PitchMethod::Yin));
  for (const auto& fr : track) {
    if (fr.timeSeconds < 0.45) {
      CHECK(relErr(fr.estimate.frequencyHz, 220.0) < 2e-3);
    } else if (fr.timeSeconds > 0.55) {
      CHECK(relErr(fr.estimate.frequencyHz, 330.0) < 2e-3);
    }
  }
  PitchSearch bad;
  bad.fMinHz = 20.0;  // needs a 2400-sample lag: too long for 2048
  CHECK(yinPitch(std::vector<double>(2048, 0.0), kRate, bad).error() ==
        QuantityError::InvalidArgument);
  bad = PitchSearch{};
  bad.fMaxHz = 30000.0;  // above Nyquist
  CHECK(autocorrelationPitch(std::vector<double>(4096, 0.0), kRate, bad)
            .error() == QuantityError::InvalidArgument);
  bad = PitchSearch{};
  bad.yinThreshold = 0.0;
  CHECK(yinPitch(std::vector<double>(4096, 0.0), kRate, bad).error() ==
        QuantityError::InvalidArgument);
}
