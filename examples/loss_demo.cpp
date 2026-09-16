// Loss demo: the D8 string and stopped pipe, now decaying from their own
// physics instead of an output fade.
//
// LOSS PARAMETERS ARE ILLUSTRATIVE, NOT MEASURED (VALIDATION.md): the T60
// pairs below were chosen to sound plausible and demonstrate the model — no
// real string or pipe was measured. Each note is rendered twice, by lossy
// FDTD (Courant 1, implicit loss) and by the exact damped modal sum, and the
// demo prints the per-mode T60s the fit produced and how far the two solvers
// disagree. No fade is applied: silence at the end comes from the model.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

#include <mmathen/physics/fdtd1d.h>
#include <mmathen/physics/medium.h>
#include <mmathen/physics/wave1d.h>
#include <mmathen/sink/sink.h>
#include <mmathen/sink/wav.h>
#include <mmathen/units/physical_units.h>

namespace {

constexpr double kRate = 48000.0;
constexpr std::uint64_t kFrames = 2 * 48000;

int fail(const char* what) {
  std::printf("%s: FAILED\n", what);
  return 1;
}

int writeNote(const std::vector<double>& field, double gain, const char* name,
              mmathen::WavSink& sink) {
  using namespace mmathen;
  std::vector<double> out(field.size());
  for (std::size_t i = 0; i < field.size(); ++i) out[i] = gain * field[i];
  RenderStats stats;
  accumulateStats(stats, out.data(),
                  FrameCount{static_cast<std::uint64_t>(out.size())});
  if (!stats.isClean()) return fail(name);
  if (!sink.consume(out.data(),
                    FrameCount{static_cast<std::uint64_t>(out.size())}))
    return fail("consume");
  // Tail level over the last 10 ms shows the model's own decay.
  double tail = 0.0;
  for (std::size_t i = out.size() - 480; i < out.size(); ++i)
    tail = (std::max)(tail, std::fabs(out[i]));
  std::printf("%s: peak=%.6f tail=%.6f clean=1\n", name, stats.peakAbs, tail);
  return 0;
}

// Render one lossy object by FDTD and by modes; print T60s and agreement.
int renderPair(const mmathen::InitialShape& shape, std::size_t intervals,
               std::size_t pickup, std::size_t modes, double gain,
               const char* name, mmathen::WavSink& sink) {
  using namespace mmathen;
  const WaveSystem1D& sys = shape.system();
  std::printf("%s: sigma0=%.4f 1/s sigma1=%.3e m^2/s  T60:", name,
              sys.loss.sigma0, sys.loss.sigma1);
  for (int n = 1; n <= 5; ++n) {
    auto t60 = modeT60(sys, n);
    if (!t60.ok()) return fail("t60");
    std::printf(" m%d=%.2fs", n, t60.value().value());
  }
  std::printf("\n");

  auto fd = Fdtd1D::create(shape, intervals, 1.0);
  auto modal = ModalSolution::create(shape, modes);
  if (!fd.ok() || !modal.ok()) return fail("solvers");
  auto fdtd = std::move(fd).value();
  const double x =
      static_cast<double>(pickup) * fdtd.gridSpacing().value();
  std::vector<double> fdOut(kFrames), modalOut(kFrames);
  double worst = 0.0;
  for (std::uint64_t n = 0; n < kFrames; ++n) {
    fdOut[n] = fdtd.valueAt(pickup).value();
    modalOut[n] = modal.value()
                      .at(DistanceMeters{x},
                          TimePointSeconds{static_cast<double>(n) / kRate})
                      .value();
    worst = (std::max)(worst, std::fabs(fdOut[n] - modalOut[n]));
    fdtd.step();
  }
  std::printf("%s: max |fdtd - modal%zu| = %.3e (modal bound %.3e)\n", name,
              modes, worst, modal.value().truncationBound());
  int rc = writeNote(fdOut, gain, "  fdtd", sink);
  rc |= writeNote(modalOut, gain, "  modal", sink);
  return rc;
}

}  // namespace

int main() {
  using namespace mmathen;
  const SampleRateHz rate{kRate};
  const FrameCount total{4 * kFrames};
  auto wav = WavSink::open("loss.wav", rate, total);
  if (!wav.ok()) return fail("open");
  auto sink = std::move(wav).value();
  int rc = 0;

  // String (D8): 240 Hz. Illustrative decay: 3 s at 240 Hz, 0.4 s at 2400 Hz.
  auto str = idealString(DistanceMeters{0.65}, TensionNewtons{97.344},
                         LinearDensityKgPerM{0.001});
  if (!str.ok()) return fail("string");
  auto strLoss = lossFromT60(str.value().speed, FrequencyHz{240.0},
                             DurationSeconds{3.0}, FrequencyHz{2400.0},
                             DurationSeconds{0.4});
  if (!strLoss.ok()) return fail("string loss");
  auto lossyStr = withLoss(str.value(), strLoss.value());
  auto m = gridIntervalsForRate(str.value(), rate);
  if (!lossyStr.ok() || !m.ok()) return fail("lossy string");
  auto pluck = InitialShape::pluck(lossyStr.value(), DistanceMeters{0.13},
                                   0.002);
  if (!pluck.ok()) return fail("pluck");
  rc |= renderPair(pluck.value(), m.value(), 13, 60, 0.3 / 0.002, "string",
                   sink);

  // Stopped pipe (D8): 133.3 Hz. Illustrative: 1.2 s at f1, 0.15 s at 2 kHz.
  auto air = makeAir(TemperatureKelvin{293.15});
  if (!air.ok()) return fail("air");
  auto c = speedOfSound(air.value());
  if (!c.ok()) return fail("c");
  const std::size_t pipeM = 90;
  const double pipeL = static_cast<double>(pipeM) * c.value().value() / kRate;
  auto pipe = stoppedPipe(DistanceMeters{pipeL}, c.value());
  if (!pipe.ok()) return fail("pipe");
  auto f1 = modeFrequency(pipe.value(), 1);
  if (!f1.ok()) return fail("pipe f1");
  auto pipeLoss = lossFromT60(c.value(), f1.value(), DurationSeconds{1.2},
                              FrequencyHz{2000.0}, DurationSeconds{0.15});
  if (!pipeLoss.ok()) return fail("pipe loss");
  auto lossyPipe = withLoss(pipe.value(), pipeLoss.value());
  if (!lossyPipe.ok()) return fail("lossy pipe");
  auto pulse = InitialShape::pluck(lossyPipe.value(),
                                   DistanceMeters{0.3 * pipeL}, 1.0);
  if (!pulse.ok()) return fail("pulse");
  rc |= renderPair(pulse.value(), pipeM, 0, 60, 0.3, "pipe", sink);

  auto closed = sink.close();
  if (!closed.ok() || !(closed.value() == total)) return fail("close");
  if (rc == 0) std::printf("loss demo certified clean\n");
  return rc;
}
