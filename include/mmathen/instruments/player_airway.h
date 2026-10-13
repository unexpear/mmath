#pragma once

// The player's side of a wind instrument: lungs, subglottal airway,
// glottis, vocal tract, tongue and the soft walls of the tract. This is
// the upstream half of the reed, lip or jet. The bore voices
// (bore_valve.h, cone_valve.h) read the mouth pressure from here each
// sample and hand back the volume flow that left the mouth through the
// valve, one sample later.
//
// Air column. A Kelly-Lochbaum waveguide of plane-wave sections, one
// sample of travel per section each way, so a section is c/fs long
// (7.15 mm at 48 kHz, 343 m/s). From the lungs:
//   - subglottal airway: a rigid uniform pipe 20 cm long with the
//     tract's radius, open at its far end (Boutin, Fletcher, Smith and
//     Wolfe, JASA 137, 1195-1209, 2015, after Lulich et al. 2011). The
//     open end is a pressure reflection of -1.
//   - glottis: 2 cm long including end effects (Boutin et al. 2015).
//     Its area is an argument. Experienced players play with a small
//     glottal opening (Mukai 1992, cited by Chen, Smith and Wolfe,
//     Science 319, 776, 2008); no area is printed there, so the default
//     below is UNSOURCED.
//   - oral tract: 17 cm, the adult vocal tract length of Fant (Acoustic
//     Theory of Speech Production, 1960). Its uniform area is the one
//     whose air compliance V/(rho c^2) equals the tract compliance
//     Cair = 7e-10 m^3/Pa fitted by Hanna et al. (2012) and quoted by
//     Boutin et al. (2015): A = rho c^2 Cair / L.
//   - tongue body: an optional constriction (position, area). That is
//     the player's tract shape (an "ee" against an "aw"). Chen, Smith
//     and Wolfe (JASA 126, 1511-1520, 2009) measured clarinet tract
//     peaks of a few tens of MPa s m^-3, comparable to the bore, when
//     players bend pitch. The constriction areas are arguments.
// Junction reflection for pressure waves from area A1 into A2 is
// k = (A1 - A2)/(A1 + A2): Kelly and Lochbaum (1962).
// Viscothermal loss: Kirchhoff's plane-wave attenuation on each
// section's radius, evaluated at lossMatchHz (an argument).
//
// Walls. Hanna et al. (2012), quoted by Boutin et al. (2015): the soft
// tract wall is an inertance Lt = 900 kg m^-4 in series with a
// compliance Ct = 7e-8 m^3/Pa, a shunt at the mouth (the tract is
// compact against the wavelength there). Neither paper prints the wall
// resistance. It is taken from the wall's own resistance-to-mass ratio:
// Ishizaka, French and Flanagan (IEEE Trans. ASSP 23, 370-373, 1975),
// relaxed cheek, per unit area Lw = 2.1 g/cm^2, Rw = 800 g cm^-2 s^-1
// (as tabulated by Wakita and Fant, STL-QPSR 19(1), 9-29, 1978). The
// ratio Rw/Lw = 381 s^-1 is area-free, so R = Lt Rw/Lw = 3.4e5 Pa s/m^3,
// 1.5 times critical for the Hanna pair.
//
// Mouth boundary. The valve draws the volume flow U. The steady part of
// that flow is the breath; the waveguide carries only U minus its
// running mean (corner meanFlowCornerHz, an UNSOURCED numerical
// separation), so the static mouth pressure is the lung pressure and the
// acoustic part is the tract's answer to the oscillating flow. With the
// wall flow u_w, p = 2 f - Z (U_ac + u_w), Z = rho c / A at the mouth.
//
// Tongue. The tongue tip on the reed (or behind the teeth for a lip or
// a flute) is a contact fraction from 0 (shut) to 1 (free) that
// multiplies the valve's opening. It moves on a critically damped
// second-order path toward its target, x'' = w^2 (x* - x) - 2 w x'.
// Li, Almeida, Smith and Wolfe (JASA 140, 1089-1100, 2016) filmed a
// clarinet reed return from the tongue to its equilibrium in about 8 ms
// when the channel had been shut. A critically damped 10-90 % rise is
// 3.36/w, so w = 3.36 / 0.008 s. Closing uses the same w (UNSOURCED
// symmetry).
//
// Lungs. Volume drains by the steady flow. Elastic recoil is
// (V - V_relax) / C_rs with C_rs = 1e-6 m^3/Pa (0.1 L/cmH2O, the total
// respiratory-system compliance of an adult; West, Respiratory
// Physiology, 9th ed., 2012). The player's expiratory muscles make up
// the difference to the target pressure, so the delivered pressure is
// the target until the lungs reach residual volume, where the breath
// runs out and the delivered pressure falls to the (negative) recoil.
// Volumes: total lung capacity 6.0 L, residual volume 1.2 L,
// relaxation volume (FRC) 2.4 L, adult male (West 2012). A phrase
// breath refills at inspirationRate (UNSOURCED, 1 L/s).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "../math/constants.h"

namespace mmathen {

struct PlayerAirwaySpec {
  double speedMetersPerSecond = 343.2;
  double densityKgPerM3 = 1.204;
  double sampleRateHz = 48000.0;
  // Fant 1960.
  double oralLengthMeters = 0.17;
  // Hanna et al. 2012 via Boutin et al. 2015.
  double tractComplianceM3PerPa = 7.0e-10;
  // Boutin et al. 2015 (Lulich et al. 2011).
  double subglottalLengthMeters = 0.20;
  // Boutin et al. 2015, including end effects.
  double glottisLengthMeters = 0.02;
  // UNSOURCED: "a small glottal opening" (Mukai 1992) prints no area.
  double glottisAreaM2 = 1.0e-5;
  // Hanna et al. 2012 via Boutin et al. 2015.
  double wallInertanceKgPerM4 = 900.0;
  double wallComplianceM3PerPa = 7.0e-8;
  // Wall resistance over wall inertance, s^-1: Ishizaka et al. (1975),
  // relaxed cheek, 800 / 2.1.
  double wallResistancePerInertance = 800.0 / 2.1;
  // Kirchhoff loss is fitted at this frequency (argument).
  double lossMatchHz = 500.0;
  // Tract loss above smooth-rigid-tube Kirchhoff. Hanna, Smith and Wolfe
  // (JASA 139, 2924-2936, 2016) measured through the lips with the glottis
  // closed: anti-resonance bandwidths of about 50-90 Hz for men, rising
  // about 1 Hz per 100 Hz from 300 Hz, wider than a smooth dry rigid
  // cylinder. At the 1048 Hz closed-tract peak that is 50 + 7.5 = 57 Hz.
  // A multiplier of 9 gives a 58 Hz -3 dB width at 1048 Hz (1 Hz sweep of
  // inputImpedanceMagnitude; peak 29.4 -> 7.3 MPa s m^-3). Fitted to that
  // measurement; the single multiplier for all frequencies is UNSOURCED.
  double tractLossScale = 9.0;
  // Li et al. 2016: reed back at equilibrium about 8 ms after release.
  double tongueReleaseSeconds = 0.008;
  // UNSOURCED numerical split of breath and acoustic flow.
  double meanFlowCornerHz = 20.0;
  // West 2012, adult male.
  double totalLungCapacityM3 = 6.0e-3;
  double residualVolumeM3 = 1.2e-3;
  double relaxationVolumeM3 = 2.4e-3;
  double respiratoryComplianceM3PerPa = 1.0e-6;
  // UNSOURCED.
  double inspirationRateM3PerS = 1.0e-3;
  // Starting lung volume: a full breath (argument).
  double initialLungVolumeM3 = 5.5e-3;
};

class PlayerAirway {
 public:
  explicit PlayerAirway(const PlayerAirwaySpec& spec) : spec_(spec) { build(); }

  // Oral tract area from Hanna's compliance and Fant's length.
  [[nodiscard]] double oralAreaM2() const { return oralArea_; }
  [[nodiscard]] int sectionCount() const { return static_cast<int>(area_.size()); }
  [[nodiscard]] int mouthSection() const { return static_cast<int>(area_.size()) - 1; }

  // Tongue-body constriction: position as a fraction of the oral tract
  // from the glottis (0..1), area in m^2, length in metres. Area at or
  // above the oral area removes it. The shape is applied at once; the
  // waves keep travelling.
  bool setTongueBody(double positionFraction, double areaM2, double lengthMeters = 0.02) {
    if (!std::isfinite(positionFraction) || positionFraction < 0.0 || positionFraction > 1.0 ||
        !std::isfinite(areaM2) || !(areaM2 > 0.0) || !std::isfinite(lengthMeters) ||
        !(lengthMeters > 0.0))
      return false;
    for (int i = oralBegin_; i < static_cast<int>(area_.size()); ++i) area_[i] = oralArea_;
    const double dx = sectionLength_;
    const int oralCount = static_cast<int>(area_.size()) - oralBegin_;
    const int center = oralBegin_ + static_cast<int>(std::lround(positionFraction * (oralCount - 1)));
    const int half = std::max(0, static_cast<int>(std::lround(0.5 * lengthMeters / dx)));
    for (int i = center - half; i <= center + half; ++i) {
      if (i < oralBegin_ || i >= static_cast<int>(area_.size())) continue;
      area_[i] = std::min(oralArea_, areaM2);
    }
    refreshJunctions();
    return true;
  }

  // Tongue tip target: 0 is on the reed (shut), 1 is free.
  void setTongueTarget(double target) { tongueTarget_ = std::clamp(target, 0.0, 1.0); }
  // Put the tongue at a position now (start of a render).
  void placeTongue(double position) {
    tongue_ = std::clamp(position, 0.0, 1.0);
    tongueVelocity_ = 0.0;
    tongueTarget_ = tongue_;
  }
  [[nodiscard]] double tongueOpenFraction() const { return tongue_; }

  // Mouth pressure for this sample, Pa, given the player's target lung
  // pressure. Zero target is no breath: zero mouth pressure.
  [[nodiscard]] double mouthPressure(double targetLungPa) {
    target_ = targetLungPa;
    if (!(targetLungPa > 0.0)) {
      lastMouth_ = 0.0;
      return 0.0;
    }
    const double delivered = deliveredLungPressure(targetLungPa);
    double mouth = delivered + acousticMouth_;
    if (!std::isfinite(mouth) || mouth < 0.0) mouth = 0.0;
    lastMouth_ = mouth;
    return mouth;
  }

  // Advance one sample. flowOut is the volume flow (m^3/s) that left the
  // mouth through the valve this sample.
  void step(double flowOut) {
    if (!std::isfinite(flowOut)) flowOut = 0.0;
    const double dt = 1.0 / spec_.sampleRateHz;
    // Breath: steady part of the flow drains the lungs; a rest refills.
    meanFlow_ += meanAlpha_ * (flowOut - meanFlow_);
    if (target_ > 0.0) {
      lungVolume_ -= std::max(0.0, flowOut) * dt;
    } else {
      lungVolume_ += spec_.inspirationRateM3PerS * dt;
    }
    lungVolume_ = std::clamp(lungVolume_, spec_.residualVolumeM3 * 0.98, spec_.totalLungCapacityM3);
    // Tongue: critically damped.
    const double w = tongueOmega_;
    const double accel = w * w * (tongueTarget_ - tongue_) - 2.0 * w * tongueVelocity_;
    tongueVelocity_ += accel * dt;
    tongue_ += tongueVelocity_ * dt;
    if (tongue_ < 0.0) {
      tongue_ = 0.0;
      if (tongueVelocity_ < 0.0) tongueVelocity_ = 0.0;
    } else if (tongue_ > 1.0) {
      tongue_ = 1.0;
      if (tongueVelocity_ > 0.0) tongueVelocity_ = 0.0;
    }
    // Waveguide.
    const double acousticFlow = flowOut - meanFlow_;
    const int n = static_cast<int>(area_.size());
    // Scatter at every junction from last sample's waves.
    for (int i = 0; i + 1 < n; ++i) {
      const double k = junction_[i];
      const double fIn = right_[i] * loss_[i];
      const double bIn = left_[i + 1] * loss_[i + 1];
      nextRight_[i + 1] = (1.0 + k) * fIn - k * bIn;
      nextLeft_[i] = k * fIn + (1.0 - k) * bIn;
    }
    // Lung end: open, -1.
    nextRight_[0] = -left_[0] * loss_[0];
    // Mouth end: valve flow plus the wall shunt, solved implicitly.
    const double f = right_[n - 1] * loss_[n - 1];
    const double z = spec_.densityKgPerM3 * spec_.speedMetersPerSecond / area_[n - 1];
    const double lt = spec_.wallInertanceKgPerM4;
    const double ct = spec_.wallComplianceM3PerPa;
    const double denom = lt / dt + wallR_ + dt / ct + z;
    const double uw = (2.0 * f - z * acousticFlow + lt * wallFlow_ / dt - wallVolume_ / ct) / denom;
    wallFlow_ = std::isfinite(uw) ? uw : 0.0;
    wallVolume_ += wallFlow_ * dt;
    const double b = f - z * (acousticFlow + wallFlow_);
    nextLeft_[n - 1] = std::isfinite(b) ? b : 0.0;
    right_.swap(nextRight_);
    left_.swap(nextLeft_);
    acousticMouth_ = right_[n - 1] + left_[n - 1];
    if (!std::isfinite(acousticMouth_)) acousticMouth_ = 0.0;
  }

  [[nodiscard]] double lungVolumeM3() const { return lungVolume_; }
  [[nodiscard]] double acousticMouthPa() const { return acousticMouth_; }
  [[nodiscard]] double recoilPa() const {
    return (lungVolume_ - spec_.relaxationVolumeM3) / spec_.respiratoryComplianceM3PerPa;
  }

  // Delivered lung pressure: the target while the muscles can make up
  // the recoil, the recoil alone at residual volume.
  [[nodiscard]] double deliveredLungPressure(double targetPa) const {
    if (lungVolume_ <= spec_.residualVolumeM3) return std::min(targetPa, recoilPa());
    return targetPa;
  }

  // Pressure at the mouth end of the tract for a steady sinusoidal flow
  // of frequencyHz, by driving the waveguide (test helper). Returns
  // |p|/|U| in Pa s m^-3 after settling.
  [[nodiscard]] static double inputImpedanceMagnitude(PlayerAirway airway, double frequencyHz,
                                                      double seconds = 0.4) {
    const double fs = airway.spec_.sampleRateHz;
    const auto total = static_cast<std::size_t>(seconds * fs);
    const std::size_t settle = total / 2;
    double sumP = 0.0, sumU = 0.0;
    airway.target_ = 1.0;
    airway.meanAlpha_ = 0.0;
    for (std::size_t i = 0; i < total; ++i) {
      const double u = 1.0e-6 * std::sin(constants::twoPi * frequencyHz * static_cast<double>(i) / fs);
      airway.step(u);
      if (i >= settle) {
        sumP += airway.acousticMouth_ * airway.acousticMouth_;
        sumU += u * u;
      }
    }
    return sumU > 0.0 ? std::sqrt(sumP / sumU) : 0.0;
  }

 private:
  void build() {
    const double c = spec_.speedMetersPerSecond;
    const double rho = spec_.densityKgPerM3;
    sectionLength_ = c / spec_.sampleRateHz;
    oralArea_ = rho * c * c * spec_.tractComplianceM3PerPa / spec_.oralLengthMeters;
    const int sub = std::max(1, static_cast<int>(std::lround(spec_.subglottalLengthMeters / sectionLength_)));
    const int glot = std::max(1, static_cast<int>(std::lround(spec_.glottisLengthMeters / sectionLength_)));
    const int oral = std::max(2, static_cast<int>(std::lround(spec_.oralLengthMeters / sectionLength_)));
    area_.assign(static_cast<std::size_t>(sub + glot + oral), oralArea_);
    for (int i = sub; i < sub + glot; ++i) area_[i] = spec_.glottisAreaM2;
    oralBegin_ = sub + glot;
    right_.assign(area_.size(), 0.0);
    left_.assign(area_.size(), 0.0);
    nextRight_.assign(area_.size(), 0.0);
    nextLeft_.assign(area_.size(), 0.0);
    refreshJunctions();
    tongueOmega_ = 3.36 / spec_.tongueReleaseSeconds;
    meanAlpha_ = 1.0 - std::exp(-constants::twoPi * spec_.meanFlowCornerHz / spec_.sampleRateHz);
    wallR_ = spec_.wallInertanceKgPerM4 * spec_.wallResistancePerInertance;
    lungVolume_ = spec_.initialLungVolumeM3;
  }

  void refreshJunctions() {
    const std::size_t n = area_.size();
    junction_.assign(n > 0 ? n - 1 : 0, 0.0);
    for (std::size_t i = 0; i + 1 < n; ++i)
      junction_[i] = (area_[i] - area_[i + 1]) / (area_[i] + area_[i + 1]);
    loss_.assign(n, 1.0);
    // Kirchhoff attenuation per section at lossMatchHz on that section's
    // radius (same law as valve_detail::kirchhoffTripMagnitude).
    const double c = spec_.speedMetersPerSecond;
    const double rho = spec_.densityKgPerM3;
    const double mu = 1.81e-5;  // dry air near 293 K (Sutherland's law)
    const double nu = mu / rho;
    const double gamma = 1.4;
    const double prandtl = 0.71;
    const double thermal = 1.0 + (gamma - 1.0) / std::sqrt(prandtl);
    const double omega = constants::twoPi * spec_.lossMatchHz;
    for (std::size_t i = 0; i < n; ++i) {
      const double radius = std::sqrt(area_[i] / constants::pi);
      const double alpha = std::sqrt(omega * nu / 2.0) * thermal / (radius * c);
      const double g = std::exp(-spec_.tractLossScale * alpha * sectionLength_);
      loss_[i] = (std::isfinite(g) && g > 0.0 && g <= 1.0) ? g : 1.0;
    }
  }

  PlayerAirwaySpec spec_;
  double sectionLength_ = 0.0;
  double oralArea_ = 0.0;
  int oralBegin_ = 0;
  std::vector<double> area_, junction_, loss_;
  std::vector<double> right_, left_, nextRight_, nextLeft_;
  double wallFlow_ = 0.0, wallVolume_ = 0.0, wallR_ = 0.0;
  double meanFlow_ = 0.0, meanAlpha_ = 0.0;
  double acousticMouth_ = 0.0, lastMouth_ = 0.0, target_ = 0.0;
  double tongue_ = 1.0, tongueVelocity_ = 0.0, tongueTarget_ = 1.0, tongueOmega_ = 420.0;
  double lungVolume_ = 0.0;
};

}  // namespace mmathen
