// Modes demo: the same C4 fundamental in an open vs a stopped pipe.
// Open keeps all harmonics; stopped keeps odd partials only (missing evens,
// clearly audible). Plus the printed impedance/reflection/absorption table
// that connects the medium to what arrives at a distance.

#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#include <mmathen/music/pitch.h>
#include <mmathen/physics/absorption.h>
#include <mmathen/physics/impedance.h>
#include <mmathen/physics/medium.h>
#include <mmathen/physics/modes.h>
#include <mmathen/render/offline.h>
#include <mmathen/signal/harmonics.h>
#include <mmathen/signal/oscillator.h>
#include <mmathen/signal/signal.h>
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

mmathen::HarmonicSeries series(std::vector<int> partials) {
  using namespace mmathen;
  std::vector<Harmonic> table;
  for (int n : partials)
    table.push_back(Harmonic{FrequencyRatio{static_cast<double>(n)},
                             AmplitudeLinear{1.0 / static_cast<double>(n)}});
  auto s = HarmonicSeries::create(std::move(table));
  if (!s.ok()) {
    std::printf("series table: FAILED\n");
    std::exit(1);
  }
  return std::move(s).value();
}

int render(const mmathen::Signal<mmathen::SignedAudioValue>& sig,
           const char* path, mmathen::WavSink& sink, double seconds) {
  using namespace mmathen;
  const SampleRateHz rate{48000.0};
  const FrameCount frames{
      static_cast<std::uint64_t>(seconds * rate.value())};
  auto stats = renderOffline(
      sig, RenderConfig{rate, frames, TimePointSeconds{0.0}, 512}, sink);
  if (!stats.ok() || !stats.value().isClean()) {
    std::printf("render %s: UNCLEAN\n", path);
    return 1;
  }
  std::printf("%s: peak=%.6f clean=1\n", path, stats.value().peakAbs);
  return 0;
}

}  // namespace

int main() {
  using namespace mmathen;
  int rc = 0;

  const auto c4 = makePitch("C4");
  if (!c4.ok()) return fail("pitch");
  const FrequencyHz f0 = c4.value().frequency;
  auto air20 = makeAir(TemperatureKelvin{293.15});
  if (!air20.ok()) return fail("medium");
  const auto c20 = speedOfSound(std::move(air20).value());
  if (!c20.ok()) return fail("medium");
  const SpeedMetersPerSecond c = std::move(c20).value();

  // Pipe lengths sounding C4: open L = c/2f, stopped L = c/4f.
  const DistanceMeters openL{c.value() / (2.0 * f0.value())};
  const DistanceMeters stoppedL{c.value() / (4.0 * f0.value())};
  std::printf("open pipe L=%.4f m, stopped L=%.4f m\n", openL.value(),
              stoppedL.value());
  for (int n = 1; n <= 3; ++n) {
    auto o = openPipeMode(openL, c, n);
    if (!o.ok()) return fail("open mode");
    std::printf("  open f%d = %.2f Hz\n", n, std::move(o).value().value());
  }
  for (int n : {1, 3, 5}) {
    auto s = stoppedPipeMode(stoppedL, c, n);
    if (!s.ok()) return fail("stopped mode");
    std::printf("  stopped f%d = %.2f Hz\n", n, std::move(s).value().value());
  }

  const auto amp = constant(AmplitudeLinear{0.25});
  auto openSig = additive(f0, amp, series({1, 2, 3, 4, 5, 6}));
  auto stoppedSig = additive(f0, amp, series({1, 3, 5}));
  if (!openSig.ok() || !stoppedSig.ok()) return fail("voices");

  const FrameCount total{96000};  // 2 x 1.0 s @ 48 kHz
  auto wav = WavSink::open("pipes.wav", SampleRateHz{48000.0}, total);
  if (!wav.ok()) return fail("open");
  auto sink = std::move(wav).value();
  rc |= render(std::move(openSig).value(), "open", sink, 1.0);
  rc |= render(std::move(stoppedSig).value(), "stopped", sink, 1.0);
  auto closed = sink.close();
  if (!closed.ok() || !(closed.value() == total)) return fail("close");

  const auto zAir = airImpedanceAt(TemperatureKelvin{293.15});
  if (!zAir.ok()) return fail("impedance");
  const ImpedancePasPerMs zW = waterImpedance();
  const auto r = reflectionCoefficient(zAir.value(), zW);
  const auto t = transmissionCoefficient(zAir.value(), zW);
  if (!r.ok() || !t.ok()) return fail("coefficients");
  std::printf("z_air=%.2f rayl z_water=%.0f rayl R=%.6f T=%.6f\n",
              zAir.value().value(), zW.value(), r.value().value(),
              t.value().value());

  auto air = makeHumidAir(TemperatureKelvin{298.15},
                          RelativeHumidity{0.8}, PressurePascals{101325.0});
  if (!air.ok()) return fail("humid air");
  for (double f : {1000.0, 4000.0}) {
    auto a = attenuationCoefficient(air.value(), FrequencyHz{f});
    if (!a.ok()) return fail("absorption");
    auto total100 =
        attenuationOverDistance(a.value(), DistanceMeters{100.0});
    if (!total100.ok()) return fail("distance");
    std::printf("alpha(%.0f Hz, 25C/80%%) = %.6f dB/m -> %.4f dB per 100 m\n",
                f, a.value().value(), total100.value().value());
  }

  if (rc == 0) std::printf("modes demo certified clean\n");
  return rc;
}
