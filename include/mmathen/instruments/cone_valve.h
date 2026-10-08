#pragma once

// A reed or a lip on a truncated cone, with an optional unflanged bell
// (docs/48).
//
// The bore is the spherical wave of a cone: pressure p = p+ + p− at the
// throat, and
//
//   Z_c U = p+ − p− + (c / (s x0)) p.
//
// The 1/s term is bilinear, which is the trapezoidal step
// y ← y + β (p + p_prev), β = c dt / (2 x0). With the mouth sealed,
// U = 0 and the ring is the root of tan(kL) = −k x0. That is the
// saxophone cone, not a cylinder of the same length. Travel time is
// still 2L/c; the junction, not a different delay, moves the pitch.
// The traveling variable is the spherical-wave component at the throat,
// p± = ψ± / x0, so a bell pressure release is ψ− = −ψ+ and the throat
// sees the same −1. The round-trip delay is fs * 2L/c with a Thiran
// fraction. This is the Smith / Scavone cone, not a cylinder of length L.
//
// The played reed is the same mass on Dalmont's spring, stopped at the
// lay. Its resonance is not a cone mode; the caller damps it enough that
// the first root of tan(kL) = −k x0 is the one that speaks. The lip keeps
// its mass and is stopped at the opening rails. The sample is the
// radiated bell pressure ρ (dU_bell/dt) / (4 π r), smoothed above ka = 1.
// U_bell uses the throat traveling wave scaled by x_bell / x0, because ψ
// is carried unchanged to the bell and the bell area is
// S_throat (x_bell / x0)².
//
// A positive ValveBlow::radiusMeters replaces the ideal open-end −1
// with OpenEndReflection. The bell radius and the end correction are
// arguments. The geometric bell area is not used as that radius. A
// non-conical area exponent is BesselHornVoice, not a chain of cones.
// No bell curve is stored. A positive cup compliance on the lip path is
// the lumped volume in parallel with the throat: C dp/dt = U_lip − U_bore, with
// C = V/(ρ c²) taken from the trumpet. A zero cup leaves the junction
// unchanged. Cup volume is an argument. An extra length on the trumpet
// or the trombone is tubing the caller adds to L: engaged valve slides,
// or the trombone F attachment. Zero leaves this cone unchanged. No
// piston chart is stored.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "../math/constants.h"
#include "../physics/radiation.h"
#include "../render/voice.h"
#include "bore_valve.h"
#include "reed.h"
#include "saxophone.h"
#include "trombone.h"
#include "trumpet.h"

namespace mmathen {

class ConeValveVoice final : public IVoice {
 public:
  [[nodiscard]] static Result<ConeValveVoice, QuantityError> fromSaxophone(
      const Saxophone& saxophone, const ValveBlow& blow, SampleRateHz rate) {
    return create(saxophone.reed, saxophone.inertia, saxophone.lengthMeters,
                  saxophone.apexDistanceMeters, saxophone.throatAreaM2,
                  saxophone.soundSpeedMetersPerSecond,
                  saxophone.airDensityKgPerM3, saxophone.acousticImpedance,
                  saxophone.fundamentalHz, blow, rate, false, Trumpet{}, 0.0);
  }

  // The lip on a cone of the trumpet's length. Apex distance, bell
  // radius, and end correction are the blow's arguments: apex is not
  // stored on the trumpet. A positive cup compliance is stepped.
  // extraLengthMeters is the F attachment when it is engaged, and 0
  // when it is not. The slide length is already L0 + 2x on the trombone.
  [[nodiscard]] static Result<ConeValveVoice, QuantityError> fromTrombone(
      const Trombone& trombone, double apexDistanceMeters, const ValveBlow& blow,
      SampleRateHz rate, double extraLengthMeters = 0.0) {
    return fromTrumpet(trombone.bore, apexDistanceMeters, blow, rate,
                       extraLengthMeters);
  }

  // extraLengthMeters is the sum of the engaged valve slides. Zero is
  // the open instrument.
  [[nodiscard]] static Result<ConeValveVoice, QuantityError> fromTrumpet(
      const Trumpet& trumpet, double apexDistanceMeters, const ValveBlow& blow,
      SampleRateHz rate, double extraLengthMeters = 0.0) {
    using R = Result<ConeValveVoice, QuantityError>;
    if (!std::isfinite(apexDistanceMeters) || !(apexDistanceMeters > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    ReedCharacteristic unused{};
    ReedInertia inertia{};
    return create(unused, inertia, trumpet.lengthMeters, apexDistanceMeters,
                  trumpet.crossSectionM2, trumpet.soundSpeedMetersPerSecond,
                  trumpet.airDensityKgPerM3, trumpet.acousticImpedance, 0.0, blow,
                  rate, true, trumpet, extraLengthMeters);
  }

  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }

  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(
      double* out, FrameCount n) override {
    using R = Result<FrameCount, QuantityError>;
    const std::uint64_t count = static_cast<std::uint64_t>(n.value());
    if (count == 0) return R::success(n);
    if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
    const double dt = 1.0 / rate_.value();
    const double beta = 0.5 * alpha_ * dt;
    for (std::uint64_t i = 0; i < count; ++i) {
      const double arriving = readBore();
      const double incoming = radiating_ ? openEnd_.step(arriving) : -arriving;
      if (!std::isfinite(incoming)) return R::failure(QuantityError::RangeError);
      const bool open = frame_ < blowFrames_;
      const double mouth = valve_detail::blowingPressure(
          pressure_, blowFrames_, attackFrames_, releaseFrames_, frame_);
      if (!open) {
        if (lip_) {
          displacement_ = -restOpening_ / slitWidth_;
          velocity_ = 0.0;
        } else {
          displacement_ = closing_;
          velocity_ = 0.0;
        }
      }
      const double known = 2.0 * incoming - integrator_ - beta * previousPressure_;
      double pressure = 0.0;
      double opening = 0.0;
      if (lip_) {
        opening = restOpening_ + slitWidth_ * displacement_;
        if (opening < 0.0) opening = 0.0;
        if (opening > maximumOpening_) opening = maximumOpening_;
      } else if (open && mouth > 0.0) {
        auto area = reedAreaAtDisplacement(reed_, inertia_, displacement_);
        if (!area.ok()) return R::failure(area.error());
        opening = area.value();
      }
      auto solved = solvePressure(opening, mouth, known, beta, cupCoeff_, cupPressure_);
      if (!solved.ok()) return R::failure(solved.error());
      pressure = solved.value();
      const double outgoing = pressure - incoming;
      if (!std::isfinite(pressure) || !std::isfinite(outgoing))
        return R::failure(QuantityError::RangeError);
      integrator_ += beta * (pressure + previousPressure_);
      previousPressure_ = pressure;
      cupPressure_ = pressure;
      if (open && mouth > 0.0) {
        const double delta = mouth - pressure;
        double acceleration = 0.0;
        if (lip_) {
          auto stepped =
              trumpetLipAcceleration(trumpet_, displacement_, velocity_, delta);
          if (!stepped.ok()) return R::failure(stepped.error());
          acceleration = stepped.value();
        } else {
          auto stepped = reedTipAcceleration(inertia_, displacement_, velocity_, delta);
          if (!stepped.ok()) return R::failure(stepped.error());
          acceleration = stepped.value();
        }
        velocity_ += acceleration * dt;
        displacement_ += velocity_ * dt;
        if (!std::isfinite(velocity_) || !std::isfinite(displacement_))
          return R::failure(QuantityError::RangeError);
        if (lip_)
          stopLip();
        else if (displacement_ > closing_) {
          displacement_ = closing_;
          if (velocity_ > 0.0) velocity_ = 0.0;
        }
      }
      line_[pos_] = outgoing;
      pos_ = (pos_ + 1) % line_.size();
      const double bellFlow =
          (arriving - incoming) / impedance_ * bellScale_;
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
    wall_.reset();
    cutoff_.reset();
    integrator_ = 0.0;
    previousPressure_ = 0.0;
    cupPressure_ = 0.0;
    bellFlow_ = 0.0;
    radiationState_ = 0.0;
    displacement_ = initialDisplacement_;
    velocity_ = 0.0;
    frame_ = 0;
  }

  [[nodiscard]] double fundamentalHz() const noexcept { return fundamentalHz_; }
  [[nodiscard]] double reflectionDecay() const noexcept { return decay_; }
  [[nodiscard]] bool radiating() const noexcept { return radiating_; }
  [[nodiscard]] int delaySamples() const noexcept {
    return static_cast<int>(line_.size());
  }

 private:
  ConeValveVoice(SampleRateHz rate, ValveBlow blow, double alpha, double impedance,
                 double density, int delay, double fundamentalHz, bool lip,
                 bool radiating)
      : rate_(rate),
        pressure_(blow.pressurePa),
        blowFrames_(blow.frames),
        initialDisplacement_(blow.initialDisplacementMeters),
        displacement_(blow.initialDisplacementMeters),
        alpha_(alpha),
        impedance_(impedance),
        density_(density),
        listener_(blow.listenerDistanceMeters),
        attackFrames_(blow.attackFrames),
        releaseFrames_(blow.releaseFrames),
        fundamentalHz_(fundamentalHz),
        lip_(lip),
        radiating_(radiating),
        line_(static_cast<std::size_t>(delay), 0.0) {}

  [[nodiscard]] static Result<ConeValveVoice, QuantityError> create(
      const ReedCharacteristic& reed, const ReedInertia& inertia, double length,
      double apex, double area, double speed, double density, double impedance,
      double coneHz, const ValveBlow& blow, SampleRateHz rate, bool lip,
      const Trumpet& trumpet, double extraLength) {
    using R = Result<ConeValveVoice, QuantityError>;
    const double fs = rate.value();
    if (!std::isfinite(blow.pressurePa) || !(blow.pressurePa > 0.0) ||
        !std::isfinite(blow.reflectionMagnitude) ||
        !(blow.reflectionMagnitude > 0.0) || blow.reflectionMagnitude > 1.0 ||
        !std::isfinite(blow.initialDisplacementMeters) || !std::isfinite(fs) ||
        !(fs > 0.0) || !std::isfinite(length) || !(length > 0.0) ||
        !std::isfinite(apex) || !(apex > 0.0) || !std::isfinite(area) ||
        !(area > 0.0) || !std::isfinite(speed) || !(speed > 0.0) ||
        !std::isfinite(density) || !(density > 0.0) ||
        !std::isfinite(impedance) || !(impedance > 0.0) ||
        !std::isfinite(blow.endCorrectionMeters) || blow.endCorrectionMeters < 0.0 ||
        !std::isfinite(extraLength) || extraLength < 0.0 ||
        !valve_detail::blowShapeSane(blow))
      return R::failure(QuantityError::InvalidArgument);
    const double sounding = length + extraLength;
    if (!std::isfinite(sounding) || !(sounding > 0.0))
      return R::failure(QuantityError::RangeError);
    const double roundTrip = 2.0 * sounding / speed;
    double timed = fs * roundTrip;
    double wallPole = 0.0;
    double cutPole = 0.0;
    const bool lossy = blow.reflectionCutoffHz > 0.0;
    const double geometricGuess = speed / (2.0 * sounding);
    if (lossy) {
      // α ∝ 1/r and r grows linearly along the cone, so the trip uses
      // the log-mean radius, not the throat alone.
      const double throat = std::sqrt(area / constants::pi);
      const double ratio = (apex + sounding) / apex;
      const double logMean =
          ratio > 1.0 ? throat * (ratio - 1.0) / std::log(ratio) : throat;
      const double match =
          std::min(0.3 * fs, std::max(12.0 * geometricGuess, 4000.0));
      const double wall = valve_detail::kirchhoffTripMagnitude(
          match, logMean, sounding, speed, density);
      wallPole = valve_detail::onePoleMatching(wall, match, fs);
      cutPole = std::exp(-constants::twoPi * blow.reflectionCutoffHz / fs);
      if (!std::isfinite(cutPole) || !(cutPole > 0.0) || !(cutPole < 1.0) ||
          !std::isfinite(logMean) || !(logMean > 0.0))
        return R::failure(QuantityError::RangeError);
      if (!(blow.radiusMeters > 0.0)) {
        timed -= valve_detail::onePolePhaseDelay(wallPole, geometricGuess, fs);
        timed -= valve_detail::onePolePhaseDelay(cutPole, geometricGuess, fs);
      }
    }
    const double samples = timed;
    if (!std::isfinite(roundTrip) || !(roundTrip > 0.0) || !std::isfinite(samples) ||
        !(samples > 1.0) ||
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
    const double alpha = speed / apex;
    if (!std::isfinite(alpha) || !(alpha > 0.0))
      return R::failure(QuantityError::RangeError);
    const bool radiating = blow.radiusMeters > 0.0;
    OpenEndReflection openEnd;
    double decay = 0.0;
    double boreThiran = 0.0;
    bool useBoreThiran = false;
    if (radiating) {
      const double fractionMeters = fraction * speed / (2.0 * fs);
      auto filter = OpenEndReflection::create(
          blow.radiusMeters, blow.endCorrectionMeters + fractionMeters, speed, rate);
      if (!filter.ok()) return R::failure(filter.error());
      openEnd = std::move(filter).value();
      auto response =
          openEnd.response(coneHz > 0.0 ? coneHz : speed / (2.0 * sounding));
      if (!response.ok()) return R::failure(response.error());
      const double magnitude =
          std::hypot(response.value().real, response.value().imag);
      if (!(magnitude > 0.0) || magnitude > 1.0)
        return R::failure(QuantityError::RangeError);
      const double trip = static_cast<double>(delay) / fs;
      decay = -std::log(magnitude) / trip;
      if (!std::isfinite(decay)) return R::failure(QuantityError::RangeError);
    } else if (fraction > 0.0) {
      boreThiran = (1.0 - fraction) / (1.0 + fraction);
      if (!std::isfinite(boreThiran) || !(std::fabs(boreThiran) < 1.0))
        return R::failure(QuantityError::RangeError);
      useBoreThiran = true;
    }
    double hertz = coneHz;
    if (!(hertz > 0.0)) {
      const double thetaLeft = 0.5 * constants::pi;
      const double thetaRight = constants::pi;
      double lo = thetaLeft;
      double hi = thetaRight;
      const double ratio = apex / sounding;
      auto den = [&](double theta) {
        return std::sin(theta) + ratio * theta * std::cos(theta);
      };
      if (!(den(lo) * den(hi) < 0.0)) return R::failure(QuantityError::RangeError);
      for (int step = 0; step < 60; ++step) {
        const double mid = 0.5 * (lo + hi);
        if (den(lo) * den(mid) <= 0.0)
          hi = mid;
        else
          lo = mid;
      }
      hertz = (0.5 * (lo + hi)) * speed / (constants::twoPi * sounding);
    }
    if (!std::isfinite(hertz) || !(hertz > 0.0))
      return R::failure(QuantityError::RangeError);
    if (lossy) {
      const double extra = valve_detail::onePoleMagnitude(wallPole, hertz, fs) *
                           valve_detail::onePoleMagnitude(cutPole, hertz, fs);
      if (!(extra > 0.0) || !std::isfinite(extra))
        return R::failure(QuantityError::RangeError);
      const double trip = roundTrip > 0.0 ? roundTrip : static_cast<double>(delay) / fs;
      decay -= std::log(extra) / trip;
      if (!std::isfinite(decay)) return R::failure(QuantityError::RangeError);
    }
    ConeValveVoice voice(rate, blow, alpha, impedance, density, delay, hertz, lip,
                         radiating);
    voice.wall_.pole = wallPole;
    voice.cutoff_.pole = cutPole;
    voice.decay_ = decay;
    voice.openEnd_ = std::move(openEnd);
    voice.boreThiran_ = boreThiran;
    voice.useBoreThiran_ = useBoreThiran;
    voice.bellScale_ = (apex + sounding) / apex;
    if (!std::isfinite(voice.bellScale_) || !(voice.bellScale_ > 0.0))
      return R::failure(QuantityError::RangeError);
    const double bellRadius = std::sqrt(area / constants::pi) * voice.bellScale_;
    voice.radiationCorner_ = speed / bellRadius;
    // A complete-cone bell radius is not a measured brass bell. When the
    // caller supplies a radiation cutoff, that corner is the ka = 1
    // smoother on the radiated pressure.
    if (lossy) voice.radiationCorner_ = constants::twoPi * blow.reflectionCutoffHz;
    if (!std::isfinite(voice.radiationCorner_) || !(voice.radiationCorner_ > 0.0))
      return R::failure(QuantityError::RangeError);
    voice.reed_ = reed;
    voice.inertia_ = inertia;
    voice.closing_ = inertia.closingDisplacementMeters;
    voice.trumpet_ = trumpet;
    if (lip) {
      voice.restOpening_ = trumpet.restOpeningAreaM2;
      voice.maximumOpening_ = trumpet.maximumOpeningAreaM2;
      voice.slitWidth_ = trumpet.slitWidthMeters;
      if (!(voice.slitWidth_ > 0.0)) return R::failure(QuantityError::InvalidArgument);
      if (!std::isfinite(trumpet.acousticCompliance) ||
          trumpet.acousticCompliance < 0.0)
        return R::failure(QuantityError::InvalidArgument);
      if (trumpet.acousticCompliance > 0.0) {
        const double coeff = impedance * trumpet.acousticCompliance * fs;
        if (!std::isfinite(coeff) || !(coeff > 0.0))
          return R::failure(QuantityError::RangeError);
        voice.cupCoeff_ = coeff;
      }
    }
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
    return cutoff_.step(wall_.step(x));
  }

  void stopLip() {
    if (!(slitWidth_ > 0.0)) return;
    const double yMin = -restOpening_ / slitWidth_;
    const double yMax = (maximumOpening_ - restOpening_) / slitWidth_;
    if (displacement_ < yMin) {
      displacement_ = yMin;
      if (velocity_ < 0.0) velocity_ = 0.0;
    } else if (displacement_ > yMax) {
      displacement_ = yMax;
      if (velocity_ > 0.0) velocity_ = 0.0;
    }
  }


  [[nodiscard]] double bernoulli(double opening, double deltaP) const {
    if (!(opening > 0.0) || deltaP == 0.0) return 0.0;
    const double speed = std::sqrt(2.0 * std::fabs(deltaP) / density_);
    return std::copysign(opening * speed, deltaP);
  }

  // cupCoeff is Z C / dt. Zero is the junction without a cup, and the
  // residual then matches the sealed cone used before the cup existed.
  [[nodiscard]] Result<double, QuantityError> solvePressure(
      double opening, double mouth, double known, double beta, double cupCoeff,
      double cupPrev) const {
    using R = Result<double, QuantityError>;
    const double scale = 1.0 + beta + cupCoeff;
    const double held = known + cupCoeff * cupPrev;
    if (!(opening > 0.0)) return R::success(held / scale);
    auto residual = [&](double pressure) {
      return scale * pressure - impedance_ * bernoulli(opening, mouth - pressure) -
             held;
    };
    double lo = held / scale;
    double hi = lo;
    const double atLo = residual(lo);
    if (!std::isfinite(atLo)) return R::failure(QuantityError::RangeError);
    if (atLo == 0.0) return R::success(lo);
    if (atLo < 0.0) {
      hi = lo;
      for (int i = 0; i < 48; ++i) {
        hi = mouth + (mouth - lo) * 0.5 + static_cast<double>(i + 1) * 50.0;
        const double atHi = residual(hi);
        if (!std::isfinite(atHi)) return R::failure(QuantityError::RangeError);
        if (atHi >= 0.0) break;
        if (i == 47) return R::failure(QuantityError::RangeError);
      }
    } else {
      lo = hi;
      for (int i = 0; i < 48; ++i) {
        lo = mouth - (hi - mouth) * 0.5 - static_cast<double>(i + 1) * 50.0;
        const double at = residual(lo);
        if (!std::isfinite(at)) return R::failure(QuantityError::RangeError);
        if (at <= 0.0) break;
        if (i == 47) return R::failure(QuantityError::RangeError);
      }
    }
    for (int i = 0; i < 64; ++i) {
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
  double alpha_ = 0.0;
  double impedance_ = 0.0;
  double density_ = 0.0;
  double integrator_ = 0.0;
  double previousPressure_ = 0.0;
  double cupCoeff_ = 0.0;
  double cupPressure_ = 0.0;
  double listener_ = 1.0;
  double bellScale_ = 1.0;
  double bellFlow_ = 0.0;
  double radiationState_ = 0.0;
  double radiationCorner_ = 1.0;
  double boreThiran_ = 0.0;
  double boreX_ = 0.0;
  double boreY_ = 0.0;
  valve_detail::TripLoss wall_{};
  valve_detail::TripLoss cutoff_{};
  std::uint64_t attackFrames_ = 0;
  std::uint64_t releaseFrames_ = 0;
  bool useBoreThiran_ = false;
  double fundamentalHz_ = 0.0;
  double decay_ = 0.0;
  double closing_ = 0.0;
  double restOpening_ = 0.0;
  double maximumOpening_ = 0.0;
  double slitWidth_ = 0.0;
  bool lip_ = false;
  bool radiating_ = false;
  OpenEndReflection openEnd_{};
  ReedCharacteristic reed_{};
  ReedInertia inertia_{};
  Trumpet trumpet_{};
  std::vector<double> line_;
  std::size_t pos_ = 0;
  std::uint64_t frame_ = 0;
};

}  // namespace mmathen
