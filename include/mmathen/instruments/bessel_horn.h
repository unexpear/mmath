#pragma once

// A lip on a Webster power-law horn (docs/48).
//
// The area from the throat x = x_a toward the bell x = x_a + L is
//
//   S(x) = S_a (x / x_a)^ε.
//
// ε = 0 is a cylinder. ε = 2 is the spherical cone already stepped by
// ConeValveVoice: p ∝ (A sin kx + B cos kx) / x and the sealed-mouth
// root is tan(kL) = −k x_a. Any other ε ≥ 0 is the non-conical member
// of the same family. Its pressure is
//
//   p(x) = x^γ [A J_ν(kx) + B Y_ν(kx)],
//   γ = (1 − ε) / 2,   ν = |γ|.
//
// A sealed throat is ∂p/∂x = 0. An ideal open bell is p = 0. For ε ≥ 1
// that pair is the cross product
//
//   J_{ν+1}(k x_a) Y_ν(k x_b) − Y_{ν+1}(k x_a) J_ν(k x_b) = 0,
//
// and for ε < 1 the closed order is ν − 1. The exponent is an argument.
// It is not replaced by a chain of cones, and no measured bell curve
// is stored.
//
// The lip drives the modal throat impedance of those roots,
//
//   Z(s) = Σ α_n s / (s^2 + ω_n^2),
//   α_n = 2 ω_n ρ p / (S_a ∂p/∂x∂ω)
//
// at the throat, with the standing-wave shape that vanishes at the
// bell. Each coordinate s_n obeys s̈ + 2σ ṡ + ω^2 s = α U, and the
// sample is p = Σ ṡ. σ is the open-end loss −ln(|R|) per round trip
// 2L/c. A positive radius uses OpenEndReflection at that mode; a zero
// radius uses ValveBlow::reflectionMagnitude for every mode. An end
// correction lengthens the stepped horn and is not 0.6133a.
// fundamentalHz() stays the root without that correction. A cup
// compliance, when the trumpet has one, is C dp/dt = U_lip − U_bore.
// Extra length is engaged valve tubing or the F attachment. Zero leaves
// the length unchanged.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "../math/constants.h"
#include "../physics/oscillator.h"
#include "../physics/radiation.h"
#include "../render/voice.h"
#include "bore_valve.h"
#include "trombone.h"
#include "trumpet.h"

namespace mmathen {

namespace bessel_detail {

[[nodiscard]] inline double cylinderJ(double nu, double z) {
  if (!(z > 0.0) || !std::isfinite(nu)) return std::numeric_limits<double>::quiet_NaN();
  if (nu >= 0.0) return std::cyl_bessel_j(nu, z);
  const double n = -nu;
  const double s = std::sin(n * constants::pi);
  const double c = std::cos(n * constants::pi);
  return c * std::cyl_bessel_j(n, z) - s * std::cyl_neumann(n, z);
}

[[nodiscard]] inline double cylinderY(double nu, double z) {
  if (!(z > 0.0) || !std::isfinite(nu)) return std::numeric_limits<double>::quiet_NaN();
  if (nu >= 0.0) return std::cyl_neumann(nu, z);
  const double n = -nu;
  const double s = std::sin(n * constants::pi);
  const double c = std::cos(n * constants::pi);
  return s * std::cyl_bessel_j(n, z) + c * std::cyl_neumann(n, z);
}

[[nodiscard]] inline double cylinderJp(double nu, double z) {
  return cylinderJ(nu - 1.0, z) - (nu / z) * cylinderJ(nu, z);
}

[[nodiscard]] inline double cylinderYp(double nu, double z) {
  return cylinderY(nu - 1.0, z) - (nu / z) * cylinderY(nu, z);
}

[[nodiscard]] inline double cylinderSecond(double nu, double z, double y, double yp) {
  const double z2 = z * z;
  return -yp / z - (1.0 - (nu * nu) / z2) * y;
}

[[nodiscard]] inline double hornOrder(double areaExponent) {
  return std::fabs((1.0 - areaExponent) / 2.0);
}

// Sealed throat, pressure release at the bell. A root is a sign change.
[[nodiscard]] inline double characteristic(double areaExponent, double apexMeters,
                                           double lengthMeters, double waveNumber) {
  const double nu = hornOrder(areaExponent);
  const double closed = (areaExponent >= 1.0) ? (nu + 1.0) : (nu - 1.0);
  const double za = waveNumber * apexMeters;
  const double zb = waveNumber * (apexMeters + lengthMeters);
  const double left = cylinderJ(closed, za) * cylinderY(nu, zb);
  const double right = cylinderY(closed, za) * cylinderJ(nu, zb);
  return left - right;
}

[[nodiscard]] inline double standingPressure(double areaExponent, double apexMeters,
                                             double lengthMeters, double waveNumber,
                                             double x) {
  const double gamma = (1.0 - areaExponent) / 2.0;
  const double nu = hornOrder(areaExponent);
  const double zb = waveNumber * (apexMeters + lengthMeters);
  const double z = waveNumber * x;
  const double u = cylinderJ(nu, z) * cylinderY(nu, zb) - cylinderY(nu, z) * cylinderJ(nu, zb);
  return std::pow(x, gamma) * u;
}

[[nodiscard]] inline double standingSlopeK(double areaExponent, double apexMeters,
                                           double lengthMeters, double waveNumber,
                                           double x) {
  const double gamma = (1.0 - areaExponent) / 2.0;
  const double nu = hornOrder(areaExponent);
  const double bell = apexMeters + lengthMeters;
  const double z = waveNumber * x;
  const double zb = waveNumber * bell;
  const double jz = cylinderJ(nu, z);
  const double yz = cylinderY(nu, z);
  const double jb = cylinderJ(nu, zb);
  const double yb = cylinderY(nu, zb);
  const double jpz = cylinderJp(nu, z);
  const double ypz = cylinderYp(nu, z);
  const double jpb = cylinderJp(nu, zb);
  const double ypb = cylinderYp(nu, zb);
  if (!std::isfinite(jz) || !std::isfinite(yz) || !std::isfinite(jb) || !std::isfinite(yb) ||
      !std::isfinite(jpz) || !std::isfinite(ypz) || !std::isfinite(jpb) || !std::isfinite(ypb))
    return std::numeric_limits<double>::quiet_NaN();
  const double jppz = cylinderSecond(nu, z, jz, jpz);
  const double yppz = cylinderSecond(nu, z, yz, ypz);
  const double uk = jpz * x * yb + jz * ypb * bell - ypz * x * jb - yz * jpb * bell;
  const double w = jpz * yb - ypz * jb;
  const double wk = jppz * x * yb + jpz * (ypb * bell) - yppz * x * jb - ypz * (jpb * bell);
  const double uxk = w + waveNumber * wk;
  return gamma * std::pow(x, gamma - 1.0) * uk + std::pow(x, gamma) * uxk;
}

[[nodiscard]] inline Result<double, QuantityError> findRoot(double areaExponent,
                                                            double apexMeters,
                                                            double lengthMeters,
                                                            double low, double high) {
  using R = Result<double, QuantityError>;
  double lo = low;
  double hi = high;
  double flo = characteristic(areaExponent, apexMeters, lengthMeters, lo);
  if (!std::isfinite(flo)) return R::failure(QuantityError::RangeError);
  for (int step = 0; step < 70; ++step) {
    const double mid = 0.5 * (lo + hi);
    const double fm = characteristic(areaExponent, apexMeters, lengthMeters, mid);
    if (!std::isfinite(fm)) return R::failure(QuantityError::RangeError);
    if (flo * fm <= 0.0)
      hi = mid;
    else {
      lo = mid;
      flo = fm;
    }
  }
  return R::success(0.5 * (lo + hi));
}

[[nodiscard]] inline Result<double, QuantityError> resonanceWaveNumber(
    double areaExponent, double apexMeters, double lengthMeters, int modeIndex) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(areaExponent) || areaExponent < 0.0 || !std::isfinite(apexMeters) ||
      !(apexMeters > 0.0) || !std::isfinite(lengthMeters) || !(lengthMeters > 0.0) ||
      modeIndex < 1)
    return R::failure(QuantityError::InvalidArgument);
  const double step = constants::pi / (40.0 * lengthMeters);
  if (!std::isfinite(step) || !(step > 0.0)) return R::failure(QuantityError::RangeError);
  const double kMax = static_cast<double>(modeIndex + 2) * 8.0 * constants::pi / lengthMeters;
  if (!std::isfinite(kMax) || !(kMax > step)) return R::failure(QuantityError::RangeError);
  double previous = characteristic(areaExponent, apexMeters, lengthMeters, step);
  int found = 0;
  for (double k = 2.0 * step; k <= kMax; k += step) {
    const double value = characteristic(areaExponent, apexMeters, lengthMeters, k);
    if (std::isfinite(previous) && std::isfinite(value) && previous * value < 0.0) {
      ++found;
      if (found == modeIndex) return findRoot(areaExponent, apexMeters, lengthMeters, k - step, k);
    }
    previous = value;
  }
  return R::failure(QuantityError::RangeError);
}

[[nodiscard]] inline Result<double, QuantityError> modalAlpha(double areaExponent,
                                                             double apexMeters,
                                                             double lengthMeters,
                                                             double waveNumber, double area,
                                                             double density, double speed) {
  using R = Result<double, QuantityError>;
  const double pressure = standingPressure(areaExponent, apexMeters, lengthMeters, waveNumber,
                                           apexMeters);
  const double slope = standingSlopeK(areaExponent, apexMeters, lengthMeters, waveNumber,
                                      apexMeters);
  if (!std::isfinite(pressure) || !std::isfinite(slope) || slope == 0.0 || area == 0.0)
    return R::failure(QuantityError::RangeError);
  const double alpha = 2.0 * waveNumber * density * speed * speed * pressure / (area * slope);
  if (!std::isfinite(alpha) || !(alpha > 0.0)) return R::failure(QuantityError::RangeError);
  return R::success(alpha);
}

}  // namespace bessel_detail

// modeIndex starts at 1. InvalidArgument: a negative exponent, a
// non-positive apex, length, or speed, or a mode index below 1.
// RangeError: the search does not find that root.
[[nodiscard]] inline Result<double, QuantityError> besselHornResonanceHz(
    double areaExponent, double apexMeters, double lengthMeters,
    double speedMetersPerSecond, int modeIndex) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(speedMetersPerSecond) || !(speedMetersPerSecond > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  auto wave = bessel_detail::resonanceWaveNumber(areaExponent, apexMeters, lengthMeters,
                                                 modeIndex);
  if (!wave.ok()) return R::failure(wave.error());
  const double hertz = wave.value() * speedMetersPerSecond / constants::twoPi;
  if (!std::isfinite(hertz) || !(hertz > 0.0)) return R::failure(QuantityError::RangeError);
  return R::success(hertz);
}

class BesselHornVoice final : public IVoice {
 public:
  // areaExponent 2 matches the cone's sealed-mouth resonances. modeCount
  // is how many of those roots the lip drives, from 1 through 8. Zero
  // extra length is the open instrument.
  [[nodiscard]] static Result<BesselHornVoice, QuantityError> fromTrombone(
      const Trombone& trombone, double apexDistanceMeters, double areaExponent,
      const ValveBlow& blow, SampleRateHz rate, int modeCount,
      double extraLengthMeters = 0.0) {
    return fromTrumpet(trombone.bore, apexDistanceMeters, areaExponent, blow, rate,
                       modeCount, extraLengthMeters);
  }

  [[nodiscard]] static Result<BesselHornVoice, QuantityError> fromTrumpet(
      const Trumpet& trumpet, double apexDistanceMeters, double areaExponent,
      const ValveBlow& blow, SampleRateHz rate, int modeCount,
      double extraLengthMeters = 0.0) {
    using R = Result<BesselHornVoice, QuantityError>;
    const double fs = rate.value();
    if (!std::isfinite(apexDistanceMeters) || !(apexDistanceMeters > 0.0) ||
        !std::isfinite(areaExponent) || areaExponent < 0.0 || modeCount < 1 ||
        modeCount > 8 || !std::isfinite(blow.pressurePa) || !(blow.pressurePa > 0.0) ||
        !std::isfinite(blow.reflectionMagnitude) || !(blow.reflectionMagnitude > 0.0) ||
        blow.reflectionMagnitude > 1.0 || !std::isfinite(blow.initialDisplacementMeters) ||
        !std::isfinite(fs) || !(fs > 0.0) || !std::isfinite(trumpet.lengthMeters) ||
        !(trumpet.lengthMeters > 0.0) || !std::isfinite(trumpet.crossSectionM2) ||
        !(trumpet.crossSectionM2 > 0.0) ||
        !std::isfinite(trumpet.soundSpeedMetersPerSecond) ||
        !(trumpet.soundSpeedMetersPerSecond > 0.0) ||
        !std::isfinite(trumpet.airDensityKgPerM3) || !(trumpet.airDensityKgPerM3 > 0.0) ||
        !std::isfinite(blow.endCorrectionMeters) || blow.endCorrectionMeters < 0.0 ||
        !std::isfinite(extraLengthMeters) || extraLengthMeters < 0.0 ||
        !std::isfinite(trumpet.acousticCompliance) || trumpet.acousticCompliance < 0.0 ||
        !(trumpet.slitWidthMeters > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    const double sounding = trumpet.lengthMeters + extraLengthMeters;
    const double stepped = sounding + blow.endCorrectionMeters;
    if (!std::isfinite(sounding) || !(sounding > 0.0) || !std::isfinite(stepped) ||
        !(stepped > 0.0))
      return R::failure(QuantityError::RangeError);
    auto fundamental = besselHornResonanceHz(areaExponent, apexDistanceMeters, sounding,
                                             trumpet.soundSpeedMetersPerSecond, 1);
    if (!fundamental.ok()) return R::failure(fundamental.error());
    const bool radiating = blow.radiusMeters > 0.0;
    OpenEndReflection openEnd;
    if (radiating) {
      auto filter = OpenEndReflection::create(blow.radiusMeters, blow.endCorrectionMeters,
                                              trumpet.soundSpeedMetersPerSecond, rate);
      if (!filter.ok()) return R::failure(filter.error());
      openEnd = std::move(filter).value();
    }
    BesselHornVoice voice(rate);
    voice.pressure_ = blow.pressurePa;
    voice.blowFrames_ = blow.frames;
    voice.initialDisplacement_ = blow.initialDisplacementMeters;
    voice.displacement_ = blow.initialDisplacementMeters;
    voice.exponent_ = areaExponent;
    voice.fundamentalHz_ = fundamental.value();
    voice.density_ = trumpet.airDensityKgPerM3;
    voice.trumpet_ = trumpet;
    voice.restOpening_ = trumpet.restOpeningAreaM2;
    voice.maximumOpening_ = trumpet.maximumOpeningAreaM2;
    voice.slitWidth_ = trumpet.slitWidthMeters;
    if (trumpet.acousticCompliance > 0.0) {
      const double cup = trumpet.acousticCompliance * fs;
      if (!std::isfinite(cup) || !(cup > 0.0)) return R::failure(QuantityError::RangeError);
      voice.cupRate_ = cup;
    }
    voice.modes_.resize(static_cast<std::size_t>(modeCount));
    voice.held_.resize(static_cast<std::size_t>(modeCount));
    voice.driven_.resize(static_cast<std::size_t>(modeCount));
    const double trip = 2.0 * stepped / trumpet.soundSpeedMetersPerSecond;
    if (!std::isfinite(trip) || !(trip > 0.0)) return R::failure(QuantityError::RangeError);
    for (int index = 1; index <= modeCount; ++index) {
      auto wave = bessel_detail::resonanceWaveNumber(areaExponent, apexDistanceMeters, stepped,
                                                    index);
      if (!wave.ok()) return R::failure(wave.error());
      const double omega = wave.value() * trumpet.soundSpeedMetersPerSecond;
      const double hertz = omega / constants::twoPi;
      if (!std::isfinite(omega) || !(omega > 0.0) || !(hertz > 0.0) || !(hertz < fs / 2.0))
        return R::failure(QuantityError::RangeError);
      auto alpha = bessel_detail::modalAlpha(areaExponent, apexDistanceMeters, stepped,
                                             wave.value(), trumpet.crossSectionM2,
                                             trumpet.airDensityKgPerM3,
                                             trumpet.soundSpeedMetersPerSecond);
      if (!alpha.ok()) return R::failure(alpha.error());
      double magnitude = blow.reflectionMagnitude;
      if (radiating) {
        auto response = openEnd.response(hertz);
        if (!response.ok()) return R::failure(response.error());
        magnitude = std::hypot(response.value().real, response.value().imag);
      }
      if (!(magnitude > 0.0) || magnitude > 1.0) return R::failure(QuantityError::RangeError);
      const double sigma = -std::log(magnitude) / trip;
      if (!std::isfinite(sigma) || sigma < 0.0 || !(sigma < omega))
        return R::failure(QuantityError::RangeError);
      Mode& mode = voice.modes_[static_cast<std::size_t>(index - 1)];
      mode.omega = omega;
      mode.alpha = alpha.value();
      mode.sigma = sigma;
    }
    voice.ringingHz_ = voice.modes_.front().omega / constants::twoPi;
    voice.decay_ = voice.modes_.front().sigma;
    return R::success(std::move(voice));
  }

  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }

  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(double* out,
                                                             FrameCount n) override {
    using R = Result<FrameCount, QuantityError>;
    const std::uint64_t count = static_cast<std::uint64_t>(n.value());
    if (count == 0) return R::success(n);
    if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
    const double dt = 1.0 / rate_.value();
    for (std::uint64_t i = 0; i < count; ++i) {
      auto sample = stepOne(dt);
      if (!sample.ok()) return R::failure(sample.error());
      out[i] = sample.value();
    }
    return R::success(n);
  }

  void reset() override {
    for (Mode& mode : modes_) {
      mode.displacement = 0.0;
      mode.velocity = 0.0;
    }
    displacement_ = initialDisplacement_;
    velocity_ = 0.0;
    cupPressure_ = 0.0;
    frame_ = 0;
  }

  [[nodiscard]] double fundamentalHz() const noexcept { return fundamentalHz_; }
  [[nodiscard]] double ringingHz() const noexcept { return ringingHz_; }
  [[nodiscard]] double reflectionDecay() const noexcept { return decay_; }
  [[nodiscard]] double areaExponent() const noexcept { return exponent_; }
  [[nodiscard]] int modeCount() const noexcept { return static_cast<int>(modes_.size()); }

 private:
  struct Mode {
    double omega = 0.0;
    double alpha = 0.0;
    double sigma = 0.0;
    double displacement = 0.0;
    double velocity = 0.0;
  };

  explicit BesselHornVoice(SampleRateHz rate) : rate_(rate) {}

  [[nodiscard]] double bernoulli(double opening, double delta) const {
    if (!(opening > 0.0) || delta == 0.0) return 0.0;
    const double speed = std::sqrt(2.0 * std::fabs(delta) / density_);
    return std::copysign(opening * speed, delta);
  }

  [[nodiscard]] Result<double, QuantityError> stepOne(double dt) {
    using R = Result<double, QuantityError>;
    const bool open = frame_ < blowFrames_;
    const double mouth = open ? pressure_ : 0.0;
    if (!open) {
      displacement_ = -restOpening_ / slitWidth_;
      velocity_ = 0.0;
    }
    double opening = restOpening_ + slitWidth_ * displacement_;
    if (opening < 0.0) opening = 0.0;
    if (opening > maximumOpening_) opening = maximumOpening_;
    if (!open) opening = 0.0;
    double pressure0 = 0.0;
    double gain = 0.0;
    for (std::size_t i = 0; i < modes_.size(); ++i) {
      const Mode& mode = modes_[i];
      OscillatorState state{mode.displacement, mode.velocity};
      auto zero = stepConstantForceOscillator(state, mode.omega, mode.sigma, 0.0, dt);
      if (!zero.ok()) return R::failure(zero.error());
      auto unit = stepConstantForceOscillator(state, mode.omega, mode.sigma, mode.alpha, dt);
      if (!unit.ok()) return R::failure(unit.error());
      held_[i] = zero.value();
      driven_[i] = unit.value();
      pressure0 += held_[i].velocity;
      gain += driven_[i].velocity - held_[i].velocity;
    }
    if (!std::isfinite(pressure0) || !std::isfinite(gain))
      return R::failure(QuantityError::RangeError);
    double flow = 0.0;
    if (!(opening > 0.0) && cupRate_ == 0.0) {
      flow = 0.0;
    } else if (!(opening > 0.0)) {
      const double scale = 1.0 + cupRate_ * gain;
      if (!std::isfinite(scale) || scale == 0.0) return R::failure(QuantityError::RangeError);
      flow = cupRate_ * (cupPressure_ - pressure0) / scale;
    } else {
      auto solved = solveFlow(opening, mouth, pressure0, gain);
      if (!solved.ok()) return R::failure(solved.error());
      flow = solved.value();
    }
    if (!std::isfinite(flow)) return R::failure(QuantityError::RangeError);
    const double pressure = pressure0 + gain * flow;
    if (!std::isfinite(pressure)) return R::failure(QuantityError::RangeError);
    for (std::size_t i = 0; i < modes_.size(); ++i) {
      modes_[i].displacement =
          held_[i].displacement + flow * (driven_[i].displacement - held_[i].displacement);
      modes_[i].velocity = held_[i].velocity + flow * (driven_[i].velocity - held_[i].velocity);
      if (!std::isfinite(modes_[i].displacement) || !std::isfinite(modes_[i].velocity))
        return R::failure(QuantityError::RangeError);
    }
    cupPressure_ = pressure;
    if (open) {
      const double delta = mouth - pressure;
      auto acceleration =
          trumpetLipAcceleration(trumpet_, displacement_, velocity_, delta);
      if (!acceleration.ok()) return R::failure(acceleration.error());
      velocity_ += acceleration.value() * dt;
      displacement_ += velocity_ * dt;
      if (!std::isfinite(velocity_) || !std::isfinite(displacement_))
        return R::failure(QuantityError::RangeError);
    }
    ++frame_;
    return R::success(pressure);
  }

  [[nodiscard]] Result<double, QuantityError> solveFlow(double opening, double mouth,
                                                        double pressure0, double gain) const {
    using R = Result<double, QuantityError>;
    auto residual = [&](double flow) {
      const double pressure = pressure0 + gain * flow;
      const double lip = bernoulli(opening, mouth - pressure);
      return cupRate_ * (pressure - cupPressure_) - (lip - flow);
    };
    const double at0 = residual(0.0);
    if (!std::isfinite(at0)) return R::failure(QuantityError::RangeError);
    if (at0 == 0.0) return R::success(0.0);
    double lo = 0.0;
    double hi = 0.0;
    if (at0 < 0.0) {
      hi = 1.0e-6;
      for (int i = 0; i < 48; ++i) {
        const double at = residual(hi);
        if (!std::isfinite(at)) return R::failure(QuantityError::RangeError);
        if (at >= 0.0) break;
        hi *= 2.0;
        if (i == 47) return R::failure(QuantityError::RangeError);
      }
    } else {
      lo = -1.0e-6;
      for (int i = 0; i < 48; ++i) {
        const double at = residual(lo);
        if (!std::isfinite(at)) return R::failure(QuantityError::RangeError);
        if (at <= 0.0) break;
        lo *= 2.0;
        if (i == 47) return R::failure(QuantityError::RangeError);
      }
    }
    for (int i = 0; i < 60; ++i) {
      const double mid = 0.5 * (lo + hi);
      const double at = residual(mid);
      if (!std::isfinite(at)) return R::failure(QuantityError::RangeError);
      if (at > 0.0)
        hi = mid;
      else
        lo = mid;
    }
    return R::success(0.5 * (lo + hi));
  }

  SampleRateHz rate_;
  double pressure_ = 0.0;
  std::uint64_t blowFrames_ = 0;
  double initialDisplacement_ = 0.0;
  double displacement_ = 0.0;
  double velocity_ = 0.0;
  double exponent_ = 0.0;
  double fundamentalHz_ = 0.0;
  double ringingHz_ = 0.0;
  double decay_ = 0.0;
  double density_ = 0.0;
  double cupRate_ = 0.0;
  double cupPressure_ = 0.0;
  double restOpening_ = 0.0;
  double maximumOpening_ = 0.0;
  double slitWidth_ = 0.0;
  Trumpet trumpet_{};
  std::vector<Mode> modes_;
  std::vector<OscillatorState> held_;
  std::vector<OscillatorState> driven_;
  std::uint64_t frame_ = 0;
};

}  // namespace mmathen
