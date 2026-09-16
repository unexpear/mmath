// Stretch demo: the same A220 phrase with harmonic vs stiff partials,
// plus the dissonance table that explains piano stretch tuning. A just
// fifth (3/2) is smooth on harmonic spectra and rough on stiff ones —
// minimizing dissonance therefore stretches the octave (Giordano 2015).

#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#include <mmathen/analysis/consonance.h>
#include <mmathen/render/offline.h>
#include <mmathen/signal/harmonics.h>
#include <mmathen/signal/inharmonic.h>
#include <mmathen/signal/oscillator.h>
#include <mmathen/signal/signal.h>
#include <mmathen/sink/sink.h>
#include <mmathen/sink/wav.h>
#include <mmathen/time/factories.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

namespace {

mmathen::Partial partial(double f, double level) {
  using namespace mmathen;
  auto p = makePartial(FrequencyHz{f}, AmplitudeLinear{level});
  if (!p.ok()) {
    std::printf("partial table: FAILED\n");
    std::exit(1);
  }
  return std::move(p).value();
}

std::vector<mmathen::Partial> spectrum(double f0, double B, int n) {
  std::vector<mmathen::Partial> out;
  for (int k = 1; k <= n; ++k)
    out.push_back(partial(k * f0 * std::sqrt(1.0 + B * k * k), 1.0 / k));
  return out;
}

void table(const std::vector<mmathen::Partial>& base, double f0, double B,
           const char* name) {
  using namespace mmathen;
  std::printf("%s timbre (B=%.0e), dyad dissonance vs ratio:\n", name, B);
  for (double r : {1.25, 4.0 / 3.0, 1.4142135623730951, 1.5}) {
    auto d = dyadDissonance(base, spectrum(f0 * r, B, 6));
    if (!d.ok()) {
      std::printf("table: FAILED\n");
      std::exit(1);
    }
    std::printf("  %.4f -> %.6f\n", r, d.value().value());
  }
}

int render(const mmathen::Signal<mmathen::SignedAudioValue>& sig,
           const char* path, double seconds) {
  using namespace mmathen;
  const SampleRateHz rate{48000.0};
  const FrameCount frames{
      static_cast<std::uint64_t>(seconds * rate.value())};
  auto wav = WavSink::open(path, rate, frames);
  if (!wav.ok()) {
    std::printf("open %s: FAILED\n", path);
    return 1;
  }
  auto sink = std::move(wav).value();
  auto stats = renderOffline(
      sig, RenderConfig{rate, frames, TimePointSeconds{0.0}, 512}, sink);
  if (!stats.ok() || !stats.value().isClean()) {
    std::printf("render %s: UNCLEAN\n", path);
    return 1;
  }
  auto closed = sink.close();
  if (!closed.ok() || !(closed.value() == frames)) {
    std::printf("close %s: FAILED\n", path);
    return 1;
  }
  std::printf("%s: peak=%.6f clean=1\n", path, stats.value().peakAbs);
  return 0;
}

}  // namespace

int main() {
  using namespace mmathen;
  int rc = 0;

  const auto amp = constant(AmplitudeLinear{0.25 / 6.0});
  // Same budget both sides: six voices sharing the 0.25 peak.
  auto series = HarmonicSeries::create(
      {{FrequencyRatio{1.0}, AmplitudeLinear{1.0}},
       {FrequencyRatio{2.0}, AmplitudeLinear{1.0}},
       {FrequencyRatio{3.0}, AmplitudeLinear{1.0}},
       {FrequencyRatio{4.0}, AmplitudeLinear{1.0}},
       {FrequencyRatio{5.0}, AmplitudeLinear{1.0}},
       {FrequencyRatio{6.0}, AmplitudeLinear{1.0}}});
  auto stiff = makeStiffSpectrum(FrequencyHz{220.0}, 2e-4, 6);
  if (!series.ok() || !stiff.ok()) {
    std::printf("tables: FAILED\n");
    return 1;
  }
  auto plainSig =
      additive(FrequencyHz{220.0}, amp, std::move(series).value());
  if (!plainSig.ok()) {
    std::printf("plain signal: FAILED\n");
    return 1;
  }
  rc |= render(std::move(plainSig).value(), "plain.wav", 1.0);
  auto stiffSig = stiffAdditive(std::move(stiff).value(), amp);
  if (!stiffSig.ok()) {
    std::printf("stiff signal: FAILED\n");
    return 1;
  }
  rc |= render(std::move(stiffSig).value(), "stiff.wav", 1.0);

  table(spectrum(220.0, 0.0, 6), 220.0, 0.0, "harmonic");
  table(spectrum(220.0, 2e-4, 6), 220.0, 2e-4, "stiff");

  if (rc == 0) std::printf("stretch demo certified clean\n");
  return rc;
}
