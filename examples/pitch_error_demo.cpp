// Pitch-error demo: how far FDTD grids at 48 kHz detune the D8/D10 string,
// predicted by the exact discrete dispersion (D12) and measured by the FFT
// estimator (D11) from the rendered solver output.
//
// For each grid the demo renders ONE pluck (all modes at once, lossless so
// partials stay sharp), then measures each partial near its predicted
// frequency. Columns: model Hz (continuous), grid Hz (predicted), measured
// Hz, predicted cents error, |measured - predicted|. Grids:
//   ideal   M = 100, Courant 1   (exact: 0 cents)
//   coarse  M = 70,  Courant 0.7 (dispersion: partials go flat)
//   stiff   B = 4e-4, finest stable grid (M = 73)
// Nothing is rendered to WAV: this demo is a measurement report.

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/physics/fdtd1d.h>
#include <mmathen/physics/wave1d.h>
#include <mmathen/units/physical_units.h>

namespace {

constexpr double kRate = 48000.0;
constexpr std::size_t kFrames = 65536;

int fail(const char* what) {
  std::printf("%s: FAILED\n", what);
  return 1;
}

int report(const char* name, const mmathen::WaveSystem1D& sys,
           std::size_t intervals, double courant, double& worstGap) {
  using namespace mmathen;
  // Pluck at an irrational-ish fraction so no low partial sits on a node.
  auto shape = InitialShape::pluck(sys, DistanceMeters{0.1234}, 0.002);
  if (!shape.ok()) return fail("pluck");
  auto fd = Fdtd1D::create(shape.value(), intervals, courant);
  if (!fd.ok()) return fail("fdtd");
  auto fdtd = std::move(fd).value();
  const std::size_t pickup = intervals / 7;
  std::vector<double> x(kFrames);
  for (auto& v : x) {
    v = fdtd.valueAt(pickup).value();
    fdtd.step();
  }
  std::printf("%s: M=%zu courant=%.4f\n", name, intervals, courant);
  std::printf("   n    model Hz     grid Hz  measured Hz   cents   |meas-grid| Hz\n");
  for (int n = 1; n <= 10; ++n) {
    auto model = modeFrequency(sys, n);
    auto grid = fdtdDiscreteMode(sys, intervals, courant, n);
    auto cents = fdtdPitchErrorCents(sys, intervals, courant, n);
    if (!model.ok() || !grid.ok() || !cents.ok()) return fail("prediction");
    const double g = grid.value().frequency.value();
    auto est = estimateSinusoid(x, SampleRateHz{kRate},
                                Window::BlackmanHarris, FrequencyHz{g - 60.0},
                                FrequencyHz{g + 60.0});
    if (!est.ok()) {
      std::printf("  %2d  partial not isolated at this pickup (skipped)\n", n);
      continue;
    }
    const double gap = std::fabs(est.value().frequency.value() - g);
    if (gap > worstGap) worstGap = gap;
    std::printf("  %2d  %10.4f  %10.4f  %11.4f  %+6.2f   %.2e\n", n,
                model.value().value(), g, est.value().frequency.value(),
                cents.value(), gap);
  }
  return 0;
}

}  // namespace

int main() {
  using namespace mmathen;
  const SampleRateHz rate{kRate};
  auto str = idealString(DistanceMeters{0.65}, TensionNewtons{97.344},
                         LinearDensityKgPerM{0.001});
  if (!str.ok()) return fail("string");
  const double c = str.value().speed.value();
  double worstGap = 0.0;
  int rc = 0;
  rc |= report("ideal", str.value(), 100, 1.0, worstGap);
  rc |= report("coarse", str.value(), 70, c / kRate * 70.0 / 0.65, worstGap);

  auto kappa = stiffnessFromB(str.value(), 4e-4);
  if (!kappa.ok()) return fail("kappa");
  auto stiff = withStiffness(str.value(), kappa.value());
  if (!stiff.ok()) return fail("stiff");
  auto grid = stableGridForRate(stiff.value(), rate);
  if (!grid.ok()) return fail("grid");
  rc |= report("stiff", stiff.value(), grid.value().intervals,
               grid.value().courant, worstGap);

  std::printf("worst |measured - predicted grid frequency| = %.2e Hz\n",
              worstGap);
  // A measured partial off its predicted grid frequency by more than 1 mHz
  // would mean the dispersion prediction or the estimator is wrong.
  if (worstGap > 1e-3) return fail("prediction vs measurement");
  if (rc == 0) std::printf("pitch error demo verified\n");
  return rc;
}
