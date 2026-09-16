// DSP demo: FM brass-ish note, PolyBLEP vs naive saw sweeps, and a
// lowpassed sweep through the stateful biquad. The bandlimited renders use
// explicit per-frame evaluation (sampled-source preview, documented);
// everything else flows through the certified offline path.

#include <cmath>
#include <cstdio>
#include <numbers>
#include <utility>
#include <vector>

#include <mmathen/dsp/biquad.h>
#include <mmathen/render/offline.h>
#include <mmathen/signal/bandlimited.h>
#include <mmathen/signal/fm.h>
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

// Sample-indexed PolyBLEP saw render (explicit per-n loop: the preview of
// sampled-source rendering, kept out of the continuous renderer).
int renderPolyblep(const char* path, mmathen::WavSink& sink, double seconds,
                   double freq) {
  using namespace mmathen;
  const SampleRateHz rate{48000.0};
  const std::uint64_t total =
      static_cast<std::uint64_t>(seconds * rate.value());
  const double dt = freq / rate.value();
  std::vector<double> block(512);
  std::uint64_t done = 0;
  RenderStats stats;
  while (done < total) {
    std::uint64_t n = 512;
    if (n > total - done) n = total - done;
    for (std::uint64_t i = 0; i < n; ++i) {
      const std::uint64_t idx = done + i;
      double phase =
          static_cast<double>(idx) * freq / rate.value();
      phase -= std::floor(phase);
      auto v = polyblepSawValue(phase, dt);
      if (!v.ok()) return fail("polyblep value");
      block[static_cast<std::size_t>(i)] = 0.25 * v.value();
    }
    accumulateStats(stats, block.data(), FrameCount{n});
    if (!sink.consume(block.data(), FrameCount{n})) return fail("consume");
    done += n;
  }
  if (!stats.isClean()) return fail("polyblep stats");
  std::printf("%s: peak=%.6f clean=1\n", path, stats.peakAbs);
  return 0;
}

}  // namespace

int main() {
  using namespace mmathen;
  int rc = 0;

  // FM brass-ish: carrier = modulator = 220 Hz, index 2, 1 s at peak 0.25.
  auto brass = fmTone(FrequencyHz{220.0}, FrequencyHz{220.0}, 2.0,
                      constant(AmplitudeLinear{0.25}));
  if (!brass.ok()) return fail("fm");
  auto table = sidebandTable(FrequencyHz{220.0}, FrequencyHz{220.0}, 2.0);
  if (!table.ok()) return fail("sidebands");
  std::printf("fm sidebands (freq, amp):");
  for (const auto& sb : table.value())
    std::printf(" (%.0f, %+.3f)", sb.frequency.value(), sb.amplitude);
  std::printf("\n");

  const FrameCount total{3 * 48000};  // brass 1 s + polyblep 1 s + filtered 1 s
  auto wav = WavSink::open("dsp.wav", SampleRateHz{48000.0}, total);
  if (!wav.ok()) return fail("open");
  auto sink = std::move(wav).value();
  rc |= render(std::move(brass).value(), "brass", sink, 1.0);
  rc |= renderPolyblep("polyblep", sink, 1.0, 220.0);

  // Lowpassed sweep: render the sweep, filter it block-wise (state carried
  // across blocks by one Biquad), write the filtered buffers.
  auto sweep = sineSweep(FrequencyHz{220.0}, FrequencySlopeHzPerSec{220.0},
                         constant(AmplitudeLinear{0.25}));
  MemorySink mem;
  auto rs = renderOffline(
      sweep,
      RenderConfig{SampleRateHz{48000.0}, FrameCount{48000},
                   TimePointSeconds{0.0}, 512},
      mem);
  if (!rs.ok() || !rs.value().isClean()) return fail("sweep render");
  auto lp = makeLowpass(FrequencyHz{800.0}, 0.7071, SampleRateHz{48000.0});
  if (!lp.ok() || !isStable(lp.value())) return fail("lowpass");
  Biquad q(std::move(lp).value());
  std::vector<double> filtered(mem.samples().size());
  std::size_t done = 0;
  while (done < filtered.size()) {
    std::size_t n = 512;
    if (n > filtered.size() - done) n = filtered.size() - done;
    q.processBlock(mem.samples().data() + done, filtered.data() + done,
                   FrameCount{static_cast<std::uint64_t>(n)});
    done += n;
  }
  RenderStats fstats;
  accumulateStats(fstats, filtered.data(),
                  FrameCount{static_cast<std::uint64_t>(filtered.size())});
  if (!fstats.isClean()) return fail("filtered stats");
  if (!sink.consume(filtered.data(),
                    FrameCount{static_cast<std::uint64_t>(filtered.size())}))
    return fail("consume");
  std::printf("filtered: peak=%.6f clean=1\n", fstats.peakAbs);

  auto closed = sink.close();
  if (!closed.ok() || !(closed.value() == total)) return fail("close");
  if (rc == 0) std::printf("dsp demo certified clean\n");
  return rc;
}
