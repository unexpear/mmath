// Voice demo: a C-F-G-C progression with minimal voice motion, a tresillo
// click coda, and the set-theoretic analysis of every chord. Voice-leading
// costs print beside each transition; Forte numbers beside each chord.

#include <cmath>
#include <cstdio>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/music/euclid.h>
#include <mmathen/music/pcset.h>
#include <mmathen/music/pitch.h>
#include <mmathen/music/voiceleading.h>
#include <mmathen/render/offline.h>
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

mmathen::Signal<mmathen::SignedAudioValue> chordSignal(
    const std::vector<int>& midis) {
  using namespace mmathen;
  const auto amp = constant(AmplitudeLinear{0.25 / 3.0});
  bool first = true;
  Signal<SignedAudioValue> mix = sine(FrequencyHz{440.0}, amp);
  for (int m : midis) {
    auto p = pitchFromMidi(m);
    if (!p.ok()) {
      std::printf("midi %d: FAILED\n", m);
      std::exit(1);
    }
    auto voice = sine(std::move(p).value().frequency, amp);
    mix = first ? std::move(voice) : (std::move(mix) + std::move(voice));
    first = false;
  }
  return mix;
}

void analyze(const std::vector<int>& midis, const char* name) {
  using namespace mmathen;
  std::vector<int> pcs;
  for (int m : midis) pcs.push_back(((m % 12) + 12) % 12);
  auto s = makePitchClassSet(pcs);
  auto num = forteNumber(s.value());
  if (!s.ok() || !num.ok()) {
    std::printf("analysis %s: FAILED\n", name);
    std::exit(1);
  }
  const auto v = intervalClassVector(s.value());
  std::printf("%s midis", name);
  for (int m : midis) std::printf(" %d", m);
  std::printf(" -> %s icv <%d,%d,%d,%d,%d,%d>\n", num.value().c_str(), v[0],
              v[1], v[2], v[3], v[4], v[5]);
}

}  // namespace

int main() {
  using namespace mmathen;
  const std::vector<std::vector<int>> prog = {
      {60, 64, 67}, {60, 65, 69}, {59, 62, 67}, {60, 64, 67}};
  const char* names[4] = {"C", "F", "G", "C"};
  for (std::size_t i = 0; i < prog.size(); ++i) analyze(prog[i], names[i]);
  for (std::size_t i = 0; i + 1 < prog.size(); ++i) {
    auto c = voiceLeadingCost(prog[i], prog[i + 1]);
    if (!c.ok()) return fail("voice leading");
    std::printf("%s->%s motion=%d fifths=%d cost=%d\n", names[i], names[i + 1],
                c.value().totalMotion, c.value().parallelFifths,
                c.value().cost);
  }

  const SampleRateHz rate{48000.0};
  const FrameCount chordFrames{38400};  // 0.8 s each
  const FrameCount clickFrames{96000};  // 2.0 s coda
  const FrameCount total{4 * 38400 + 96000};
  auto wav = WavSink::open("voices.wav", rate, total);
  if (!wav.ok()) return fail("open");
  auto sink = std::move(wav).value();
  int rc = 0;
  for (const auto& chord : prog) {
    auto stats = renderOffline(
        chordSignal(chord),
        RenderConfig{rate, chordFrames, TimePointSeconds{0.0}, 512}, sink);
    if (!stats.ok() || !stats.value().isClean()) return fail("chord render");
  }
  auto rhythm = euclideanRhythm(3, 8);
  if (!rhythm.ok()) return fail("rhythm");
  const auto& pattern = rhythm.value();
  auto clicks = wrap<SignedAudioValue>(
      [pattern](TimePointSeconds t) {
        double x = 0.0;
        for (std::size_t i = 0; i < pattern.size(); ++i) {
          if (!pattern[i]) continue;
          const double dt = t.value() - static_cast<double>(i) * 0.25;
          if (dt >= 0.0 && dt < 0.05)
            x += 0.25 * std::sin(2.0 * std::numbers::pi * 1568.0 * dt);
        }
        return SignedAudioValue{x};
      },
      "tresillo clicks");
  auto stats = renderOffline(
      Signal<SignedAudioValue>{std::move(clicks)},
      RenderConfig{rate, clickFrames, TimePointSeconds{0.0}, 512}, sink);
  if (!stats.ok() || !stats.value().isClean()) return fail("click render");
  auto closed = sink.close();
  if (!closed.ok() || !(closed.value() == total)) return fail("close");
  if (rc == 0) std::printf("voice demo certified clean\n");
  return rc;
}
