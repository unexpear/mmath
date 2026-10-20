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
// is not attached: the hinged shapes are zero at the bridge, so that
// series cannot feed a bridge velocity back into the string.
//
// HammeredStringVoice is the path that can. The resonator is the tuned
// loop, split at the strike. The felt law is the same Hunt–Crossley
// force. With λ > 0 the string velocity v = v_in + F/(2Z) is solved
// with the factor (1 + λ ξ'), and a non-positive factor is loss of
// contact. The bridge is the yielding termination used by the bowed
// string: Z (2 v+ − v_bridge) drives the caller's modes, and their
// velocity replaces the rigid reflection. The sample is the compact
// monopole of the bridge acceleration. Frequency, Q, mass, area, and
// distance are arguments. Stulov's kernel is still not discretized.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "../math/constants.h"
#include "../physics/impedance.h"
#include "../physics/medium.h"
#include "../physics/oscillator.h"
#include "../physics/tuned_waveguide.h"
#include "../render/voice.h"
#include "body.h"
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

// Caller modes, area, distance, and air. The zeros are not a piano
// soundboard; create() refuses a non-positive area, distance, or pressure.
struct HammerBodyDrive {
  std::span<const BodyModeSpec> modes;
  double radiatingAreaM2 = 0.0;
  double distanceMeters = 0.0;
  double airPressurePa = 0.0;
  double temperatureKelvin = 0.0;
  // Optional per-mode effective areas A_n (m^2, signed), as in
  // SoundboardVoice: the radiated volume velocity is sum A_n v_n and
  // radiatingAreaM2 then only has to be positive (it is not a factor).
  // Empty keeps the single area: U = radiatingAreaM2 * sum v_n.
  std::span<const double> modeAreasM2 = {};
};

class HammeredStringVoice final : public IVoice {
 public:
  // fitPartials and dispersionStages follow designTunedLoop. A stiff
  // course needs at least one dispersion stage. The body is required:
  // this voice is the hammer, the waveguide, and the radiated board.
  [[nodiscard]] static Result<HammeredStringVoice, QuantityError> create(
      const StringCourse& course, const HammerStrike& strike, LossModel loss,
      double stiffnessKappa, double feltStiffness, double feltExponent,
      double viscousPerMeter, int fitPartials, int dispersionStages,
      SampleRateHz rate, const HammerBodyDrive& body) {
    using R = Result<HammeredStringVoice, QuantityError>;
    const double fs = rate.value();
    const double position = strike.positionFraction;
    if (!std::isfinite(fs) || !(fs > 0.0) || !std::isfinite(position) ||
        !(position > 0.0) || !(position < 1.0) || !std::isfinite(strike.massKg) ||
        !(strike.massKg > 0.0) || !std::isfinite(strike.speedMetersPerSecond) ||
        !(strike.speedMetersPerSecond > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    if (!std::isfinite(feltStiffness) || !(feltStiffness > 0.0) ||
        !std::isfinite(feltExponent) || !(feltExponent > 0.0) ||
        !std::isfinite(viscousPerMeter) || viscousPerMeter < 0.0)
      return R::failure(QuantityError::InvalidArgument);
    auto system = pluckedCourseSystem(course, loss, stiffnessKappa);
    if (!system.ok()) return R::failure(system.error());
    auto design =
        designTunedLoop(system.value(), rate, fitPartials, dispersionStages);
    if (!design.ok()) return R::failure(design.error());
    const LoopFilters filters = design.value().filters;
    if (filters.intervals < 2 ||
        filters.intervals > static_cast<std::size_t>(
                                std::numeric_limits<int>::max() / 2))
      return R::failure(QuantityError::InvalidArgument);
    const int loop = static_cast<int>(filters.intervals) * 2;
    int nut = static_cast<int>(std::llround(position * static_cast<double>(loop)));
    if (nut < 1) nut = 1;
    if (nut > loop - 1) nut = loop - 1;
    const int bridge = loop - nut;
    const double density = course.material.linearDensity.value();
    const double tension = course.tension.value();
    if (!std::isfinite(density) || !(density > 0.0) || !std::isfinite(tension) ||
        !(tension > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    const double impedance = density * std::sqrt(tension / density);
    if (!std::isfinite(impedance) || !(impedance > 0.0))
      return R::failure(QuantityError::RangeError);
    auto pole = loopPole(filters, rate, 1);
    if (!pole.ok()) return R::failure(pole.error());
    HammeredStringVoice voice{filters, rate, strike.massKg,
                              strike.speedMetersPerSecond, feltStiffness,
                              feltExponent, viscousPerMeter, impedance, bridge, nut,
                              pole.value().frequency.value(),
                              pole.value().decayRate};
    auto attached = voice.attachBody(body);
    if (!attached.ok()) return R::failure(attached.error());
    return R::success(std::move(voice));
  }

  // One sample of hammer and string against a bridge moving at
  // bridgeVelocity (the body's velocity from the previous sample). Returns
  // the force the string puts on the bridge, Z (2 v+ - v_bridge). The
  // voice's own board is not stepped: renderBlock does that, and a caller
  // that couples several strings to one shared board (piano unisons) steps
  // the strings here and sums the forces on that board.
  [[nodiscard]] Result<double, QuantityError> stepString(double bridgeVelocity) {
    using Rs = Result<double, QuantityError>;
    const double dt = 1.0 / rate_.value();
    const double twoZ = 2.0 * impedance_;
    const double delayedBridge = bridge_[bridgePos_];
    const double delayedNut = nut_[nutPos_];
    const double filtered = applyFilter(delayedBridge);
    if (!std::isfinite(filtered)) return Rs::failure(QuantityError::RangeError);
    const double incomingBridge = bridgeVelocity - filtered;
    const double incomingNut = -delayedNut;
    const double history = incomingBridge + incomingNut;
    const double compression = hammerY_ - stringY_;
    double force = 0.0;
    if (compression > 0.0) {
      const double amplitude = stiffness_ * std::pow(compression, exponent_);
      if (!std::isfinite(amplitude) || amplitude < 0.0)
        return Rs::failure(QuantityError::RangeError);
      if (viscous_ == 0.0) {
        force = amplitude;
      } else {
        const double openFactor = 1.0 + viscous_ * (hammerV_ - history);
        if (openFactor > 0.0) {
          const double denom = 1.0 + amplitude * viscous_ / twoZ;
          force = amplitude * openFactor / denom;
        }
      }
    }
    if (!std::isfinite(force) || force < 0.0)
      return Rs::failure(QuantityError::RangeError);
    const double added = force / twoZ;
    const double stringVelocity = history + added;
    const double outgoingBridge = incomingNut + added;
    const double outgoingNut = incomingBridge + added;
    if (!std::isfinite(stringVelocity) || !std::isfinite(outgoingBridge) ||
        !std::isfinite(outgoingNut))
      return Rs::failure(QuantityError::RangeError);
    bridge_[bridgePos_] = outgoingBridge;
    nut_[nutPos_] = outgoingNut;
    bridgePos_ = (bridgePos_ + 1) % bridge_.size();
    nutPos_ = (nutPos_ + 1) % nut_.size();
    hammerV_ += (-force / mass_) * dt;
    hammerY_ += hammerV_ * dt;
    stringY_ += stringVelocity * dt;
    if (!std::isfinite(hammerY_) || !std::isfinite(hammerV_) ||
        !std::isfinite(stringY_))
      return Rs::failure(QuantityError::RangeError);
    const double bridgeForce = impedance_ * (2.0 * filtered - bridgeVelocity);
    if (!std::isfinite(bridgeForce)) return Rs::failure(QuantityError::RangeError);
    return Rs::success(bridgeForce);
  }

  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }

  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(
      double* out, FrameCount n) override {
    return renderUnison(std::span<HammeredStringVoice>(), out, n);
  }

  // A unison: this voice and `others` all end on this voice's board, so
  // the bridge force is the sum of every string's force and every string
  // sees the one bridge velocity (Weinreich, JASA 62, 1474 (1977): the
  // coupled strings of a mistuned unison give the double decay). The
  // others' own boards are not stepped. Same rate required.
  [[nodiscard]] Result<FrameCount, QuantityError> renderUnison(
      std::span<HammeredStringVoice> others, double* out, FrameCount n) {
    using R = Result<FrameCount, QuantityError>;
    const std::uint64_t count = static_cast<std::uint64_t>(n.value());
    if (count == 0) return R::success(n);
    if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
    for (const HammeredStringVoice& other : others)
      if (&other == this || other.rate_.value() != rate_.value())
        return R::failure(QuantityError::InvalidArgument);
    const double fs = rate_.value();
    const double dt = 1.0 / fs;
    for (std::uint64_t i = 0; i < count; ++i) {
      auto stringStep = stepString(bridgeVelocity_);
      if (!stringStep.ok()) return R::failure(stringStep.error());
      double bridgeForce = stringStep.value();
      for (HammeredStringVoice& other : others) {
        auto otherStep = other.stepString(bridgeVelocity_);
        if (!otherStep.ok()) return R::failure(otherStep.error());
        bridgeForce += otherStep.value();
      }
      double bridgeVelocity = 0.0;
      double volume = 0.0;
      for (BodyMode& mode : modes_) {
        auto stepped = stepConstantForceOscillator(
            mode.state, mode.omega, mode.sigma, bridgeForce / mode.mass, dt);
        if (!stepped.ok()) return R::failure(stepped.error());
        mode.state = stepped.value();
        bridgeVelocity += mode.state.velocity;
        volume += mode.area * mode.state.velocity;
      }
      if (!std::isfinite(bridgeVelocity) || !std::isfinite(volume))
        return R::failure(QuantityError::RangeError);
      const double acceleration = (volume - volume_) * fs;
      bridgeVelocity_ = bridgeVelocity;
      volume_ = volume;
      const double delayed = radiation_[radiationPos_];
      radiation_[radiationPos_] = acceleration;
      radiationPos_ = (radiationPos_ + 1) % radiation_.size();
      const double sample = scale_ * delayed;
      if (!std::isfinite(sample)) return R::failure(QuantityError::RangeError);
      out[i] = sample;
    }
    return R::success(n);
  }

  void reset() override {
    std::fill(bridge_.begin(), bridge_.end(), 0.0);
    std::fill(nut_.begin(), nut_.end(), 0.0);
    bridgePos_ = 0;
    nutPos_ = 0;
    thX_ = thY_ = lsY_ = 0.0;
    std::fill(dsX_.begin(), dsX_.end(), 0.0);
    std::fill(dsY_.begin(), dsY_.end(), 0.0);
    hammerY_ = 0.0;
    hammerV_ = hammerSpeed_;
    stringY_ = 0.0;
    bridgeVelocity_ = 0.0;
    volume_ = 0.0;
    for (BodyMode& mode : modes_) mode.state = {};
    std::fill(radiation_.begin(), radiation_.end(), 0.0);
    radiationPos_ = 0;
  }

  [[nodiscard]] double fundamentalHz() const noexcept { return fundamentalHz_; }
  [[nodiscard]] double fundamentalDecay() const noexcept {
    return fundamentalDecay_;
  }
  [[nodiscard]] bool radiating() const noexcept { return true; }

 private:
  struct BodyMode {
    double omega = 0.0;
    double sigma = 0.0;
    double mass = 0.0;
    OscillatorState state;
    double area = 1.0;
  };

  [[nodiscard]] Result<int, QuantityError> attachBody(const HammerBodyDrive& body) {
    using R = Result<int, QuantityError>;
    const double fs = rate_.value();
    if (body.modes.empty() || !std::isfinite(body.radiatingAreaM2) ||
        !(body.radiatingAreaM2 > 0.0) || !std::isfinite(body.distanceMeters) ||
        !(body.distanceMeters > 0.0) || !std::isfinite(body.airPressurePa) ||
        !(body.airPressurePa > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    const bool perMode = !body.modeAreasM2.empty();
    if (perMode && body.modeAreasM2.size() != body.modes.size())
      return R::failure(QuantityError::InvalidArgument);
    std::vector<BodyMode> built;
    built.reserve(body.modes.size());
    for (const BodyModeSpec& spec : body.modes) {
      auto resonator = makeBodyResonator(spec);
      if (!resonator.ok()) return R::failure(resonator.error());
      const double omega = resonator.value().omega0;
      const double sigma = omega / (2.0 * resonator.value().qualityFactor);
      if (!(sigma < omega)) return R::failure(QuantityError::DomainError);
      const double area = perMode ? body.modeAreasM2[built.size()] : 1.0;
      if (!std::isfinite(area)) return R::failure(QuantityError::InvalidArgument);
      built.push_back(BodyMode{omega, sigma, resonator.value().modalMassKg, {}, area});
    }
    auto air = makeAir(TemperatureKelvin{body.temperatureKelvin});
    if (!air.ok()) return R::failure(air.error());
    auto speed = speedOfSound(air.value());
    if (!speed.ok()) return R::failure(speed.error());
    auto rho = airDensity(PressurePascals{body.airPressurePa},
                          TemperatureKelvin{body.temperatureKelvin});
    if (!rho.ok()) return R::failure(rho.error());
    const double travel = body.distanceMeters / speed.value().value();
    if (!std::isfinite(travel) || travel < 0.0)
      return R::failure(QuantityError::RangeError);
    const double samples = fs * travel;
    if (samples > static_cast<double>(std::numeric_limits<int>::max() / 4))
      return R::failure(QuantityError::RangeError);
    int delay = static_cast<int>(std::llround(samples));
    if (delay < 1) delay = 1;
    const double scale = rho.value().value() * (perMode ? 1.0 : body.radiatingAreaM2) /
                         (4.0 * constants::pi * body.distanceMeters);
    if (!std::isfinite(scale) || !(scale > 0.0))
      return R::failure(QuantityError::RangeError);
    modes_ = std::move(built);
    scale_ = scale;
    radiation_.assign(static_cast<std::size_t>(delay), 0.0);
    radiationPos_ = 0;
    bridgeVelocity_ = 0.0;
    volume_ = 0.0;
    return R::success(delay);
  }

  HammeredStringVoice(LoopFilters filters, SampleRateHz rate, double mass,
                      double speed, double stiffness, double exponent,
                      double viscous, double impedance, int bridge, int nut,
                      double fundamentalHz, double fundamentalDecay)
      : filters_(filters),
        rate_(rate),
        mass_(mass),
        hammerSpeed_(speed),
        hammerV_(speed),
        stiffness_(stiffness),
        exponent_(exponent),
        viscous_(viscous),
        impedance_(impedance),
        fundamentalHz_(fundamentalHz),
        fundamentalDecay_(fundamentalDecay),
        bridge_(static_cast<std::size_t>(bridge), 0.0),
        nut_(static_cast<std::size_t>(nut), 0.0),
        dsX_(static_cast<std::size_t>(filters.dispersionStages), 0.0),
        dsY_(static_cast<std::size_t>(filters.dispersionStages), 0.0) {}

  [[nodiscard]] double applyFilter(double x) {
    {
      const double y = filters_.thiran * x + thX_ - filters_.thiran * thY_;
      thX_ = x;
      thY_ = y;
      x = y;
    }
    for (int i = 0; i < filters_.dispersionStages; ++i) {
      const auto ii = static_cast<std::size_t>(i);
      const double y =
          filters_.dispersion * x + dsX_[ii] - filters_.dispersion * dsY_[ii];
      dsX_[ii] = x;
      dsY_[ii] = y;
      x = y;
    }
    {
      const double y = filters_.lossGain * (1.0 - filters_.lossPole) * x +
                       filters_.lossPole * lsY_;
      lsY_ = y;
      x = y;
    }
    return x;
  }

  LoopFilters filters_;
  SampleRateHz rate_;
  double mass_;
  double hammerSpeed_;
  double hammerY_ = 0.0;
  double hammerV_;
  double stiffness_;
  double exponent_;
  double viscous_;
  double impedance_;
  double fundamentalHz_;
  double fundamentalDecay_;
  std::vector<double> bridge_;
  std::vector<double> nut_;
  std::size_t bridgePos_ = 0;
  std::size_t nutPos_ = 0;
  double thX_ = 0.0;
  double thY_ = 0.0;
  double lsY_ = 0.0;
  std::vector<double> dsX_;
  std::vector<double> dsY_;
  double stringY_ = 0.0;
  double scale_ = 0.0;
  double bridgeVelocity_ = 0.0;
  double volume_ = 0.0;
  std::vector<BodyMode> modes_;
  std::vector<double> radiation_;
  std::size_t radiationPos_ = 0;
};

}  // namespace mmathen
