// Stiff-string demo: the D8/D9 string with and without bending stiffness.
//
// B = 4e-4 (inside the KTH piano range quoted in D1; ILLUSTRATIVE for this
// string, not measured) stretches every partial sharp by f_n = n f0
// sqrt(1 + B n^2) — the same law as D1's stiffAdditive, here produced by the
// physics solvers. Loss parameters are D9's illustrative T60 pair.
//
// Rendered back to back: ideal lossy string (FDTD on the exact Courant-1
// grid), stiff lossy string by FDTD on the finest stable grid at 48 kHz,
// and the same stiff string by 60 exact damped modes. The demo prints the
// partial stretch in cents and how far FDTD departs from the exact modal
// answer at this coarse audio-rate grid (modal truncation plus FDTD
// discretization error, printed together rather than hidden). Both strings
// are picked up at the same position. Output: stiff_string.wav (D1's
// stretch_demo already owns stiff.wav).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

#include <mmathen/physics/fdtd1d.h>
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
  std::printf("%s: peak=%.6f clean=1\n", name, stats.peakAbs);
  return 0;
}

std::vector<double> renderFdtd(mmathen::Fdtd1D& fdtd, std::size_t pickup) {
  std::vector<double> out(kFrames);
  for (std::uint64_t n = 0; n < kFrames; ++n) {
    out[n] = fdtd.valueAt(pickup).value();
    fdtd.step();
  }
  return out;
}

}  // namespace

int main() {
  using namespace mmathen;
  const SampleRateHz rate{kRate};
  const double height = 0.002;

  auto base = idealString(DistanceMeters{0.65}, TensionNewtons{97.344},
                          LinearDensityKgPerM{0.001});
  if (!base.ok()) return fail("string");
  auto loss = lossFromT60(base.value().speed, FrequencyHz{240.0},
                          DurationSeconds{3.0}, FrequencyHz{2400.0},
                          DurationSeconds{0.4});
  if (!loss.ok()) return fail("loss");
  auto ideal = withLoss(base.value(), loss.value());
  if (!ideal.ok()) return fail("ideal");
  auto kappa = stiffnessFromB(ideal.value(), 4e-4);
  if (!kappa.ok()) return fail("kappa");
  auto stiff = withStiffness(ideal.value(), kappa.value());
  if (!stiff.ok()) return fail("stiff");

  std::printf("B=%.1e kappa=%.4f m^2/s\n partial  ideal Hz   stiff Hz  cents\n",
              inharmonicityB(stiff.value()), kappa.value());
  for (int n = 1; n <= 8; ++n) {
    auto fi = modeFrequency(ideal.value(), n);
    auto fs = modeFrequency(stiff.value(), n);
    if (!fi.ok() || !fs.ok()) return fail("modes");
    std::printf(" %7d %10.2f %10.2f %+6.2f\n", n, fi.value().value(),
                fs.value().value(),
                1200.0 * std::log2(fs.value().value() / fi.value().value()));
  }

  // Ideal lossy string on the exact grid.
  auto exactM = gridIntervalsForRate(ideal.value(), rate);
  auto idealShape = InitialShape::pluck(ideal.value(), DistanceMeters{0.13},
                                        height);
  if (!exactM.ok() || !idealShape.ok()) return fail("ideal grid");
  auto idealFd = Fdtd1D::create(idealShape.value(), exactM.value(), 1.0);
  if (!idealFd.ok()) return fail("ideal fdtd");
  auto idealSolver = std::move(idealFd).value();
  const auto idealOut = renderFdtd(idealSolver, 13);

  // Stiff lossy string: finest stable grid at 48 kHz.
  auto grid = stableGridForRate(stiff.value(), rate);
  if (!grid.ok()) return fail("stable grid");
  auto stiffShape = InitialShape::pluck(stiff.value(), DistanceMeters{0.13},
                                        height);
  if (!stiffShape.ok()) return fail("stiff pluck");
  auto stiffFd = Fdtd1D::create(stiffShape.value(), grid.value().intervals,
                                grid.value().courant);
  if (!stiffFd.ok()) return fail("stiff fdtd");
  auto stiffSolver = std::move(stiffFd).value();
  // Same pickup position as the ideal string (node 13 of M = 100, 0.0845 m),
  // snapped to the nearest node of this coarser grid.
  const double idealPickupX = 13.0 * idealSolver.gridSpacing().value();
  const auto pickup = static_cast<std::size_t>(
      std::lround(idealPickupX / stiffSolver.gridSpacing().value()));
  const double pickupX =
      static_cast<double>(pickup) * stiffSolver.gridSpacing().value();
  std::printf("stiff grid: M=%zu courant=%.4f (dt = 1/48000), pickup "
              "%.4f m vs ideal %.4f m\n",
              grid.value().intervals, grid.value().courant, pickupX,
              idealPickupX);
  const auto stiffOut = renderFdtd(stiffSolver, pickup);

  auto modal = ModalSolution::create(stiffShape.value(), 60);
  if (!modal.ok()) return fail("modal");
  std::vector<double> modalOut(kFrames);
  double worst = 0.0;
  for (std::uint64_t n = 0; n < kFrames; ++n) {
    modalOut[n] = modal.value()
                      .at(DistanceMeters{pickupX},
                          TimePointSeconds{static_cast<double>(n) / kRate})
                      .value();
    worst = (std::max)(worst, std::fabs(modalOut[n] - stiffOut[n]));
  }
  // Not decomposed: the difference mixes modal truncation (<= the bound)
  // with FDTD discretization error (dispersion) at this coarse grid.
  std::printf("max |stiff fdtd - modal60| = %.3e m of %.3e m pluck "
              "(modal truncation bound %.3e; remainder is FDTD "
              "discretization at M=%zu, not separated)\n",
              worst, height, modal.value().truncationBound(),
              grid.value().intervals);

  const FrameCount total{3 * kFrames};
  auto wav = WavSink::open("stiff_string.wav", rate, total);
  if (!wav.ok()) return fail("open");
  auto sink = std::move(wav).value();
  const double gain = 0.3 / height;
  int rc = 0;
  rc |= writeNote(idealOut, gain, "ideal fdtd", sink);
  rc |= writeNote(stiffOut, gain, "stiff fdtd", sink);
  rc |= writeNote(modalOut, gain, "stiff modal60", sink);
  auto closed = sink.close();
  if (!closed.ok() || !(closed.value() == total)) return fail("close");
  if (rc == 0) std::printf("stiff demo certified clean\n");
  return rc;
}
