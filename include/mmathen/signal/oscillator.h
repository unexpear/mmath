#pragma once

// Phase 2 — sine oscillators and level application.
//
// Architecture (never SineOscillator(float)):
//   Frequency shape -> Phase accumulator -> sin() -> Amplitude application.
// x(t) = A(t)*sin(phi(t)) with phi from phase.h. Envelope A(t) is any pure
// Function1D<AmplitudeLinear> (constants included); ADSR and other shaped
// envelopes arrive as such functions later — no envelope subsystem here.
// Intermediate values may exceed [-1,1]; overload is an export-boundary
// concern (Phase 2.5), never silently normalized here.

#include <cmath>
#include <string>
#include <utility>

#include "../time/factories.h"
#include "../time/function.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"
#include "phase.h"
#include "signal.h"

namespace mmathen {

namespace detail {
template <typename Shape>
[[nodiscard]] inline Signal<SignedAudioValue> sineFromShape(
    Shape shape, Function1D<AmplitudeLinear> amp, PhaseRadians phase0,
    std::string formula) {
  return Signal<SignedAudioValue>{wrap<SignedAudioValue>(
      [shape, amp = std::move(amp), phase0](TimePointSeconds t) {
        const double a = amp.at(t).value();
        const double p = shape.phaseAt(t, phase0).value();
        return SignedAudioValue{a * std::sin(p)};
      },
      std::move(formula))};
}
}  // namespace detail

// Constant-frequency sine. Certified audio path: f < fs/2 (checked at the
// Phase-2.5 sampling boundary, not here).
[[nodiscard]] inline Signal<SignedAudioValue> sine(
    FrequencyHz f,
    Function1D<AmplitudeLinear> amp = constant(AmplitudeLinear{1.0}),
    PhaseRadians phase0 = PhaseRadians{0.0}) {
  return detail::sineFromShape(ConstantFrequency{f}, std::move(amp), phase0,
                               "sine(const)");
}

// Linear-sweep sine: f(t) = f0 + slope*t.
[[nodiscard]] inline Signal<SignedAudioValue> sineSweep(
    FrequencyHz f0, FrequencySlopeHzPerSec slope,
    Function1D<AmplitudeLinear> amp = constant(AmplitudeLinear{1.0}),
    PhaseRadians phase0 = PhaseRadians{0.0}) {
  return detail::sineFromShape(LinearSweep{f0, slope}, std::move(amp), phase0,
                               "sine(sweep)");
}

// Sinusoidal-FM sine: f(t) = carrier + deviation*sin(2*pi*rate*t), with the
// integrated phase from phase.h (never the naive sin(2*pi*f(t)*t)).
[[nodiscard]] inline Signal<SignedAudioValue> sineFM(
    FrequencyHz carrier, FrequencyHz deviation, FrequencyHz rate,
    Function1D<AmplitudeLinear> amp = constant(AmplitudeLinear{1.0}),
    PhaseRadians phase0 = PhaseRadians{0.0}) {
  return detail::sineFromShape(SinusoidalFM{carrier, deviation, rate},
                               std::move(amp), phase0, "sine(fm)");
}

// Named cross-type application: osc(t) * env(t). Not an operator.
[[nodiscard]] inline Signal<SignedAudioValue> applyEnvelope(
    Signal<SignedAudioValue> osc, Function1D<AmplitudeLinear> env) {
  const std::string formula = "(" + osc.formula() + " * " + env.formula() + ")";
  return Signal<SignedAudioValue>{wrap<SignedAudioValue>(
      [osc = std::move(osc), env = std::move(env)](TimePointSeconds t) {
        return SignedAudioValue{osc.at(t).value() * env.at(t).value()};
      },
      formula)};
}

// Named gain application. Not an operator.
[[nodiscard]] inline Signal<SignedAudioValue> applyGain(
    Signal<SignedAudioValue> s, GainLinear g) {
  const double v = g.value();
  const std::string formula = "(" + s.formula() + " * gain)";
  return Signal<SignedAudioValue>{wrap<SignedAudioValue>(
      [s = std::move(s), v](TimePointSeconds t) {
        return SignedAudioValue{s.at(t).value() * v};
      },
      formula)};
}

}  // namespace mmathen
