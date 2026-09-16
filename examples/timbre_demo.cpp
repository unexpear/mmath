// Timbre demo: one pitch and duration, three harmonic recipes, all
// ADSR-shaped, rendered back-to-back. Pure sine | octave+fifth recipe |
// full 1/n series (8 partials). Same certified render path throughout.

#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

#include <mmathen/render/offline.h>
#include <mmathen/signal/envelope.h>
#include <mmathen/signal/harmonics.h>
#include <mmathen/signal/oscillator.h>
#include <mmathen/signal/signal.h>
#include <mmathen/sink/sink.h>
#include <mmathen/sink/wav.h>
#include <mmathen/time/function.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

namespace {

int renderRecipe(const mmathen::HarmonicSeries& series, const char* name,
                 mmathen::WavSink& sink,
                 const mmathen::Function1D<mmathen::AmplitudeLinear>& env,
                 double seconds) {
  using namespace mmathen;
  auto sig = additive(FrequencyHz{220.0}, env, series);
  if (!sig.ok()) {
    std::printf("recipe %s: FAILED\n", name);
    return 1;
  }
  const SampleRateHz rate{48000.0};
  const FrameCount frames{
      static_cast<std::uint64_t>(seconds * rate.value())};
  auto stats = renderOffline(std::move(sig).value(),
                             RenderConfig{rate, frames,
                                          TimePointSeconds{0.0}, 512},
                             sink);
  if (!stats.ok() || !stats.value().isClean()) {
    std::printf("recipe %s: UNCLEAN\n", name);
    return 1;
  }
  std::printf("recipe %s: peak=%.6f clean=1 (%s)\n", name,
              stats.value().peakAbs, series.formula().c_str());
  return 0;
}

mmathen::HarmonicSeries mustSucceed(
    mmathen::Result<mmathen::HarmonicSeries, mmathen::QuantityError> r,
    const char* name) {
  if (!r.ok()) {
    std::printf("table %s: FAILED\n", name);
    std::exit(1);
  }
  return std::move(r).value();
}

}  // namespace

int main() {
  using namespace mmathen;
  auto segsRes =
      makeADSR(DurationSeconds{0.05}, DurationSeconds{0.1},
               AmplitudeLinear{0.7}, DurationSeconds{0.2});
  if (!segsRes.ok()) {
    std::printf("adsr table: FAILED\n");
    return 1;
  }
  const ADSRSegments segs = std::move(segsRes).value();
  // on = 0, off = 0.8, release ends exactly at the 1.0 s note end.
  auto envRes = adsr(segs, TimePointSeconds{0.0}, TimePointSeconds{0.8});
  if (!envRes.ok()) {
    std::printf("adsr fn: FAILED\n");
    return 1;
  }

  const auto pure = mustSucceed(
      HarmonicSeries::create({{FrequencyRatio{1.0}, AmplitudeLinear{0.25}}}),
      "pure");
  const auto bright = mustSucceed(
      HarmonicSeries::create({{FrequencyRatio{1.0}, AmplitudeLinear{0.2}},
                              {FrequencyRatio{2.0}, AmplitudeLinear{0.1}},
                              {FrequencyRatio{3.0}, AmplitudeLinear{0.05}}}),
      "bright");
  std::vector<Harmonic> full;
  for (int n = 1; n <= 8; ++n)
    full.push_back(Harmonic{FrequencyRatio{static_cast<double>(n)},
                            AmplitudeLinear{0.2 / static_cast<double>(n)}});
  const auto rich = mustSucceed(HarmonicSeries::create(std::move(full)),
                                "rich");

  const FrameCount total{144000};  // 3 x 1.0 s @ 48 kHz
  auto wav = WavSink::open("timbre.wav", SampleRateHz{48000.0}, total);
  if (!wav.ok()) {
    std::printf("open: FAILED\n");
    return 1;
  }
  auto sink = std::move(wav).value();
  auto makeEnv = [&] {
    auto e = adsr(segs, TimePointSeconds{0.0}, TimePointSeconds{0.8});
    if (!e.ok()) {
      std::printf("adsr fn: FAILED\n");
      std::exit(1);
    }
    return std::move(e).value();
  };
  int rc = 0;
  rc |= renderRecipe(pure, "pure", sink, makeEnv(), 1.0);
  rc |= renderRecipe(bright, "bright", sink, makeEnv(), 1.0);
  rc |= renderRecipe(rich, "rich", sink, makeEnv(), 1.0);
  auto closed = sink.close();
  if (!closed.ok() || !(closed.value() == total)) {
    std::printf("close: FAILED\n");
    return 1;
  }
  if (rc == 0) std::printf("timbre certified clean\n");
  return rc;
}
