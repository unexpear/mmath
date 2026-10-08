#pragma once

// A felt mallet on a drum head or a free circular plate, at the sample
// rate (docs/48).
//
// The head uses the Bessel modes of drum.h. φ = J_m(α r/a) cos(m(θ−θ_s)),
// the norm is the same integral as membraneStrikeCoefficient, and the
// modal mass is σ N. The plate uses one or more free-edge Kirchhoff
// modes: φ = R(r) cos(n(θ−θ_s)), modal mass ρ h N, with N from
// freePlateModeNorm. Both advance with the exact oscillator step. The
// felt law is the Hunt–Crossley form used by FeltHammerVoice, and the
// mallet acceleration is feltContactRate's −F/m. The string quotient
// F/(2Z) is not applied.
//
// Every included mode uses the caller's quality factor, σ = ω/(2Q).
// That Q is not a measured drum or cymbal. Air-cavity loading, a second
// head, snare wires, and plate nonlinearity are not in this step. The
// sample is transverse displacement in metres at the observe point.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "../math/constants.h"
#include "../physics/circular_plate.h"
#include "../physics/modes.h"
#include "../physics/oscillator.h"
#include "../render/voice.h"
#include "cymbal.h"
#include "drum.h"

namespace mmathen {

struct MalletStrike {
  double radialFraction;
  double angleRadians;
  double massKg;
  double speedMetersPerSecond;
  double feltStiffness;
  double feltExponent;
  double viscousPerMeter;
  double qualityFactor;
};

namespace mallet_detail {

struct Mode {
  double omega = 0.0;
  double sigma = 0.0;
  double q = 0.0;
  double v = 0.0;
  double phiStrike = 0.0;
  double phiObserve = 0.0;
  double drive = 0.0;
};

[[nodiscard]] inline double membraneModeNorm(int nodalDiameters, double radius,
                                             double alpha) {
  const double radial =
      std::cyl_bessel_j(static_cast<double>(nodalDiameters + 1), alpha);
  const double disk = 0.5 * radius * radius * radial * radial;
  const double angular =
      nodalDiameters == 0 ? constants::twoPi : constants::pi;
  return angular * disk;
}

[[nodiscard]] inline bool strikeSane(const MalletStrike& strike) {
  return std::isfinite(strike.radialFraction) && strike.radialFraction >= 0.0 &&
         strike.radialFraction < 1.0 && std::isfinite(strike.angleRadians) &&
         std::isfinite(strike.massKg) && strike.massKg > 0.0 &&
         std::isfinite(strike.speedMetersPerSecond) &&
         strike.speedMetersPerSecond > 0.0 &&
         std::isfinite(strike.feltStiffness) && strike.feltStiffness > 0.0 &&
         std::isfinite(strike.feltExponent) && strike.feltExponent > 0.0 &&
         std::isfinite(strike.viscousPerMeter) && strike.viscousPerMeter >= 0.0 &&
         std::isfinite(strike.qualityFactor) && strike.qualityFactor > 0.5;
}

[[nodiscard]] inline Result<double, QuantityError> feltForce(
    double compression, double compressionRate, const MalletStrike& strike) {
  using R = Result<double, QuantityError>;
  if (!(compression > 0.0)) return R::success(0.0);
  const double factor = 1.0 + strike.viscousPerMeter * compressionRate;
  if (!(factor > 0.0)) return R::success(0.0);
  const double force =
      strike.feltStiffness * std::pow(compression, strike.feltExponent) * factor;
  if (!std::isfinite(force)) return R::failure(QuantityError::RangeError);
  return R::success(force);
}

[[nodiscard]] inline Result<FrameCount, QuantityError> stepModes(
    double* out, FrameCount n, SampleRateHz rate, const MalletStrike& strike,
    std::vector<Mode>& modes, double& malletY, double& malletV);

}  // namespace mallet_detail

class MembraneMalletVoice final : public IVoice {
 public:
  // maxNodalDiameters >= 0, maxRadial >= 1. The observe fraction is in
  // [0, 1]; the rim is a legal observe point and sits at zero.
  [[nodiscard]] static Result<MembraneMalletVoice, QuantityError> create(
      const MembraneHead& head, const MalletStrike& strike, int maxNodalDiameters,
      int maxRadial, SampleRateHz rate, double observeRadialFraction,
      double observeAngle) {
    using R = Result<MembraneMalletVoice, QuantityError>;
    const double fs = rate.value();
    if (!std::isfinite(fs) || !(fs > 0.0) || !mallet_detail::strikeSane(strike) ||
        maxNodalDiameters < 0 || maxRadial < 1 ||
        !std::isfinite(observeRadialFraction) || observeRadialFraction < 0.0 ||
        observeRadialFraction > 1.0 || !std::isfinite(observeAngle))
      return R::failure(QuantityError::InvalidArgument);
    const double radius = head.radius.value();
    const double density = head.arealDensity.value();
    const double tension = head.tension.value();
    if (!std::isfinite(radius) || !(radius > 0.0) || !std::isfinite(density) ||
        !(density > 0.0) || !std::isfinite(tension) || !(tension > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    std::vector<mallet_detail::Mode> modes;
    for (int m = 0; m <= maxNodalDiameters; ++m) {
      for (int n = 1; n <= maxRadial; ++n) {
        auto alpha = besselJZero(m, n);
        if (!alpha.ok()) return R::failure(alpha.error());
        auto hertz = circularMembraneMode(head.radius, head.tension,
                                          head.arealDensity, m, n);
        if (!hertz.ok()) return R::failure(hertz.error());
        const double norm =
            mallet_detail::membraneModeNorm(m, radius, alpha.value());
        if (!(norm > 0.0) || !std::isfinite(norm))
          return R::failure(QuantityError::RangeError);
        const double modalMass = density * norm;
        const double omega = constants::twoPi * hertz.value().value();
        const double sigma = omega / (2.0 * strike.qualityFactor);
        if (!(sigma < omega) || !(modalMass > 0.0))
          return R::failure(QuantityError::RangeError);
        const double strikeR = strike.radialFraction * radius;
        const double observeR = observeRadialFraction * radius;
        const double order = static_cast<double>(m);
        const double phiStrike =
            std::cyl_bessel_j(order, alpha.value() * strikeR / radius) *
            std::cos(order * (strike.angleRadians - strike.angleRadians));
        const double phiObserve =
            std::cyl_bessel_j(order, alpha.value() * observeR / radius) *
            std::cos(order * (observeAngle - strike.angleRadians));
        if (!std::isfinite(phiStrike) || !std::isfinite(phiObserve))
          return R::failure(QuantityError::RangeError);
        mallet_detail::Mode mode;
        mode.omega = omega;
        mode.sigma = sigma;
        mode.phiStrike = phiStrike;
        mode.phiObserve = phiObserve;
        mode.drive = phiStrike / modalMass;
        modes.push_back(mode);
      }
    }
    if (modes.empty()) return R::failure(QuantityError::InvalidArgument);
    const double sigma0 = modes.front().sigma;
    const double omega0 = modes.front().omega;
    const double fundamentalHz =
        std::sqrt(omega0 * omega0 - sigma0 * sigma0) / constants::twoPi;
    return R::success(MembraneMalletVoice{rate, strike, std::move(modes),
                                          fundamentalHz, sigma0});
  }

  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }

  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(
      double* out, FrameCount n) override {
    return mallet_detail::stepModes(out, n, rate_, strike_, modes_, malletY_,
                                    malletV_);
  }

  void reset() override {
    for (auto& mode : modes_) {
      mode.q = 0.0;
      mode.v = 0.0;
    }
    malletY_ = 0.0;
    malletV_ = strike_.speedMetersPerSecond;
  }

  [[nodiscard]] double fundamentalHz() const noexcept { return fundamentalHz_; }
  [[nodiscard]] double fundamentalDecay() const noexcept {
    return fundamentalDecay_;
  }

 private:
  MembraneMalletVoice(SampleRateHz rate, MalletStrike strike,
                      std::vector<mallet_detail::Mode> modes,
                      double fundamentalHz, double fundamentalDecay)
      : rate_(rate),
        strike_(strike),
        modes_(std::move(modes)),
        fundamentalHz_(fundamentalHz),
        fundamentalDecay_(fundamentalDecay),
        malletV_(strike.speedMetersPerSecond) {}

  SampleRateHz rate_;
  MalletStrike strike_;
  std::vector<mallet_detail::Mode> modes_;
  double fundamentalHz_;
  double fundamentalDecay_;
  double malletY_ = 0.0;
  double malletV_;
};

namespace mallet_detail {

[[nodiscard]] inline Result<FrameCount, QuantityError> stepModes(
    double* out, FrameCount n, SampleRateHz rate, const MalletStrike& strike,
    std::vector<Mode>& modes, double& malletY, double& malletV) {
  using R = Result<FrameCount, QuantityError>;
  const std::uint64_t count = static_cast<std::uint64_t>(n.value());
  if (count == 0) return R::success(n);
  if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
  const double dt = 1.0 / rate.value();
  for (std::uint64_t i = 0; i < count; ++i) {
    double atStrike = 0.0;
    double rateAtStrike = 0.0;
    for (const Mode& mode : modes) {
      atStrike += mode.q * mode.phiStrike;
      rateAtStrike += mode.v * mode.phiStrike;
    }
    auto force = feltForce(malletY - atStrike, malletV - rateAtStrike, strike);
    if (!force.ok()) return R::failure(force.error());
    for (Mode& mode : modes) {
      auto stepped = stepConstantForceOscillator(
          OscillatorState{mode.q, mode.v}, mode.omega, mode.sigma,
          force.value() * mode.drive, dt);
      if (!stepped.ok()) return R::failure(stepped.error());
      mode.q = stepped.value().displacement;
      mode.v = stepped.value().velocity;
    }
    malletV += (-force.value() / strike.massKg) * dt;
    malletY += malletV * dt;
    if (!std::isfinite(malletY) || !std::isfinite(malletV))
      return R::failure(QuantityError::RangeError);
    double sample = 0.0;
    for (const Mode& mode : modes) sample += mode.q * mode.phiObserve;
    if (!std::isfinite(sample)) return R::failure(QuantityError::RangeError);
    out[i] = sample;
  }
  return R::success(n);
}

}  // namespace mallet_detail

class PlateMalletVoice final : public IVoice {
 public:
  // Each pair is (nodal diameters, radial index), radial index >= 1.
  // An empty list is InvalidArgument. The observe fraction is in [0, 1].
  [[nodiscard]] static Result<PlateMalletVoice, QuantityError> create(
      const CymbalPlate& plate, const CymbalMaterial& material,
      const MalletStrike& strike, const int* nodalDiameters,
      const int* radialIndices, int modeCount, SampleRateHz rate,
      double observeRadialFraction, double observeAngle) {
    using R = Result<PlateMalletVoice, QuantityError>;
    const double fs = rate.value();
    if (!std::isfinite(fs) || !(fs > 0.0) || !mallet_detail::strikeSane(strike) ||
        !cymbal_detail::materialSane(material) || nodalDiameters == nullptr ||
        radialIndices == nullptr || modeCount < 1 ||
        !std::isfinite(observeRadialFraction) || observeRadialFraction < 0.0 ||
        observeRadialFraction > 1.0 || !std::isfinite(observeAngle))
      return R::failure(QuantityError::InvalidArgument);
    const double radius = plate.radius.value();
    if (!std::isfinite(radius) || !(radius > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    std::vector<mallet_detail::Mode> modes;
    modes.reserve(static_cast<std::size_t>(modeCount));
    for (int i = 0; i < modeCount; ++i) {
      auto shapeMode = freeCircularPlateMode(nodalDiameters[i], radialIndices[i],
                                             material.poisson);
      if (!shapeMode.ok()) return R::failure(shapeMode.error());
      auto hertz = freeCircularPlateFrequency(
          plate.radius, material.thicknessMeters, material.youngsModulusPa,
          material.densityKgPerM3, material.poisson, nodalDiameters[i],
          radialIndices[i]);
      if (!hertz.ok()) return R::failure(hertz.error());
      auto norm = freePlateModeNorm(shapeMode.value(), radius);
      if (!norm.ok()) return R::failure(norm.error());
      const double modalMass =
          material.densityKgPerM3 * material.thicknessMeters * norm.value();
      const double omega = constants::twoPi * hertz.value().value();
      const double sigma = omega / (2.0 * strike.qualityFactor);
      if (!(modalMass > 0.0) || !(sigma < omega))
        return R::failure(QuantityError::RangeError);
      const double strikeR = strike.radialFraction * radius;
      const double observeR = observeRadialFraction * radius;
      auto strikeShape =
          freePlateRadialShape(shapeMode.value(), radius, strikeR);
      if (!strikeShape.ok()) return R::failure(strikeShape.error());
      auto observeShape =
          freePlateRadialShape(shapeMode.value(), radius, observeR);
      if (!observeShape.ok()) return R::failure(observeShape.error());
      const double order = static_cast<double>(nodalDiameters[i]);
      const double phiStrike = strikeShape.value();
      const double phiObserve =
          observeShape.value() *
          std::cos(order * (observeAngle - strike.angleRadians));
      mallet_detail::Mode mode;
      mode.omega = omega;
      mode.sigma = sigma;
      mode.phiStrike = phiStrike;
      mode.phiObserve = phiObserve;
      mode.drive = phiStrike / modalMass;
      if (!std::isfinite(mode.drive) || !std::isfinite(phiObserve))
        return R::failure(QuantityError::RangeError);
      modes.push_back(mode);
    }
    const double sigma0 = modes.front().sigma;
    const double fundamentalHz =
        std::sqrt(modes.front().omega * modes.front().omega - sigma0 * sigma0) /
        constants::twoPi;
    return R::success(PlateMalletVoice{rate, strike, std::move(modes),
                                       fundamentalHz, sigma0});
  }

  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }

  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(
      double* out, FrameCount n) override {
    return mallet_detail::stepModes(out, n, rate_, strike_, modes_, malletY_,
                                    malletV_);
  }

  void reset() override {
    for (auto& mode : modes_) {
      mode.q = 0.0;
      mode.v = 0.0;
    }
    malletY_ = 0.0;
    malletV_ = strike_.speedMetersPerSecond;
  }

  [[nodiscard]] double fundamentalHz() const noexcept { return fundamentalHz_; }
  [[nodiscard]] double fundamentalDecay() const noexcept {
    return fundamentalDecay_;
  }

 private:
  PlateMalletVoice(SampleRateHz rate, MalletStrike strike,
                   std::vector<mallet_detail::Mode> modes, double fundamentalHz,
                   double fundamentalDecay)
      : rate_(rate),
        strike_(strike),
        modes_(std::move(modes)),
        fundamentalHz_(fundamentalHz),
        fundamentalDecay_(fundamentalDecay),
        malletV_(strike.speedMetersPerSecond) {}

  SampleRateHz rate_;
  MalletStrike strike_;
  std::vector<mallet_detail::Mode> modes_;
  double fundamentalHz_;
  double fundamentalDecay_;
  double malletY_ = 0.0;
  double malletV_;
};

}  // namespace mmathen
