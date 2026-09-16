// Wavetable demo: one 16-partial 1/n (bandlimited saw) table, played as a
// linear-interpolated tone, a cubic-interpolated tone, and a cubic sweep
// 110 -> 880 Hz. Bounds and the frequency limit print first; everything
// renders through the certified offline path (no hand-rolled loops).

#include <cstdio>
#include <utility>
#include <vector>

#include <mmathen/render/offline.h>
#include <mmathen/signal/harmonics.h>
#include <mmathen/signal/phase.h>
#include <mmathen/signal/signal.h>
#include <mmathen/signal/wavetable.h>
#include <mmathen/sink/sink.h>
#include <mmathen/sink/wav.h>
#include <mmathen/time/factories.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

namespace {

int fail(const char* what) {
  std::printf("%s: FAILED\n", what);
  return 1;
}

int render(const mmathen::Result<mmathen::Signal<mmathen::SignedAudioValue>,
                                  mmathen::QuantityError>& sig,
           const char* name, mmathen::WavSink& sink) {
  using namespace mmathen;
  if (!sig.ok()) return fail(name);
  auto stats = renderOffline(
      sig.value(),
      RenderConfig{SampleRateHz{48000.0}, FrameCount{48000},
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
  using Interp = WavetableInterpolation;

  std::vector<Harmonic> partials;
  for (int k = 1; k <= 16; ++k)
    partials.push_back(Harmonic{FrequencyRatio{static_cast<double>(k)},
                                AmplitudeLinear{1.0 / k}});
  auto series = HarmonicSeries::create(std::move(partials));
  if (!series.ok()) return fail("series");
  auto made = Wavetable::fromHarmonics(series.value(), 2048);
  if (!made.ok()) return fail("table");
  const Wavetable table = std::move(made).value();

  auto lin = interpolationErrorBound(table, Interp::Linear);
  auto cub = interpolationErrorBound(table, Interp::Cubic);
  auto limit = wavetableFrequencyLimit(table, SampleRateHz{48000.0});
  if (!lin.ok() || !cub.ok() || !limit.ok()) return fail("bounds");
  std::printf("%s: linear bound=%.3e cubic bound=%.3e f < %.1f Hz\n",
              table.formula().c_str(), lin.value(), cub.value(),
              limit.value().value());

  const FrameCount total{3 * 48000};
  auto wav = WavSink::open("wavetable.wav", SampleRateHz{48000.0}, total);
  if (!wav.ok()) return fail("open");
  auto sink = std::move(wav).value();
  const auto amp = constant(AmplitudeLinear{0.25});

  int rc = 0;
  rc |= render(wavetableTone(table, FrequencyHz{220.0}, amp,
                             PhaseRadians{0.0}, Interp::Linear),
               "tone linear", sink);
  rc |= render(wavetableTone(table, FrequencyHz{220.0}, amp,
                             PhaseRadians{0.0}, Interp::Cubic),
               "tone cubic", sink);
  // Peak instantaneous frequency 880 Hz, below the 1500 Hz table limit.
  rc |= render(wavetableSweep(table, FrequencyHz{110.0},
                              FrequencySlopeHzPerSec{770.0}, amp,
                              PhaseRadians{0.0}, Interp::Cubic),
               "sweep cubic", sink);

  auto closed = sink.close();
  if (!closed.ok() || !(closed.value() == total)) return fail("close");
  if (rc == 0) std::printf("wavetable demo certified clean\n");
  return rc;
}
