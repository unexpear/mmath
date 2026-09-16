// PM + envelope demo: three one-second notes through the certified offline
// path. (1) Two-operator electric-piano-ish tone: a decaying 2:1 modulator
// phase-modulates the carrier, so brightness fades faster than loudness.
// (2) Three-operator stack under a curved-attack gated envelope. (3) The
// FM == PM identity made audible: sinusoidal FM, then PM with theta = -pi/2
// and phase0 = I, back to back — the two halves are the same waveform.

#include <cstdint>
#include <cstdio>
#include <numbers>
#include <utility>

#include <mmathen/render/offline.h>
#include <mmathen/signal/fm.h>
#include <mmathen/signal/oscillator.h>
#include <mmathen/signal/pm.h>
#include <mmathen/signal/segment_envelope.h>
#include <mmathen/signal/signal.h>
#include <mmathen/sink/sink.h>
#include <mmathen/sink/wav.h>
#include <mmathen/time/factories.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

namespace {

using SigResult = mmathen::Result<mmathen::Signal<mmathen::SignedAudioValue>,
                                  mmathen::QuantityError>;

int fail(const char* what) {
  std::printf("%s: FAILED\n", what);
  return 1;
}

int render(const SigResult& sig, const char* name, mmathen::WavSink& sink,
           double seconds) {
  using namespace mmathen;
  if (!sig.ok()) return fail(name);
  auto stats = renderOffline(
      sig.value(),
      RenderConfig{SampleRateHz{48000.0},
                   FrameCount{static_cast<std::uint64_t>(seconds * 48000.0)},
                   TimePointSeconds{0.0}, 512},
      sink);
  if (!stats.ok() || !stats.value().isClean()) {
    std::printf("%s: UNCLEAN\n", name);
    return 1;
  }
  std::printf("%s: peak=%.6f clean=1\n", name, stats.value().peakAbs);
  return 0;
}

}  // namespace

int main() {
  using namespace mmathen;
  const FrameCount total{4 * 48000};
  auto wav = WavSink::open("pm.wav", SampleRateHz{48000.0}, total);
  if (!wav.ok()) return fail("open");
  auto sink = std::move(wav).value();
  int rc = 0;

  // (1) Electric piano: modulator amplitude decays with T60 = 0.4 s (so the
  // index falls from 2.5 toward 0), carrier with T60 = 1.5 s at peak 0.3.
  {
    auto modEnv = t60Decay(TimePointSeconds{0.0}, DurationSeconds{0.4});
    auto carEnv = t60Decay(TimePointSeconds{0.0}, DurationSeconds{1.5});
    if (!modEnv.ok() || !carEnv.ok()) return fail("t60");
    const auto mod = sine(FrequencyHz{440.0}, std::move(modEnv).value());
    auto car = std::move(carEnv).value();
    auto amp = wrap<AmplitudeLinear>(
        [car](TimePointSeconds t) {
          return AmplitudeLinear{0.3 * car.at(t).value()};
        },
        "0.3*" + car.formula());
    rc |= render(phaseModulate(FrequencyHz{220.0}, mod, 2.5, std::move(amp)),
                 "epiano", sink, 1.0);
  }

  // (2) Three-operator stack, slow curved swell and exponential release.
  {
    auto strike = SegmentEnvelope::create(
        AmplitudeLinear{0.0},
        {EnvelopeSegment{DurationSeconds{0.4}, AmplitudeLinear{0.25},
                         SegmentCurve::Curved, 3.0}});
    if (!strike.ok()) return fail("strike");
    auto env = gatedEnvelope(
        std::move(strike).value(),
        {EnvelopeSegment{DurationSeconds{0.3}, AmplitudeLinear{0.001},
                         SegmentCurve::Exponential}},
        TimePointSeconds{0.0}, TimePointSeconds{0.7});
    if (!env.ok()) return fail("gated");
    auto op2 = phaseModulate(FrequencyHz{330.0}, sine(FrequencyHz{495.0}), 1.2);
    if (!op2.ok()) return fail("op2");
    rc |= render(phaseModulate(FrequencyHz{165.0}, std::move(op2).value(), 1.8,
                               std::move(env).value()),
                 "stack", sink, 1.0);
  }

  // (3) FM, then the identical PM.
  rc |= render(fmTone(FrequencyHz{220.0}, FrequencyHz{110.0}, 2.0,
                      constant(AmplitudeLinear{0.25})),
               "fm", sink, 1.0);
  rc |= render(pmTone(FrequencyHz{220.0}, FrequencyHz{110.0}, 2.0,
                      PhaseRadians{-std::numbers::pi / 2.0},
                      constant(AmplitudeLinear{0.25}), PhaseRadians{2.0}),
               "pm (== fm)", sink, 1.0);

  auto closed = sink.close();
  if (!closed.ok() || !(closed.value() == total)) return fail("close");
  if (rc == 0) std::printf("pm demo certified clean\n");
  return rc;
}
