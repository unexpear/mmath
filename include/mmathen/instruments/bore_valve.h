#pragma once

// One pressure-controlled valve on a cylindrical bore delay (docs/48).
//
// The bore is a round trip of fs * 2L/c samples. The integer part is a
// delay line; the fraction is a first-order Thiran interpolator, or, when
// the far end is the Levine–Schwinger load, that fraction is added to the
// end correction so it is not applied twice. One sample per step is the
// Courant condition for a wave moving at c. The far-end pressure
// reflection is the bore's open-end sign, −1, times the caller's
// magnitude in (0, 1]. A positive radiusMeters replaces that constant
// with the unflanged open-end filter in physics/radiation.h: the
// Levine–Schwinger load, end correction and radius both arguments.
// |R| = 1 does not decay. After the blow, a reed
// or a lip is held shut, so the mouth is a closed end and the ringing
// samples are that delay. Decay of the traveling wave is −ln(|R|) per
// round trip. Holding the valve shut is the player stopping, not an
// amplitude envelope. attackFrames and releaseFrames are a raised-cosine
// ramp of the blowing pressure. Zero leaves a step.
//
// The sample is the radiated pressure of the bell, not the mouth
// pressure. A compact open end is a monopole, p = ρ (dU_bell/dt) / (4 π r),
// with r = listenerDistanceMeters. The derivative is the radiation
// high-pass: a steady mouth pressure does not radiate. r is an argument.
// The omitted value is one metre.
//
// Three mouths share the junction. An inward reed solves
// Z U + ΔP = p_m − 2 p_in with U the odd Bernoulli jet through the
// opening S(y). y is the reed mass on Dalmont's spring, stopped at the
// lay. The embouchure damping is the caller's: if it is light, the reed
// can ring at its own resonance instead of the bore. The lip keeps its
// mass: opening is clamp(S0 + w y, 0, Smax) and the acceleration is
// trumpetLipAcceleration. The spring continues past those area limits,
// as trumpet.h states: there is no contact solver on this cylinder.
// The cone voice stops the displacement at the rails, because otherwise
// a high blowing pressure winds y up and pins the opening at Smax.
// Outward-striking lips oscillate when their resonance sits below a
// bore peak. A quasi-static outward lip does not.
//
// A flute jet is not that closed junction. The embouchure reflection is
// −1, and the acoustic jet flow is added through the bore impedance.
// The edge sees a Boussinesq jet: the captured fraction is tanh of the
// delayed deflection over the jet thickness (the integral of a sech²
// profile), minus the value at the caller's aim. Aim zero is an odd
// function of deflection and matches the small-signal slope of
// 2 U_j y / h. A nonzero aim is an even-harmonic operating point.
// y at the edge is the flue displacement delayed by W/(σ U). The flue
// displacement is the acoustic velocity times W/U, with the sign that
// pulls the jet out of the bore when flow enters the pipe. The
// mouthpiece cup, the saxophone cone, and a bell flare are not in this
// delay.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "../math/constants.h"
#include "../physics/radiation.h"
#include "../render/voice.h"
#include "clarinet.h"
#include "flute.h"
#include "saxophone.h"
#include "trombone.h"
#include "trumpet.h"

namespace mmathen {

enum class ValveLaw { InwardReed, OutwardLip, Jet };

// pressurePa is the blowing pressure while the valve is open. frames is
// how long it stays open. reflectionMagnitude is |R| at the far end.
// initialDisplacementMeters is the reed or lip displacement, or the
// flute's centerline displacement, at the start. All four are arguments.
// radiusMeters and endCorrectionMeters are the unflanged-pipe load.
// Both are arguments. A non-positive radius means the far end stays the
// constant magnitude above.
// attackFrames and releaseFrames ramp the blowing pressure with a raised
// cosine. listenerDistanceMeters is the free-field distance of the
// radiated sample. jetAimMeters shifts the flute jet off the edge center;
// zero is centered.
struct ValveBlow {
  double pressurePa = 0.0;
  std::uint64_t frames = 0;
  double reflectionMagnitude = 0.0;
  double initialDisplacementMeters = 0.0;
  double radiusMeters = 0.0;
  double endCorrectionMeters = 0.0;
  std::uint64_t attackFrames = 0;
  std::uint64_t releaseFrames = 0;
  double listenerDistanceMeters = 1.0;
  double jetAimMeters = 0.0;
};

namespace valve_detail {

// Raised cosine, 0 at the start of the attack and at the end of the
// release. A zero attack or release is a step. Past blowFrames the
// player has stopped.
[[nodiscard]] inline double blowingPressure(double full, std::uint64_t blowFrames,
                                            std::uint64_t attackFrames,
                                            std::uint64_t releaseFrames,
                                            std::uint64_t frame) {
  if (!(full > 0.0) || frame >= blowFrames) return 0.0;
  if (attackFrames > 0 && frame < attackFrames) {
    const double x =
        static_cast<double>(frame) / static_cast<double>(attackFrames);
    return full * 0.5 * (1.0 - std::cos(constants::pi * x));
  }
  if (releaseFrames > 0 && blowFrames > releaseFrames &&
      frame + releaseFrames >= blowFrames) {
    const double x = static_cast<double>(blowFrames - frame) /
                     static_cast<double>(releaseFrames);
    return full * 0.5 * (1.0 - std::cos(constants::pi * x));
  }
  return full;
}

// Compact monopole of the bell flow, valid while ka is small:
// p = ρ (dU/dt) / (4 π r). Above ka = 1 the unflanged pipe stops
// getting brighter, so the derivative is smoothed with a one-pole at
// ω = c / a. The stored flow and state are the previous sample.
// Distance, corner, and dt are strictly positive.
[[nodiscard]] inline double radiateBell(double density, double distance, double flow,
                                        double& previousFlow, double& state,
                                        double cornerRadPerSec, double dt) {
  const double derivative = (flow - previousFlow) / dt;
  previousFlow = flow;
  const double gain = 1.0 - std::exp(-cornerRadPerSec * dt);
  state += gain * (derivative - state);
  return density * state / (4.0 * constants::pi * distance);
}

[[nodiscard]] inline bool blowShapeSane(const ValveBlow& blow) {
  if (!std::isfinite(blow.listenerDistanceMeters) ||
      !(blow.listenerDistanceMeters > 0.0) || !std::isfinite(blow.jetAimMeters))
    return false;
  if (blow.attackFrames > blow.frames || blow.releaseFrames > blow.frames)
    return false;
  if (blow.attackFrames > 0 && blow.releaseFrames > 0 &&
      blow.attackFrames + blow.releaseFrames > blow.frames)
    return false;
  return true;
}

}  // namespace valve_detail

class BoreValveVoice final : public IVoice {
 public:
  [[nodiscard]] static Result<BoreValveVoice, QuantityError> fromClarinet(
      const Clarinet& clarinet, const ValveBlow& blow, SampleRateHz rate) {
    return fromReed(clarinet.reed, clarinet.inertia, clarinet.bore.lengthMeters,
                    clarinet.bore.crossSectionM2,
                    clarinet.bore.soundSpeedMetersPerSecond,
                    clarinet.bore.airDensityKgPerM3,
                    clarinet.bore.acousticImpedance, blow, rate);
  }

  // The reed and the throat area of the saxophone, on a cylinder of the
  // saxophone's length. The apex distance, the bell area, and the cone
  // impedance are not read. This is not a truncated-cone stepper.
  [[nodiscard]] static Result<BoreValveVoice, QuantityError>
  fromSaxophoneCylinder(const Saxophone& saxophone, const ValveBlow& blow,
                        SampleRateHz rate) {
    return fromReed(saxophone.reed, saxophone.inertia, saxophone.lengthMeters,
                    saxophone.throatAreaM2, saxophone.soundSpeedMetersPerSecond,
                    saxophone.airDensityKgPerM3, saxophone.acousticImpedance,
                    blow, rate);
  }

  // The lip on the trumpet's cylinder. The cup compliance is not stepped.
  [[nodiscard]] static Result<BoreValveVoice, QuantityError> fromTrumpet(
      const Trumpet& trumpet, const ValveBlow& blow, SampleRateHz rate) {
    return fromLip(trumpet, blow, rate);
  }

  // The lip on the slide's air column L = L0 + 2x. The cup is not stepped.
  [[nodiscard]] static Result<BoreValveVoice, QuantityError> fromTrombone(
      const Trombone& trombone, const ValveBlow& blow, SampleRateHz rate) {
    return fromLip(trombone.bore, blow, rate);
  }

  [[nodiscard]] static Result<BoreValveVoice, QuantityError> fromFlute(
      const Flute& flute, const ValveBlow& blow, SampleRateHz rate) {
    using R = Result<BoreValveVoice, QuantityError>;
    if (!blowSane(blow) || !(blow.pressurePa > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    const double fs = rate.value();
    if (!std::isfinite(fs) || !(fs > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    auto travel = fluteConvectionTime(flute, blow.pressurePa);
    if (!travel.ok()) return R::failure(travel.error());
    const double samples = fs * travel.value();
    if (!std::isfinite(samples) ||
        samples > static_cast<double>(std::numeric_limits<int>::max() / 4))
      return R::failure(QuantityError::RangeError);
    int convection = static_cast<int>(std::llround(samples));
    if (convection < 1) convection = 1;
    auto built = makeCylinder(flute.lengthMeters, flute.crossSectionM2,
                              flute.soundSpeedMetersPerSecond,
                              flute.airDensityKgPerM3, flute.acousticImpedance,
                              ValveLaw::Jet, blow, rate);
    if (!built.ok()) return R::failure(built.error());
    BoreValveVoice voice = std::move(built).value();
    voice.jetArea_ = flute.jetAreaM2;
    voice.jetThickness_ = flute.jetThicknessMeters;
    voice.flueToEdge_ = flute.flueToEdgeMeters;
    voice.jetAim_ = blow.jetAimMeters;
    voice.jetDelay_.assign(static_cast<std::size_t>(convection),
                           blow.initialDisplacementMeters);
    voice.flueY_ = blow.initialDisplacementMeters;
    voice.flowOutput_ = false;
    return R::success(std::move(voice));
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
      const double arriving = readBore();
      const double incoming =
          radiating_ ? openEnd_.step(arriving) : reflection_ * arriving;
      if (!std::isfinite(incoming)) return R::failure(QuantityError::RangeError);
      double outgoing = 0.0;
      if (law_ == ValveLaw::Jet) {
        auto stepped = stepJet(incoming, outgoing, dt);
        if (!stepped.ok()) return R::failure(stepped.error());
      } else if (law_ == ValveLaw::InwardReed) {
        auto stepped = stepReed(incoming, outgoing, dt);
        if (!stepped.ok()) return R::failure(stepped.error());
      } else {
        auto stepped = stepLip(incoming, outgoing, dt);
        if (!stepped.ok()) return R::failure(stepped.error());
      }
      if (!std::isfinite(outgoing)) return R::failure(QuantityError::RangeError);
      line_[pos_] = outgoing;
      pos_ = (pos_ + 1) % line_.size();
      const double bellFlow = (arriving - incoming) / impedance_;
      out[i] = valve_detail::radiateBell(density_, listener_, bellFlow, bellFlow_,
                                        radiationState_, radiationCorner_, dt);
      if (!std::isfinite(out[i])) return R::failure(QuantityError::RangeError);
      ++frame_;
    }
    return R::success(n);
  }

  void reset() override {
    std::fill(line_.begin(), line_.end(), 0.0);
    pos_ = 0;
    boreX_ = 0.0;
    boreY_ = 0.0;
    openEnd_.reset();
    std::fill(jetDelay_.begin(), jetDelay_.end(), initialDisplacement_);
    jetPos_ = 0;
    displacement_ = initialDisplacement_;
    velocity_ = 0.0;
    flueY_ = initialDisplacement_;
    bellFlow_ = 0.0;
    radiationState_ = 0.0;
    frame_ = 0;
  }

  [[nodiscard]] ValveLaw law() const noexcept { return law_; }
  [[nodiscard]] bool outputsFlow() const noexcept { return flowOutput_; }
  [[nodiscard]] double fundamentalHz() const noexcept { return fundamentalHz_; }
  [[nodiscard]] double reflectionDecay() const noexcept {
    return reflectionDecay_;
  }
  [[nodiscard]] int delaySamples() const noexcept { return delaySamples_; }

 private:
  struct ReedPack {
    ReedCharacteristic reed;
    ReedInertia inertia;
  };

  BoreValveVoice(SampleRateHz rate, ValveLaw law, ValveBlow blow, double zeta,
                 double density, double area, int delay, double fundamentalHz,
                 double reflectionDecay)
      : rate_(rate),
        law_(law),
        pressure_(blow.pressurePa),
        blowFrames_(blow.frames),
        initialDisplacement_(blow.initialDisplacementMeters),
        displacement_(blow.initialDisplacementMeters),
        flueY_(blow.initialDisplacementMeters),
        reflection_(zeta),
        density_(density),
        area_(area),
        listener_(blow.listenerDistanceMeters),
        attackFrames_(blow.attackFrames),
        releaseFrames_(blow.releaseFrames),
        jetAim_(blow.jetAimMeters),
        delaySamples_(delay),
        fundamentalHz_(fundamentalHz),
        reflectionDecay_(reflectionDecay),
        line_(static_cast<std::size_t>(delay), 0.0) {}

  [[nodiscard]] static bool blowSane(const ValveBlow& blow) {
    return std::isfinite(blow.pressurePa) && blow.pressurePa >= 0.0 &&
           std::isfinite(blow.reflectionMagnitude) &&
           blow.reflectionMagnitude > 0.0 && blow.reflectionMagnitude <= 1.0 &&
           std::isfinite(blow.initialDisplacementMeters) &&
           valve_detail::blowShapeSane(blow);
  }

  [[nodiscard]] static Result<BoreValveVoice, QuantityError> makeCylinder(
      double length, double area, double speed, double density, double impedance,
      ValveLaw law, const ValveBlow& blow, SampleRateHz rate) {
    using R = Result<BoreValveVoice, QuantityError>;
    const double fs = rate.value();
    if (!blowSane(blow) || !std::isfinite(fs) || !(fs > 0.0) ||
        !std::isfinite(length) || !(length > 0.0) || !std::isfinite(area) ||
        !(area > 0.0) || !std::isfinite(speed) || !(speed > 0.0) ||
        !std::isfinite(density) || !(density > 0.0) ||
        !std::isfinite(impedance) || !(impedance > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    const double roundTrip = 2.0 * length / speed;
    const double samples = fs * roundTrip;
    if (!std::isfinite(roundTrip) || !(roundTrip > 0.0) || !std::isfinite(samples) ||
        samples > static_cast<double>(std::numeric_limits<int>::max() / 4))
      return R::failure(QuantityError::RangeError);
    int delay = static_cast<int>(std::floor(samples));
    double fraction = samples - static_cast<double>(delay);
    if (delay < 1) {
      delay = 1;
      fraction = 0.0;
    }
    if (fraction < 1.0e-9) fraction = 0.0;
    if (fraction > 1.0 - 1.0e-9) {
      fraction = 0.0;
      ++delay;
    }
    const bool radiating = blow.radiusMeters > 0.0;
    double zeta = -blow.reflectionMagnitude;
    double decay = 0.0;
    double hertz = 0.0;
    double boreThiran = 0.0;
    bool useBoreThiran = false;
    OpenEndReflection openEnd;
    if (radiating) {
      const double fractionMeters = fraction * speed / (2.0 * fs);
      auto filter = OpenEndReflection::create(
          blow.radiusMeters, blow.endCorrectionMeters + fractionMeters, speed, rate);
      if (!filter.ok()) return R::failure(filter.error());
      openEnd = std::move(filter).value();
      const bool openOpen = law == ValveLaw::Jet;
      auto solved = openEnd.resonanceHz(delay, openOpen);
      if (!solved.ok()) return R::failure(solved.error());
      auto response = openEnd.response(solved.value());
      if (!response.ok()) return R::failure(response.error());
      const double magnitude =
          std::hypot(response.value().real, response.value().imag);
      if (!(magnitude > 0.0) || !(magnitude <= 1.0))
        return R::failure(QuantityError::RangeError);
      hertz = solved.value();
      const double trip = openOpen ? 1.0 / hertz : 1.0 / (2.0 * hertz);
      decay = -std::log(magnitude) / trip;
      zeta = -1.0;
    } else {
      if (fraction > 0.0) {
        boreThiran = (1.0 - fraction) / (1.0 + fraction);
        if (!std::isfinite(boreThiran) || !(std::fabs(boreThiran) < 1.0))
          return R::failure(QuantityError::RangeError);
        useBoreThiran = true;
      }
      decay = -std::log(blow.reflectionMagnitude) / roundTrip;
      hertz = (law == ValveLaw::Jet) ? speed / (2.0 * length) : speed / (4.0 * length);
    }
    if (!std::isfinite(decay) || !std::isfinite(hertz) || !(hertz > 0.0))
      return R::failure(QuantityError::RangeError);
    BoreValveVoice voice(rate, law, blow, zeta, density, area, delay, hertz,
                         decay);
    voice.impedance_ = impedance;
    voice.radiating_ = radiating;
    voice.openEnd_ = std::move(openEnd);
    voice.boreThiran_ = boreThiran;
    voice.useBoreThiran_ = useBoreThiran;
    const double radius = std::sqrt(area / constants::pi);
    voice.radiationCorner_ = speed / radius;
    if (!std::isfinite(voice.radiationCorner_) || !(voice.radiationCorner_ > 0.0))
      return R::failure(QuantityError::RangeError);
    return R::success(std::move(voice));
  }

  [[nodiscard]] static Result<BoreValveVoice, QuantityError> fromReed(
      const ReedCharacteristic& reed, const ReedInertia& inertia, double length,
      double area, double speed, double density, double impedance,
      const ValveBlow& blow, SampleRateHz rate) {
    using R = Result<BoreValveVoice, QuantityError>;
    if (!(blow.pressurePa > 0.0)) return R::failure(QuantityError::InvalidArgument);
    auto built = makeCylinder(length, area, speed, density, impedance,
                              ValveLaw::InwardReed, blow, rate);
    if (!built.ok()) return R::failure(built.error());
    BoreValveVoice voice = std::move(built).value();
    voice.reed_.reed = reed;
    voice.reed_.inertia = inertia;
    voice.closing_ = inertia.closingDisplacementMeters;
    return R::success(std::move(voice));
  }

  [[nodiscard]] static Result<BoreValveVoice, QuantityError> fromLip(
      const Trumpet& trumpet, const ValveBlow& blow, SampleRateHz rate) {
    using R = Result<BoreValveVoice, QuantityError>;
    if (!(blow.pressurePa > 0.0)) return R::failure(QuantityError::InvalidArgument);
    auto built = makeCylinder(
        trumpet.lengthMeters, trumpet.crossSectionM2,
        trumpet.soundSpeedMetersPerSecond, trumpet.airDensityKgPerM3,
        trumpet.acousticImpedance, ValveLaw::OutwardLip, blow, rate);
    if (!built.ok()) return R::failure(built.error());
    BoreValveVoice voice = std::move(built).value();
    voice.trumpet_ = trumpet;
    voice.restOpening_ = trumpet.restOpeningAreaM2;
    voice.maximumOpening_ = trumpet.maximumOpeningAreaM2;
    voice.slitWidth_ = trumpet.slitWidthMeters;
    return R::success(std::move(voice));
  }


  [[nodiscard]] double readBore() {
    double x = line_[pos_];
    if (useBoreThiran_) {
      const double y = boreThiran_ * x + boreX_ - boreThiran_ * boreY_;
      boreX_ = x;
      boreY_ = y;
      x = y;
    }
    return x;
  }

  [[nodiscard]] double mouthNow() const {
    return valve_detail::blowingPressure(pressure_, blowFrames_, attackFrames_,
                                         releaseFrames_, frame_);
  }

  [[nodiscard]] Result<double, QuantityError> solveDelta(
      double opening, double target) const {
    using R = Result<double, QuantityError>;
    if (!(opening > 0.0)) return R::success(target);
    double lo = target < 0.0 ? target : 0.0;
    double hi = target > 0.0 ? target : 0.0;
    if (!(hi > lo)) return R::success(lo);
    for (int iteration = 0; iteration < 64; ++iteration) {
      const double mid = 0.5 * (lo + hi);
      const double flow = bernoulli(opening, mid);
      if (!std::isfinite(flow)) return R::failure(QuantityError::RangeError);
      const double residual = impedance_ * flow + mid - target;
      if (!std::isfinite(residual)) return R::failure(QuantityError::RangeError);
      if (residual > 0.0)
        hi = mid;
      else
        lo = mid;
    }
    return R::success(0.5 * (lo + hi));
  }

  [[nodiscard]] double bernoulli(double opening, double deltaP) const {
    if (!(opening > 0.0) || deltaP == 0.0) return 0.0;
    const double speed = std::sqrt(2.0 * std::fabs(deltaP) / density_);
    return std::copysign(opening * speed, deltaP);
  }

  [[nodiscard]] Result<bool, QuantityError> stepReed(double incoming, double& outgoing,
                                                     double dt) {
    using R = Result<bool, QuantityError>;
    const bool open = frame_ < blowFrames_;
    const double mouth = mouthNow();
    if (!open) {
      displacement_ = closing_;
      velocity_ = 0.0;
    }
    auto opening =
        reedAreaAtDisplacement(reed_.reed, reed_.inertia, displacement_);
    if (!opening.ok()) return R::failure(opening.error());
    const double target = mouth - 2.0 * incoming;
    auto delta = solveDelta(opening.value(), target);
    if (!delta.ok()) return R::failure(delta.error());
    const double pressure = mouth - delta.value();
    outgoing = pressure - incoming;
    if (!std::isfinite(outgoing) || !std::isfinite(pressure))
      return R::failure(QuantityError::RangeError);
    if (open && mouth > 0.0) {
      auto acceleration = reedTipAcceleration(
          reed_.inertia, displacement_, velocity_, delta.value());
      if (!acceleration.ok()) return R::failure(acceleration.error());
      velocity_ += acceleration.value() * dt;
      displacement_ += velocity_ * dt;
      if (!std::isfinite(velocity_) || !std::isfinite(displacement_))
        return R::failure(QuantityError::RangeError);
      if (displacement_ > closing_) {
        displacement_ = closing_;
        if (velocity_ > 0.0) velocity_ = 0.0;
      }
    }
    return R::success(true);
  }

  [[nodiscard]] Result<bool, QuantityError> stepLip(double incoming, double& outgoing,
                                                    double dt) {
    using R = Result<bool, QuantityError>;
    const bool open = frame_ < blowFrames_;
    const double mouth = mouthNow();
    if (!open) {
      displacement_ = -restOpening_ / slitWidth_;
      velocity_ = 0.0;
    }
    double opening = restOpening_ + slitWidth_ * displacement_;
    if (opening < 0.0) opening = 0.0;
    if (opening > maximumOpening_) opening = maximumOpening_;
    const double target = mouth - 2.0 * incoming;
    auto delta = solveDelta(opening, target);
    if (!delta.ok()) return R::failure(delta.error());
    const double pressure = mouth - delta.value();
    outgoing = pressure - incoming;
    if (!std::isfinite(outgoing) || !std::isfinite(pressure))
      return R::failure(QuantityError::RangeError);
    if (open && mouth > 0.0) {
      auto acceleration = trumpetLipAcceleration(
          trumpet_, displacement_, velocity_, delta.value());
      if (!acceleration.ok()) return R::failure(acceleration.error());
      velocity_ += acceleration.value() * dt;
      displacement_ += velocity_ * dt;
      if (!std::isfinite(velocity_) || !std::isfinite(displacement_))
        return R::failure(QuantityError::RangeError);
    }
    return R::success(true);
  }

  [[nodiscard]] Result<bool, QuantityError> stepJet(double incoming, double& outgoing,
                                                    double dt) {
    using R = Result<bool, QuantityError>;
    (void)dt;
    const bool open = frame_ < blowFrames_;
    const double mouth = mouthNow();
    double jetFlow = 0.0;
    if (mouth > 0.0) {
      auto flow = fluteJetFlow(jetFlute(), mouth);
      if (!flow.ok()) return R::failure(flow.error());
      jetFlow = flow.value();
    }
    const double yEdge = jetDelay_.empty() ? flueY_ : jetDelay_[jetPos_];
    // tanh is the captured fraction of a sech² jet. The factor 2 matches
    // the centered small-signal slope 2 U_j y / h of the top-hat jet.
    const double thickness = jetThickness_;
    if (!(thickness > 0.0)) return R::failure(QuantityError::InvalidArgument);
    const double eta = std::tanh(yEdge / thickness + jetAim_ / thickness);
    const double eta0 = std::tanh(jetAim_ / thickness);
    const double acoustic = jetFlow * 2.0 * (eta - eta0);
    outgoing = -incoming + impedance_ * acoustic;
    const double boreFlow = (outgoing - incoming) / impedance_;
    if (!std::isfinite(outgoing) || !std::isfinite(boreFlow))
      return R::failure(QuantityError::RangeError);
    // The flue channel sets the centerline from the acoustic velocity.
    // The scale W/U is the jet's own travel time across the mouth, not a
    // second time constant. Positive bore flow pulls the jet out of the
    // bore; the convection delay then carries that deflection to the edge.
    if (open && mouth > 0.0 && area_ > 0.0) {
      auto speed = fluteJetSpeed(jetFlute(), mouth);
      if (!speed.ok()) return R::failure(speed.error());
      if (speed.value() > 0.0) {
        flueY_ = -(boreFlow / area_) * (flueToEdge_ / speed.value());
        if (!std::isfinite(flueY_)) return R::failure(QuantityError::RangeError);
      }
    } else if (!open) {
      flueY_ = 0.0;
    }
    if (!jetDelay_.empty()) {
      jetDelay_[jetPos_] = flueY_;
      jetPos_ = (jetPos_ + 1) % jetDelay_.size();
    }
    return R::success(true);
  }

  // fluteJetFlow and fluteJetFraction need a Flute. The stored jet
  // numbers are copied onto a temporary that carries only those fields
  // plus the bore area already checked at construction.
  [[nodiscard]] Flute jetFlute() const {
    Flute flute;
    flute.lengthMeters = 1.0;
    flute.crossSectionM2 = area_;
    flute.soundSpeedMetersPerSecond = 1.0;
    flute.airDensityKgPerM3 = density_;
    flute.acousticImpedance = impedance_;
    flute.jetAreaM2 = jetArea_;
    flute.jetThicknessMeters = jetThickness_;
    flute.flueToEdgeMeters = 1.0;
    flute.convectionRatio = 1.0;
    flute.pressureReflection = -1.0;
    return flute;
  }

  SampleRateHz rate_;
  ValveLaw law_ = ValveLaw::InwardReed;
  double pressure_ = 0.0;
  std::uint64_t blowFrames_ = 0;
  double initialDisplacement_ = 0.0;
  double displacement_ = 0.0;
  double velocity_ = 0.0;
  double flueY_ = 0.0;
  double reflection_ = -1.0;
  double impedance_ = 0.0;
  double density_ = 0.0;
  double area_ = 0.0;
  double listener_ = 1.0;
  std::uint64_t attackFrames_ = 0;
  std::uint64_t releaseFrames_ = 0;
  double jetAim_ = 0.0;
  double bellFlow_ = 0.0;
  double radiationState_ = 0.0;
  double radiationCorner_ = 1.0;
  double boreThiran_ = 0.0;
  double boreX_ = 0.0;
  double boreY_ = 0.0;
  bool useBoreThiran_ = false;
  double closing_ = 0.0;
  double restOpening_ = 0.0;
  double maximumOpening_ = 0.0;
  double slitWidth_ = 0.0;
  double jetArea_ = 0.0;
  double jetThickness_ = 0.0;
  double flueToEdge_ = 0.0;
  int delaySamples_ = 1;
  double fundamentalHz_ = 0.0;
  double reflectionDecay_ = 0.0;
  bool flowOutput_ = false;
  bool radiating_ = false;
  OpenEndReflection openEnd_{};
  ReedPack reed_{};
  Trumpet trumpet_{};
  std::vector<double> line_;
  std::vector<double> jetDelay_;
  std::size_t pos_ = 0;
  std::size_t jetPos_ = 0;
  std::uint64_t frame_ = 0;
};

}  // namespace mmathen
