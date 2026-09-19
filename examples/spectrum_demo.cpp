// Spectrum analyzer + visualizer + equalizer demo (D19-D21).
//
// A plucked stiff, lossy string (the D10 model, 40 exact damped modes) is
// rendered for 2 s at 48 kHz, then passed through an octave graphic EQ
// ("smile": bass and treble up, mids down). The demo writes:
//   bars_before.svg / bars_after.svg   octave "EQ bars" (IEC 61260-1 bands)
//   spectrum.svg                       log-frequency amplitude spectrum
//   spectrogram.svg                    STFT heat map of the note's decay
//   eq_response.svg                    the EQ's exact response curve
//   eq.wav                             2 s before, then 2 s after the EQ
// and prints the band levels before/after with the EQ's response at each
// band centre. Loss parameters are illustrative (as in D9); nothing here
// claims to be a measured instrument.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <mmathen/analysis/spectrum.h>
#include <mmathen/dsp/equalizer.h>
#include <mmathen/physics/wave1d.h>
#include <mmathen/plot/svg.h>
#include <mmathen/sink/sink.h>
#include <mmathen/sink/wav.h>
#include <mmathen/units/physical_units.h>

namespace {

constexpr double kRate = 48000.0;
constexpr std::size_t kFrames = 2 * 48000;

int fail(const char* what) {
  std::printf("%s: FAILED\n", what);
  return 1;
}

int writeSvg(const mmathen::Result<std::string, mmathen::QuantityError>& svg,
             const char* path) {
  if (!svg.ok()) return fail(path);
  auto w = mmathen::writeTextFile(path, svg.value());
  if (!w.ok()) return fail(path);
  std::printf("wrote %s (%zu bytes)\n", path, w.value());
  return 0;
}

// Largest power of two <= n.
std::size_t pow2Floor(std::size_t n) {
  std::size_t p = 1;
  while (p * 2 <= n) p *= 2;
  return p;
}

}  // namespace

int main() {
  using namespace mmathen;
  const SampleRateHz rate{kRate};

  // 1. The note: D8/D10 string, B = 4e-4, illustrative D9 loss, modal.
  auto base = idealString(DistanceMeters{0.65}, TensionNewtons{97.344},
                          LinearDensityKgPerM{0.001});
  if (!base.ok()) return fail("string");
  auto kappa = stiffnessFromB(base.value(), 4e-4);
  auto loss = lossFromT60(base.value().speed, FrequencyHz{240.0},
                          DurationSeconds{3.0}, FrequencyHz{2400.0},
                          DurationSeconds{0.4});
  if (!kappa.ok() || !loss.ok()) return fail("parameters");
  auto stiff = withStiffness(base.value(), kappa.value());
  if (!stiff.ok()) return fail("stiff");
  auto sys = withLoss(stiff.value(), loss.value());
  if (!sys.ok()) return fail("lossy");
  auto pluck = InitialShape::pluck(sys.value(), DistanceMeters{0.1234}, 0.002);
  if (!pluck.ok()) return fail("pluck");
  auto modal = ModalSolution::create(pluck.value(), 40);
  if (!modal.ok()) return fail("modal");
  std::vector<double> before(kFrames);
  for (std::size_t i = 0; i < kFrames; ++i)
    before[i] = 150.0 * modal.value()
                            .at(DistanceMeters{0.0845},
                                TimePointSeconds{static_cast<double>(i) / kRate})
                            .value();

  // 2. Octave graphic EQ "smile".
  auto bands = fractionalOctaveBands(1, FrequencyHz{30.0}, FrequencyHz{17000.0});
  if (!bands.ok()) return fail("bands");
  const std::vector<double> gains = {6.0, 5.0, 3.0, 0.0, -3.0,
                                     -3.0, 0.0, 3.0, 5.0, 6.0};
  auto eq = makeGraphicEq(bands.value(), gains, rate);
  if (!eq.ok()) return fail("eq");
  std::vector<double> after(kFrames);
  auto eqRun = std::move(eq).value();
  eqRun.processBlock(before.data(), after.data(),
                     FrameCount{static_cast<std::uint64_t>(kFrames)});

  // 3. Analysis: band levels over the whole note, before and after.
  const std::size_t n = pow2Floor(kFrames);
  const std::vector<double> segB(before.begin(), before.begin() + n);
  const std::vector<double> segA(after.begin(), after.begin() + n);
  auto specB = computeSpectrum(segB, rate, Window::Hann);
  auto specA = computeSpectrum(segA, rate, Window::Hann);
  if (!specB.ok() || !specA.ok()) return fail("spectrum");
  const auto levB = bandLevels(specB.value(), bands.value());
  const auto levA = bandLevels(specA.value(), bands.value());
  // A band's change follows the EQ's response at the partials inside it
  // (240, 480, 720 Hz ...), which sit away from the band centres, so it need
  // not equal the EQ at the centre. Bands more than 100 dB down hold no note
  // content (only the EQ's start-up transient and leakage) and are marked.
  std::printf("  band Hz   before dB   after dB   change   EQ at centre\n");
  for (std::size_t i = 0; i < levB.size(); ++i) {
    auto r = eqRun.responseDb(levB[i].band.center);
    if (!r.ok()) return fail("response");
    if (levB[i].levelDb < -100.0) {
      std::printf("  %7.1f   (below -100 dB: no signal)        %+6.2f\n",
                  levB[i].band.center.value(), r.value());
      continue;
    }
    std::printf("  %7.1f   %8.2f   %8.2f   %+6.2f   %+6.2f\n",
                levB[i].band.center.value(), levB[i].levelDb, levA[i].levelDb,
                levA[i].levelDb - levB[i].levelDb, r.value());
  }

  // 4. Figures.
  int rc = 0;
  PlotStyle bars;
  bars.dbMin = -100.0;
  bars.dbMax = 0.0;
  bars.title = "Plucked stiff string: octave band levels (before EQ)";
  rc |= writeSvg(svgBandBars(levB, bars), "bars_before.svg");
  bars.title = "Plucked stiff string: octave band levels (after smile EQ)";
  rc |= writeSvg(svgBandBars(levA, bars), "bars_after.svg");

  PlotStyle line;
  line.dbMin = -120.0;
  line.dbMax = 0.0;
  line.title = "Amplitude spectrum (dBFS), Hann, 65536 points";
  rc |= writeSvg(svgSpectrum(specB.value(), FrequencyHz{20.0},
                             FrequencyHz{20000.0}, line),
                 "spectrum.svg");

  auto gram = computeSpectrogram(before, rate, Window::Hann, 2048, 512);
  if (!gram.ok()) return fail("spectrogram");
  PlotStyle heat;
  heat.dbMin = -110.0;
  heat.dbMax = -10.0;
  heat.title = "Spectrogram: partials decay, the highest first";
  rc |= writeSvg(svgSpectrogram(gram.value(), FrequencyHz{5000.0}, 120, 90,
                                heat),
                 "spectrogram.svg");

  // EQ response as a "spectrum" of |H| on a fine grid (exact, not measured).
  Spectrum resp;
  resp.binHz = 5.0;
  resp.amplitude.resize(4801);
  resp.power.assign(4801, 0.0);
  for (std::size_t k = 0; k < resp.amplitude.size(); ++k) {
    auto db = eqRun.responseDb(FrequencyHz{static_cast<double>(k) * 5.0});
    if (!db.ok()) return fail("response curve");
    resp.amplitude[k] = std::pow(10.0, db.value() / 20.0);
  }
  PlotStyle eqStyle;
  eqStyle.dbMin = -12.0;
  eqStyle.dbMax = 12.0;
  eqStyle.title = "Graphic EQ response (exact cascade magnitude)";
  rc |= writeSvg(svgSpectrum(resp, FrequencyHz{20.0}, FrequencyHz{20000.0},
                             eqStyle),
                 "eq_response.svg");

  // 5. Audio: before, then after.
  auto wav = WavSink::open("eq.wav", rate,
                           FrameCount{static_cast<std::uint64_t>(2 * kFrames)});
  if (!wav.ok()) return fail("open");
  auto sink = std::move(wav).value();
  RenderStats stats;
  for (const auto* buf : {&before, &after}) {
    accumulateStats(stats, buf->data(),
                    FrameCount{static_cast<std::uint64_t>(buf->size())});
    if (!sink.consume(buf->data(),
                      FrameCount{static_cast<std::uint64_t>(buf->size())}))
      return fail("consume");
  }
  auto closed = sink.close();
  if (!closed.ok() || !stats.isClean()) return fail("eq.wav");
  std::printf("wrote eq.wav: peak=%.4f clean=1\n", stats.peakAbs);
  if (rc == 0) std::printf("spectrum demo complete\n");
  return rc;
}
