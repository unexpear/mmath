// Phase 2 — signal and oscillation tests (Catch2, test-only dependency).
//
// Closed-form expectations are written inline from the mathematics, never by
// calling the implementation under test. The FM discriminator pins the one
// distinction that matters: integrated phase vs naive sin(2*pi*f(t)*t).

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <string>
#include <utility>

#include <mmathen/math/comparison.h>
#include <mmathen/signal/oscillator.h>
#include <mmathen/signal/phase.h>
#include <mmathen/signal/signal.h>
#include <mmathen/time/factories.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
const auto kEnvQuarter = constant(AmplitudeLinear{0.25});

bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
}  // namespace

TEST_CASE("constant sine matches its closed form", "[signal]") {
  // x(t) = 0.25*sin(2*pi*440*t), expected written directly.
  const auto s = sine(FrequencyHz{440.0}, kEnvQuarter);
  for (const double t : {0.0, 0.000283, 0.01, 0.371}) {
    const double expected =
        0.25 * std::sin(2.0 * std::numbers::pi * 440.0 * t);
    CHECK(
        nearlyEqual(s.at(TimePointSeconds{t}), SignedAudioValue{expected},
                    SignedAudioValue{1e-12}, 0.0));
  }
  CHECK(s.at(TimePointSeconds{0.0}) == SignedAudioValue{0.0});
  CHECK(contains(s.formula(), "sine"));
}

TEST_CASE("initial phase offsets the waveform", "[signal]") {
  // phi0 = pi/2 turns sine into cosine: x(0) = A.
  const auto s =
      sine(FrequencyHz{440.0}, kEnvQuarter,
           PhaseRadians{std::numbers::pi / 2.0});
  CHECK(nearlyEqual(s.at(TimePointSeconds{0.0}), SignedAudioValue{0.25},
                    SignedAudioValue{1e-12}, 0.0));
}

TEST_CASE("linear sweep matches its analytic phase", "[signal]") {
  // f(t) = 220 + 220*t, phi(t) = 2*pi*(220*t + 110*t^2).
  const auto s =
      sineSweep(FrequencyHz{220.0}, FrequencySlopeHzPerSec{220.0},
                kEnvQuarter);
  for (const double t : {0.0, 0.1, 0.5, 1.0}) {
    const double expected =
        0.25 * std::sin(2.0 * std::numbers::pi * (220.0 * t + 110.0 * t * t));
    CHECK(
        nearlyEqual(s.at(TimePointSeconds{t}), SignedAudioValue{expected},
                    SignedAudioValue{1e-9}, 1e-9));
  }
  // The sweep shape itself reports instantaneous frequency f0 + a*t.
  CHECK(LinearSweep{FrequencyHz{220.0}, FrequencySlopeHzPerSec{220.0}}
            .frequencyAt(TimePointSeconds{2.0}) == FrequencyHz{660.0});
}

TEST_CASE("FM uses integrated phase, not the naive product", "[signal]") {
  // f(t) = 440 + 10*sin(2*pi*5*t), A = 0.25.
  const auto s =
      sineFM(FrequencyHz{440.0}, FrequencyHz{10.0}, FrequencyHz{5.0},
             kEnvQuarter);
  const double t = 0.05;
  // Integrated phase (correct): phi = 2*pi*440*t + (10/5)*(1-cos(2*pi*5*t)).
  const double phiCorrect = 2.0 * std::numbers::pi * 440.0 * t +
                            2.0 * (1.0 - std::cos(2.0 * std::numbers::pi *
                                                  5.0 * t));
  CHECK(nearlyEqual(s.at(TimePointSeconds{t}),
                    SignedAudioValue{0.25 * std::sin(phiCorrect)},
                    SignedAudioValue{1e-9}, 1e-9));
  // Naive product (wrong): sin(2*pi*f(t)*t) demonstrably differs here.
  const double fNow = 440.0 + 10.0 * std::sin(2.0 * std::numbers::pi * 5.0 * t);
  const double naive = 0.25 * std::sin(2.0 * std::numbers::pi * fNow * t);
  CHECK_FALSE(nearlyEqual(s.at(TimePointSeconds{t}),
                          SignedAudioValue{naive}, SignedAudioValue{1e-6},
                          0.0));
}

TEST_CASE("composition is sample-wise with provenance", "[signal]") {
  const auto a = sine(FrequencyHz{440.0}, kEnvQuarter);
  const auto b = sine(FrequencyHz{660.0}, kEnvQuarter);
  const auto mixed = a + b;
  const double t = 0.013;
  CHECK(nearlyEqual(mixed.at(TimePointSeconds{t}),
                    SignedAudioValue{a.at(TimePointSeconds{t}).value() +
                                     b.at(TimePointSeconds{t}).value()},
                    SignedAudioValue{1e-12}, 0.0));
  CHECK(contains(mixed.formula(), "+"));

  const auto env = constant(AmplitudeLinear{0.5});
  const auto applied = applyEnvelope(a, env);
  CHECK(nearlyEqual(applied.at(TimePointSeconds{t}),
                    SignedAudioValue{a.at(TimePointSeconds{t}).value() * 0.5},
                    SignedAudioValue{1e-12}, 0.0));
  CHECK(contains(applied.formula(), "*"));

  const auto gained = applyGain(a, GainLinear{2.0});
  CHECK(nearlyEqual(gained.at(TimePointSeconds{t}),
                    SignedAudioValue{a.at(TimePointSeconds{t}).value() * 2.0},
                    SignedAudioValue{1e-12}, 0.0));
}

TEST_CASE("full-scale-peak fixture: mean ~0, RMS ~0.25/sqrt(2)", "[signal]") {
  // Pre-quantization float-buffer check over exactly 44 cycles
  // (440 Hz * 0.1 s at 48 kHz = 4800 frames): the certified-path statistics.
  const auto s = sine(FrequencyHz{440.0}, kEnvQuarter);
  double sum = 0.0;
  double sumSq = 0.0;
  constexpr int n = 4800;
  for (int i = 0; i < n; ++i) {
    const double x =
        s.at(TimePointSeconds{static_cast<double>(i) / 48000.0}).value();
    sum += x;
    sumSq += x * x;
  }
  CHECK(nearlyEqual(sum / n, 0.0, 1e-3, 0.0));
  CHECK(nearlyEqual(std::sqrt(sumSq / n), 0.25 / std::numbers::sqrt2, 1e-6,
                    1e-3));
  CHECK(s.tryAt(TimePointSeconds{0.01}).ok());
}

TEST_CASE("zero FM rate is flagged downstream, never hidden", "[signal]") {
  // rate = 0 divides by zero in the FM phase integral (audit F-case).
  const auto s =
      sineFM(FrequencyHz{440.0}, FrequencyHz{10.0}, FrequencyHz{0.0},
             kEnvQuarter);
  CHECK(s.tryAt(TimePointSeconds{0.05}).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("numerical phase integration meets its tolerance", "[signal]") {
  // Against all three closed forms with tol = 1e-9.
  const auto constant = [](TimePointSeconds) {
    return FrequencyHz{440.0};
  };
  CHECK(nearlyEqual(
      unwrap(
          integratePhase(wrap<FrequencyHz>(constant, "440"),
                         TimePointSeconds{0.0}, TimePointSeconds{1.0}, 1e-9))
          .value(),
      2.0 * std::numbers::pi * 440.0, 1e-9, 1e-9));
  const auto sweep = [](TimePointSeconds t) {
    return FrequencyHz{220.0 + 220.0 * t.value()};
  };
  CHECK(nearlyEqual(
      unwrap(
          integratePhase(wrap<FrequencyHz>(sweep, "sweep"),
                         TimePointSeconds{0.0}, TimePointSeconds{1.0}, 1e-9))
          .value(),
      2.0 * std::numbers::pi * (220.0 + 110.0), 1e-9, 1e-9));
  const auto vib = [](TimePointSeconds t) {
    return FrequencyHz{440.0 +
                       10.0 * std::sin(2.0 * std::numbers::pi * 5.0 *
                                       t.value())};
  };
  CHECK(nearlyEqual(
      unwrap(
          integratePhase(wrap<FrequencyHz>(vib, "vibrato"),
                         TimePointSeconds{0.0}, TimePointSeconds{0.05}, 1e-9))
          .value(),
      2.0 * std::numbers::pi * 440.0 * 0.05 +
          2.0 * (1.0 - std::cos(2.0 * std::numbers::pi * 5.0 * 0.05)),
      1e-9, 1e-9));
  CHECK(integratePhase(wrap<FrequencyHz>(constant, "440"),
                       TimePointSeconds{1.0}, TimePointSeconds{0.0}, 1e-9)
            .error() == QuantityError::InvalidArgument);  // t1 < t0
  CHECK(integratePhase(wrap<FrequencyHz>(constant, "440"),
                       TimePointSeconds{0.0}, TimePointSeconds{1.0}, 0.0)
            .error() == QuantityError::InvalidArgument);  // bad tol
  CHECK(unwrap(integratePhase(wrap<FrequencyHz>(constant, "440"),
                                   TimePointSeconds{2.0},
                                   TimePointSeconds{2.0}, 1e-9))
            .value() == 0.0);  // empty interval
}
