// Comma demo: the same major third three ways — Pythagorean (81/64),
// just (5/4), 12-TET (400 cents) — plus the printed 12/19/31 error tables
// and the MOS scales this engine can construct. The ~22-cent comma between
// the first two is clearly audible; the table shows what each EDO buys.

#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#include <mmathen/music/commas.h>
#include <mmathen/music/mos.h>
#include <mmathen/music/temperament.h>
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

void errorTable(const std::vector<mmathen::FrequencyRatio>& targets,
                const char* names[], int count) {
  using namespace mmathen;
  for (int N : {12, 19, 31}) {
    std::printf("EDO %d:", N);
    for (int i = 0; i < count; ++i) {
      auto e = nearestDegreeError(N, targets[static_cast<std::size_t>(i)]);
      if (!e.ok()) {
        std::printf(" FAILED\n");
        std::exit(1);
      }
      std::printf(" %s %+.2f", names[i], e.value().errorCents);
    }
    auto r = errorReport(N, targets);
    if (!r.ok()) {
      std::printf(" FAILED\n");
      std::exit(1);
    }
    std::printf(" | worst %.2f mean %.2f\n", r.value().worstAbsCents,
                r.value().meanAbsCents);
  }
}

}  // namespace

int main() {
  using namespace mmathen;
  const FrequencyHz root{261.6255653005986};  // C4
  const auto amp = constant(AmplitudeLinear{0.25});
  // Pythagorean third 81/64, just third 5/4, 12-TET third 2^(1/3).
  const double thirds[3] = {81.0 / 64.0, 5.0 / 4.0,
                            std::pow(2.0, 1.0 / 3.0)};
  const char* names[3] = {"pythagorean", "just", "12tet"};
  const FrameCount total{3 * 38400};  // 3 x 0.8 s @ 48 kHz
  auto wav = WavSink::open("thirds.wav", SampleRateHz{48000.0}, total);
  if (!wav.ok()) return fail("open");
  auto sink = std::move(wav).value();
  int rc = 0;
  for (int i = 0; i < 3; ++i) {
    auto third = sine(FrequencyHz{root.value() * thirds[i]}, amp);
    auto rootNote = sine(root, amp);
    rc |= render(third + rootNote, names[i], sink, 0.8);
  }
  auto closed = sink.close();
  if (!closed.ok() || !(closed.value() == total)) return fail("close");

  const std::vector<FrequencyRatio> triad{FrequencyRatio{1.5},
                                          FrequencyRatio{1.25},
                                          FrequencyRatio{1.2}};
  const char* labels[3] = {"fifth", "maj3", "min3"};
  errorTable(triad, labels, 3);

  auto syn = commaCents(commas::syntonic());
  auto pc = commaCents(commas::pythagorean());
  auto sch = commaCents(commas::schisma());
  if (!syn.ok() || !pc.ok() || !sch.ok()) return fail("commas");
  std::printf("syntonic %.4f = pythagorean %.4f - schisma %.4f\n",
              syn.value().value(), pc.value().value(), sch.value().value());

  auto diatonic = mosWord(Cents{701.9550008653874}, 7);
  if (!diatonic.ok() || !diatonic.value().isMOS) return fail("mos");
  std::printf("diatonic MOS: %zu pitch classes, %zu step sizes\n",
              diatonic.value().pitchClasses.size(),
              diatonic.value().sizes.size());

  if (rc == 0) std::printf("comma demo certified clean\n");
  return rc;
}
