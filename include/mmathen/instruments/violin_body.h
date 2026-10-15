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
// Hill damping from Woodhouse 2005 eq. (19): zeta_b = Re(R_inf) * I * Omega_b / 2, with the rotational body admittance Re(R_inf) ~ 100 rad m^-1 s^-1 N^-1 (Woodhouse 2005, value used for a ~2.5 kHz hill) and bridge moment of inertia I = 4.3e-7 kg m^2 (euphonics 7.5 fit). Q = 1/(2 zeta_b) ~ 1.28.
inline constexpr double kWoodhouseRotationalAdmittance = 100.0;   // Woodhouse 2005 (Acta Acustica 91)
inline constexpr double kViolinBridgeInertiaKgM2 = 4.3e-7;          // euphonics 7.5 fit
[[nodiscard]] inline double violinBridgeHillQ(double rockingHz = kViolinBridgeRockingHz) {
  const double zeta = kWoodhouseRotationalAdmittance * kViolinBridgeInertiaKgM2 * constants::twoPi * rockingHz / 2.0;
  return 1.0 / (2.0 * zeta);
}
inline const double kViolinBridgeHillQ = violinBridgeHillQ();
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

// Low-mid signature modes. Frequencies are MEASURED means (Bissinger 2008,
// JASA 124, 17 violins). Q per mode from wood internal loss + radiation
// damping: tan delta_wood ~ 0.008 for resonance spruce along grain
// (Brémaud, JASA 2012, typical; VERIFY vs her full table), Q_int = 1/tan delta.
// Strong radiators (A0, B1-, B1+) take FRAD ~ 0.5 so Q_tot = Q_int/2
// (Bissinger, ARL Online 2004 / JASA 2008: radiation damping comparable to
// internal for those modes); CBR radiates weakly (Bissinger 2008), FRAD ~ 0,
// so Q_tot ~ Q_int. Relative gains follow radiation ranking: A0 and B1 strong,
// CBR weak (Bissinger 2008). Overall scale kViolinSignatureGainScale is TUNED
// (chosen so dry 400-500 Hz share moves toward the real recording).
inline constexpr double kViolinSignatureHz[4] = {275.0, 407.0, 475.0, 541.0};  // A0, CBR, B1-, B1+
inline constexpr double kViolinWoodTanDelta = 0.008;  // Brémaud 2012 spruce VERIFY
inline constexpr double kViolinSignatureFrad[4] = {0.5, 0.05, 0.5, 0.5};  // A0,CBR,B1-,B1+; CBR weak Bissinger 2008
inline constexpr double kViolinSignatureRelGain[4] = {1.0, 0.25, 1.2, 1.2};  // radiation ranking; CBR scale UNSOURCED
inline constexpr double kViolinSignatureGainScale = 8.0;  // TUNED
// Bandpass-EQ is not a true modal radiator: physics Q (~60) is too narrow to
// fill 400-500 Hz. Scale Q down toward the old TUNED Q 15 so band energy
// recovers while keeping Bremaud/Bissinger relative Q ranking. TUNED.
inline constexpr double kViolinSignatureQScale = 0.35;  // TUNED (~0.35*62.5 ~ 22)
[[nodiscard]] inline double violinSignatureQ(int mode) {
  const double qInt = 1.0 / kViolinWoodTanDelta;
  const double frad = kViolinSignatureFrad[mode];
  return qInt * (1.0 - frad);  // Q_tot = Q_int * (1 - FRAD), FRAD = zeta_rad / zeta_tot
}

class SignatureModeFilter {
 public:
  explicit SignatureModeFilter(double sampleRate, double gainScale = kViolinSignatureGainScale) {
    for (int m = 0; m < 4; ++m) {
      const double q = std::max(8.0, violinSignatureQ(m) * kViolinSignatureQScale);
      g_[m] = gainScale * kViolinSignatureRelGain[m];
      const double w = constants::twoPi * kViolinSignatureHz[m];
      const double k = w / std::tan(w / (2.0 * sampleRate));
      const double a0 = k * k + k * w / q + w * w;
      b0_[m] = k * w / q / a0; b2_[m] = -b0_[m];
      a1_[m] = (2.0 * w * w - 2.0 * k * k) / a0; a2_[m] = (k * k - k * w / q + w * w) / a0;
    }
  }
  double process(double x) noexcept {
    double y = x;
    for (int m = 0; m < 4; ++m) {
      const double v = b0_[m] * x + s1_[m];
      s1_[m] = -a1_[m] * v + s2_[m];
      s2_[m] = b2_[m] * x - a2_[m] * v;
      y += g_[m] * v;
    }
    return y;
  }
  void processInPlace(std::vector<double>& v) noexcept { for (double& x : v) x = process(x); }
 private:
  double g_[4]{};
  double b0_[4]{}, b2_[4]{}, a1_[4]{}, a2_[4]{}, s1_[4]{}, s2_[4]{};
};

// Below A0 the f-hole air flow is in antiphase with the plate volume
// velocity, so the net monopole radiation falls ~12 dB/octave (Gough,
// "Science of the violin", sec. A0; Weinreich 1997). Modelled as a 2nd-order
// high-pass. The 12 dB/oct order is sourced; the corner is TUNED to 200 Hz
// (just above open G, 196 Hz): at A0 itself (275 Hz) it cut the G-string
// fundamentals 3 dB below the recording. Q 0.707 is UNSOURCED.
inline constexpr double kViolinRadiationCornerHz = 200.0;  // TUNED
class RadiationHighpass {
 public:
  explicit RadiationHighpass(double sampleRate, double cornerHz = kViolinRadiationCornerHz, double q = 0.70710678) {
    const double w = constants::twoPi * cornerHz, k = w / std::tan(w / (2.0 * sampleRate));
    const double a0 = k * k + k * w / q + w * w;
    b0_ = k * k / a0; a1_ = (2.0 * w * w - 2.0 * k * k) / a0; a2_ = (k * k - k * w / q + w * w) / a0;
  }
  double process(double x) noexcept {
    const double y = b0_ * x + s1_; s1_ = -2.0 * b0_ * x - a1_ * y + s2_; s2_ = b0_ * x - a2_ * y; return y;
  }
  void processInPlace(std::vector<double>& v) noexcept { for (double& x : v) x = process(x); }
 private:
  double b0_, a1_, a2_, s1_ = 0, s2_ = 0;
};

}  // namespace mmathen
