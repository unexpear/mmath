#pragma once

// Nonlinear plane-wave steepening along a duct: the inviscid Burgers
// equation in retarded time,
//   dp/dx - a p dp/dtau = 0,   a = beta / (rho c^3),  beta = (gamma+1)/2,
// whose solution p(x, tau) = p(0, tau + a p x) advances high-pressure
// samples (Hirschberg, Gilbert, Msallam and Wijnands, "Shock waves in
// trombones", JASA 99(3), 1754-1758, 1996, eq. 2 and the shock distance
// x_s = 1 / (a max dp/dtau)). The bore's flare enters through an
// effective length L_eff = int D0/D dx (Myers, Pyle, Gilbert, Campbell,
// Chick and Logie, "Effects of nonlinear sound propagation on the
// characteristic timbres of brass instruments", JASA 131(1), 678-688,
// 2012), passed in by the caller.
//
// Scheme. x is marched in M equal steps; each step is a conservative
// Godunov update in tau of the flux F(p) = -a p^2 / 2 (concave), so a
// shock forms and travels at the Rankine-Hugoniot speed instead of
// folding over, and the scheme is total-variation diminishing (no new
// extrema). Streaming: a step needs the next tau sample, so each step
// delays its output one sample; the line delays M samples overall. The
// Courant number a |p| dx / dtau is held at or below 1 by clamping the
// characteristic speed (reported by clampCount()); with M = 32 over
// 1.7 m at 48 kHz that bound is about 15 kPa. Wall loss along the way is
// not modelled, so the steepening is an upper bound (as Myers et al.).

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace mmathen {

class BurgersLine {
 public:
  // lengthM is L_eff; stages M >= 1; rate in Hz; rho, c the air.
  bool configure(double lengthM, int stages, double rateHz, double rho, double c,
                 double gamma = 1.4) {
    if (!(lengthM > 0.0) || stages < 1 || !(rateHz > 0.0) || !(rho > 0.0) || !(c > 0.0) ||
        !std::isfinite(lengthM))
      return false;
    a_ = 0.5 * (gamma + 1.0) / (rho * c * c * c);
    lambda_ = (lengthM / stages) * rateHz;  // dx / dtau
    stage_.assign(static_cast<std::size_t>(stages), Stage{});
    clamps_ = 0;
    return true;
  }
  // New L_eff (a note's length change) without clearing the wave.
  bool setLength(double lengthM, double rateHz) {
    if (!(lengthM > 0.0) || !std::isfinite(lengthM) || stage_.empty() || !(rateHz > 0.0)) return false;
    lambda_ = (lengthM / static_cast<double>(stage_.size())) * rateHz;
    return true;
  }
  void reset() {
    for (Stage& s : stage_) s = Stage{};
    clamps_ = 0;
  }
  [[nodiscard]] int stages() const noexcept { return static_cast<int>(stage_.size()); }
  [[nodiscard]] std::uint64_t clampCount() const noexcept { return clamps_; }
  // Pressure (Pa) at which the Courant number reaches 1.
  [[nodiscard]] double courantLimitPa() const noexcept { return 1.0 / (a_ * lambda_); }

  // Push one input sample; returns the sample M steps (and M samples) on.
  double step(double p) {
    double v = p;
    for (Stage& s : stage_) v = s.push(v, *this);
    return v;
  }

 private:
  // Godunov flux of the concave F(u) = -a u^2/2 with the speed clamp.
  [[nodiscard]] double flux(double uL, double uR) {
    const double lim = courantLimitPa();
    if (std::fabs(uL) > lim || std::fabs(uR) > lim) ++clamps_;
    uL = std::clamp(uL, -lim, lim);
    uR = std::clamp(uR, -lim, lim);
    auto F = [&](double u) { return -0.5 * a_ * u * u; };
    if (uL <= uR) return std::min(F(uL), F(uR));
    if (uR <= 0.0 && 0.0 <= uL) return 0.0;
    return std::max(F(uL), F(uR));
  }
  struct Stage {
    double prev = 0.0, cur = 0.0;  // u_{i-1}, u_i
    double push(double next, BurgersLine& line) {
      const double out = cur - line.lambda_ * (line.flux(cur, next) - line.flux(prev, cur));
      prev = cur;
      cur = next;
      return out;
    }
  };
  std::vector<Stage> stage_;
  double a_ = 0.0, lambda_ = 0.0;
  std::uint64_t clamps_ = 0;
};

}  // namespace mmathen
