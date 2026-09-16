// D7 — phase modulation tests (Catch2).
//
// Every expectation comes from an independent path: the inline textbook
// formula, the Jacobi-Anger Bessel expansion, the D5 adaptive-Simpson
// integrator, and the existing FM oscillator (FM == PM identity).

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/math/comparison.h>
#include <mmathen/render/offline.h>
#include <mmathen/signal/fm.h>
#include <mmathen/signal/oscillator.h>
#include <mmathen/signal/phase.h>
#include <mmathen/signal/pm.h>
#include <mmathen/signal/segment_envelope.h>
#include <mmathen/signal/signal.h>
#include <mmathen/sink/sink.h>
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

constexpr double kPi = std::numbers::pi;
const std::vector<double> kTimes = {0.0, 0.000283, 0.0137, 0.2, 0.371, 0.9999};
}  // namespace

TEST_CASE("PM tone matches the textbook formula", "[d7pm]") {
  const auto sig = unwrap(pmTone(FrequencyHz{440.0}, FrequencyHz{110.0}, 2.0,
                                 PhaseRadians{0.3},
                                 constant(AmplitudeLinear{0.25}),
                                 PhaseRadians{0.1}));
  for (const double t : kTimes) {
    const double expected =
        0.25 * std::sin(2.0 * kPi * 440.0 * t + 0.1 +
                        2.0 * std::sin(2.0 * kPi * 110.0 * t + 0.3));
    CHECK(nearlyEqual(sig.at(TimePointSeconds{t}).value(), expected, 1e-12,
                      1e-12));
  }
  CHECK(sig.formula() == "sine(pm)");
}

TEST_CASE("PM equals its Jacobi-Anger Bessel expansion", "[d7pm]") {
  // sin(a + beta*sin(psi)) = sum_k J_k(beta) sin(a + k*psi),
  // J_{-k} = (-1)^k J_k. |J_31(2)| < 1e-30: truncation is invisible.
  const double beta = 2.0;
  const double theta = 0.3;
  const double phi0 = 0.1;
  const auto sig = unwrap(pmTone(FrequencyHz{440.0}, FrequencyHz{110.0}, beta,
                                 PhaseRadians{theta},
                                 constant(AmplitudeLinear{1.0}),
                                 PhaseRadians{phi0}));
  for (const double t : kTimes) {
    const double a = 2.0 * kPi * 440.0 * t + phi0;
    const double psi = 2.0 * kPi * 110.0 * t + theta;
    double sum = std::cyl_bessel_j(0, beta) * std::sin(a);
    for (int k = 1; k <= 30; ++k) {
      const double jk = std::cyl_bessel_j(k, beta);
      const double jNeg = (k % 2 == 0) ? jk : -jk;
      sum += jk * std::sin(a + k * psi) + jNeg * std::sin(a - k * psi);
    }
    CHECK(nearlyEqual(sig.at(TimePointSeconds{t}).value(), sum, 1e-12,
                      1e-12));
  }
}

TEST_CASE("PM phase is the integral of its instantaneous frequency",
          "[d7pm]") {
  const SinusoidalPM shape{FrequencyHz{440.0}, 2.0, FrequencyHz{110.0},
                           PhaseRadians{0.3}};
  const auto freq = wrap<FrequencyHz>(
      [shape](TimePointSeconds t) { return shape.frequencyAt(t); },
      "fc + beta*fm*cos(2*pi*fm*t + theta)");
  const double p0 = shape.phaseAt(TimePointSeconds{0.0}, PhaseRadians{0.0})
                        .value();
  for (const double t : {0.0137, 0.2, 0.371}) {
    const double integrated =
        unwrap(integratePhase(freq, TimePointSeconds{0.0},
                              TimePointSeconds{t}, 1e-10))
            .value();
    const double closed =
        shape.phaseAt(TimePointSeconds{t}, PhaseRadians{0.0}).value() - p0;
    CHECK(nearlyEqual(integrated, closed, 1e-8, 0.0));
  }
  // Peak deviation beta*fm: it grows with the modulator rate.
  CHECK(unwrap(pmPeakDeviation(FrequencyHz{5.0}, 2.0)) == FrequencyHz{10.0});
  CHECK(unwrap(pmPeakDeviation(FrequencyHz{50.0}, 2.0)) ==
        FrequencyHz{100.0});
  const SinusoidalPM fast{FrequencyHz{440.0}, 2.0, FrequencyHz{50.0},
                          PhaseRadians{0.0}};
  CHECK(fast.frequencyAt(TimePointSeconds{0.0}) == FrequencyHz{540.0});
}

TEST_CASE("sinusoidal FM is PM with a quarter-cycle modulator shift",
          "[d7pm]") {
  // FM phase I*(1 - cos(psi)) = I + I*sin(psi - pi/2).
  const auto fm = unwrap(fmTone(FrequencyHz{440.0}, FrequencyHz{5.0}, 2.0,
                                constant(AmplitudeLinear{0.25})));
  const auto pm = unwrap(pmTone(FrequencyHz{440.0}, FrequencyHz{5.0}, 2.0,
                                PhaseRadians{-kPi / 2.0},
                                constant(AmplitudeLinear{0.25}),
                                PhaseRadians{2.0}));
  for (const double t : kTimes)
    CHECK(nearlyEqual(fm.at(TimePointSeconds{t}).value(),
                      pm.at(TimePointSeconds{t}).value(), 1e-12, 1e-12));
}

TEST_CASE("phaseModulate generalizes PM and stacks operators", "[d7pm]") {
  const auto viaSignal = unwrap(
      phaseModulate(FrequencyHz{440.0}, sine(FrequencyHz{110.0}), 2.0));
  const auto viaTone =
      unwrap(pmTone(FrequencyHz{440.0}, FrequencyHz{110.0}, 2.0));
  for (const double t : kTimes)
    CHECK(nearlyEqual(viaSignal.at(TimePointSeconds{t}).value(),
                      viaTone.at(TimePointSeconds{t}).value(), 1e-12, 1e-12));
  CHECK(viaSignal.formula() == "pm(sine(const))");

  // Three-operator stack: 330 -> 220 -> 110.
  auto op2 = unwrap(
      phaseModulate(FrequencyHz{220.0}, sine(FrequencyHz{330.0}), 1.5));
  const auto out = unwrap(phaseModulate(FrequencyHz{110.0}, std::move(op2),
                                        0.8, constant(AmplitudeLinear{0.5})));
  for (const double t : kTimes) {
    const double expected =
        0.5 * std::sin(2.0 * kPi * 110.0 * t +
                       0.8 * std::sin(2.0 * kPi * 220.0 * t +
                                      1.5 * std::sin(2.0 * kPi * 330.0 * t)));
    CHECK(nearlyEqual(out.at(TimePointSeconds{t}).value(), expected, 1e-12,
                      1e-12));
  }
  CHECK(out.formula() == "pm(pm(sine(const)))");
}

TEST_CASE("enveloped PM renders block-invariant and clean", "[d7pm]") {
  const auto strike = unwrap(SegmentEnvelope::create(
      AmplitudeLinear{0.0},
      {EnvelopeSegment{DurationSeconds{0.01}, AmplitudeLinear{0.25},
                       SegmentCurve::Curved, -3.0},
       EnvelopeSegment{DurationSeconds{0.05}, AmplitudeLinear{0.1},
                       SegmentCurve::Exponential}}));
  const auto env = unwrap(gatedEnvelope(
      strike,
      {EnvelopeSegment{DurationSeconds{0.02}, AmplitudeLinear{0.0},
                       SegmentCurve::Linear}},
      TimePointSeconds{0.0}, TimePointSeconds{0.08}));
  const auto sig = unwrap(pmTone(FrequencyHz{220.0}, FrequencyHz{440.0}, 3.0,
                                 PhaseRadians{0.0}, env));
  std::vector<std::vector<double>> runs;
  for (const std::uint64_t bs : {64ULL, 127ULL, 512ULL}) {
    MemorySink mem;
    const auto stats = unwrap(renderOffline(
        sig,
        RenderConfig{SampleRateHz{48000.0}, FrameCount{6000},
                     TimePointSeconds{0.0}, bs},
        mem));
    CHECK(stats.isClean());
    runs.push_back(mem.samples());
  }
  CHECK(runs[0] == runs[1]);
  CHECK(runs[1] == runs[2]);
  // Release ends at off + 0.02 = 0.1 s (frame 4800): the tail after it is
  // exactly zero, and the release itself is not.
  CHECK(runs[0][4799] != 0.0);
  bool silentTail = true;
  for (std::size_t i = 4801; i < runs[0].size(); ++i)
    silentTail = silentTail && runs[0][i] == 0.0;
  CHECK(silentTail);
}

TEST_CASE("PM rejects invalid input", "[d7pm]") {
  const double nan = std::numeric_limits<double>::quiet_NaN();
  CHECK(pmTone(FrequencyHz{0.0}, FrequencyHz{5.0}, 2.0).error() ==
        QuantityError::DomainError);
  CHECK(pmTone(FrequencyHz{440.0}, FrequencyHz{0.0}, 2.0).error() ==
        QuantityError::DomainError);
  CHECK(pmTone(FrequencyHz{nan}, FrequencyHz{5.0}, 2.0).error() ==
        QuantityError::InvalidArgument);
  CHECK(pmTone(FrequencyHz{440.0}, FrequencyHz{5.0}, -1.0).error() ==
        QuantityError::InvalidArgument);
  CHECK(pmTone(FrequencyHz{440.0}, FrequencyHz{5.0}, 2.0, PhaseRadians{nan})
            .error() == QuantityError::InvalidArgument);
  CHECK(phaseModulate(FrequencyHz{0.0}, sine(FrequencyHz{110.0}), 1.0)
            .error() == QuantityError::DomainError);
  CHECK(phaseModulate(FrequencyHz{440.0}, sine(FrequencyHz{110.0}), nan)
            .error() == QuantityError::InvalidArgument);
  CHECK(pmPeakDeviation(FrequencyHz{0.0}, 1.0).error() ==
        QuantityError::InvalidArgument);
}
