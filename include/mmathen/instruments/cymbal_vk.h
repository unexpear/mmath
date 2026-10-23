#pragma once

// Nonlinear cymbal (step 19): the free-edge circular plate with von Karman
// geometric nonlinearity, in modal form.
//
// Model. M. Ducceschi & C. Touze, "Modal approach for nonlinear vibrations
// of damped impacted plates: Application to sound synthesis of gongs and
// cymbals", J. Sound Vib. 344, 313-331 (2015) (D&T below):
//   rho h w_tt + D LapLap w = L(w, F) + p - losses,   LapLap F = -(E h / 2) L(w, w)
// with L the von Karman bracket (their eq. 2; polar form in C. Touze,
// O. Thomas & A. Chaigne, J. Sound Vib. 258, 649 (2002)). The transverse
// displacement is expanded on the free-edge plate modes Phi (circular_plate.h,
// D&T app. B.1), the Airy stress function on the in-plane modes Psi, which
// for the free edge are the clamped-plate modes (Psi = Psi_r = 0 at r = a,
// D&T app. B.2, roots of J_{l-1} I_l - I_{l-1} J_l = 0). The coupling
//   H^l_ij = int Psi_l L(Phi_i, Phi_j) dS                      (D&T eq. 9)
// and E^s_il = H^l_si (triple self-adjointness, D&T eq. 18) give, with every
// mode normalised to mean square 1 over the plate and q the modal
// displacement in metres,
//   q_s'' + 2 sigma_s q_s' + w_s^2 q_s = -dV/dq_s + P Phi_s(x0) / (rho h pi a^2),
//   V = sum_l E / (8 rho pi^2 a^4 zeta_l^4) (sum_ij H^l_ij q_i q_j)^2,
// zeta_l the unit-disk in-plane eigenvalue. That is D&T eq. (14) written as
// a potential (see docs/49-cymbal-von-karman.md); h enters only the
// linear part, so the nonlinearity matters once q is of order h.
//
// Mode sets. Transverse: every free-edge mode below maxModeHz. Only the
// cos(k theta) companions measured from the strike point are kept: a
// perfect plate struck at one point stays mirror-symmetric about that
// diameter, and cos x cos couples only to cos in-plane modes, so the sin
// family stays exactly zero (a modelling choice; it drops the symmetry-
// breaking instabilities of a real, imperfect cymbal). In-plane: the
// inPlaneModes lowest clamped modes (D&T sec. 3.2.2: 60 give converged
// couplings up to the 1000th transverse mode).
//
// Time stepping. Explicit and energy stable: the scalar-auxiliary-variable
// form of S. Bilbao, M. Ducceschi & F. Zama, "Explicit exactly energy-
// conserving methods for Hamiltonian systems", J. Comput. Phys. 472, 111697
// (2023): psi = sqrt(2V + eps), g = grad V / psi,
//   delta_tt q + 2 sigma delta_t. q + W^2 q = -g mu_t. psi + f,
//   delta_t+ psi = g . delta_t. q,
// solved per step with one Sherman-Morrison rank-one update (O(N)). The
// linear frequency is the exact one, W^2 = (2/k^2)(1 - cos w k), so a mode
// keeps its frequency at the sample rate and the lossless energy
//   sum (delta_t- q)^2/2 + W^2 q^n q^{n-1} / 2 + psi^2 / 2
// is non-negative for every mode below fs/2: the update cannot blow up,
// whatever the strike. Losses only remove energy.
//
// Strike. The stick is the engine's Hertz contact (mallet_voice.h feltForce,
// MalletStrike), coupled explicitly at the strike point.
//
// Output. D&T sec. 3.2.3 synthesise from the output-point velocity
// (VonKarmanOutput::Velocity). The default is its time derivative, the
// acceleration: the far-field pressure of a small vibrating region is
// proportional to its volume acceleration, p = rho0 dQ/dt / (4 pi r)
// (simple source; P. M. Morse & K. U. Ingard, Theoretical Acoustics, 1968,
// sec. 7.1), so a microphone hears the acceleration spectrum, not the
// velocity one. One point stands in for the radiating surface (VERIFY: no
// radiation efficiency or directivity, no free-edge dipole short circuit).
//
// What is not here: taper and dome (Q. B. Nguyen & C. Touze, JASA 145,
// 977 (2019) show both matter for a crash), a radiation model, the sin family.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "../math/constants.h"
#include "../physics/circular_plate.h"
#include "../render/voice.h"
#include "cymbal.h"
#include "mallet_voice.h"

namespace mmathen {

// B20 bronze: density 8607 kg/m^3, Young's modulus 112.6 GPa, Poisson 0.342,
// the values used for a B20 cymbal in K. Marogiannis et al. (FEM-BEM
// vibroacoustic simulations of motion driven cymbal-drumstick interactions,
// Acoustics 5(1), 10, 2023), from a materials database (VERIFY on a
// measured cymbal).
inline constexpr double kB20DensityKgPerM3 = 8607.0;
inline constexpr double kB20YoungsModulusPa = 112.6e9;
inline constexpr double kB20Poisson = 0.342;
// Crash thickness: Nguyen & Touze (JASA 145, 977, 2019) measured crash
// cymbals in two Paris stores, edge 0.5-0.9 mm (mean 0.65-0.7 mm), centre
// 0.9-1.4 mm; D&T's crash is a uniform 0.5 mm plate. A uniform plate at the
// thin end of the measured edge range, 0.5 mm: TUNED inside that range (a
// uniform 0.7 mm plate cascades too weakly, centroid ~1.2-2.4 kHz at
// 10 m/s against ~2.4-4 kHz measured, docs/49-cymbal-von-karman.md; Nguyen &
// Touze report the same for uniform plates). VERIFY: a real crash is
// tapered and domed, so the edge is where it is hit, not the whole plate.
inline constexpr double kCrashCymbalThicknessM = 0.5e-3;
// Modal losses mu_k = 2 sigma_k = 0.007 w_k^0.6 + 2 (1/s), Nguyen & Touze
// 2019 sec. III A (power law after Humbert et al., EPL 102, 30002, 2013,
// measured on large thin plates; constant term theirs). VERIFY for bronze.
inline constexpr double kCymbalLossCoefficient = 0.007;
inline constexpr double kCymbalLossExponent = 0.6;
inline constexpr double kCymbalLossConstantPerS = 2.0;
// Scale on that law for the crash: 0.5, TUNED. N&T's law is for a steel
// plate (populated "with ad-hoc values", their words); with it the crash rang out in
// 5.3 / 4.0 / 3.4 / 3.1 s (broadband -5..-35 dB slope, velocity 30 / 70 /
// 110 / 127) against 9.4 / 7.4 / 6.2 / 6.2 s for the Virtuosity Drums CC0
// crash (layers vl1 / vl2 / vl3), i.e. the bronze rings about twice as long
// at the same frequency. Halving the law gives 10.4 / 7.7 / 6.6 / 6.1 s;
// 0.4 gave 13.1 / 9.5 / 7.8 / 7.7 s (docs/49-cymbal-von-karman.md). The shape
// (power 0.6 plus a constant) stays N&T's. VERIFY against a measured B20
// crash's modal decay times.
inline constexpr double kCrashCymbalLossScale = 0.5;
// Output point: D&T sec. 3.2.3, r = 0.896 a at 0.519 rad from the strike
// ("to avoid a radius with too many modes having a nodal point").
inline constexpr double kVonKarmanObserveRadialFraction = 0.896;
inline constexpr double kVonKarmanObserveAngle = 0.519;
// In-plane modes: D&T sec. 3.2.2 (60). TUNED down for cost, see NOTES.
inline constexpr int kVonKarmanInPlaneModes = 40;
// Transverse truncation: TUNED for cost (D&T's crash ran 1000 modes to
// 11.5 kHz and found the truncation "has no dramatic influence on the
// sound produced").
inline constexpr double kVonKarmanCrashMaxModeHz = 10000.0;

// Time-step oversampling: the voice steps at oversample x the output rate
// and decimates. 4 (192 kHz at a 48 kHz output), TUNED from a step-size
// test on the velocity-127 strike (docs/49-cymbal-von-karman.md): against an 8x
// (384 kHz) run, 4x keeps the 0-0.3 / 0.3-1 / 1-2 s levels within 0.4 dB
// and the centroid within ~7 % (the cascade is chaotic, so only its
// statistics converge), where 2x (96 kHz) was 1.4-2 dB loud after 0.3 s
// with the HF energy peak 270 ms late. The linear part is exact at any
// rate; it is the nonlinear (SAV) update whose error grows with omega k.
inline constexpr int kVonKarmanOversample = 4;

struct VonKarmanPlateSpec {
  double radiusM = 0.2032;
  double thicknessM = kCrashCymbalThicknessM;
  double youngsModulusPa = kB20YoungsModulusPa;
  double densityKgPerM3 = kB20DensityKgPerM3;
  double poisson = kB20Poisson;
  double maxModeHz = kVonKarmanCrashMaxModeHz;
  int inPlaneModes = kVonKarmanInPlaneModes;
  double lossCoefficient = kCymbalLossCoefficient;
  double lossExponent = kCymbalLossExponent;
  double lossConstantPerS = kCymbalLossConstantPerS;
  double lossScale = kCrashCymbalLossScale;  // multiplies the whole law
};

// Precomputed modes and couplings; shared by every voice on the same plate.
struct VonKarmanPlateModel {
  struct Mode {
    int k = 0;              // nodal diameters
    double lambda = 0.0;    // free-edge eigenvalue (unit disk)
    double c = 0.0;         // R = J_k(lambda r) + c I_k(lambda r)
    double scale = 0.0;     // mean-square normalisation of R cos(k theta)
    double omega = 0.0;     // rad/s
    double sigma = 0.0;     // 1/s
  };
  VonKarmanPlateSpec spec;
  std::vector<Mode> modes;
  std::vector<int> inPlaneOrder;    // l per in-plane mode
  std::vector<double> inPlaneZeta;  // unit-disk eigenvalue
  // Coupling pairs grouped by in-plane mode: eta_l = sum w q_i q_j over
  // [groupStart[l], groupStart[l+1]).
  std::vector<std::int32_t> pairI, pairJ;
  std::vector<double> pairW;
  // Packed copy of (pairW, pairI, pairJ) for the time loop (cache footprint).
  struct PackedPair { float w; std::uint16_t i, j; };
  std::vector<PackedPair> packed;
  std::vector<std::size_t> groupStart;
  std::vector<double> groupC;  // E / (8 rho pi^2 a^4 zeta^4)
  double modalMassKg = 0.0;    // rho h pi a^2
  // Transverse shape at a point (unit-disk radius, angle from the strike).
  [[nodiscard]] double shape(std::size_t m, double radialFraction, double angle) const {
    const Mode& md = modes[m];
    const double x = md.lambda * radialFraction;
    const double radial = std::cyl_bessel_j(static_cast<double>(md.k), x) +
                          md.c * std::cyl_bessel_i(static_cast<double>(md.k), x);
    return md.scale * radial * std::cos(static_cast<double>(md.k) * angle);
  }
};

namespace vk_detail {

// Gauss-Legendre nodes and weights on [0, 1].
inline void gaussLegendre(int n, std::vector<double>& x, std::vector<double>& w) {
  x.assign(static_cast<std::size_t>(n), 0.0);
  w.assign(static_cast<std::size_t>(n), 0.0);
  for (int i = 0; i < n; ++i) {
    double z = std::cos(constants::pi * (i + 0.75) / (n + 0.5));
    double pp = 0.0;
    for (int it = 0; it < 100; ++it) {
      double p1 = 1.0, p2 = 0.0;
      for (int j = 1; j <= n; ++j) {
        const double p3 = p2;
        p2 = p1;
        p1 = ((2.0 * j - 1.0) * z * p2 - (j - 1.0) * p3) / j;
      }
      pp = n * (z * p1 - p2) / (z * z - 1.0);
      const double dz = p1 / pp;
      z -= dz;
      if (std::fabs(dz) < 1e-15) break;
    }
    x[static_cast<std::size_t>(i)] = 0.5 * (1.0 - z);
    w[static_cast<std::size_t>(i)] = 1.0 / ((1.0 - z * z) * pp * pp);
  }
}

// Roots of f in (lo, hi) by a 0.05 scan and bisection; exact zeros and
// non-finite samples are skipped (underflow at large order).
template <class F>
inline std::vector<double> scanRoots(F f, double lo, double hi) {
  std::vector<double> roots;
  constexpr double step = 0.05;
  double x0 = lo, f0 = f(x0);
  for (double x1 = lo + step; x1 <= hi; x1 += step) {
    const double f1 = f(x1);
    if (std::isfinite(f0) && std::isfinite(f1) && f0 != 0.0 && f1 != 0.0 && (f0 < 0.0) != (f1 < 0.0)) {
      double a = x0, b = x1, fa = f0;
      for (int it = 0; it < 70; ++it) {
        const double m = 0.5 * (a + b), fm = f(m);
        if ((fm < 0.0) == (fa < 0.0)) { a = m; fa = fm; } else { b = m; }
      }
      roots.push_back(0.5 * (a + b));
    }
    x0 = x1;
    f0 = f1;
  }
  return roots;
}

inline double besselJ(int n, double x) {
  if (n < 0) return (n % 2 == 0 ? 1.0 : -1.0) * std::cyl_bessel_j(static_cast<double>(-n), x);
  return std::cyl_bessel_j(static_cast<double>(n), x);
}
inline double besselI(int n, double x) { return std::cyl_bessel_i(static_cast<double>(n < 0 ? -n : n), x); }

// int_0^{2 pi} cos(l t) cos(m t) dt
inline double cosCos(int l, int m) {
  l = l < 0 ? -l : l;
  m = m < 0 ? -m : m;
  if (l != m) return 0.0;
  return l == 0 ? 2.0 * constants::pi : constants::pi;
}

}  // namespace vk_detail

// Builds the modes and the coupling table. InvalidArgument on a bad spec.
[[nodiscard]] inline Result<std::shared_ptr<const VonKarmanPlateModel>, QuantityError>
buildVonKarmanPlate(const VonKarmanPlateSpec& spec) {
  using R = Result<std::shared_ptr<const VonKarmanPlateModel>, QuantityError>;
  if (!(spec.radiusM > 0.0) || !(spec.thicknessM > 0.0) || !(spec.youngsModulusPa > 0.0) ||
      !(spec.densityKgPerM3 > 0.0) || !(spec.poisson > -1.0 && spec.poisson < 0.5) ||
      !(spec.maxModeHz > 0.0) || spec.inPlaneModes < 0 || !(spec.lossCoefficient >= 0.0) ||
      !std::isfinite(spec.lossExponent) || !(spec.lossConstantPerS >= 0.0) || !(spec.lossScale >= 0.0))
    return R::failure(QuantityError::InvalidArgument);
  auto model = std::make_shared<VonKarmanPlateModel>();
  model->spec = spec;
  const double a = spec.radiusM, h = spec.thicknessM, nu = spec.poisson;
  // f = lambda^2 h / (2 pi a^2) sqrt(E / (12 rho (1 - nu^2)))  (circular_plate.h)
  const double fPerLambda2 = h / (constants::twoPi * a * a) *
                             std::sqrt(spec.youngsModulusPa / (12.0 * spec.densityKgPerM3 * (1.0 - nu * nu)));
  const double lambdaMax = std::sqrt(spec.maxModeHz / fPerLambda2);
  for (int k = 0;; ++k) {
    const double start = std::max(0.5, 0.7 * k);
    if (start >= lambdaMax) break;
    auto ch = [&](double l) {
      auto v = freePlateCharacteristic(k, l, nu);
      return v.ok() ? v.value() : std::nan("");
    };
    for (double lam : vk_detail::scanRoots(ch, start, lambdaMax)) {
      const auto bp = plate_detail::pairAt(k, lam);
      const auto rows = plate_detail::boundaryRows(k, lam, nu, bp);
      VonKarmanPlateModel::Mode md;
      md.k = k;
      md.lambda = lam;
      md.c = rows.momentJ / rows.momentI;
      md.omega = constants::twoPi * fPerLambda2 * lam * lam;
      md.sigma = 0.5 * spec.lossScale * (spec.lossCoefficient * std::pow(md.omega, spec.lossExponent) + spec.lossConstantPerS);
      if (!std::isfinite(md.c)) return R::failure(QuantityError::RangeError);
      model->modes.push_back(md);
    }
  }
  if (model->modes.empty()) return R::failure(QuantityError::InvalidArgument);
  std::sort(model->modes.begin(), model->modes.end(),
            [](const auto& x, const auto& y) { return x.omega < y.omega; });
  const std::size_t n = model->modes.size();
  if (n > 65535u) return R::failure(QuantityError::RangeError);  // PackedPair holds 16-bit mode indices

  // In-plane (clamped) modes: lowest inPlaneModes over all orders.
  struct IP { int l; double z; };
  std::vector<IP> ips;
  for (double zcap = 16.0; spec.inPlaneModes > 0; zcap *= 1.5) {
    ips.clear();
    for (int l = 0; l < 4 * static_cast<int>(zcap) + 4; ++l) {
      auto f = [&](double z) {
        return vk_detail::besselJ(l - 1, z) * vk_detail::besselI(l, z) -
               vk_detail::besselI(l - 1, z) * vk_detail::besselJ(l, z);
      };
      for (double z : vk_detail::scanRoots(f, std::max(0.5, 0.9 * l), zcap)) ips.push_back({l, z});
    }
    if (static_cast<int>(ips.size()) >= spec.inPlaneModes) break;
  }
  std::sort(ips.begin(), ips.end(), [](const IP& x, const IP& y) { return x.z < y.z; });
  if (static_cast<int>(ips.size()) > spec.inPlaneModes) ips.resize(static_cast<std::size_t>(spec.inPlaneModes));

  // Radial quadrature: Gauss-Legendre, enough nodes for the fastest product.
  double zMax = 0.0;
  for (const IP& p : ips) zMax = std::max(zMax, p.z);
  const int nr = 48 + static_cast<int>(1.2 * (2.0 * lambdaMax + zMax));
  std::vector<double> rx, rw;
  vk_detail::gaussLegendre(nr, rx, rw);
  // Transverse radial tables R, R', R'' (unit disk), normalised.
  std::vector<double> tR(n * static_cast<std::size_t>(nr)), tR1(tR.size()), tR2(tR.size());
  for (std::size_t m = 0; m < n; ++m) {
    auto& md = model->modes[m];
    const int k = md.k;
    const double kk = static_cast<double>(k) * k;
    double norm = 0.0;
    for (int q = 0; q < nr; ++q) {
      const double r = rx[static_cast<std::size_t>(q)], x = md.lambda * r;
      const double J = vk_detail::besselJ(k, x), Jm = vk_detail::besselJ(k - 1, x);
      const double I = vk_detail::besselI(k, x), Im = vk_detail::besselI(k - 1, x);
      const double Jp = (k == 0 ? -vk_detail::besselJ(1, x) : Jm - k / x * J);
      const double Ip = (k == 0 ? vk_detail::besselI(1, x) : Im - k / x * I);
      const double Jpp = -Jp / x - (1.0 - kk / (x * x)) * J;
      const double Ipp = -Ip / x + (1.0 + kk / (x * x)) * I;
      const std::size_t o = m * static_cast<std::size_t>(nr) + static_cast<std::size_t>(q);
      tR[o] = J + md.c * I;
      tR1[o] = md.lambda * (Jp + md.c * Ip);
      tR2[o] = md.lambda * md.lambda * (Jpp + md.c * Ipp);
      norm += rw[static_cast<std::size_t>(q)] * r * tR[o] * tR[o];
    }
    const double ang = k == 0 ? constants::twoPi : constants::pi;
    md.scale = std::sqrt(constants::pi / (ang * norm));
    for (int q = 0; q < nr; ++q) {
      const std::size_t o = m * static_cast<std::size_t>(nr) + static_cast<std::size_t>(q);
      tR[o] *= md.scale; tR1[o] *= md.scale; tR2[o] *= md.scale;
    }
  }
  // Modes grouped by k.
  int kMax = 0;
  for (const auto& md : model->modes) kMax = std::max(kMax, md.k);
  std::vector<std::vector<std::int32_t>> byK(static_cast<std::size_t>(kMax) + 1);
  for (std::size_t m = 0; m < n; ++m) byK[static_cast<std::size_t>(model->modes[m].k)].push_back(static_cast<std::int32_t>(m));

  const double cBase = spec.youngsModulusPa / (8.0 * spec.densityKgPerM3 * constants::pi * constants::pi * a * a * a * a);
  std::vector<double> S(static_cast<std::size_t>(nr));
  model->groupStart.push_back(0);
  for (const IP& ip : ips) {
    const int l = ip.l;
    const double z = ip.z;
    const double ratio = vk_detail::besselJ(l, z) / vk_detail::besselI(l, z);
    double norm = 0.0;
    for (int q = 0; q < nr; ++q) {
      const double r = rx[static_cast<std::size_t>(q)];
      S[static_cast<std::size_t>(q)] = vk_detail::besselJ(l, z * r) - ratio * vk_detail::besselI(l, z * r);
      norm += rw[static_cast<std::size_t>(q)] * r * S[static_cast<std::size_t>(q)] * S[static_cast<std::size_t>(q)];
    }
    const double sc = std::sqrt(constants::pi / ((l == 0 ? constants::twoPi : constants::pi) * norm));
    for (double& v : S) v *= sc;
    for (int ki = 0; ki <= kMax; ++ki) {
      int partners[3] = {l - ki, ki + l, ki - l};
      std::sort(partners, partners + 3);
      for (int pi = 0; pi < 3; ++pi) {
        const int kj = partners[pi];
        if (kj < ki || kj > kMax || (pi > 0 && kj == partners[pi - 1])) continue;
        const double c1 = 0.5 * (vk_detail::cosCos(l, ki - kj) + vk_detail::cosCos(l, ki + kj));
        const double c2 = 0.5 * ki * kj * (vk_detail::cosCos(l, ki - kj) - vk_detail::cosCos(l, ki + kj));
        if (c1 == 0.0 && c2 == 0.0) continue;
        for (std::int32_t i : byK[static_cast<std::size_t>(ki)]) {
          for (std::int32_t j : byK[static_cast<std::size_t>(kj)]) {
            if (ki == kj && j < i) continue;
            const std::size_t oi = static_cast<std::size_t>(i) * static_cast<std::size_t>(nr);
            const std::size_t oj = static_cast<std::size_t>(j) * static_cast<std::size_t>(nr);
            const double kj2 = static_cast<double>(kj) * kj, ki2 = static_cast<double>(ki) * ki;
            double val = 0.0;
            for (int q = 0; q < nr; ++q) {
              const std::size_t uq = static_cast<std::size_t>(q);
              const double r = rx[uq], ir = 1.0 / r, ir2 = ir * ir;
              const double Ri = tR[oi + uq], Ri1 = tR1[oi + uq], Ri2 = tR2[oi + uq];
              const double Rj = tR[oj + uq], Rj1 = tR1[oj + uq], Rj2 = tR2[oj + uq];
              const double a1 = Ri2 * (Rj1 * ir - kj2 * Rj * ir2) + Rj2 * (Ri1 * ir - ki2 * Ri * ir2);
              const double a2 = (Ri1 * ir - Ri * ir2) * (Rj1 * ir - Rj * ir2);
              val += rw[uq] * r * S[uq] * (c1 * a1 - 2.0 * c2 * a2);
            }
            if (val == 0.0 || !std::isfinite(val)) continue;
            model->pairI.push_back(i);
            model->pairJ.push_back(j);
            model->pairW.push_back(i == j ? val : 2.0 * val);
          }
        }
      }
    }
    model->groupStart.push_back(model->pairW.size());
    for (std::size_t p = model->packed.size(); p < model->pairW.size(); ++p)
      model->packed.push_back({static_cast<float>(model->pairW[p]), static_cast<std::uint16_t>(model->pairI[p]),
                               static_cast<std::uint16_t>(model->pairJ[p])});
    model->groupC.push_back(cBase / (z * z * z * z));
    model->inPlaneOrder.push_back(l);
    model->inPlaneZeta.push_back(z);
  }
  model->modalMassKg = spec.densityKgPerM3 * h * constants::pi * a * a;
  return R::success(std::shared_ptr<const VonKarmanPlateModel>(std::move(model)));
}

enum class VonKarmanOutput { Velocity, Acceleration };

// One struck von Karman plate. The strike point is the angle origin.
class VonKarmanCymbalVoice final : public IVoice {
 public:
  [[nodiscard]] static Result<VonKarmanCymbalVoice, QuantityError> create(
      std::shared_ptr<const VonKarmanPlateModel> model, const MalletStrike& strike, SampleRateHz rate,
      double observeRadialFraction = kVonKarmanObserveRadialFraction,
      double observeAngle = kVonKarmanObserveAngle, bool nonlinear = true,
      VonKarmanOutput output = VonKarmanOutput::Acceleration, int oversample = kVonKarmanOversample) {
    using R = Result<VonKarmanCymbalVoice, QuantityError>;
    const double fs = rate.value();
    if (!model || !std::isfinite(fs) || !(fs > 0.0) || oversample < 1 || oversample > 8 || !mallet_detail::strikeSane(strike) ||
        !(observeRadialFraction >= 0.0 && observeRadialFraction <= 1.0) || !std::isfinite(observeAngle))
      return R::failure(QuantityError::InvalidArgument);
    VonKarmanCymbalVoice v(rate, strike, std::move(model), nonlinear);
    v.accelerationOut_ = output == VonKarmanOutput::Acceleration;
    v.oversample_ = oversample;
    v.stepRate_ = fs * oversample;
    if (oversample > 1) {
      // Decimation lowpass: Blackman-windowed sinc, cutoff 0.45 fs (modes stop
      // at maxModeHz, well below), 24 taps per output sample, unit DC gain.
      const int taps = 24 * oversample + 1;
      const double fc = 0.45 / oversample;
      double sum = 0.0;
      for (int i = 0; i < taps; ++i) {
        const double x = i - 0.5 * (taps - 1);
        const double sinc = x == 0.0 ? 2.0 * fc : std::sin(2.0 * constants::pi * fc * x) / (constants::pi * x);
        const double w = 0.42 - 0.5 * std::cos(2.0 * constants::pi * i / (taps - 1)) +
                         0.08 * std::cos(4.0 * constants::pi * i / (taps - 1));
        v.fir_.push_back(sinc * w);
        sum += sinc * w;
      }
      for (double& c : v.fir_) c /= sum;
      v.hist_.assign(static_cast<std::size_t>(taps), 0.0);
    }
    const auto& md = v.model_->modes;
    const std::size_t n = md.size();
    const double k = 1.0 / (fs * oversample);
    v.a_.resize(n); v.b_.resize(n); v.d_.resize(n); v.phiS_.resize(n); v.phiO_.resize(n);
    v.q_.assign(n, 0.0); v.qPrev_.assign(n, 0.0); v.r_.assign(n, 0.0); v.g_.assign(n, 0.0);
    v.eta_.assign(v.model_->groupC.size(), 0.0);
    std::size_t live = 0;
    for (std::size_t m = 0; m < n; ++m) {
      // Modes at or above fs/2 cannot be represented; they are left at rest.
      const double wk = md[m].omega * k;
      const double w2 = wk < constants::pi ? 2.0 * (1.0 - std::cos(wk)) : -1.0;  // k^2 W^2
      v.active_.push_back(w2 >= 0.0 && wk < 0.98 * constants::pi);
      const double sk = md[m].sigma * k;
      v.d_[m] = 1.0 / (1.0 + sk);
      v.a_[m] = 2.0 - w2;
      v.b_[m] = 1.0 - sk;
      v.phiS_[m] = v.model_->shape(m, strike.radialFraction, 0.0);
      v.phiO_[m] = v.model_->shape(m, observeRadialFraction, observeAngle - strike.angleRadians);
      if (!v.active_.back()) { v.phiS_[m] = 0.0; v.phiO_[m] = 0.0; }
      if (!std::isfinite(v.phiS_[m]) || !std::isfinite(v.phiO_[m])) return R::failure(QuantityError::RangeError);
      live += v.active_.back() ? 1u : 0u;
    }
    if (live == 0) return R::failure(QuantityError::InvalidArgument);
    v.psi_ = std::sqrt(kEps);
    return R::success(std::move(v));
  }

  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }

  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(double* out, FrameCount count) override {
    using R = Result<FrameCount, QuantityError>;
    const std::uint64_t frames = static_cast<std::uint64_t>(count.value());
    if (frames == 0) return R::success(count);
    if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
    const auto& M = *model_;
    const std::size_t n = q_.size();
    const double k = 1.0 / stepRate_, k2 = k * k;
    const double forceScale = 1.0 / M.modalMassKg;
    for (std::uint64_t f = 0; f < frames; ++f) {
     for (int os = 0; os < oversample_; ++os) {
      // Contact at the strike point (explicit).
      double w0 = 0.0, v0 = 0.0;
      for (std::size_t m = 0; m < n; ++m) { w0 += phiS_[m] * q_[m]; v0 += phiS_[m] * (q_[m] - qPrev_[m]); }
      v0 /= k;
      double force = 0.0;
      {
        auto fr = mallet_detail::feltForce(stickY_ - w0, stickV_ - v0, strike_);
        if (!fr.ok()) return R::failure(fr.error());
        force = fr.value();
        stickV_ -= force / strike_.massKg * k;
        stickY_ += stickV_ * k;
      }
      // Nonlinear gradient g = grad V / sqrt(2V + eps).
      double gg = 0.0, V = 0.0;
      std::fill(g_.begin(), g_.end(), 0.0);
      if (nonlinear_) {
        // Per in-plane mode l: eta_l = sum H q_i q_j, then scatter its gradient
        // while the group is still in cache.
        const std::size_t groups = M.groupC.size();
        const auto* pk = M.packed.data();
        const double* q = q_.data();
        double* g = g_.data();
        for (std::size_t l = 0; l < groups; ++l) {
          const std::size_t p0 = M.groupStart[l], p1 = M.groupStart[l + 1];
          double e = 0.0;
          for (std::size_t p = p0; p < p1; ++p) e += static_cast<double>(pk[p].w) * q[pk[p].i] * q[pk[p].j];
          V += M.groupC[l] * e * e;
          const double s = 2.0 * M.groupC[l] * e;
          if (s == 0.0) continue;
          for (std::size_t p = p0; p < p1; ++p) {
            const double w = s * static_cast<double>(pk[p].w);
            g[pk[p].i] += w * q[pk[p].j];
            g[pk[p].j] += w * q[pk[p].i];
          }
        }
        lastV_ = V;
        const double inv = 1.0 / std::sqrt(2.0 * V + kEps);
        for (std::size_t m = 0; m < n; ++m) g_[m] = active_[m] ? g_[m] * inv : 0.0;
      }
      // r = a q - b qPrev - k^2 g psi + (k^2/4) g (g . qPrev) + k^2 f, then
      // (diag(1 + sigma k) + (k^2/4) g g^T) q+ = r by Sherman-Morrison.
      double gqPrev = 0.0;
      for (std::size_t m = 0; m < n; ++m) gqPrev += g_[m] * qPrev_[m];
      double dr = 0.0;
      for (std::size_t m = 0; m < n; ++m) {
        if (!active_[m]) { r_[m] = 0.0; continue; }
        const double rhs = a_[m] * q_[m] - b_[m] * qPrev_[m] - k2 * g_[m] * psi_ + 0.25 * k2 * g_[m] * gqPrev +
                           k2 * force * forceScale * phiS_[m];
        r_[m] = rhs * d_[m];  // D^-1 r
        dr += g_[m] * r_[m];
        gg += g_[m] * g_[m] * d_[m];
      }
      const double coef = 0.25 * k2 * dr / (1.0 + 0.25 * k2 * gg);
      double sample = 0.0, dpsi = 0.0;
      for (std::size_t m = 0; m < n; ++m) {
        const double next = active_[m] ? r_[m] - coef * g_[m] * d_[m] : 0.0;
        dpsi += g_[m] * (next - qPrev_[m]);
        sample += phiO_[m] * (next - q_[m]);
        qPrev_[m] = q_[m];
        q_[m] = next;
      }
      psi_ += 0.5 * dpsi;
      sample /= k;
      if (accelerationOut_) {
        const double vel = sample;
        sample = (vel - lastVelocity_) / k;
        lastVelocity_ = vel;
      }
      if (!std::isfinite(sample) || !std::isfinite(psi_)) return R::failure(QuantityError::RangeError);
      if (oversample_ == 1) { out[f] = sample; continue; }
      hist_[histPos_] = sample;
      histPos_ = histPos_ + 1 == hist_.size() ? 0 : histPos_ + 1;
     }
      if (oversample_ > 1) {
        double y = 0.0;
        std::size_t h = histPos_;
        for (double c : fir_) { y += c * hist_[h]; h = h + 1 == hist_.size() ? 0 : h + 1; }
        out[f] = y;
      }
    }
    return R::success(count);
  }

  void reset() override {
    std::fill(q_.begin(), q_.end(), 0.0);
    std::fill(qPrev_.begin(), qPrev_.end(), 0.0);
    psi_ = std::sqrt(kEps);
    stickY_ = 0.0;
    stickV_ = strike_.speedMetersPerSecond;
    lastVelocity_ = 0.0;
    lastV_ = 0.0;
    std::fill(hist_.begin(), hist_.end(), 0.0);
    histPos_ = 0;
  }

  // Kinetic + linear bending energy per modal mass (m^2/s^2).
  [[nodiscard]] double linearEnergy() const {
    const double k = 1.0 / stepRate_;
    double e = 0.0;
    for (std::size_t m = 0; m < q_.size(); ++m) {
      if (!active_[m]) continue;
      const double v = (q_[m] - qPrev_[m]) / k;
      const double w2 = (2.0 - a_[m]) / (k * k);
      e += 0.5 * v * v + 0.5 * w2 * q_[m] * qPrev_[m];
    }
    return e;
  }
  // Lossless numerical energy per modal mass (m^2/s^2): kinetic + linear
  // potential + psi^2/2 (Bilbao, Ducceschi & Zama 2023). For tests.
  [[nodiscard]] double numericalEnergy() const { return linearEnergy() + 0.5 * psi_ * psi_; }
  // The von Karman potential V at the last step (m^2/s^2 per modal mass).
  [[nodiscard]] double nonlinearPotential() const noexcept { return lastV_; }
  [[nodiscard]] std::size_t modeCount() const noexcept { return q_.size(); }
  [[nodiscard]] const VonKarmanPlateModel& model() const noexcept { return *model_; }

 private:
  static constexpr double kEps = 1e-20;
  VonKarmanCymbalVoice(SampleRateHz rate, MalletStrike strike, std::shared_ptr<const VonKarmanPlateModel> model,
                       bool nonlinear)
      : rate_(rate), strike_(strike), model_(std::move(model)), nonlinear_(nonlinear),
        stickV_(strike.speedMetersPerSecond) {}
  SampleRateHz rate_;
  MalletStrike strike_;
  std::shared_ptr<const VonKarmanPlateModel> model_;
  bool nonlinear_ = true;
  double lastV_ = 0.0;
  bool accelerationOut_ = true;
  int oversample_ = 1;
  double stepRate_ = 0.0;
  std::vector<double> fir_, hist_;
  std::size_t histPos_ = 0;
  double lastVelocity_ = 0.0;
  std::vector<double> a_, b_, d_, phiS_, phiO_, q_, qPrev_, r_, g_, eta_;
  std::vector<bool> active_;
  double psi_ = 0.0;
  double stickY_ = 0.0;
  double stickV_ = 0.0;
};

}  // namespace mmathen
