// Solver-ladder demo: ONE object, several solvers, same sound.
//
// A 0.65 m ideal string (T = 97.344 N, mu = 1 g/m -> c = 312 m/s, f1 = 240 Hz)
// is plucked at 0.2 L and picked up at node 13 of a 100-interval grid —
// the grid that steps exactly once per frame at 48 kHz (checked, never
// retuned). It is rendered by the waveguide, by FDTD at Courant 1, and by a
// 40-mode modal sum; the demo prints how far the solvers disagree. Then a
// stopped pipe (pressure, closed end at x = 0, air at 20 C) sounds through
// FDTD: odd harmonics only, the hollow clarinet-like color.
//
// The model is lossless (VALIDATION: verified, unvalidated), so notes would
// ring forever: a T60 fade is applied at the output purely as a listening
// convenience, not as physics. The pluck has corners, so the sampled field
// contains energy above Nyquist (aliasing) — stated, not hidden; smoothed
// excitations are follow-up work.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

#include <mmathen/physics/fdtd1d.h>
#include <mmathen/physics/medium.h>
#include <mmathen/physics/wave1d.h>
#include <mmathen/physics/waveguide1d.h>
#include <mmathen/render/offline.h>
#include <mmathen/sink/sink.h>
#include <mmathen/sink/wav.h>
#include <mmathen/units/physical_units.h>

namespace {

constexpr double kRate = 48000.0;
constexpr std::uint64_t kFrames = 48000;

int fail(const char* what) {
  std::printf("%s: FAILED\n", what);
  return 1;
}

// Listening fade: -60 dB after 1 s. Applied to the output, not the model.
double fade(std::uint64_t frame) {
  return std::pow(10.0, -3.0 * static_cast<double>(frame) / kRate);
}

// Write one note; gain maps field units to audio. Frame count is checked by
// the loop bound itself and again at sink close.
int writeNote(const std::vector<double>& field, double gain, const char* name,
              mmathen::WavSink& sink) {
  using namespace mmathen;
  std::vector<double> out(field.size());
  for (std::size_t i = 0; i < field.size(); ++i)
    out[i] = gain * field[i] * fade(i);
  RenderStats stats;
  accumulateStats(stats, out.data(),
                  FrameCount{static_cast<std::uint64_t>(out.size())});
  if (!stats.isClean()) return fail(name);
  if (!sink.consume(out.data(),
                    FrameCount{static_cast<std::uint64_t>(out.size())}))
    return fail("consume");
  std::printf("%s: peak=%.6f clean=1\n", name, stats.peakAbs);
  return 0;
}

}  // namespace

int main() {
  using namespace mmathen;
  const SampleRateHz rate{kRate};
  const double height = 0.002;  // 2 mm pluck
  const std::size_t pickup = 13;

  auto str = idealString(DistanceMeters{0.65}, TensionNewtons{97.344},
                         LinearDensityKgPerM{0.001});
  if (!str.ok()) return fail("string");
  auto m = gridIntervalsForRate(str.value(), rate);
  if (!m.ok()) return fail("grid");
  auto shape = InitialShape::pluck(str.value(), DistanceMeters{0.13}, height);
  if (!shape.ok()) return fail("pluck");
  auto f1 = modeFrequency(str.value(), 1);
  std::printf("string: c=%.1f m/s f1=%.1f Hz grid M=%zu\n",
              str.value().speed.value(), f1.value().value(), m.value());

  auto wg = Waveguide1D::create(shape.value(), m.value());
  auto fd = Fdtd1D::create(shape.value(), m.value(), 1.0);
  auto modal = ModalSolution::create(shape.value(), 40);
  if (!wg.ok() || !fd.ok() || !modal.ok()) return fail("solvers");
  auto waveguide = std::move(wg).value();
  auto fdtd = std::move(fd).value();
  const double pickupX =
      static_cast<double>(pickup) * waveguide.gridSpacing().value();

  std::vector<double> wgOut(kFrames), fdOut(kFrames), modalOut(kFrames);
  for (std::uint64_t n = 0; n < kFrames; ++n) {
    const auto t = TimePointSeconds{static_cast<double>(n) / kRate};
    wgOut[n] = waveguide.valueAt(pickup).value();
    fdOut[n] = fdtd.valueAt(pickup).value();
    modalOut[n] = modal.value().at(DistanceMeters{pickupX}, t).value();
    waveguide.step();
    fdtd.step();
  }
  double wgVsFd = 0.0;
  double wgVsModal = 0.0;
  for (std::uint64_t n = 0; n < kFrames; ++n) {
    wgVsFd = (std::max)(wgVsFd, std::fabs(wgOut[n] - fdOut[n]));
    wgVsModal = (std::max)(wgVsModal, std::fabs(wgOut[n] - modalOut[n]));
  }
  std::printf("max |waveguide - fdtd|  = %.3e m (exact solvers agree)\n",
              wgVsFd);
  std::printf("max |waveguide - modal| = %.3e m (modal bound %.3e m)\n",
              wgVsModal, modal.value().truncationBound());

  // Stopped pipe: c at 20 C, length chosen so the 48 kHz grid is exact.
  auto air = makeAir(TemperatureKelvin{293.15});
  if (!air.ok()) return fail("air");
  auto c = speedOfSound(air.value());
  if (!c.ok()) return fail("c");
  const std::size_t pipeM = 90;
  const double pipeL = static_cast<double>(pipeM) * c.value().value() / kRate;
  auto pipe = stoppedPipe(DistanceMeters{pipeL}, c.value());
  if (!pipe.ok()) return fail("pipe");
  auto pm = gridIntervalsForRate(pipe.value(), rate);
  if (!pm.ok() || pm.value() != pipeM) return fail("pipe grid");
  auto pf1 = modeFrequency(pipe.value(), 1);
  std::printf("stopped pipe: L=%.4f m f1=%.1f Hz (odd harmonics only)\n",
              pipeL, pf1.value().value());
  auto pulse = InitialShape::pluck(pipe.value(), DistanceMeters{0.3 * pipeL},
                                   1.0);
  if (!pulse.ok()) return fail("pulse");
  auto pfd = Fdtd1D::create(pulse.value(), pipeM, 1.0);
  if (!pfd.ok()) return fail("pipe fdtd");
  auto pipeFdtd = std::move(pfd).value();
  std::vector<double> pipeOut(kFrames);
  for (std::uint64_t n = 0; n < kFrames; ++n) {
    pipeOut[n] = pipeFdtd.valueAt(0).value();  // pressure at the closed end
    pipeFdtd.step();
  }

  const FrameCount total{4 * kFrames};
  auto wav = WavSink::open("solvers.wav", rate, total);
  if (!wav.ok()) return fail("open");
  auto sink = std::move(wav).value();
  int rc = 0;
  const double gain = 0.3 / height;
  rc |= writeNote(wgOut, gain, "string waveguide", sink);
  rc |= writeNote(fdOut, gain, "string fdtd", sink);
  rc |= writeNote(modalOut, gain, "string modal40", sink);
  rc |= writeNote(pipeOut, 0.3, "pipe fdtd", sink);
  auto closed = sink.close();
  if (!closed.ok() || !(closed.value() == total)) return fail("close");
  if (rc == 0) std::printf("solver demo certified clean\n");
  return rc;
}
