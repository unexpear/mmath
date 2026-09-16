// D5 — biquad stateful processor tests (Catch2).
//
// Coefficient anchors recomputed from the RBJ formulae (a0-normalized);
// transfer anchors are analytic identities (DC pass-through, Nyquist null);
// the step response, reset semantics, and block invariance pin the
// stateful contract (init/reset, sequential blocks, no silent reseeks).

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include <mmathen/dsp/biquad.h>
#include <mmathen/math/comparison.h>
#include <mmathen/signal/oscillator.h>
#include <mmathen/signal/signal.h>
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

BiquadCoeffs lowpass1k() {
  return unwrap(
      makeLowpass(FrequencyHz{1000.0}, 0.7071, SampleRateHz{48000.0}));
}

std::vector<double> processAll(Biquad& q, const std::vector<double>& in,
                               std::uint64_t block) {
  std::vector<double> out(in.size(), 0.0);
  std::size_t done = 0;
  while (done < in.size()) {
    std::size_t n = block;
    if (n > in.size() - done) n = in.size() - done;
    q.processBlock(in.data() + done, out.data() + done,
                   FrameCount{static_cast<std::uint64_t>(n)});
    done += n;
  }
  return out;
}
}  // namespace

TEST_CASE("RBJ coefficients match the cookbook", "[d5biquad]") {
  const auto k = lowpass1k();
  CHECK(nearlyEqual(k.b0, 0.003916123487156441, 1e-12, 1e-12));
  CHECK(nearlyEqual(k.b1, 0.007832246974312881, 1e-12, 1e-12));
  CHECK(nearlyEqual(k.b2, 0.003916123487156441, 1e-12, 1e-12));
  CHECK(nearlyEqual(k.a1, -1.8153396116625289, 1e-12, 1e-12));
  CHECK(nearlyEqual(k.a2, 0.8310041056111547, 1e-12, 1e-12));
  CHECK(isStable(k));
  CHECK_FALSE(isStable(BiquadCoeffs{1.0, 0.0, 0.0, 0.0, -1.5}));
  CHECK(makeLowpass(FrequencyHz{0.0}, 0.7071, SampleRateHz{48000.0}).error() ==
        QuantityError::InvalidArgument);
  CHECK(makeLowpass(FrequencyHz{24000.0}, 0.7071, SampleRateHz{48000.0})
            .error() == QuantityError::InvalidArgument);  // at Nyquist
  CHECK(makeLowpass(FrequencyHz{30000.0}, 0.7071, SampleRateHz{48000.0})
            .error() == QuantityError::InvalidArgument);
  CHECK(makeLowpass(FrequencyHz{1000.0}, 0.0, SampleRateHz{48000.0}).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("transfer anchors: DC passes, Nyquist nulls", "[d5biquad]") {
  const auto k = lowpass1k();
  CHECK(nearlyEqual(
      unwrap(transferMagnitude(k, FrequencyHz{0.0}, SampleRateHz{48000.0})),
      1.0, 1e-9, 1e-9));
  CHECK(unwrap(transferMagnitude(k, FrequencyHz{24000.0},
                                 SampleRateHz{48000.0})) < 1e-12);
  const auto hp = unwrap(
      makeHighpass(FrequencyHz{1000.0}, 0.7071, SampleRateHz{48000.0}));
  CHECK(isStable(hp));
  CHECK(unwrap(transferMagnitude(hp, FrequencyHz{0.0},
                                 SampleRateHz{48000.0})) < 1e-12);
  CHECK(transferMagnitude(k, FrequencyHz{25000.0}, SampleRateHz{48000.0})
            .error() == QuantityError::InvalidArgument);  // past Nyquist
}

TEST_CASE("step response starts at b0 and settles at unity", "[d5biquad]") {
  Biquad q(lowpass1k());
  std::vector<double> in(48000, 1.0);
  const auto out = processAll(q, in, 512);
  CHECK(out[0] == lowpass1k().b0);  // y[0] = b0*x[0] from clean state
  CHECK(nearlyEqual(out.back(), 1.0, 1e-3, 1e-3));
  // Reset restores pristine state: impulse response after reset is
  // identical to a fresh object (same operations, same clean state).
  std::vector<double> impulse(8, 0.0);
  impulse[0] = 1.0;
  Biquad fresh(lowpass1k());
  const auto ref = processAll(fresh, impulse, 8);
  q.reset();
  const auto after = processAll(q, impulse, 8);
  CHECK(after == ref);
}

TEST_CASE("block size changes batching, never values", "[d5biquad]") {
  const auto sig =
      sine(FrequencyHz{440.0}, constant(AmplitudeLinear{0.25}));
  std::vector<double> in(4800);
  for (std::size_t i = 0; i < in.size(); ++i)
    in[i] = sig.at(TimePointSeconds{static_cast<double>(i) / 48000.0}).value();
  std::vector<std::vector<double>> runs;
  for (std::uint64_t bs : {64ULL, 127ULL, 512ULL}) {
    Biquad q(lowpass1k());
    runs.push_back(processAll(q, in, bs));
  }
  CHECK(runs[0] == runs[1]);
  CHECK(runs[1] == runs[2]);
}
