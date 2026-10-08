#pragma once

// A felt hammer on one stiff or ideal course, stepped at the sample rate.
//
// The string is the hinged modal series of wave1d.h. Mode n has shape
// sin(n π x/L), modal mass μ L/2, and the undamped ω and decay σ of that
// mode. Each sample holds the contact force constant and advances every
// mode with the exact oscillator step. There is no contact envelope.
//
// The felt law is Hunt and Crossley, Journal of Applied Mechanics 42
// (1975): F = K ξ^p (1 + λ ξ') while ξ > 0 and the factor is positive,
// and F = 0 otherwise (no tension, and loss of contact). λ is the
// caller's viscous coefficient in s/m. λ = 0 is the elastic law
// F = K ξ^p already in feltHammerForce. Stulov, JASA 97 (1995), is a
// hereditary kernel; its time constant and hysteresis coefficient are
// not discretized here because they are not arguments.
//
// The hammer is a point mass integrated with the semi-implicit Euler
// step a = −F/m. K, p, λ, mass, speed, and the strike position are
// arguments. No hammer mass is stored on the piano. The sample is the
// string displacement in metres at the listen fraction. A soundboard
// is not attached.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "../math/constants.h"
#include "../physics/oscillator.h"
#include "../render/voice.h"
#include "hammer.h"
#include "plucked_string.h"

namespace mmathen {

class FeltHammerVoice final : public IVoice {
 public:
  // modeCount is 1..32. listenFraction is in (0, 1). viscousPerMeter
  // may be zero and must be finite and non-negative.
  [[nodiscard]] static Result<FeltHammerVoice, QuantityError> create(
      const StringCourse& course, const HammerStrike& strike, LossModel loss,
      double stiffnessKappa, double feltStiffness, double feltExponent,
      double viscousPerMeter, int modeCount, SampleRateHz rate,
      double listenFraction) {
    using R = Result<FeltHammerVoice, QuantityError>;
    const double fs = rate.value();
    const double position = strike.positionFraction;
    if (!std::isfinite(fs) || !(fs > 0.0) || !std::isfinite(listenFraction) ||
        !(listenFraction > 0.0) || !(listenFraction < 1.0) ||
        !std::isfinite(position) || !(position > 0.0) || !(position < 1.0) ||
        !std::isfinite(strike.massKg) || !(strike.massKg > 0.0) ||
        !std::isfinite(strike.speedMetersPerSecond) ||
        !(strike.speedMetersPerSecond > 0.0) || modeCount < 1 || modeCount > 32)
      return R::failure(QuantityError::InvalidArgument);
    if (!std::isfinite(feltStiffness) || !(feltStiffness > 0.0) ||
        !std::isfinite(feltExponent) || !(feltExponent > 0.0) ||
        !std::isfinite(viscousPerMeter) || viscousPerMeter < 0.0)
      return R::failure(QuantityError::InvalidArgument);
    auto system = pluckedCourseSystem(course, loss, stiffnessKappa);
    if (!system.ok()) return R::failure(system.error());
    const double length = system.value().length.value();
    const double density = course.material.linearDensity.value();
    if (!std::isfinite(length) || !(length > 0.0) || !std::isfinite(density) ||
        !(density > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    const double modalMass = 0.5 * density * length;
    if (!std::isfinite(modalMass) || !(modalMass > 0.0))
      return R::failure(QuantityError::RangeError);
    std::vector<Mode> modes;
    modes.reserve(static_cast<std::size_t>(modeCount));
    for (int n = 1; n <= modeCount; ++n) {
      auto hertz = modeFrequency(system.value(), n);
      if (!hertz.ok()) return R::failure(hertz.error());
      auto decay = modeDecayRate(system.value(), n);
      if (!decay.ok()) return R::failure(decay.error());
      const double sigma = decay.value();
      const double damped = constants::twoPi * hertz.value().value();
      const double omega = std::sqrt(damped * damped + sigma * sigma);
      if (!std::isfinite(omega) || !(omega > sigma))
        return R::failure(QuantityError::RangeError);
      const double nn = static_cast<double>(n);
      Mode mode;
      mode.omega = omega;
      mode.sigma = sigma;
      mode.phiHammer = std::sin(nn * constants::pi * position);
      mode.phiListen = std::sin(nn * constants::pi * listenFraction);
      mode.drive = mode.phiHammer / modalMass;
      modes.push_back(mode);
    }
    const double fundamentalHz =
        std::sqrt(modes.front().omega * modes.front().omega -
                  modes.front().sigma * modes.front().sigma) /
        constants::twoPi;
    const double fundamentalDecay = modes.front().sigma;
    return R::success(FeltHammerVoice{
        rate, strike.massKg, strike.speedMetersPerSecond, feltStiffness,
        feltExponent, viscousPerMeter, std::move(modes), fundamentalHz,
        fundamentalDecay});
  }

  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }

  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(
      double* out, FrameCount n) override {
    using R = Result<FrameCount, QuantityError>;
    const std::uint64_t count = static_cast<std::uint64_t>(n.value());
    if (count == 0) return R::success(n);
    if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
    const double dt = 1.0 / rate_.value();
    for (std::uint64_t i = 0; i < count; ++i) {
      const double atHammer = contactDisplacement();
      const double rateAtHammer = contactVelocity();
      const double compression = hammerY_ - atHammer;
      const double compressionRate = hammerV_ - rateAtHammer;
      double force = 0.0;
      if (compression > 0.0) {
        const double factor = 1.0 + viscous_ * compressionRate;
        if (factor > 0.0)
          force = stiffness_ * std::pow(compression, exponent_) * factor;
      }
      if (!std::isfinite(force)) return R::failure(QuantityError::RangeError);
      for (Mode& mode : modes_) {
        auto stepped = stepConstantForceOscillator(
            OscillatorState{mode.q, mode.v}, mode.omega, mode.sigma,
            force * mode.drive, dt);
        if (!stepped.ok()) return R::failure(stepped.error());
        mode.q = stepped.value().displacement;
        mode.v = stepped.value().velocity;
      }
      hammerV_ += (-force / mass_) * dt;
      hammerY_ += hammerV_ * dt;
      if (!std::isfinite(hammerY_) || !std::isfinite(hammerV_))
        return R::failure(QuantityError::RangeError);
      double sample = 0.0;
      for (const Mode& mode : modes_) sample += mode.q * mode.phiListen;
      if (!std::isfinite(sample)) return R::failure(QuantityError::RangeError);
      out[i] = sample;
    }
    return R::success(n);
  }

  void reset() override {
    for (Mode& mode : modes_) {
      mode.q = 0.0;
      mode.v = 0.0;
    }
    hammerY_ = 0.0;
    hammerV_ = hammerSpeed_;
  }

  [[nodiscard]] double fundamentalHz() const noexcept { return fundamentalHz_; }
  [[nodiscard]] double fundamentalDecay() const noexcept {
    return fundamentalDecay_;
  }
  [[nodiscard]] double hammerDisplacement() const noexcept { return hammerY_; }

 private:
  struct Mode {
    double omega = 0.0;
    double sigma = 0.0;
    double q = 0.0;
    double v = 0.0;
    double phiHammer = 0.0;
    double phiListen = 0.0;
    double drive = 0.0;
  };

  FeltHammerVoice(SampleRateHz rate, double mass, double speed, double stiffness,
                  double exponent, double viscous, std::vector<Mode> modes,
                  double fundamentalHz, double fundamentalDecay)
      : rate_(rate),
        mass_(mass),
        hammerSpeed_(speed),
        hammerV_(speed),
        stiffness_(stiffness),
        exponent_(exponent),
        viscous_(viscous),
        modes_(std::move(modes)),
        fundamentalHz_(fundamentalHz),
        fundamentalDecay_(fundamentalDecay) {}

  [[nodiscard]] double contactDisplacement() const {
    double sum = 0.0;
    for (const Mode& mode : modes_) sum += mode.q * mode.phiHammer;
    return sum;
  }

  [[nodiscard]] double contactVelocity() const {
    double sum = 0.0;
    for (const Mode& mode : modes_) sum += mode.v * mode.phiHammer;
    return sum;
  }

  SampleRateHz rate_;
  double mass_;
  double hammerSpeed_;
  double hammerY_ = 0.0;
  double hammerV_;
  double stiffness_;
  double exponent_;
  double viscous_;
  std::vector<Mode> modes_;
  double fundamentalHz_;
  double fundamentalDecay_;
};

}  // namespace mmathen
