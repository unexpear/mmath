#pragma once

// A plucked course closed on a modal soundboard, then radiated (docs/48).
//
// The string is the tuned waveguide. stepYieldingBridge replaces the
// rigid bridge reflection: the end displacement is the soundboard's,
// and the force Z (v_in − v_out) drives each mode. Modal mass, Q, and
// resonance are arguments. The mode shape at the bridge is absorbed
// into that mass, as in body.h. No A0 or B1 is stored.
//
// Each mode is stepConstantForceOscillator with σ = ω/(2Q) and the
// specific force F/m. The bridge velocity is the sum of the modal
// velocities. That velocity is integrated into the end displacement,
// which is the feedback.
//
// The sample is the compact monopole in the time domain. The frequency
// form jωU is dU/dt, delayed by r/c:
//
//   p(t) = ρ A a(t − r/c) / (4 π r),
//
// A the caller's radiating area and a the bridge acceleration. Distance,
// area, and the air state are arguments. The delay is propagation, not
// an envelope.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "../math/constants.h"
#include "../physics/medium.h"
#include "../physics/oscillator.h"
#include "../physics/tuned_waveguide.h"
#include "../render/voice.h"
#include "body.h"
#include "plucked_string.h"

namespace mmathen {

class SoundboardVoice final : public IVoice {
 public:
  [[nodiscard]] static Result<SoundboardVoice, QuantityError> create(
      const StringCourse& course, const Excitation& pluck, LossModel loss,
      std::span<const BodyModeSpec> modes, double radiatingAreaM2,
      double distanceMeters, double airPressurePa, double temperatureKelvin,
      SampleRateHz rate) {
    using R = Result<SoundboardVoice, QuantityError>;
    const double fs = rate.value();
    if (!std::isfinite(fs) || !(fs > 0.0) || modes.empty() ||
        !std::isfinite(radiatingAreaM2) || !(radiatingAreaM2 > 0.0) ||
        !std::isfinite(distanceMeters) || !(distanceMeters > 0.0) ||
        !std::isfinite(airPressurePa) || !(airPressurePa > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    auto system = pluckedCourseSystem(course, loss, 0.0);
    if (!system.ok()) return R::failure(system.error());
    auto shape = pluckedCourseShape(system.value(), pluck);
    if (!shape.ok()) return R::failure(shape.error());
    auto design = designTunedLoop(system.value(), rate, 4, 0);
    if (!design.ok()) return R::failure(design.error());
    auto guide = TunedWaveguide::create(shape.value(), design.value().filters);
    if (!guide.ok()) return R::failure(guide.error());
    const double density = course.material.linearDensity.value();
    const double tension = course.tension.value();
    if (!(density > 0.0) || !(tension > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    const double impedance = density * std::sqrt(tension / density);
    if (!std::isfinite(impedance) || !(impedance > 0.0))
      return R::failure(QuantityError::RangeError);
    std::vector<Mode> built;
    built.reserve(modes.size());
    for (const BodyModeSpec& spec : modes) {
      auto resonator = makeBodyResonator(spec);
      if (!resonator.ok()) return R::failure(resonator.error());
      const double omega = resonator.value().omega0;
      const double sigma = omega / (2.0 * resonator.value().qualityFactor);
      if (!(sigma < omega)) return R::failure(QuantityError::DomainError);
      built.push_back(Mode{omega, sigma, resonator.value().modalMassKg, {}});
    }
    auto air = makeAir(TemperatureKelvin{temperatureKelvin});
    if (!air.ok()) return R::failure(air.error());
    auto speed = speedOfSound(air.value());
    if (!speed.ok()) return R::failure(speed.error());
    auto rho = airDensity(PressurePascals{airPressurePa},
                          TemperatureKelvin{temperatureKelvin});
    if (!rho.ok()) return R::failure(rho.error());
    const double travel = distanceMeters / speed.value().value();
    if (!std::isfinite(travel) || travel < 0.0)
      return R::failure(QuantityError::RangeError);
    const double samples = fs * travel;
    if (samples > static_cast<double>(std::numeric_limits<int>::max() / 4))
      return R::failure(QuantityError::RangeError);
    int delay = static_cast<int>(std::llround(samples));
    if (delay < 1) delay = 1;
    const double scale =
        rho.value().value() * radiatingAreaM2 /
        (4.0 * constants::pi * distanceMeters);
    if (!std::isfinite(scale) || !(scale > 0.0))
      return R::failure(QuantityError::RangeError);
    const LoopPartial fundamental = design.value().partials.front();
    SoundboardVoice voice(std::move(guide).value(), rate, impedance, scale, delay,
                          fundamental.frequency.value(), fundamental.decayRate,
                          std::move(built));
    return R::success(std::move(voice));
  }

  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }

  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(
      double* out, FrameCount n) override {
    using R = Result<FrameCount, QuantityError>;
    const std::uint64_t count = static_cast<std::uint64_t>(n.value());
    if (count == 0) return R::success(n);
    if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
    const double fs = rate_.value();
    const double dt = 1.0 / fs;
    for (std::uint64_t i = 0; i < count; ++i) {
      const double arrival = guide_.stepYieldingBridge(bridgeDisplacement_);
      const double outgoing = bridgeDisplacement_ - arrival;
      const double velocityIn = (arrival - previousArrival_) * fs;
      const double velocityOut = (outgoing - previousOutgoing_) * fs;
      const double force = impedance_ * (velocityIn - velocityOut);
      if (!std::isfinite(force)) return R::failure(QuantityError::RangeError);
      double velocity = 0.0;
      for (Mode& mode : modes_) {
        auto stepped = stepConstantForceOscillator(
            mode.state, mode.omega, mode.sigma, force / mode.mass, dt);
        if (!stepped.ok()) return R::failure(stepped.error());
        mode.state = stepped.value();
        velocity += mode.state.velocity;
      }
      if (!std::isfinite(velocity)) return R::failure(QuantityError::RangeError);
      const double acceleration = (velocity - previousVelocity_) * fs;
      bridgeDisplacement_ += velocity * dt;
      previousArrival_ = arrival;
      previousOutgoing_ = outgoing;
      previousVelocity_ = velocity;
      const double delayed = radiation_[radiationPos_];
      radiation_[radiationPos_] = acceleration;
      radiationPos_ = (radiationPos_ + 1) % radiation_.size();
      out[i] = scale_ * delayed;
    }
    return R::success(n);
  }

  void reset() override {
    guide_.reset();
    bridgeDisplacement_ = 0.0;
    previousArrival_ = 0.0;
    previousOutgoing_ = 0.0;
    previousVelocity_ = 0.0;
    for (Mode& mode : modes_) mode.state = {};
    std::fill(radiation_.begin(), radiation_.end(), 0.0);
    radiationPos_ = 0;
  }

  [[nodiscard]] double fundamentalHz() const noexcept { return fundamentalHz_; }
  [[nodiscard]] double stringDecay() const noexcept { return stringDecay_; }
  [[nodiscard]] std::size_t intervals() const noexcept {
    return guide_.filters().intervals;
  }
  // Interior rail displacement in metres, after the last step.
  [[nodiscard]] Result<double, QuantityError> stringDisplacement(
      std::size_t node) const {
    return guide_.valueAt(node);
  }

 private:
  struct Mode {
    double omega = 0.0;
    double sigma = 0.0;
    double mass = 0.0;
    OscillatorState state;
  };

  SoundboardVoice(TunedWaveguide guide, SampleRateHz rate, double impedance,
                  double scale, int delay, double fundamentalHz,
                  double stringDecay, std::vector<Mode> modes)
      : guide_(std::move(guide)),
        rate_(rate),
        impedance_(impedance),
        scale_(scale),
        fundamentalHz_(fundamentalHz),
        stringDecay_(stringDecay),
        modes_(std::move(modes)),
        radiation_(static_cast<std::size_t>(delay), 0.0) {}

  TunedWaveguide guide_;
  SampleRateHz rate_;
  double impedance_ = 0.0;
  double scale_ = 0.0;
  double fundamentalHz_ = 0.0;
  double stringDecay_ = 0.0;
  double bridgeDisplacement_ = 0.0;
  double previousArrival_ = 0.0;
  double previousOutgoing_ = 0.0;
  double previousVelocity_ = 0.0;
  std::vector<Mode> modes_;
  std::vector<double> radiation_;
  std::size_t radiationPos_ = 0;
};

}  // namespace mmathen
