// Objects demo (D17): beyond one dimension, beyond one method, beyond one
// object.
//
// 1. A struck rectangular membrane (0.6 m x 0.4 m, c = 100 m/s), 2-D FDTD at
//    48 kHz — inharmonic drum-like partials (Rayleigh f_mn), measured and
//    printed against the grid prediction.
// 2. A plucked string coupled to a bridge oscillator (5 g, 2e4 N/m, damped):
//    the bridge drains energy, so the note decays from the coupling itself.
//    Coupled modes printed from the transcendental equation.
// 3. FEM vs determinant for a clamped-free stiff bar (tension ~ 0): the
//    classic cantilever frequencies from two independent methods.
// The membrane and coupled string render to objects.wav (a listening fade
// is applied to the lossless membrane only, labeled, as in D8).

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/physics/coupled_string.h>
#include <mmathen/physics/fdtd_box.h>
#include <mmathen/physics/fem1d.h>
#include <mmathen/physics/wave1d.h>
#include <mmathen/sink/sink.h>
#include <mmathen/sink/wav.h>
#include <mmathen/units/physical_units.h>

namespace {

constexpr double kRate = 48000.0;
constexpr std::size_t kFrames = 65536;

int fail(const char* what) {
  std::printf("%s: FAILED\n", what);
  return 1;
}

int writeNote(std::vector<double> x, double gain, bool fade, const char* name,
              mmathen::WavSink& sink) {
  using namespace mmathen;
  for (std::size_t i = 0; i < x.size(); ++i) {
    x[i] *= gain;
    if (fade) x[i] *= std::pow(10.0, -3.0 * static_cast<double>(i) / kRate);
  }
  RenderStats stats;
  accumulateStats(stats, x.data(),
                  FrameCount{static_cast<std::uint64_t>(x.size())});
  if (!stats.isClean()) return fail(name);
  if (!sink.consume(x.data(), FrameCount{static_cast<std::uint64_t>(x.size())}))
    return fail("consume");
  std::printf("%s: peak=%.6f clean=1\n", name, stats.peakAbs);
  return 0;
}

}  // namespace

int main() {
  using namespace mmathen;
  const SampleRateHz rate{kRate};
  int rc = 0;

  // 1. Membrane at 48 kHz: h = 0.004 m, lam = c dt/h = 100/(48000*0.004).
  auto grid = makeBoxGrid(2, {150, 100, 0}, DistanceMeters{0.004},
                          SpeedMetersPerSecond{100.0}, BoxWalls::Dirichlet);
  if (!grid.ok()) return fail("membrane grid");
  const double lam = 100.0 / (kRate * 0.004);
  auto bump = boxBumpInitial(grid.value(), {0.21, 0.13, 0.0},
                             DistanceMeters{0.03}, 1e-3);
  if (!bump.ok()) return fail("membrane bump");
  auto drum = FdtdBox::create(grid.value(), bump.value(), lam);
  if (!drum.ok()) return fail("membrane");
  auto membrane = std::move(drum).value();
  std::vector<double> m(kFrames);
  for (auto& v : m) {
    v = membrane.valueAt(97, 61).value();
    membrane.step();
  }
  std::printf("membrane 0.6x0.4 m, courant %.4f:\n", lam);
  for (const auto mode : {std::array<int, 3>{1, 1, 0}, std::array<int, 3>{2, 1, 0},
                          std::array<int, 3>{1, 2, 0}, std::array<int, 3>{3, 1, 0}}) {
    const auto cont = boxModeFrequency(grid.value(), mode);
    const auto disc = boxDiscreteModeFrequency(grid.value(), lam, mode);
    if (!cont.ok() || !disc.ok()) return fail("membrane modes");
    const double d = disc.value().value();
    auto est = estimateSinusoid(m, rate, Window::BlackmanHarris,
                                FrequencyHz{d * 0.98}, FrequencyHz{d * 1.02});
    std::printf("  (%d,%d) Rayleigh %.3f Hz  grid %.4f Hz  measured %s\n",
                mode[0], mode[1], cont.value().value(), d,
                est.ok() ? "ok" : "not isolated");
    if (est.ok() && std::fabs(est.value().frequency.value() - d) > 1e-3)
      return fail("membrane measurement vs grid");
  }

  // 2. String + damped bridge.
  auto str = idealString(DistanceMeters{0.65}, TensionNewtons{97.344},
                         LinearDensityKgPerM{0.001});
  if (!str.ok()) return fail("string");
  auto coupled = makeCoupledString(str.value(), TensionNewtons{97.344},
                                   BridgeOscillator{0.005, 2.0e4, 0.8});
  if (!coupled.ok()) return fail("coupled");
  auto modes = coupledModeFrequencies(coupled.value(), 4);
  if (!modes.ok()) return fail("coupled modes");
  std::printf("string + bridge (5 g, 2e4 N/m) coupled modes:");
  for (const auto& f : modes.value()) std::printf(" %.3f", f.value());
  std::printf(" Hz\n");
  auto pluck = InitialShape::pluck(str.value(), DistanceMeters{0.1234}, 0.002);
  if (!pluck.ok()) return fail("pluck");
  // M = 100 at Courant 1 is the D8 grid, dt = 1/48000 exactly; the bridge's
  // Gershgorin condition holds there (create() would reject it otherwise).
  auto fd = CoupledStringFdtd::create(coupled.value(), pluck.value(), 100, 1.0);
  if (!fd.ok()) return fail("coupled fdtd");
  auto cs = std::move(fd).value();
  std::vector<double> s(kFrames);
  for (auto& v : s) {
    v = cs.field()[13];
    cs.step();
  }
  double early = 0.0, late = 0.0;
  for (std::size_t i = 0; i < 4800; ++i) early = (std::max)(early, std::fabs(s[i]));
  for (std::size_t i = kFrames - 4800; i < kFrames; ++i)
    late = (std::max)(late, std::fabs(s[i]));
  std::printf("  bridge damping decay: early peak %.3e, late peak %.3e\n",
              early, late);

  // 3. Cantilever: FEM vs determinant (tension 1e-6 of bending scale).
  auto bar = makeWaveSystem1D(DistanceMeters{1.0}, SpeedMetersPerSecond{1e-6},
                              Boundary::Fixed, Boundary::Free);
  if (!bar.ok()) return fail("bar");
  auto cant = withStiffness(bar.value(), 1.0, StiffEnd::Clamped, StiffEnd::Free);
  if (!cant.ok()) return fail("cantilever");
  auto fem = femStiffFrequencies(cant.value(), 64, 3);
  if (!fem.ok()) return fail("fem");
  std::printf("cantilever (kappa = 1, L = 1): beta L determinant vs FEM(64):\n");
  for (int n = 1; n <= 3; ++n) {
    auto det = modeFrequency(cant.value(), n);
    if (!det.ok()) return fail("det");
    const auto i = static_cast<std::size_t>(n - 1);
    std::printf("  mode %d: %.8f Hz vs %.8f Hz (beta L = %.6f)\n", n,
                det.value().value(), fem.value()[i].value(),
                std::sqrt(2.0 * 3.141592653589793 * det.value().value()));
  }

  const FrameCount total{2 * kFrames};
  auto wav = WavSink::open("objects.wav", rate, total);
  if (!wav.ok()) return fail("open");
  auto sink = std::move(wav).value();
  rc |= writeNote(m, 0.25 / 1e-3, true, "membrane (listening fade)", sink);
  rc |= writeNote(s, 0.3 / 0.002, false, "string+bridge (physical decay)", sink);
  auto closed = sink.close();
  if (!closed.ok() || !(closed.value() == total)) return fail("close");
  if (rc == 0) std::printf("objects demo verified\n");
  return rc;
}
