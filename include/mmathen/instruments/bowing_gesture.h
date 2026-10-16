#pragma once
// Bowing-gesture model: the player's three controls (bow speed, bow force,
// bow-bridge distance beta) coordinated per note from a dynamic in [0, 1].
//
// Sourced shape (values VERIFY against the primary papers):
//  - Players change dynamics mainly with bow speed and bow-bridge distance,
//    louder = faster and closer to the bridge, with force raised to stay in
//    the playable (Schelleng) range: Askenfelt, JASA 80, 1986 and JASA 86,
//    1989; Schoonderwaldt & Demoucron, JASA 126, 2009 (measured violin
//    bowing: beta about 0.05-0.2, bow speed about 0.05-1 m/s).
//  - Helmholtz amplitude ~ v / beta (Cremer, "Physics of the Violin", ch. 3),
//    so the level change in dB is split between speed and beta.
// UNSOURCED (marked): the 0.6/0.4 speed/beta split of the level change, the
// geometric force interpolation p->f, the 0.9 Schelleng cap, and the contact-point
// drift (seeded Ornstein-Uhlenbeck, tau 1.5 s, 8 % of beta) that varies on
// its own timescale independent of dynamics.
#include <algorithm>
#include <cmath>
#include <cstdint>

#include "bowed_string.h"

namespace mmathen {

struct BowingGestureParams {
  double speedPiano = 0.08, speedForte = 0.5;           // m/s, Askenfelt 1989 range VERIFY
  // Measured detache on a violin: bow-bridge distance 30-50 mm piano,
  // 20-30 mm mezzo, 10-20 mm forte; bow force ~0.5 N piano rising to ~2 N
  // forte. A. Askenfelt, "Measurement of the bowing parameters in violin
  // playing", STL-QPSR 29(1), 1-30 (1988), sec. 1 / Fig. 3 (open G, two
  // players); also JASA 86, 503 (1989). beta = distance / 325 mm string
  // (standard violin vibrating length): 40 mm -> 0.123, 15 mm -> 0.046
  // (mid-range values).
  double betaPiano = 0.123, betaForte = 0.046;          // Askenfelt 1988 measured
  double forceNewtonsPiano = 0.5, forceNewtonsForte = 2.0;  // Askenfelt 1988 measured
  // The model's Helmholtz motion needs F below Schelleng's maximum; the
  // measured force is capped at this fraction of it (players stay inside
  // the playable range, Askenfelt 1989 "safety margins"). 0.9 UNSOURCED.
  double forceCapOfSchellengMax = 0.9;
  double forceFracPiano = 0.15, forceFracForte = 0.35;  // legacy, used only if forceNewtons* <= 0; UNSOURCED
  double speedShare = 0.6;                              // UNSOURCED
  double driftTauSeconds = 1.5, driftFraction = 0.08;   // UNSOURCED
  // Contact point follows the dynamic with a lag (players move the bow
  // gradually toward/away from the bridge; Schoonderwaldt 2009 VERIFY).
  double betaLagSeconds = 0.0;                          // 0 = per-note jump; UNSOURCED value
};

class BowingGestureModel {
 public:
  explicit BowingGestureModel(std::uint64_t seed, BowingGestureParams p = {}) : p_(p), s_(seed ? seed : 1) {}
  // Stroke for a note of `seconds` at dynamic d (0 = pp, 1 = ff).
  BowStroke strokeForNote(const StringCourse& course, double d, double seconds) {
    d = std::clamp(d, 0.0, 1.0);
    const double sMax = 20.0 * std::log10(p_.speedForte / p_.speedPiano);
    const double bMax = 20.0 * std::log10(p_.betaPiano / p_.betaForte);
    const double span = sMax + bMax;
    const double dbSpeed = d * span * p_.speedShare, dbBeta = d * span * (1.0 - p_.speedShare);
    const double speed = p_.speedPiano * std::pow(p_.speedForte / p_.speedPiano, std::min(1.0, dbSpeed / sMax));
    double beta = p_.betaPiano * std::pow(p_.betaForte / p_.betaPiano, std::min(1.0, dbBeta / bMax));
    if (p_.betaLagSeconds > 0.0) {
      if (betaState_ <= 0.0) betaState_ = beta;
      const double k = std::exp(-std::max(seconds, 0.0) / p_.betaLagSeconds);
      betaState_ = k * betaState_ + (1.0 - k) * beta;
      beta = betaState_;
    }
    // Contact-point drift on its own timescale (independent of d).
    const double a = std::exp(-std::max(seconds, 0.0) / p_.driftTauSeconds);
    drift_ = a * drift_ + std::sqrt(1.0 - a * a) * p_.driftFraction * gauss();
    beta = std::clamp(beta * (1.0 + drift_), 0.04, 0.25);
    BowStroke s = violinBowStroke(course, beta, speed);
    if (p_.forceNewtonsPiano > 0.0 && p_.forceNewtonsForte > 0.0) {
      const double maxF = s.normalForceNewtons / kViolinBowForceFraction;
      const double f = p_.forceNewtonsPiano * std::pow(p_.forceNewtonsForte / p_.forceNewtonsPiano, d);
      s.normalForceNewtons = std::min(f, p_.forceCapOfSchellengMax * maxF);
    } else {
      const double frac = p_.forceFracPiano + d * (p_.forceFracForte - p_.forceFracPiano);
      s.normalForceNewtons *= frac / kViolinBowForceFraction;
    }
    return s;
  }
  double drift() const noexcept { return drift_; }

 private:
  double uniform() noexcept {
    s_ ^= s_ << 13; s_ ^= s_ >> 7; s_ ^= s_ << 17;
    return (static_cast<double>(s_ >> 11) + 0.5) * (1.0 / 9007199254740992.0);
  }
  double gauss() noexcept { return std::sqrt(-2.0 * std::log(uniform())) * std::cos(6.283185307179586 * uniform()); }
  BowingGestureParams p_;
  std::uint64_t s_;
  double drift_ = 0.0;
  double betaState_ = 0.0;
};

}  // namespace mmathen
