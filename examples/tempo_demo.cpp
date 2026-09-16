// Metronome demo: an accelerating click track from a tempo curve.
// A 60 -> 120 BPM ramp over 8 s schedules 0.06 s 1568 Hz clicks at beats
// 0..8 through secondsAt(); the clicks form one piecewise signal rendered
// in a single certified pass. Beat times print for inspection.

#include <cmath>
#include <cstdio>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/render/offline.h>
#include <mmathen/signal/signal.h>
#include <mmathen/sink/sink.h>
#include <mmathen/sink/wav.h>
#include <mmathen/time/factories.h>
#include <mmathen/time/tempo.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

int main() {
  using namespace mmathen;
  auto rampRes = LinearRampTempo::create(TempoBPM{60.0}, TempoBPM{120.0},
                                         TimePointSeconds{0.0},
                                         DurationSeconds{8.0});
  if (!rampRes.ok()) {
    std::printf("tempo: FAILED\n");
    return 1;
  }
  const LinearRampTempo ramp = std::move(rampRes).value();

  std::vector<double> beatTimes;
  for (int k = 0; k <= 8; ++k) {
    auto t = ramp.secondsAt(Beat{static_cast<double>(k)});
    if (!t.ok()) {
      std::printf("beat %d: FAILED\n", k);
      return 1;
    }
    beatTimes.push_back(std::move(t).value().value());
    std::printf("beat %d at %.6f s\n", k, beatTimes.back());
  }

  constexpr double clickHz = 1568.0;
  constexpr double clickLen = 0.06;
  auto clicks = wrap<SignedAudioValue>(
      [beatTimes](TimePointSeconds t) {
        double x = 0.0;
        for (double tb : beatTimes) {
          const double dt = t.value() - tb;
          if (dt >= 0.0 && dt < clickLen)
            x += 0.25 * std::sin(2.0 * std::numbers::pi * clickHz * dt);
        }
        return SignedAudioValue{x};
      },
      "metronome(ramp 60->120)");
  const Signal<SignedAudioValue> sig{std::move(clicks)};

  const SampleRateHz rate{48000.0};
  const double tail = beatTimes.back() + 0.3;
  const FrameCount total{static_cast<std::uint64_t>(tail * rate.value())};
  auto wav = WavSink::open("metronome.wav", rate, total);
  if (!wav.ok()) {
    std::printf("open: FAILED\n");
    return 1;
  }
  auto sink = std::move(wav).value();
  auto stats = renderOffline(
      sig, RenderConfig{rate, total, TimePointSeconds{0.0}, 512}, sink);
  if (!stats.ok() || !stats.value().isClean()) {
    std::printf("render: UNCLEAN\n");
    return 1;
  }
  std::printf("metronome: %llu frames peak=%.6f clean=1\n",
              static_cast<unsigned long long>(
                  stats.value().framesRendered),
              stats.value().peakAbs);
  auto closed = sink.close();
  if (!closed.ok() || !(closed.value() == total)) {
    std::printf("close: FAILED\n");
    return 1;
  }
  std::printf("metronome certified clean\n");
  return 0;
}
