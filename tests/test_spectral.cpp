// D5 — time-varying spectra tests (Catch2).
//
// Each partial follows its own envelope (attack transients differ per
// partial — the actual acoustics of onsets); the mix is sample-identical
// to the manually summed voices.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/math/comparison.h>
#include <mmathen/signal/oscillator.h>
#include <mmathen/signal/signal.h>
#include <mmathen/signal/spectral.h>
#include <mmathen/time/factories.h>
#include <mmathen/time/function.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
}  // namespace

TEST_CASE("each partial follows its own envelope", "[d5spectral]") {
  // Voice 1: full amplitude at 220 Hz. Voice 2: half amplitude at 440 Hz,
  // gated to the second half-second (attack transient is brighter... here
  // inverted for testability: the octave enters late).
  auto gate = wrap<AmplitudeLinear>(
      [](TimePointSeconds t) {
        return AmplitudeLinear{t.value() < 0.5 ? 0.0 : 0.5};
      },
      "late-gate");
  std::vector<SpectralVoice> voices;
  voices.push_back(
      SpectralVoice{FrequencyRatio{1.0}, constant(AmplitudeLinear{1.0})});
  voices.push_back(SpectralVoice{FrequencyRatio{2.0}, std::move(gate)});
  const auto sig = unwrap(timeVaryingAdditive(FrequencyHz{220.0},
                                              std::move(voices)));
  // Before the gate: pure 220 Hz sine.
  CHECK(nearlyEqual(sig.at(TimePointSeconds{0.25}).value(),
                    std::sin(2.0 * std::numbers::pi * 220.0 * 0.25), 1e-9,
                    1e-9));
  // After: fundamental plus half-amplitude octave, written inline.
  const double t = 0.75;
  const double expected =
      std::sin(2.0 * std::numbers::pi * 220.0 * t) +
      0.5 * std::sin(2.0 * std::numbers::pi * 440.0 * t);
  CHECK(nearlyEqual(sig.at(TimePointSeconds{t}).value(), expected, 1e-9,
                    1e-9));
  CHECK(timeVaryingAdditive(FrequencyHz{0.0}, {}).error() ==
        QuantityError::InvalidArgument);  // empty + bad root
}
