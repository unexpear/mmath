#pragma once
// Violin body, stage 1: construction-physics pieces that do not need a
// measured modal table.
//  - Bridge rocking resonance (bridge hill). Woodhouse, "On the bridge hill
//    of the violin", Acta Acustica 91 (2005): the bridge top rocks on the
//    island; with the feet on the body the string-to-body force transfer is
//    a damped mass-spring resonance near 2-3 kHz and falls ~12 dB/octave
//    above it. Fitted rocking frequency 2.9 kHz (Woodhouse, euphonics.org
//    section 7.5 example fit); effective top mass ~1.2 g (Jansson, "On
//    tuning of the violin bridge", from a 10 % shift for 0.26 g added).
//  - Helmholtz air resonance A0 of the cavity through the f-holes
//    (Rayleigh): f = c/(2 pi) sqrt(A / (V L')), lowered by wall compliance
//    in a real violin (Bissinger 2008 measured mean 275 Hz).
// Corpus modes (CBR, B1-, B1+) are NOT here: a flat orthotropic-plate
// estimate gives ~100 Hz for the top's first mode, far below the measured
// 400-600 Hz, because arching dominates; an arched-shell model is stage 2.
#include <cmath>
#include <vector>
#include "../math/constants.h"

namespace mmathen {

inline constexpr double kViolinBridgeRockingHz = 2900.0;  // Woodhouse euphonics 7.5 fit (one violin)
inline constexpr double kViolinBridgeHillQ = 1.5;         // UNSOURCED (body loading; chosen so 2-3 kHz band ~ real)
inline constexpr double kViolinBridgeEffectiveMassKg = 1.2e-3;  // Jansson bridge-tuning paper (estimate)

// Rayleigh Helmholtz frequency with end-corrected neck length.
[[nodiscard]] inline double helmholtzFrequencyHz(double volumeM3, double areaM2, double effectiveLengthM,
                                                 double soundSpeed = 343.0) {
  if (!(volumeM3 > 0.0) || !(areaM2 > 0.0) || !(effectiveLengthM > 0.0)) return 0.0;
  return soundSpeed / constants::twoPi * std::sqrt(areaM2 / (volumeM3 * effectiveLengthM));
}

// Second-order bridge-hill transfer (string force -> body force), bilinear
// with prewarping at the resonance. Unity gain at DC.
class BridgeHillFilter {
 public:
  BridgeHillFilter(double sampleRate, double rockingHz = kViolinBridgeRockingHz, double q = kViolinBridgeHillQ) {
    const double w = constants::twoPi * rockingHz;
    const double k = w / std::tan(w / (2.0 * sampleRate));
    const double a0 = k * k + k * w / q + w * w;
    b0_ = w * w / a0; b1_ = 2.0 * b0_; b2_ = b0_;
    a1_ = (2.0 * w * w - 2.0 * k * k) / a0;
    a2_ = (k * k - k * w / q + w * w) / a0;
  }
  double process(double x) noexcept {
    const double y = b0_ * x + s1_;
    s1_ = b1_ * x - a1_ * y + s2_;
    s2_ = b2_ * x - a2_ * y;
    return y;
  }
  void processInPlace(std::vector<double>& v) noexcept { for (double& x : v) x = process(x); }
 private:
  double b0_ = 1, b1_ = 0, b2_ = 0, a1_ = 0, a2_ = 0, s1_ = 0, s2_ = 0;
};

}  // namespace mmathen
