#pragma once

// One pressure-controlled valve on a cylindrical bore delay (docs/48).
//
// The bore is a round trip of N = round(fs * 2L/c) samples. The far-end
// pressure reflection is the bore's open-end sign, −1, times the caller's
// magnitude in (0, 1]. The magnitude is not a bell filter: a radiation
// load is a later piece. |R| = 1 does not decay. After the blow, a reed
// or a lip is held shut, so the mouth is a closed end and the ringing
// samples are that delay. Decay of the traveling wave is −ln(|R|) per
// round trip. Holding the valve shut is the player stopping, not an
// amplitude envelope.
//
// Three mouths share the junction. An inward reed and an outward lip
// solve Z U(ΔP) + ΔP = p_m − 2 p_in by bisection. U is the odd Bernoulli
// jet: sign(ΔP) S sqrt(2|ΔP|/ρ). reedVolumeFlow still refuses ΔP < 0;
// this voice extends the same slit. S for the reed is reedAreaAtDisplacement.
// S for the lip is clamp(S0 + w y, 0, Smax), and the lip acceleration is
// trumpetLipAcceleration. The sample for those two is mouth pressure.
//
// A flute jet is not that closed junction. The embouchure reflection is
// −1, and the acoustic jet flow is added through the bore impedance:
//   p+ = −p_in + Z U_jet (2 fraction(y) − 1).
// fraction is fluteJetFraction. y at the edge is the flue displacement
// delayed by the regime-1 convection time W/(σ U). The flue displacement
// is the acoustic velocity times the jet's travel scale W/U, with the
// sign that pulls the jet out of the bore when flow enters the pipe.
// Positive y remains into the bore, as in flute.h. The sample is bore flow,
// because an open embouchure is a pressure node. The mouthpiece cup, the
// saxophone cone, and a bell flare are not in this delay.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "../math/constants.h"
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
struct ValveBlow {
  double pressurePa = 0.0;
  std::uint64_t frames = 0;
  double reflectionMagnitude = 0.0;
  double initialDisplacementMeters = 0.0;
};

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
    voice.jetDelay_.assign(static_cast<std::size_t>(convection),
                           blow.initialDisplacementMeters);
    voice.flueY_ = blow.initialDisplacementMeters;
    voice.flowOutput_ = true;
    voice.fundamentalHz_ = fs / static_cast<double>(voice.delaySamples_);
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
      const double incoming = reflection_ * line_[pos_];
      if (!std::isfinite(incoming)) return R::failure(QuantityError::RangeError);
      double sample = 0.0;
      double outgoing = 0.0;
      if (law_ == ValveLaw::Jet) {
        auto stepped = stepJet(incoming, outgoing, sample, dt);
        if (!stepped.ok()) return R::failure(stepped.error());
      } else if (law_ == ValveLaw::InwardReed) {
        auto stepped = stepReed(incoming, outgoing, sample, dt);
        if (!stepped.ok()) return R::failure(stepped.error());
      } else {
        auto stepped = stepLip(incoming, outgoing, sample, dt);
        if (!stepped.ok()) return R::failure(stepped.error());
      }
      line_[pos_] = outgoing;
      pos_ = (pos_ + 1) % line_.size();
      out[i] = sample;
      ++frame_;
    }
    return R::success(n);
  }

  void reset() override {
    std::fill(line_.begin(), line_.end(), 0.0);
    pos_ = 0;
    std::fill(jetDelay_.begin(), jetDelay_.end(), initialDisplacement_);
    jetPos_ = 0;
    displacement_ = initialDisplacement_;
    velocity_ = 0.0;
    flueY_ = initialDisplacement_;
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
        delaySamples_(delay),
        fundamentalHz_(fundamentalHz),
        reflectionDecay_(reflectionDecay),
        line_(static_cast<std::size_t>(delay), 0.0) {}

  [[nodiscard]] static bool blowSane(const ValveBlow& blow) {
    return std::isfinite(blow.pressurePa) && blow.pressurePa >= 0.0 &&
           std::isfinite(blow.reflectionMagnitude) &&
           blow.reflectionMagnitude > 0.0 && blow.reflectionMagnitude <= 1.0 &&
           std::isfinite(blow.initialDisplacementMeters);
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
    int delay = static_cast<int>(std::llround(samples));
    if (delay < 1) delay = 1;
    const double zeta = -blow.reflectionMagnitude;
    const double trip = static_cast<double>(delay) / fs;
    const double decay = -std::log(blow.reflectionMagnitude) / trip;
    const double hertz = (law == ValveLaw::Jet)
                             ? fs / static_cast<double>(delay)
                             : fs / (2.0 * static_cast<double>(delay));
    if (!std::isfinite(decay) || !std::isfinite(hertz) || !(hertz > 0.0))
      return R::failure(QuantityError::RangeError);
    BoreValveVoice voice(rate, law, blow, zeta, density, area, delay, hertz,
                         decay);
    voice.impedance_ = impedance;
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

  [[nodiscard]] Result<bool, QuantityError> stepReed(
      double incoming, double& outgoing, double& sample, double dt) {
    using R = Result<bool, QuantityError>;
    const bool open = frame_ < blowFrames_;
    const double mouth = open ? pressure_ : 0.0;
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
    sample = pressure;
    if (!std::isfinite(outgoing) || !std::isfinite(sample))
      return R::failure(QuantityError::RangeError);
    if (open) {
      auto acceleration = reedTipAcceleration(
          reed_.inertia, displacement_, velocity_, delta.value());
      if (!acceleration.ok()) return R::failure(acceleration.error());
      velocity_ += acceleration.value() * dt;
      displacement_ += velocity_ * dt;
      if (!std::isfinite(velocity_) || !std::isfinite(displacement_))
        return R::failure(QuantityError::RangeError);
    }
    return R::success(true);
  }

  [[nodiscard]] Result<bool, QuantityError> stepLip(
      double incoming, double& outgoing, double& sample, double dt) {
    using R = Result<bool, QuantityError>;
    const bool open = frame_ < blowFrames_;
    const double mouth = open ? pressure_ : 0.0;
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
    sample = pressure;
    if (!std::isfinite(outgoing) || !std::isfinite(sample))
      return R::failure(QuantityError::RangeError);
    if (open) {
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

  [[nodiscard]] Result<bool, QuantityError> stepJet(
      double incoming, double& outgoing, double& sample, double dt) {
    using R = Result<bool, QuantityError>;
    (void)dt;
    const bool open = frame_ < blowFrames_;
    const double mouth = open ? pressure_ : 0.0;
    double jetFlow = 0.0;
    if (mouth > 0.0) {
      auto flow = fluteJetFlow(jetFlute(), mouth);
      if (!flow.ok()) return R::failure(flow.error());
      jetFlow = flow.value();
    }
    const double yEdge = jetDelay_.empty() ? flueY_ : jetDelay_[jetPos_];
    auto fraction = fluteJetFraction(jetFlute(), yEdge);
    if (!fraction.ok()) return R::failure(fraction.error());
    const double acoustic = jetFlow * (2.0 * fraction.value() - 1.0);
    outgoing = -incoming + impedance_ * acoustic;
    const double boreFlow = (outgoing - incoming) / impedance_;
    sample = boreFlow;
    if (!std::isfinite(outgoing) || !std::isfinite(sample))
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
  ReedPack reed_{};
  Trumpet trumpet_{};
  std::vector<double> line_;
  std::vector<double> jetDelay_;
  std::size_t pos_ = 0;
  std::size_t jetPos_ = 0;
  std::uint64_t frame_ = 0;
};

}  // namespace mmathen
