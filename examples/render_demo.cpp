// Render demo: equation -> samples -> audible files (MVP end-to-end).
// Writes a4.wav (golden fixture), fifth.wav (440+660 mix), sweep.wav
// (220->660 Hz over 2 s) next to the working directory. Every render is
// checked: configuration valid, stats clean, file closed and verified.

#include <cstdio>
#include <utility>

#include <mmathen/render/offline.h>
#include <mmathen/signal/oscillator.h>
#include <mmathen/signal/signal.h>
#include <mmathen/sink/sink.h>
#include <mmathen/sink/wav.h>
#include <mmathen/time/factories.h>
#include <mmathen/units/music_units.h>
#include <mmathen/units/physical_units.h>

namespace {

int renderToFile(const mmathen::Signal<mmathen::SignedAudioValue>& sig,
                 const char* path, mmathen::SampleRateHz rate,
                 mmathen::FrameCount frames) {
  using namespace mmathen;
  auto wav = WavSink::open(path, rate, frames);
  if (!wav.ok()) {
    std::printf("open %s: FAILED\n", path);
    return 1;
  }
  auto sink = std::move(wav).value();
  auto stats = renderOffline(
      sig, RenderConfig{rate, frames, TimePointSeconds{0.0}, 512}, sink);
  if (!stats.ok()) {
    std::printf("render %s: FAILED\n", path);
    return 1;
  }
  const RenderStats& s = stats.value();
  std::printf("%s: frames=%llu peak=%.6f clean=%d formula=%s\n", path,
              static_cast<unsigned long long>(s.framesRendered), s.peakAbs,
              s.isClean() ? 1 : 0, sig.formula().c_str());
  if (!s.isClean()) {
    std::printf("render %s: UNCLEAN stats\n", path);
    return 1;
  }
  auto closed = sink.close();
  if (!closed.ok() || !(closed.value() == frames)) {
    std::printf("close %s: FAILED\n", path);
    return 1;
  }
  return 0;
}

}  // namespace

int main() {
  using namespace mmathen;
  const auto amp = constant(AmplitudeLinear{0.25});
  int rc = 0;

  // Golden fixture: 440 Hz sine at peak 0.25, 1 s @ 48 kHz.
  rc |= renderToFile(sine(FrequencyHz{440.0}, amp), "a4.wav",
                     SampleRateHz{48000.0}, FrameCount{48000});

  // Perfect fifth: 440 + 660 mixed, scaled back to peak 0.25 via gain
  // (mixing two 0.25-peak sines can reach 0.5, so halve explicitly).
  auto fifth =
      applyGain(sine(FrequencyHz{440.0}, amp) + sine(FrequencyHz{660.0}, amp),
                GainLinear{0.5});
  rc |= renderToFile(fifth, "fifth.wav", SampleRateHz{48000.0},
                     FrameCount{48000});

  // Linear sweep 220 -> 660 Hz over 2 s (slope 220 Hz/s).
  rc |= renderToFile(
      sineSweep(FrequencyHz{220.0}, FrequencySlopeHzPerSec{220.0}, amp),
      "sweep.wav", SampleRateHz{48000.0}, FrameCount{96000});

  if (rc == 0) std::printf("all renders certified clean\n");
  return rc;
}
