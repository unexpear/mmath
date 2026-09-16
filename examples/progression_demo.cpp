// Progression demo: the same C-major symbol in two tuning systems,
// rendered back-to-back. Chord = root + ratios (derived structure);
// polyphony is the existing signal mix operator. Just major third
// (5/4 = 386.3 cents) against 12-TET major third (400 cents).

#include <cstdio>
#include <utility>

#include <mmathen/render/offline.h>
#include <mmathen/signal/oscillator.h>
#include <mmathen/signal/signal.h>
#include <mmathen/sink/sink.h>
#include <mmathen/sink/wav.h>
#include <mmathen/time/factories.h>
#include <mmathen/music/structures.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

namespace {

int renderChord(const mmathen::Chord& chord, mmathen::WavSink& sink,
                double seconds) {
  using namespace mmathen;
  auto freqs = chord.frequencies();
  if (!freqs.ok()) {
    std::printf("chord %s: FAILED\n", chord.formula().c_str());
    return 1;
  }
  // Each voice at 1/3 peak so the mix can never exceed the 0.25 fixture.
  const auto amp = constant(AmplitudeLinear{0.25 / 3.0});
  bool first = true;
  Signal<SignedAudioValue> mix{
      wrap<SignedAudioValue>([](TimePointSeconds) {
        return SignedAudioValue{0.0};
      }, "silence")};
  for (const auto& f : std::move(freqs).value()) {
    auto voice = sine(f, amp);
    mix = first ? std::move(voice) : (std::move(mix) + std::move(voice));
    first = false;
  }
  const SampleRateHz rate{48000.0};
  const FrameCount frames{
      static_cast<std::uint64_t>(seconds * rate.value())};
  auto stats = renderOffline(
      mix, RenderConfig{rate, frames, TimePointSeconds{0.0}, 512}, sink);
  if (!stats.ok() || !stats.value().isClean()) {
    std::printf("chord %s: UNCLEAN\n", chord.formula().c_str());
    return 1;
  }
  std::printf("chord %s: peak=%.6f clean=1\n", chord.formula().c_str(),
              stats.value().peakAbs);
  return 0;
}

}  // namespace

int main() {
  using namespace mmathen;
  const FrequencyHz c4{261.6255653005986};
  auto just = Chord::create(c4,
                            {FrequencyRatio{1.0}, FrequencyRatio{5.0 / 4.0},
                             FrequencyRatio{3.0 / 2.0}},
                            "Cmajor-just");
  auto tet = Chord::create(c4,
                           {FrequencyRatio{1.0},
                            FrequencyRatio{1.2599210498948732},
                            FrequencyRatio{1.4983070768766815}},
                           "Cmajor-12tet");
  if (!just.ok() || !tet.ok()) {
    std::printf("chord table: FAILED\n");
    return 1;
  }
  const FrameCount total{96000};  // 2 x 1.0 s @ 48 kHz
  auto wav = WavSink::open("progression.wav", SampleRateHz{48000.0}, total);
  if (!wav.ok()) {
    std::printf("open: FAILED\n");
    return 1;
  }
  auto sink = std::move(wav).value();
  int rc = 0;
  rc |= renderChord(std::move(just).value(), sink, 1.0);
  rc |= renderChord(std::move(tet).value(), sink, 1.0);
  auto closed = sink.close();
  if (!closed.ok() || !(closed.value() == total)) {
    std::printf("close: FAILED\n");
    return 1;
  }
  if (rc == 0) std::printf("progression certified clean\n");
  return rc;
}
