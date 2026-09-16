// Tuning demo: the same scale degrees through different tuning systems.
// Renders nineteenth.wav (19-TET 220 Hz reference, degrees 0..18) and
// justmajor.wav (just-intoned C major arpeggio from C4), each note 0.3 s at
// peak 0.25. Tuning (music/) only produces frequencies; this adapter wires
// them to oscillators — the engine's domains stay decoupled.

#include <cstdio>
#include <utility>
#include <vector>

#include <mmathen/music/tuning.h>
#include <mmathen/render/offline.h>
#include <mmathen/signal/oscillator.h>
#include <mmathen/signal/signal.h>
#include <mmathen/sink/sink.h>
#include <mmathen/sink/wav.h>
#include <mmathen/time/factories.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

namespace {

template <typename Tuning>
int renderDegrees(const Tuning& tuning, const std::vector<int>& degrees,
                  const char* path, double noteSeconds) {
  using namespace mmathen;
  const SampleRateHz rate{48000.0};
  const auto noteFrames =
      static_cast<std::uint64_t>(noteSeconds * rate.value());
  const FrameCount total{noteFrames * degrees.size()};
  auto wav = WavSink::open(path, rate, total);
  if (!wav.ok()) {
    std::printf("open %s: FAILED\n", path);
    return 1;
  }
  auto sink = std::move(wav).value();
  const auto amp = constant(AmplitudeLinear{0.25});
  for (int deg : degrees) {
    auto freq = tuning.degreeToFrequency(deg);
    if (!freq.ok()) {
      std::printf("degree %d: FAILED\n", deg);
      return 1;
    }
    const auto note = sine(std::move(freq).value(), amp);
    auto stats = renderOffline(
        note, RenderConfig{rate, FrameCount{noteFrames},
                           TimePointSeconds{0.0}, 512},
        sink);
    if (!stats.ok() || !stats.value().isClean()) {
      std::printf("degree %d: UNCLEAN\n", deg);
      return 1;
    }
  }
  auto closed = sink.close();
  if (!closed.ok() || !(closed.value() == total)) {
    std::printf("close %s: FAILED\n", path);
    return 1;
  }
  std::printf("%s: %llu frames from %s\n", path,
              static_cast<unsigned long long>(total.value()),
              tuning.formula().c_str());
  return 0;
}

}  // namespace

int main() {
  using namespace mmathen;
  int rc = 0;

  const NineteenTET nineteen{FrequencyHz{220.0}, 0};
  std::vector<int> scale;
  for (int d = 0; d <= 18; ++d) scale.push_back(d);
  rc |= renderDegrees(nineteen, scale, "nineteenth.wav", 0.3);

  auto just = JustIntonation::create(
      FrequencyHz{261.6255653005986},  // C4 in 12-TET as root reference
      {FrequencyRatio{1.0}, FrequencyRatio{5.0 / 4.0}, FrequencyRatio{3.0 / 2.0},
       FrequencyRatio{2.0}});
  if (!just.ok()) {
    std::printf("just table: FAILED\n");
    return 1;
  }
  rc |= renderDegrees(std::move(just).value(), {0, 1, 2, 3}, "justmajor.wav",
                      0.5);

  if (rc == 0) std::printf("all tuning renders certified clean\n");
  return rc;
}
