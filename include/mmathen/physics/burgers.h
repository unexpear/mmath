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
// 1.7 m at 48 kHz that bound is about 15 kPa. Wall loss (setStageLoss) is
// a one-pole low-pass and flat gain applied after every step, so the loss
// of the duct is spread along the march and acts on the harmonics as the
// steepening makes them (the generalized Burgers equation's split step).

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
  // Per-step wall loss: (1-p)/(1-p z^-1) times gain; p = 0, gain 1 is none.
  bool setStageLoss(double pole, double gain) {
    if (!(pole >= 0.0) || !(pole < 1.0) || !(gain > 0.0) || !(gain <= 1.0)) return false;
    lossPole_ = pole;
    lossGain_ = gain;
    lossAt_ = -1;
    lossLumps_ = 1;
    return true;
  }
  // The duct's whole wall loss as one filter after step `atStage` (0 is the
  // first step), the other steps lossless. A one-pole's M-th root is not a
  // one-pole: M equal one-poles matched at one frequency fall off far
  // faster above it (pole 0.8, M 32, matched at 1 kHz: -10.7 dB at 4 kHz,
  // -47 dB at 8 kHz re the single pole). Lumped, the linear magnitude is
  // the duct's own at every frequency.
  bool setLumpedLoss(double pole, double gain, int atStage) {
    if (!setStageLoss(pole, gain) || atStage < 0 || atStage >= stages()) return false;
    lossAt_ = atStage;
    lossLumps_ = 1;
    return true;
  }
  // The loss as `lumps` equal filters after evenly spaced steps (the
  // middle of each of `lumps` equal sections); pole and gain are one
  // lump's. lumps = 1 is setLumpedLoss at the middle step.
  bool setSectionLoss(double pole, double gain, int lumps) {
    if (!setStageLoss(pole, gain) || lumps < 1 || lumps > stages()) return false;
    lossLumps_ = lumps;
    lossAt_ = stages() / (2 * lumps);
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
    for (std::size_t i = 0; i < stage_.size(); ++i)
      v = stage_[i].push(v, *this, lossy(static_cast<int>(i)));
    return v;
  }

 private:
  [[nodiscard]] bool lossy(int i) const noexcept {
    if (lossAt_ < 0) return true;
    const int section = stages() / lossLumps_;
    return i % section == lossAt_ && i / section < lossLumps_;
  }
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
    double lp = 0.0;
    double push(double next, BurgersLine& line, bool lossy) {
      double out = cur - line.lambda_ * (line.flux(cur, next) - line.flux(prev, cur));
      prev = cur;
      cur = next;
      if (!lossy) return out;
      if (line.lossPole_ > 0.0) {
        lp = (1.0 - line.lossPole_) * out + line.lossPole_ * lp;
        out = lp;
      }
      return line.lossGain_ * out;
    }
  };
  std::vector<Stage> stage_;
  double a_ = 0.0, lambda_ = 0.0, lossPole_ = 0.0, lossGain_ = 1.0;
  int lossAt_ = -1, lossLumps_ = 1;
  std::uint64_t clamps_ = 0;
};

}  // namespace mmathen
