#pragma once

// Phase 2 — Signal: S: t -> A composing Function1D outputs.
//
// Signal<Output> is a distinct audio-domain type (not an alias): it owns a
// Function1D<Output> and adds signal-graph composition. Same-type mix (+) and
// modulate (*) produce new Signals with composed formula provenance.
// Cross-type application (envelope, gain) uses named functions in
// oscillator.h, not operators. Signals are pure time functions in this phase;
// stateful processors (filters, delays) are explicitly out of scope.

#include <string>
#include <utility>

#include "../core/result.h"
#include "../time/factories.h"
#include "../time/function.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"

namespace mmathen {

template <typename Output>
class Signal {
 public:
  using output_type = Output;

  explicit Signal(Function1D<Output> fn) : fn_(std::move(fn)) {}

  [[nodiscard]] Output at(TimePointSeconds t) const { return fn_.at(t); }

  [[nodiscard]] Result<Output, QuantityError> tryAt(TimePointSeconds t) const {
    return fn_.tryAt(t);
  }

  [[nodiscard]] const std::string& formula() const noexcept {
    return fn_.formula();
  }

 private:
  Function1D<Output> fn_;
};

// Mix: (a + b)(t) = a(t) + b(t), sample-wise on signed values.
[[nodiscard]] inline Signal<SignedAudioValue> operator+(
    Signal<SignedAudioValue> a, Signal<SignedAudioValue> b) {
  const std::string formula = "(" + a.formula() + " + " + b.formula() + ")";
  return Signal<SignedAudioValue>{wrap<SignedAudioValue>(
      [a = std::move(a), b = std::move(b)](TimePointSeconds t) {
        return SignedAudioValue{a.at(t).value() + b.at(t).value()};
      },
      formula)};
}

// Modulate: (a * b)(t) = a(t) * b(t), sample-wise.
[[nodiscard]] inline Signal<SignedAudioValue> operator*(
    Signal<SignedAudioValue> a, Signal<SignedAudioValue> b) {
  const std::string formula = "(" + a.formula() + " * " + b.formula() + ")";
  return Signal<SignedAudioValue>{wrap<SignedAudioValue>(
      [a = std::move(a), b = std::move(b)](TimePointSeconds t) {
        return SignedAudioValue{a.at(t).value() * b.at(t).value()};
      },
      formula)};
}

}  // namespace mmathen
