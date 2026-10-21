#pragma once

// A reed or a lip on a truncated cone, with an optional unflanged bell
// (docs/48).
//
// The bore is the spherical wave of a cone: pressure p = p+ + pâˆ’ at the
// throat, and
//
//   Z_c U = p+ âˆ’ pâˆ’ + (c / (s x0)) p.
//
// The 1/s term is bilinear, which is the trapezoidal step
// y â† y + Î² (p + p_prev), Î² = c dt / (2 x0). With the mouth sealed,
// U = 0 and the ring is the root of tan(kL) = âˆ’k x0. That is the
// saxophone cone, not a cylinder of the same length. Travel time is
// still 2L/c; the junction, not a different delay, moves the pitch.
// The traveling variable is the spherical-wave component at the throat,
// pÂ± = ÏˆÂ± / x0, so a bell pressure release is Ïˆâˆ’ = âˆ’Ïˆ+ and the throat
// sees the same âˆ’1. The round-trip delay is fs * 2L/c with a Thiran
// fraction. This is the Smith / Scavone cone, not a cylinder of length L.
//
// The played reed is the same mass on Dalmont's spring, stopped at the
// lay. Its resonance is not a cone mode; the caller damps it enough that
// the first root of tan(kL) = âˆ’k x0 is the one that speaks. The lip keeps
// its mass and is stopped at the opening rails. The sample is the
// radiated bell pressure Ï (dU_bell/dt) / (4 Ï€ r), smoothed above ka = 1.
// U_bell uses the throat traveling wave scaled by x_bell / x0, because Ïˆ
// is carried unchanged to the bell and the bell area is
// S_throat (x_bell / x0)Â².
//
// A positive ValveBlow::radiusMeters replaces the ideal open-end âˆ’1
// with OpenEndReflection. The bell radius and the end correction are
// arguments. The geometric bell area is not used as that radius. A
// positive bell length is a different termination: the input reflectance
// of a Bessel flare a(x) = b (x + x0)^{âˆ’Î³}, ended by Levineâ€“Schwinger at
// that same mouth. The horn travel is not added to the sounding length.
// A non-conical area exponent on the whole air column is
// BesselHornVoice, not a chain of cones. No measured bell curve is
// stored. A positive cup compliance on the lip path is
// the lumped volume in parallel with the throat: C dp/dt = U_lip âˆ’ U_bore, with
// C = V/(Ï cÂ²) taken from the trumpet. A zero cup leaves the junction
// unchanged. Cup volume is an argument. An extra length on the trumpet
// or the trombone is tubing the caller adds to L: engaged valve slides,
// or the trombone F attachment. Zero leaves this cone unchanged. No
// piston chart is stored.

#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "../math/constants.h"
#include "../physics/burgers.h"
#include "../physics/radiation.h"
#include "../render/voice.h"
#include "bore_valve.h"
#include "brass_bore.h"
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
    auto made = create(unused, inertia, trumpet.lengthMeters, apexDistanceMeters,
                       trumpet.crossSectionM2, trumpet.soundSpeedMetersPerSecond,
                       trumpet.airDensityKgPerM3, trumpet.acousticImpedance, 0.0, blow,
                       rate, true, trumpet, extraLengthMeters);
    if (!made.ok() || !(blow.burgersFraction > 0.0 || blow.lipQualityFactor > 0.0)) return made;
    ConeValveVoice voice = std::move(made).value();
    if (blow.lipQualityFactor > 0.0 && !voice.setLipQualityFactor(blow.lipQualityFactor))
      return R::failure(QuantityError::InvalidArgument);
    if (!(blow.burgersFraction > 0.0)) return R::success(std::move(voice));
    if (!std::isfinite(blow.burgersFraction) || !voice.setBoreBurgers(true, blow.burgersFraction))
      return R::failure(QuantityError::InvalidArgument);
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
    const double beta = 0.5 * alpha_ * dt;
    const double dcLeak = boreDcLeak(dt);
    for (std::uint64_t i = 0; i < count; ++i) {
      // The delay output is the wave that has traveled the bore. Wall loss
      // and the Nyquist zero are on that trip. The reflection cutoff shapes
      // only the wave that comes back to the reed: a passive open end
      // radiates what it does not reflect, so the bell flow uses the
      // incident wave, not the filtered return.
      const double incident = rfOn_ ? 0.0 : readBore();
      const double reflected = rfOn_ ? 0.0 : cutoff_.step(incident);
      const double incoming = rfOn_ ? convolveHistory(rf_, hist_)
                            : (flareOn_ ? -flareLoss_.step(reflected)
                                    : (radiating_ ? openEnd_.step(reflected) : -reflected));
      if (!std::isfinite(incoming)) return R::failure(QuantityError::RangeError);
      const bool open = frame_ < blowFrames_;
      double mouth = valve_detail::blowingPressure(
          pressure_, blowFrames_, attackFrames_, releaseFrames_, frame_);
      // The player's airway, when attached: lung target in, mouth
      // pressure with the tract's acoustic answer out (player_airway.h).
      if (airway_ != nullptr) mouth = airway_->mouthPressure(mouth);
      const double tongueOpen = airway_ != nullptr ? airway_->tongueOpenFraction() : 1.0;
      // A disturbance of this blowing pressure is cancelled by the pressure
      // search, so it does not radiate, and on the shorter tubes it is
      // enough to lock the second mode. The fraction is applied to the
      // radiated pressure below, after the loop. A lip uses the same
      // radiated breath, with the lip period in place of the reed period.
      // A tongue holds the valve shut while mouth pressure rises, then
      // lets go. The release frame is the start of the acoustic attack.
      // Stopping the breath (the stored pressure is zero, or the blow
      // has ended) parks the lip on the closed stop. The bore and bell
      // then ring at the loop's own loss. The radiated sample is not
      // scaled. A tongue back on the mouthpiece is a later step and is
      // not this seal.
      const bool breathOff = lip_ && !(pressure_ > 0.0);
      const bool tongued = tongueFrames_ > 0 && frame_ < tongueFrames_;
      if (!open || tongued || breathOff) {
        if (lip_) {
          displacement_ = lipPreload() - restOpening_ / slitWidth_;
          velocity_ = 0.0;
        } else {
          displacement_ = closing_;
          velocity_ = 0.0;
        }
      } else if (tongueFrames_ > 0 && frame_ == tongueFrames_) {
        // A reed leaves the lay at the caller's initial displacement.
        // A lip stays on the closed stop; blowing pressure accelerates
        // it. Placing the lip at its rest opening would start the note
        // already open.
        if (!lip_) displacement_ = initialDisplacement_;
        velocity_ = 0.0;
      }
      const double known = 2.0 * incoming - integrator_ - beta * previousPressure_;
      double pressure = 0.0;
      double opening = 0.0;
      if (lip_) {
        opening = restOpening_ + slitWidth_ * (displacement_ - lipPreload());
        if (opening < 0.0) opening = 0.0;
        if (opening > maximumOpening_) opening = maximumOpening_;
        if (as_.on) opening = std::max(2.0 * as_.b * as_.y, 0.0);
        // A tongue, a finished blow, or a breath pressure of zero holds
        // the lip on the closed stop. The stop arithmetic can leave a
        // roundoff slit, and the pressure search then fails against a
        // bore that is already sounding.
        if (tongued || !open || breathOff) opening = 0.0;
        // The closed stop is âˆ’rest/slit. Storing that displacement and
        // multiplying back by the slit leaves a roundoff opening, about
        // 1eâˆ’22 mÂ². solvePressure then cannot bracket: the Bernoulli
        // term is femtopascals, the residual at the sealed guess does
        // not change sign, and a sounding bore aborts the note. A buzz
        // is a finite fraction of the rest opening, so this cut is
        // roundoff, not a lip area.
        // The tongue and the lips are two orifices in series (jb11). The
        // tongue channel is tongueOpen times the oral tract's area
        // (PlayerAirway::oralAreaM2, about 5.8 cm^2); the lip slit is
        // about 1e-5 m^2. Steady Bernoulli flow through both: the gap
        // between them sits at p_gap = p_cup + (p_mouth - p_cup) At^2 /
        // (At^2 + Al^2) and the flow sees At Al / sqrt(At^2 + Al^2).
        // Under the closed tongue the lips feel no mouth pressure and
        // rest; once the tongue has moved a few percent of the way the
        // gap is at mouth pressure, so the lips take the pressure as a
        // step (a plosive, Bianco et al., JNMR 41, 49 (2012): the lips
        // start in the pressure drop of the tongue release) and ring at
        // amplitude, instead of following the 8 ms tongue path and
        // growing from the noise seed. Multiplying the slit by the
        // tongue fraction (the earlier model) held the lips at their
        // blown equilibrium under the tongue and gave no step.
        if (airway_ != nullptr) {
          const double at = tongueOpen * airway_->oralAreaM2(), al = opening;
          lipSide_ = at > 0.0 ? at * at / (at * at + al * al) : 0.0;
          opening = (at > 0.0 && al > 0.0) ? at * al / std::sqrt(at * at + al * al) : 0.0;
        } else {
          lipSide_ = 1.0;
          opening *= tongueOpen;
        }
        if (restOpening_ > 0.0 && opening < restOpening_ * 1.0e-6) opening = 0.0;
        lastOpening_ = opening;
      } else if (open && mouth > 0.0) {
        auto area = reedAreaAtDisplacement(reed_, inertia_, displacement_);
        if (!area.ok()) return R::failure(area.error());
        // The tongue on the reed shuts part of the channel (Li et al. 2016).
        opening = area.value() * tongueOpen;
      }
      auto solved = solvePressure(opening, mouth, known, beta, cupCoeff_, cupPressure_);
      if (!solved.ok()) return R::failure(solved.error());
      pressure = solved.value();
      apertureFlow_ = 0.0;
      if (open && mouth > 0.0 && !tongued && opening > 0.0)
        apertureFlow_ = std::fabs(bernoulli(opening, mouth - pressure));
      double outgoing = pressure - incoming;
      if (apertureFlow_ > 0.0) {
        // Reed: f_res. Lip: the caller's lip resonance. Both seed the
        // bore with a slice of the aperture flow. Fletcher and Rossing,
        // chap. 13, call that jet turbulent and print no ratio.
        double seedHz = inertia_.resonanceHz;
        if (lip_) {
          if (trumpet_.lipMassKg > 0.0 && trumpet_.lipStiffnessNewtonsPerMeter > 0.0) {
            seedHz = std::sqrt(trumpet_.lipStiffnessNewtonsPerMeter / trumpet_.lipMassKg) /
                     constants::twoPi;
          } else {
            seedHz = 0.0;
          }
        }
        // A lip's seed is the breath the buzz grows from. Once the buzz
        // is louder than that jet the seed stops, as the radiated jet
        // does below: a seed left on under a locked brass note is the
        // same broadband floor in the bore.
        if (!(lip_ && lipLocked_))
          outgoing += valve_detail::reedGapSeedPressure(
              noise_, breathState_, apertureFlow_, reedTurbulence_, impedance_, seedHz, dt);
      }
      if (!std::isfinite(pressure) || !std::isfinite(outgoing))
        return R::failure(QuantityError::RangeError);
      integrator_ = dcLeak * integrator_ + beta * (pressure + previousPressure_);
      previousPressure_ = pressure;
      cupPressure_ = pressure;
      if (open && mouth > 0.0 && !tongued) {
        const double delta = (lip_ ? lipSide_ : 1.0) * (mouth - pressure);  // lips feel the gap pressure
        double acceleration = 0.0;
        if (lip_ && as_.on) {
          stepAdachiSato(delta, pressure, opening, dt);
        } else if (lip_) {
          // Two-lip contact, when enabled, is measured from this voice's
          // closed stop, which carries the threshold preload.
          Trumpet lipNow = trumpet_;
          if (lipNow.lipCollisionStiffness > 0.0)
            lipNow.lipClosureMeters = lipPreload() - restOpening_ / slitWidth_;
          // The contact spring and damper are stepped backward-Euler
          // (implicit in x and v), so a stiff, heavily damped contact
          // stays stable at the audio rate; the free lip keeps the
          // semi-implicit step below.
          const bool touching = lipNow.lipCollisionStiffness > 0.0 &&
                                displacement_ < lipNow.lipClosureMeters;
          if (touching) {
            Trumpet free = lipNow;
            free.lipCollisionStiffness = 0.0;
            auto f = trumpetLipAcceleration(free, displacement_, velocity_, delta);
            if (!f.ok()) return R::failure(f.error());
            const double m = lipNow.lipMassKg, kc = lipNow.lipCollisionStiffness,
                         rc = lipNow.lipCollisionDamping;
            const double pen = displacement_ - lipNow.lipClosureMeters;
            const double vNew = (velocity_ + dt * (f.value() - kc * pen / m)) /
                                (1.0 + dt * rc / m + dt * dt * kc / m);
            acceleration = (vNew - velocity_) / dt;
          } else {
            auto stepped =
                trumpetLipAcceleration(lipNow, displacement_, velocity_, delta);
            if (!stepped.ok()) return R::failure(stepped.error());
            acceleration = stepped.value();
          }
        } else {
          auto moved = stepReedTip(inertia_, displacement_, velocity_, delta, dt);
          if (!moved.ok()) return R::failure(moved.error());
          displacement_ = moved.value().displacementMeters;
          velocity_ = moved.value().velocityMetersPerSecond;
          if (displacement_ > closing_) {
            displacement_ = closing_;
            if (velocity_ > 0.0) velocity_ = 0.0;
          }
        }
        if (lip_ && !as_.on) {
          velocity_ += acceleration * dt;
          displacement_ += velocity_ * dt;
          if (!std::isfinite(velocity_) || !std::isfinite(displacement_))
            return R::failure(QuantityError::RangeError);
          stopLip();
        }
      }
      if (rfOn_) {
        hist_[hpos_] = outgoing;
        if (steepRfOn_) histS_[hpos_] = steepenMeasured(outgoing);
        hpos_ = (hpos_ + 1) % hist_.size();
      } else {
        line_[pos_] = outgoing;
        pos_ = (pos_ + 1) % line_.size();
      }
      // One pass of cylindrical steepening on the wave that has already
      // traveled the bore. The lip keeps the linear return, so the
      // limit cycle (and a length change) stays the one the note was
      // tuned with. See ValveBlow::brassiness.
      double atBell = incident;
      if (!rfOn_ && lip_ && burgersOn_) {
        if (soundingLength_ != burgersLength_ && soundingLength_ > 0.0) {
          burgersLength_ = soundingLength_;
          burgers_.setLength(burgersFraction_ * soundingLength_, rate_.value());
        }
        // The wave enters the line before the trip's wall loss; the loss is
        // the trip's own one-pole and gain, lumped at the middle step
        // (BurgersLine::setLumpedLoss), so steepening acts on both sides of
        // it and the linear magnitude is the trip's at every frequency. The
        // earlier spread (32 one-poles matched at 1 kHz) cut 4 kHz by 11 dB
        // more than the trip at pole 0.8 and darkened the bell (Lick
        // excerpt centroid 741 Hz; lumped 1150, recording 1436).
        if (wall_.pole != burgersWallPole_ || wall_.gain != burgersWallGain_) {
          burgersWallPole_ = wall_.pole;
          burgersWallGain_ = wall_.gain;
          const double gain = wall_.gain > 0.0 && wall_.gain <= 1.0 ? wall_.gain : 1.0;
          const int lumps = burgersLossLumps_;
          const double pole = lumps == 1 ? std::max(0.0, wall_.pole)
                                         : sectionPole(std::max(0.0, wall_.pole), lumps, rate_.value());
          burgers_.setSectionLoss(pole, std::pow(gain, 1.0 / lumps), lumps);
        }
        atBell = burgers_.step(preWall_);
      } else if (!rfOn_ && lip_ && brassiness_ > 0.0) {
        atBell = steepenAtBell(incident);
      }
      // The Burgers line delays its output `stages` samples; the return
      // wave R p+ is delayed to match, so the bell sees (1 + R) of the
      // same p+. R of the steepening's own harmonics is left out: a
      // flared bell reflects little above its cutoff, where they lie.
      double atBellReturn = incoming;
      if (!rfOn_ && lip_ && !burgersOn_ && brassiness_ > 0.0) {
        // steepenAtBell reads its wave four samples late (kSteepAtBellDelay);
        // the return is delayed the same so (1 + R) acts on one p+.
        atBellReturn = steepReturn_[steepReturnPos_];
        steepReturn_[steepReturnPos_] = incoming;
        steepReturnPos_ = (steepReturnPos_ + 1) % kSteepAtBellDelay;
      }
      if (!rfOn_ && lip_ && burgersOn_) {
        const std::size_t m = static_cast<std::size_t>(burgers_.stages());
        if (burgersReturn_.size() != m) { burgersReturn_.assign(m, 0.0); burgersPos_ = 0; }
        atBellReturn = burgersReturn_[burgersPos_];
        burgersReturn_[burgersPos_] = incoming;
        burgersPos_ = (burgersPos_ + 1) % m;
      }
      // ValveBlow::bellTransmission: the transmitted wave (1 + R) p+.
      const double bellFlow = rfOn_ ? convolveHistory(uf_, steepRfOn_ ? histS_ : hist_)
                              : (bellTransmit_ && flareOn_)
                                    ? (atBell + atBellReturn) / impedance_ * bellScale_
                                    : (atBell - atBellReturn) / impedance_ * bellScale_;
      double radiated = valve_detail::radiateBell(density_, listener_, bellFlow, bellFlow_,
                                                 radiationState_, radiationCorner_, dt);
      // Benade and Lutgen's external envelope, when a break frequency
      // was supplied. The monopole above is still the bell flow; this
      // transfer is the measured saxophone spectrum of a beating reed.
      if (break_.active()) radiated = break_.step(radiated);
      // Reed-gap or lip jet, after the loop. The same mouth as the
      // clarinet. See reedApertureBreath.
      if (reedTurbulence_ > 0.0 && open && mouth > 0.0 && !tongued) {
        const double trip =
            line_.empty() ? 1.0 : static_cast<double>(line_.size());
        double period = 0.0;
        if (!lip_ && inertia_.resonanceHz > 0.0) period = 1.0 / inertia_.resonanceHz;
        if (lip_ && trumpet_.lipMassKg > 0.0 &&
            trumpet_.lipStiffnessNewtonsPerMeter > 0.0) {
          const double omega =
              std::sqrt(trumpet_.lipStiffnessNewtonsPerMeter / trumpet_.lipMassKg);
          if (omega > 0.0) period = constants::twoPi / omega;
        }
        // A lip radiates the jet only while it is louder than the tone
        // (the breath before the buzz). Once the buzz is louder, the
        // sustained fraction is zero: the 0.20 fraction on a locked brass
        // note was a steady broadband floor about 23 dB under the tone
        // above 6 kHz, the fizz heard on the excerpts. Fletcher and
        // Rossing (chap. 13) call the lip jet turbulent and print no
        // radiated ratio, so no sustained level is claimed here.
        radiated += valve_detail::reedApertureBreath(
            flowMean_, breathEnv_, noise_, breathState_, apertureFlow_, radiated,
            reedTurbulence_, density_, listener_, trip, rate_.value(), period,
            /*sustain=*/!lip_, &lipLocked_, lockedBreath_);
      }
      out[i] = radiated;
      if (airway_ != nullptr) airway_->step(apertureFlow_);
      if (!std::isfinite(out[i])) return R::failure(QuantityError::RangeError);
      ++frame_;
    }
    return R::success(n);
  }

  void reset() override {
    std::fill(line_.begin(), line_.end(), 0.0);
    lipSide_ = 1.0;
    pos_ = 0;
    boreX_ = 0.0;
    boreY_ = 0.0;
    nyquist_ = 0.0;
    openEnd_.reset();
    wall_.reset();
    cutoff_.reset();
    integrator_ = 0.0;
    previousPressure_ = 0.0;
    cupPressure_ = 0.0;
    bellFlow_ = 0.0;
    radiationState_ = 0.0;
    break_.reset();
    noise_ = 1u;
    breathEnv_ = 0.0;
    breathState_ = 0.0;
    flowMean_ = 0.0;
    apertureFlow_ = 0.0;
    lipLocked_ = false;
    prevOutgoing_ = 0.0;
    std::fill(steepHist_, steepHist_ + 16, 0.0);
    burgers_.reset();
    burgersWallPole_ = -1.0;
    burgersWallGain_ = -1.0;
    std::fill(burgersReturn_.begin(), burgersReturn_.end(), 0.0);
    std::fill(steepReturn_, steepReturn_ + kSteepAtBellDelay, 0.0);
    steepReturnPos_ = 0;
    steepPos_ = 0;
    displacement_ = initialDisplacement_;
    velocity_ = 0.0;
    frame_ = 0;
    if (playingWallSaved_) wall_.pole = playingWallPole_;
    playingWallSaved_ = false;
    flareLoss_.reset();
    flareMatchHz_ = 0.0;
  }

  [[nodiscard]] double fundamentalHz() const noexcept { return fundamentalHz_; }
  [[nodiscard]] double reflectionDecay() const noexcept { return decay_; }

  // Half-power bandwidth of this bore's loop at frequencyHz.
  // Î”f = f/Q. The traveling wave is multiplied by the loop magnitude
  // once per round trip T = 2L/c, so the amplitude is exp(t ln|H| / T).
  // A mode's amplitude is exp(âˆ’Ï€ Î”f t), and 20 log10 of that falls at
  // 20 log10(e) Ï€ Î”f â‰ˆ 27.29 Î”f decibels per second. Wall loss, the
  // reflection cutoff, and an unflanged bell, when one is engaged, are
  // the loop. The lip is not in |H|.
  [[nodiscard]] double halfPowerBandwidthHz(double frequencyHz) const {
    const double fs = rate_.value();
    if (!std::isfinite(frequencyHz) || !(frequencyHz > 0.0) || !(fs > frequencyHz * 2.0) ||
        !(soundingLength_ > 0.0) || !(speed_ > 0.0))
      return 0.0;
    double magnitude = valve_detail::onePoleMagnitude(wall_.pole, frequencyHz, fs) *
                       valve_detail::onePoleMagnitude(cutoff_.pole, frequencyHz, fs);
    if (flareOn_) {
      magnitude *= besselBellReflectionMagnitude(bell_, frequencyHz, speed_);
    } else if (radiating_) {
      auto reflected = openEnd_.response(frequencyHz);
      if (!reflected.ok()) return 0.0;
      magnitude *= std::hypot(reflected.value().real, reflected.value().imag);
    }
    if (!std::isfinite(magnitude) || !(magnitude > 0.0) || magnitude > 1.0) return 0.0;
    const double trip = 2.0 * soundingLength_ / speed_;
    if (!(trip > 0.0)) return 0.0;
    const double bandwidth = -std::log(magnitude) / (constants::pi * trip);
    if (!std::isfinite(bandwidth) || bandwidth < 0.0) return 0.0;
    return bandwidth;
  }
  [[nodiscard]] bool radiating() const noexcept { return radiating_; }
  [[nodiscard]] int delaySamples() const noexcept {
    return static_cast<int>(line_.size());
  }

  // Fit the wall one-pole to Kirchhoff's round-trip transmission at
  // frequencyHz on the cylindrical bore (Zc = Ï c / S). The cone's
  // log-mean fit is left in place until this is called. Zero clears
  // the pole. The sounding length is the one already set.
  bool setWallMatchHz(double frequencyHz) {
    const double fs = rate_.value();
    if (!std::isfinite(frequencyHz) || frequencyHz < 0.0 || !(fs > 0.0) ||
        !(soundingLength_ > 0.0) || !(speed_ > 0.0) || !(density_ > 0.0) ||
        !(impedance_ > 0.0))
      return false;
    if (frequencyHz == 0.0) {
      wall_.pole = 0.0;
      wall_.state = 0.0;
      return true;
    }
    if (!(frequencyHz < 0.5 * fs)) return false;
    const double area = density_ * speed_ / impedance_;
    const double throat =
        wallRadius_ > 0.0 ? wallRadius_ : std::sqrt(area / constants::pi);
    const double transmission = valve_detail::kirchhoffTripMagnitude(
        frequencyHz, throat, soundingLength_, speed_, density_);
    const double pole = valve_detail::onePoleMatching(transmission, frequencyHz, fs);
    if (!(pole > 0.0) || !(pole < 1.0)) return false;
    wall_.pole = pole;
    if (flareOn_ && frequencyHz > 0.0) {
      flareMatchHz_ = frequencyHz;
      const double reflected =
          besselBellReflectionMagnitude(bell_, frequencyHz, speed_);
      const double flarePole = valve_detail::onePoleMatching(reflected, frequencyHz, fs);
      if (flarePole > 0.0) flareLoss_.pole = flarePole;
    }
    return true;
  }

  // Mouth pressure after the attack. The raised cosine still uses this
  // value as its full pressure. Zero stops the breath: a lip parks on
  // the closed stop and the bore rings. The radiated sample is not scaled.
  bool setBlowingPressure(double pressurePa) {
    if (!std::isfinite(pressurePa) || pressurePa < 0.0) return false;
    pressure_ = pressurePa;
    return true;
  }

  // Absolute frame at which the tongue lets the valve go. Until that
  // frame the reed or the lip stays on its closed stop. Zero clears it.
  void scheduleTongue(std::uint64_t releaseFrame) { tongueFrames_ = releaseFrame; }

  // Fit Kirchhoff wall loss at frequencyHz for the free ring, keeping
  // the playing pole so restorePlayingWall can put it back. The delay
  // is not retuned here; the caller sets the sounding length again,
  // which rebuilds the phase at the new pole. This is the wall the bore
  // uses once the breath has stopped. It is not a tongue.
  bool engageRingWall(double frequencyHz) {
    if (!playingWallSaved_) {
      playingWallPole_ = wall_.pole;
      playingWallSaved_ = true;
    }
    return setWallMatchHz(frequencyHz);
  }

  bool restorePlayingWall() {
    if (playingWallSaved_) {
      wall_.pole = playingWallPole_;
      playingWallSaved_ = false;
    }
    // The next sounding length matches the flare at c/(2L) again.
    flareMatchHz_ = 0.0;
    return true;
  }

  // Sounding length in metres. Travel time is 2L/c; the apex distance
  // stays, so Î± = c/x0 is unchanged. The delay line is the construction
  // length and cannot grow. This is the air column to an open hole, a
  // valve slide, or a trombone slide, not a lattice and not a piston
  // chart. Until this succeeds, readBore is the construction delay.
  // fundamentalHz() stays the construction root.
  bool setSoundingLength(double lengthMeters) {
    if (!std::isfinite(lengthMeters) || !(lengthMeters > 0.0) || !(speed_ > 0.0) ||
        !(apex_ > 0.0))
      return false;
    if (rfOn_) return selectMeasuredFingering(lengthMeters);
    const double fs = rate_.value();
    if (!std::isfinite(fs) || !(fs > 0.0)) return false;
    // Half a sample is the Nyquist average in readBore, as in create().
    double timed = fs * 2.0 * lengthMeters / speed_ - 0.5;
    const double guess = speed_ / (2.0 * lengthMeters);
    if (flareOn_) {
      // The ring matches the flare at the impedance peak. While the lip
      // is driving, the pole stays a wire: a match at c/(2L) sits far
      // under the note, and its phase put the opening YIN on the bore
      // peak. The sounding length is already the turning plane.
      if (flareMatchHz_ > 0.0) {
        const double reflected =
            besselBellReflectionMagnitude(bell_, flareMatchHz_, speed_);
        const double flarePole =
            valve_detail::onePoleMatching(reflected, flareMatchHz_, fs);
        if (flarePole > 0.0) flareLoss_.pole = flarePole;
      } else {
        flareLoss_.pole = 0.0;
      }
    }
    // A straight-pipe mouth keeps its fraction inside OpenEndReflection.
    // A flare is a one-pole on the same loop as the wall, so its phase
    // comes off the line and the geometric length stays the round trip.
    if (!radiating_) {
      timed -= valve_detail::onePolePhaseDelay(wall_.pole, guess, fs);
      timed -= valve_detail::onePolePhaseDelay(cutoff_.pole, guess, fs);
      if (flareOn_ && flareLoss_.pole > 0.0)
        timed -= valve_detail::onePolePhaseDelay(flareLoss_.pole, guess, fs);
    }
    if (!std::isfinite(timed) || !(timed > 1.0)) return false;
    const double limit = static_cast<double>(line_.size());
    if (timed > limit + 2.0) return false;
    if (timed > limit) timed = limit;
    const double scale = (apex_ + lengthMeters) / apex_;
    if (!std::isfinite(scale) || !(scale > 0.0)) return false;
    variableAge_ = timed;
    variableRead_ = true;
    bellScale_ = scale;
    soundingLength_ = lengthMeters;
    return true;
  }

  // Couple the player's airway (not owned). Null detaches it.
  void attachAirway(PlayerAirway* airway) { airway_ = airway; }

  // Threshold blowing pressure of the pressed embouchure, Pa. Zero is a
  // lip that rests open at the rest area. See lipPreload().
  bool setLipThresholdPa(double pascals) {
    if (!lip_ || !std::isfinite(pascals) || pascals < 0.0) return false;
    lipThresholdPa_ = pascals;
    return true;
  }

  // Unloaded lip resonance. k = m (2 Ï€ f)^2. A reed has no lip spring.
  bool setLipResonanceHz(double hertz) {
    if (!lip_) return false;
    if (as_.on) {
      if (!std::isfinite(hertz) || !(hertz > 0.0)) return false;
      as_.m = 1.5 / (constants::twoPi * constants::twoPi * hertz);
      as_.k = 1.5 * hertz;
      return true;
    }
    if (!std::isfinite(hertz) || !(hertz > 0.0) || !(trumpet_.lipMassKg > 0.0))
      return false;
    const double omega = constants::twoPi * hertz;
    const double stiffness = trumpet_.lipMassKg * omega * omega;
    if (!std::isfinite(stiffness) || !(stiffness > 0.0)) return false;
    trumpet_.lipStiffnessNewtonsPerMeter = stiffness;
    if (lipQ_ > 0.0) trumpet_.lipDampingNewtonSecondsPerMeter = trumpet_.lipMassKg * omega / lipQ_;
    if (trumpet_.lipCollisionStiffness > 0.0) enableLipCollision(trumpet_);
    return true;
  }

  // Pole q of one of `lumps` equal one-poles whose cascade best matches
  // the one-pole `pole` in dB, minimax over 100 Hz - 6 kHz (64 log-spaced
  // points, golden-section on q). Two lumps hold the trip within 1.1 dB
  // below 4 kHz at pole 0.8 (3.7 dB at 0.94).
  [[nodiscard]] static double sectionPole(double pole, int lumps, double fs) {
    if (!(pole > 0.0) || lumps <= 1) return pole;
    auto err = [&](double q) {
      double worst = 0.0;
      for (int k = 0; k < 64; ++k) {
        const double f = 100.0 * std::pow(60.0, k / 63.0);
        const double d = lumps * std::log(valve_detail::onePoleMagnitude(q, f, fs)) -
                         std::log(valve_detail::onePoleMagnitude(pole, f, fs));
        worst = std::max(worst, std::fabs(d));
      }
      return worst;
    };
    double lo = 0.0, hi = pole;
    const double g = 0.5 * (std::sqrt(5.0) - 1.0);
    for (int it = 0; it < 60; ++it) {
      const double a = hi - g * (hi - lo), b = lo + g * (hi - lo);
      if (err(a) < err(b)) hi = b; else lo = a;
    }
    return 0.5 * (lo + hi);
  }
  // Wall-loss sections in the Burgers line (1 = the trip's own filter at
  // the middle step). Default kBurgersLossSections = 2, TUNED on the Lick
  // excerpt (tscore 1 / 2 / 4 / 8 sections: 54.6 / 51.1 / 51.4 / 55.6;
  // H2 / H3 re H1 at 2: +5.7 / +5.9 dB, recording +5.3 / +6.0): the loss
  // spread along the march as it is in the duct, with each section's
  // one-pole still close to its share (sectionPole).
  static constexpr int kBurgersLossSections = 2;
  bool setBurgersLossLumps(int lumps) {
    if (lumps < 1 || lumps > 32) return false;
    burgersLossLumps_ = lumps;
    burgersWallPole_ = -1.0;
    return true;
  }

  // Lip quality factor Q = m omega / r, held as the resonance moves.
  // Newton, Campbell and Gilbert, JASA 123, EL14-EL20 (2008): human
  // lips Q about 1.2-1.8 (artificial 8-10). Zero keeps the fixed r.
  bool setLipQualityFactor(double q) {
    if (!lip_ || !std::isfinite(q) || q < 0.0) return false;
    lipQ_ = q;
    if (q > 0.0 && trumpet_.lipStiffnessNewtonsPerMeter > 0.0)
      trumpet_.lipDampingNewtonSecondsPerMeter = std::sqrt(trumpet_.lipStiffnessNewtonsPerMeter * trumpet_.lipMassKg) / q;
    if (trumpet_.lipCollisionStiffness > 0.0) enableLipCollision(trumpet_);
    return true;
  }

  // Two-lip contact on or off (Ishizaka-Flanagan, see enableLipCollision).
  // Opt-in measured Bb trumpet bore (brass_bore.h): the delay line, wall
  // pole, and bell filter are replaced by the bore's own reflection
  // function and mouth-flow transfer, from the chain-matrix input
  // impedance with Keefe wall loss and Levine-Schwinger radiation. The
  // lip sees the throat characteristic impedance; the cup is the bore's
  // own V0. Fingering inserts the valve loops. Off by default.
  // Opt-in measured Bb tenor trombone bore (brass_bore::bbTromboneProfile).
  // Sounding lengths select slide positions 1-7 as real added tube.
  // Trombone kernels are 8192 taps from a 32768-point grid: at 2048/8192 the
  // truncated reflection function made the pitch glide up by 300+ cents in 1.6 s.
  bool useMeasuredTromboneBore() {
    measuredTrombone_ = true;
    for (auto& slot : kernelCache_) slot = {};
    slideCache_.clear();
    slideKey_ = -1;
    brass_bore::Fingering fg; fg.trombone = true;
    return useMeasuredTrumpetBore(fg);
  }
  bool useMeasuredTrumpetBore(brass_bore::Fingering fingering = {}) {
    if (!lip_) return false;
    if (!rfOn_) measuredBase_ = soundingLength_;
    measuredMask_ = (fingering.v1 ? 1 : 0) | (fingering.v2 ? 2 : 0) | (fingering.v3 ? 4 : 0);
    const double fs = rate_.value();
    auto k = brass_bore::makeKernels(fs, density_, speed_, fingering, fingering.trombone ? 8192 : 2048, fingering.trombone ? 32768 : 8192, measuredBell_);
    const double zc = density_ * speed_ /
                      (constants::pi * k.throatRadius * k.throatRadius);
    if (!std::isfinite(zc) || !(zc > 0.0)) return false;
    rf_ = std::move(k.reflection);
    uf_ = std::move(k.mouthFlow);
    hist_.assign(rf_.size(), 0.0);
    histS_.assign(rf_.size(), 0.0);
    hpos_ = 0;
    steepLength_ = brass_bore::steepeningLengthMeters(fingering);
    impedance_ = zc;
    cupCoeff_ = zc * k.cupVolume / (density_ * speed_ * speed_) * fs;
    rfOn_ = true;
    return true;
  }
  [[nodiscard]] bool measuredBoreOn() const noexcept { return rfOn_; }
  // Steady flow through the cone. integrator_ is the apex term of the
  // spherical-wave admittance, (c / x0) times the integral of the cup
  // pressure: the inertance L = rho x0 / S0 that carries the mean flow
  // (U = S0 integrator / (rho c)). Alone it has no restoring force, so the
  // mean flow was free to take any value and kept the history of every
  // attack (JB trumpet: the same note 20% apart in mean flow and 1-3 dB in
  // level after different attacks). The bore's steady (Poiseuille) flow
  // resistance in series, R = 8 mu / pi * integral dx / r^4 along the
  // cone r = r0 x / x0 (Hagen-Poiseuille; mu of air 1.81e-5 Pa s, CRC
  // Handbook, 20 C), relaxes it with tau = L / R
  //   = 3 rho r0^2 / (8 mu (1 - (x0 / (x0 + l))^3)).
  // Off with the measured bore (its reflection kernel carries its own
  // low-frequency behaviour) or when the geometry is unset.
  [[nodiscard]] double boreDcTauSeconds() const {
    if (rfOn_ || !(apex_ > 0.0) || !(impedance_ > 0.0) || !(density_ > 0.0) || !(speed_ > 0.0)) return 0.0;
    constexpr double kAirViscosityPaS = 1.81e-5;
    const double r0sq = density_ * speed_ / (constants::pi * impedance_);
    const double q = apex_ / (apex_ + std::max(0.0, soundingLength_));
    const double tau = 3.0 * density_ * r0sq / (8.0 * kAirViscosityPaS * (1.0 - q * q * q));
    return std::isfinite(tau) && tau > 0.0 ? tau : 0.0;
  }
  [[nodiscard]] double boreDcLeak(double dt) const {
    const double tau = boreDcTauSeconds();
    return tau > 0.0 ? std::exp(-dt / tau) : 1.0;
  }
  // Bell load of the measured bore (brass_bore::BellRadiation). Default
  // LevineSchwinger; HelieRodet is the opt-in spherical-cap load. Call
  // before useMeasuredTrumpetBore (cached fingerings are dropped).
  void setMeasuredBellRadiation(brass_bore::BellRadiation bell) {
    measuredBell_ = bell;
    for (auto& slot : kernelCache_) slot = {};
  }

  // Phrase use of the measured bore: a sounding length is read as
  // semitones below the open length the voice was built with, and the
  // standard valve combination for 0-6 semitones (open, 2, 1, 1+2, 2+3,
  // 1+3, 1+2+3; Campbell and Greated) swaps in that fingering's kernels.
  // The p+ history is kept, so the bore carries over the change.
  bool selectMeasuredFingering(double lengthMeters) {
    if (!(measuredBase_ > 0.0)) return false;
    if (measuredTrombone_) {
      // The slide is continuous: the length is read as fractional
      // semitones of slide, cached to 1 cent (winds pro2; integer
      // positions left partial 3 about 90 cents sharp on E3-B2).
      double s = 12.0 * std::log2(lengthMeters / measuredBase_);
      s = std::max(0.0, std::min(7.0, s));
      const long key = std::lround(100.0 * s);
      if (key == slideKey_) return true;
      auto& slot = slideCache_[key];
      if (slot.first.empty()) {
        brass_bore::Fingering fg{false, false, false, true, brass_bore::tromboneSlideMeters(key / 100.0)};
        auto k = brass_bore::makeKernels(rate_.value(), density_, speed_, fg, 8192, 32768, measuredBell_);
        slot = {std::move(k.reflection), std::move(k.mouthFlow)};
      }
      if (slideKey_ < 0) slideCache_[0] = {rf_, uf_};
      rf_ = slot.first;
      uf_ = slot.second;
      slideKey_ = key;
      return true;
    }
    int n = static_cast<int>(std::lround(12.0 * std::log2(lengthMeters / measuredBase_)));
    n = std::max(0, std::min(6, n));
    static constexpr int kMask[7] = {0, 2, 1, 3, 6, 5, 7};
    const int mask = measuredTrombone_ ? n : kMask[n];
    if (mask == measuredMask_) return true;
    auto& slot = kernelCache_[mask];
    if (slot.first.empty()) {
      brass_bore::Fingering fg{(mask & 1) != 0, (mask & 2) != 0, (mask & 4) != 0};
      if (measuredTrombone_) fg = brass_bore::Fingering{false, false, false, true, brass_bore::tromboneSlideMeters(n)};
      auto k = brass_bore::makeKernels(rate_.value(), density_, speed_, fg, fg.trombone ? 8192 : 2048, fg.trombone ? 32768 : 8192, measuredBell_);
      slot = {std::move(k.reflection), std::move(k.mouthFlow)};
    }
    if (measuredMask_ >= 0 && kernelCache_[measuredMask_].first.empty())
      kernelCache_[measuredMask_] = {rf_, uf_};
    rf_ = slot.first;
    uf_ = slot.second;
    measuredMask_ = mask;
    return true;
  }

  // Nonlinear steepening on the measured bore (off by default). The
  // outgoing throat wave p+ advances by beta p+ L_eff / (rho c^3)
  // seconds, beta = (gamma+1)/2 (Hirschberg, Gilbert, Msallam and
  // Wijnands, JASA 99, 1754-1758, 1996), with the throat-referred
  // effective length L_eff = int r_throat / r(x) dx of the profile
  // (plane-wave amplitude ~ 1/r; the brassiness integral of Gilbert,
  // Myers et al., Myers, Pyle, Gilbert, Campbell, Chick and Logie, JASA
  // 131, 678-688, 2012). Wall loss along the way is not in the advance,
  // so this is an upper bound. The steepened wave feeds only the bell
  // flow; the lip keeps the linear return (as ValveBlow::brassiness).
  bool setMeasuredBoreSteepening(bool on) {
    if (!rfOn_ || !(steepLength_ > 0.0)) return false;
    steepRfOn_ = on;
    std::fill(steepRf_, steepRf_ + kSteepRfSize, 0.0);
    return true;
  }
  [[nodiscard]] bool measuredBoreSteepeningOn() const noexcept { return steepRfOn_; }

  // Opt-in conservative nonlinear steepening of the wave heard at the
  // bell (physics/burgers.h): the inviscid Burgers equation marched over
  // L_eff = fraction x the sounding length in `stages` Godunov steps
  // (Hirschberg, Gilbert, Msallam and Wijnands, JASA 99, 1754-1758,
  // 1996; L_eff as the brassiness integral of Myers, Pyle, Gilbert,
  // Campbell, Chick and Logie, JASA 131, 678-688, 2012: B = L_eff/L_ecl
  // about 0.58-0.76 for Bb trumpets). Replaces the four-sample advance
  // of ValveBlow::brassiness while on. The radiated wave only: the lip
  // keeps the linear return, so tuning is unchanged; the line adds
  // `stages` samples of latency to the radiated sound. Cone/cylinder
  // bore only (not the measured-bore reflection function).
  bool setBoreBurgers(bool on, double lengthFraction = 1.0, int stages = 32) {
    if (!lip_ || rfOn_) return false;
    if (on && (!std::isfinite(lengthFraction) || !(lengthFraction > 0.0) || stages < 1)) return false;
    burgersOn_ = on;
    if (!on) return true;
    burgersFraction_ = lengthFraction;
    burgersLength_ = soundingLength_;
    return burgers_.configure(lengthFraction * std::max(soundingLength_, 1e-3), stages, rate_.value(),
                              density_, speed_, acoustic_constants::dryAirGamma);
  }
  [[nodiscard]] const BurgersLine& boreBurgers() const noexcept { return burgers_; }

  // Two-dimensional swinging-door lip of Adachi and Sato, JASA 99,
  // 1200-1209 (1996), eqs. 1-5 and 8, Table I (checked against the
  // paper): b = 7 mm, d = 2 mm, joint (0, 4 mm), rest x 1 mm, rest y
  // 0.1-2.0 mm (an embouchure argument, chosen per note in the paper),
  // m = 1.5 / ((2 pi)^2 f_lip) kg, k = 1.5 f_lip N/m, Q 3 open and 0.5
  // closed, contact spring 3k on y < 0, S_cup = 2.3e-4 m^2. Their Bb3
  // optimum: f_lip 250 Hz, sounding 236 Hz at p0 2.5 kPa (Table III).
  // Opening 2 b y. The lip-swept flow U_lip (eq. 6, small in the paper)
  // is not added. Replaces the one-mass lip; off by default.
  bool useAdachiSatoLip(double lipHz, double restYMeters) {
    if (!lip_ || !(lipHz > 0.0) || !std::isfinite(restYMeters)) return false;
    as_ = AdachiSato{};
    as_.on = true;
    as_.m = 1.5 / (constants::twoPi * constants::twoPi * lipHz);
    as_.k = 1.5 * lipHz;
    as_.eqY = restYMeters;
    as_.x = as_.eqX; as_.y = as_.eqY;
    return true;
  }
  [[nodiscard]] bool adachiSatoLipOn() const noexcept { return as_.on; }

  // Per-note embouchure for the Adachi-Sato lip, chosen the way Adachi
  // and Sato (1996, Sec. II B) chose theirs: f_lip is varied and the
  // oscillation that sounds the wanted note is kept (they stepped 10 Hz
  // and judged stability and amplitude; here the criterion is the
  // target pitch, as a player tunes by ear). Each probe is a fresh copy
  // of this voice, without the airway, at the note's fingering and
  // pressure, rendered 0.4 s; pitch by autocorrelation near the target.
  // Secant search from f_lip = f_note (their Table III ratios are
  // 1.01-1.08), at most 8 probes. Returns the lip frequency, 0 if none.
  [[nodiscard]] double tuneAdachiSatoLipHz(double noteHz, double lengthMeters,
                                           double pressurePa) const {
    if (!as_.on || !(noteHz > 0.0)) return 0.0;
    // A phrase repeats notes: the search is a pure function of the voice
    // set-up, the note, the length, and the pressure, so its answer is
    // kept (keys at 1 mHz, 1 um, 1 mPa). Each probe is 0.4 s of an
    // 8192-tap convolution on the measured trombone bore.
    const auto key = std::make_tuple(std::llround(noteHz * 1e3), std::llround(lengthMeters * 1e6),
                                     std::llround(pressurePa * 1e3));
    if (auto hit = tuneCache_.find(key); hit != tuneCache_.end()) return hit->second;
    const double result = tuneAdachiSatoLipHzUncached(noteHz, lengthMeters, pressurePa);
    tuneCache_[key] = result;
    return result;
  }
  [[nodiscard]] double tuneAdachiSatoLipHzUncached(double noteHz, double lengthMeters,
                                                   double pressurePa) const {
    const double fs = rate_.value();
    auto probe = [&](double lipHz) -> double {
      ConeValveVoice v = *this;
      v.tuneCache_.clear();
      v.airway_ = nullptr;
      v.reset();
      v.frame_ = 0;
      std::fill(v.hist_.begin(), v.hist_.end(), 0.0);
      std::fill(v.histS_.begin(), v.histS_.end(), 0.0);
      std::fill(v.steepRf_, v.steepRf_ + kSteepRfSize, 0.0);
      v.as_.x = v.as_.eqX; v.as_.y = v.as_.eqY; v.as_.vx = v.as_.vy = 0.0;
      if (!v.setSoundingLength(lengthMeters) || !v.setBlowingPressure(pressurePa) ||
          !v.setLipResonanceHz(lipHz))
        return 0.0;
      const std::size_t n = static_cast<std::size_t>(0.4 * fs);
      std::vector<double> out(n, 0.0);
      if (!v.renderBlock(out.data(), FrameCount{n}).ok()) return 0.0;
      const std::size_t w = 4096, s0 = n - w - static_cast<std::size_t>(fs / 60.0);
      const int lo = static_cast<int>(fs / (noteHz * 1.2)), hi = static_cast<int>(fs / (noteHz / 1.2));
      double best = -2.0; int bl = 0; std::vector<double> r(hi + 2, 0.0);
      for (int L = lo - 1; L <= hi + 1; ++L) {
        double xy = 0, xx = 0, yy = 0;
        for (std::size_t j = 0; j < w; ++j) { const double x = out[s0 + j], y = out[s0 + j + L]; xy += x * y; xx += x * x; yy += y * y; }
        r[L] = xy / std::sqrt(xx * yy + 1e-300);
        if (L >= lo && L <= hi && r[L] > best) { best = r[L]; bl = L; }
      }
      if (bl == 0 || best < 0.8) return 0.0;
      const double den = r[bl - 1] - 2 * r[bl] + r[bl + 1];
      const double d = den != 0.0 ? 0.5 * (r[bl - 1] - r[bl + 1]) / den : 0.0;
      return fs / (bl + d);
    };
    auto cents = [&](double f) { return 1200.0 * std::log2(f / noteHz); };
    double x0 = noteHz, f0 = probe(x0);
    if (!(f0 > 0.0)) { x0 = 0.92 * noteHz; f0 = probe(x0); }
    if (!(f0 > 0.0)) return 0.0;
    double x1 = x0 * std::pow(2.0, -cents(f0) / 1200.0), f1 = probe(x1);
    double bestX = x0, bestE = std::fabs(cents(f0));
    for (int it = 0; it < 6 && f1 > 0.0; ++it) {
      const double e0 = cents(f0), e1 = cents(f1);
      if (std::fabs(e1) < bestE) { bestE = std::fabs(e1); bestX = x1; }
      if (std::fabs(e1) < 5.0 || e1 == e0) break;
      double x2 = x1 - e1 * (x1 - x0) / (e1 - e0);
      x2 = std::max(0.7 * noteHz, std::min(1.2 * noteHz, x2));
      x0 = x1; f0 = f1; x1 = x2; f1 = probe(x1);
    }
    if (f1 > 0.0 && std::fabs(cents(f1)) < bestE) bestX = x1;
    return bestX;
  }
  [[nodiscard]] double measuredBoreSteepeningLength() const noexcept { return steepLength_; }

  // Published trumpet embouchure, for the measured bore. One-dof
  // outward-striking lip, "Numerical continuation of a physical model
  // of brass instruments: application to trumpet comparisons" (Freour,
  // Guillot, Masuda, Usa, Tominaga, Tohgi, Vergez, Cochelin; JASA 148,
  // 2020; author list VERIFY), eq. 15: Q_l = 3, mu_l = 2 kg/m^2, rest opening
  // y0 = 0.1 mm, width b = 8 mm. mu_l is m / A here (only the ratio
  // enters the lip equation), rest area = b y0, no threshold preload,
  // Ishizaka-Flanagan contact on. That paper prints no maximum opening;
  // the cap is lifted to 1 cm^2 (UNSOURCED; a non-binding bound).
  // Optional overrides: kQ (Newton, Campbell and Gilbert 2008 give 1.2-1.8
  // for human lips), kRest (rest opening height, m) and thresholdPa (a
  // pressed-embouchure preload, see lipPreload). Measured on the 0.1 mm
  // rest opening the lips sit statically open (no buzz, 9-21 mm^2 at
  // 4-8 kPa); a near-closed rest with a preload does buzz but locks to
  // bore peak 3 (~363 Hz) for lip tunings 195-225 Hz. See [.boreexp].
  bool usePublishedTrumpetLips(double lipHz, double kQ = 3.0, double kRest = 1.0e-4,
                               double thresholdPa = 0.0, double kMu = 2.0,
                               double kWidth = 0.008) {
    if (!lip_ || !std::isfinite(lipHz) || !(lipHz > 0.0)) return false;
    if (!(kQ > 0.0) || !(kRest > 0.0) || !(thresholdPa >= 0.0)) return false;
    if (!(kMu > 0.0) || !(kWidth > 0.0)) return false;
    trumpet_.lipMassKg = kMu * trumpet_.lipPressureAreaM2;
    trumpet_.slitWidthMeters = kWidth;
    trumpet_.restOpeningAreaM2 = kWidth * kRest;
    trumpet_.maximumOpeningAreaM2 = 1.0e-4;
    slitWidth_ = kWidth;
    restOpening_ = kWidth * kRest;
    maximumOpening_ = 1.0e-4;
    lipThresholdPa_ = thresholdPa;
    lipQ_ = kQ;
    trumpet_.lipCollisionStiffness = 1.0;  // so setLipResonanceHz re-derives contact
    if (!setLipResonanceHz(lipHz)) return false;
    enableLipCollision(trumpet_);
    displacement_ = 0.0;
    velocity_ = 0.0;
    return true;
  }

  // stiffnessRatio is the contact spring over k; dampingRatio the closed
  // lip's damping ratio. Defaults are Ishizaka-Flanagan (3, 1.1).
  bool setLipCollision(bool on, double stiffnessRatio = 3.0, double dampingRatio = 1.1) {
    if (!lip_) return false;
    if (on && (!std::isfinite(stiffnessRatio) || !(stiffnessRatio > 0.0) ||
               !std::isfinite(dampingRatio) || dampingRatio < 0.0)) return false;
    trumpet_.lipCollisionStiffnessRatio = stiffnessRatio;
    trumpet_.lipCollisionDampingRatio = dampingRatio;
    if (on) enableLipCollision(trumpet_); else { trumpet_.lipCollisionStiffness = 0.0; trumpet_.lipCollisionDamping = 0.0; }
    return true;
  }

  // Lip opening (m^2) of the last sample; zero is closed.
  [[nodiscard]] double lipOpeningM2() const noexcept { return lastOpening_; }

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
        tongueFrames_(blow.tongueReleaseFrames),
        reedTurbulence_(blow.reedTurbulence),
        lockedBreath_(blow.lockedBreathFraction),
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
    // Half a sample is the linear-phase delay of the loop's Nyquist zero
    // (readBore averages adjacent samples). Shortening the line by that
    // amount keeps the low-frequency round trip on the geometric length.
    double timed = fs * roundTrip - 0.5;
    double wallPole = 0.0;
    double cutPole = 0.0;
    const bool lossy = blow.reflectionCutoffHz > 0.0;
    const double geometricGuess = speed / (2.0 * sounding);
    const bool flare = blow.bellLengthMeters > 0.0 && blow.bellFlare > 0.0 &&
                       blow.bellCoefficient > 0.0 && blow.bellMouthOffsetMeters > 0.0 &&
                       blow.radiusMeters > 0.0;
    // A straight-pipe mouth keeps OpenEndReflection, fraction and all.
    // A Bessel flare is a one-pole of that horn's throat reflectance, so
    // the line keeps the geometric length the way a lossless âˆ’1 does.
    const bool radiating = blow.radiusMeters > 0.0 && !flare;
    BesselBellProfile bell{};
    double flarePole = 0.0;
    if (flare) {
      bell.lengthMeters = blow.bellLengthMeters;
      bell.gamma = blow.bellFlare;
      bell.coefficient = blow.bellCoefficient;
      bell.mouthOffsetMeters = blow.bellMouthOffsetMeters;
      bell.mouthRadiusMeters = blow.radiusMeters;
      const double reflected =
          besselBellReflectionMagnitude(bell, geometricGuess, speed);
      flarePole = valve_detail::onePoleMatching(reflected, geometricGuess, fs);
      if (flarePole > 0.0)
        timed -= valve_detail::onePolePhaseDelay(flarePole, geometricGuess, fs);
    }
    if (lossy) {
      // Î± âˆ 1/r and r grows linearly along the cone, so the trip uses
      // the log-mean radius, not the throat alone.
      const double throat = std::sqrt(area / constants::pi);
      const double ratio = (apex + sounding) / apex;
      const double logMean =
          ratio > 1.0 ? throat * (ratio - 1.0) / std::log(ratio) : throat;
      // The default one-pole is matched high, on the log-mean radius,
      // and is mild at a playing frequency. A caller match frequency
      // fits Kirchhoff at that frequency on the cylindrical bore
      // (the throat), which is the leadpipe loss a bandwidth is
      // compared with. The flare's log-mean stays the default.
      double match = std::min(0.3 * fs, std::max(12.0 * geometricGuess, 4000.0));
      double lossRadius = logMean;
      if (blow.wallMatchHz > 0.0) {
        if (!(blow.wallMatchHz < 0.5 * fs)) return R::failure(QuantityError::RangeError);
        match = blow.wallMatchHz;
        lossRadius = throat;
      }
      const double wall = valve_detail::kirchhoffTripMagnitude(
          match, lossRadius, sounding, speed, density);
      wallPole = valve_detail::onePoleMatching(wall, match, fs);
      cutPole = std::exp(-constants::twoPi * blow.reflectionCutoffHz / fs);
      if (!std::isfinite(cutPole) || !(cutPole > 0.0) || !(cutPole < 1.0) ||
          !std::isfinite(logMean) || !(logMean > 0.0))
        return R::failure(QuantityError::RangeError);
      if (!radiating) {
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
    if (flare) {
      const double reflected = besselBellReflectionMagnitude(bell, hertz, speed);
      if (!(reflected > 0.0) || reflected > 1.0)
        return R::failure(QuantityError::RangeError);
      const double trip = roundTrip > 0.0 ? roundTrip : static_cast<double>(delay) / fs;
      decay -= std::log(reflected) / trip;
      if (!std::isfinite(decay)) return R::failure(QuantityError::RangeError);
    }
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
    voice.flareLoss_.pole = flarePole;
    voice.flareOn_ = flare;
    voice.bellTransmit_ = blow.bellTransmission;
    voice.bell_ = bell;
    voice.wallRadius_ = blow.wallRadiusMeters;
    voice.decay_ = decay;
    voice.openEnd_ = std::move(openEnd);
    voice.boreThiran_ = boreThiran;
    voice.useBoreThiran_ = useBoreThiran;
    voice.apex_ = apex;
    voice.speed_ = speed;
    voice.bellScale_ = (apex + sounding) / apex;
    if (!std::isfinite(voice.bellScale_) || !(voice.bellScale_ > 0.0))
      return R::failure(QuantityError::RangeError);
    voice.soundingLength_ = sounding;
    voice.brassiness_ = blow.brassiness;
    const double bellRadius = std::sqrt(area / constants::pi) * voice.bellScale_;
    voice.radiationCorner_ = speed / bellRadius;
    // ka = 1 of this cone's bell is the radiation smoother. A positive
    // radiationHz replaces it and does not change the loop filter. The
    // reflection cutoff is a different section: it is what returns to
    // the valve. Copying that cutoff onto the radiator low-passes the
    // partials the open end just passed. Benade's envelope, when set,
    // is the external saxophone transfer and is not filtered again.
    // A lip with no envelope keeps the older coupling, because that
    // cutoff (several kHz on these cones) is the brightness control
    // the brass notes were tuned with.
    if (blow.radiationHz > 0.0)
      voice.radiationCorner_ = constants::twoPi * blow.radiationHz;
    else if (lossy && !(blow.spectrumBreakHz > 0.0))
      voice.radiationCorner_ = constants::twoPi * blow.reflectionCutoffHz;
    if (!std::isfinite(voice.radiationCorner_) || !(voice.radiationCorner_ > 0.0))
      return R::failure(QuantityError::RangeError);
    voice.break_.design(blow.spectrumBreakHz, fs);
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
    if (variableRead_) {
      x = valve_detail::delayRead(line_, pos_, variableAge_);
    } else if (useBoreThiran_) {
      const double y = boreThiran_ * x + boreX_ - boreThiran_ * boreY_;
      boreX_ = x;
      boreY_ = y;
      x = y;
    }
    preWall_ = x;
    x = wall_.step(x);
    // A zero at z = âˆ’1. An odd-length delay plus a reed that is still
    // a negative resistance at fs/2 otherwise locks onto Nyquist, and
    // the bell differentiator then radiates only that rail. The average
    // is linear phase of half a sample, already removed from the line.
    const double y = 0.5 * (x + nyquist_);
    nyquist_ = x;
    return y;
  }

  // One trip of plane-wave steepening, heard at the bell and not
  // written back. Peaks advance by Î² p L / (Ï cÂ³) samples,
  // Î² = (Î³+1)/2 (Hirschberg, Gilbert, Msallam, and Wijnands, JASA 99,
  // 1754â€“1758, 1996). A one-sample Taylor step had to stop that advance
  // at one sample. On these notes the peaks ask for about two, so the
  // radiated sample is a four-sample causal delay read at age 4 âˆ’ adv,
  // linearly interpolated. The lip still sees the linear return. A
  // length change is a step in the incident wave; the read smooths it
  // instead of adding a quarter-wave click. See ValveBlow::brassiness.
  [[nodiscard]] double steepenAtBell(double incident) {
    steepHist_[steepPos_] = incident;
    prevOutgoing_ = incident;
    auto at = [&](int ago) {
      int idx = steepPos_ - ago;
      if (idx < 0) idx += 16;
      return steepHist_[idx];
    };
    const double c = speed_;
    const double denom = density_ * c * c * c;
    if (!(denom > 0.0) || !(soundingLength_ > 0.0) || !std::isfinite(incident)) {
      const double held = at(0);
      steepPos_ = (steepPos_ + 1) % 16;
      return held;
    }
    const double beta = 0.5 * (acoustic_constants::dryAirGamma + 1.0);
    double adv = brassiness_ * beta * incident * soundingLength_ * rate_.value() / denom;
    constexpr double kDelay = static_cast<double>(kSteepAtBellDelay);
    if (adv > kDelay) adv = kDelay;
    if (adv < -kDelay) adv = -kDelay;
    const double age = kDelay - adv;
    const int i0 = static_cast<int>(age);
    const double frac = age - static_cast<double>(i0);
    const double y = at(i0) * (1.0 - frac) + at(std::min(i0 + 1, 15)) * frac;
    steepPos_ = (steepPos_ + 1) % 16;
    if (!std::isfinite(y)) return incident;
    return y;
  }

  // Spring displacement at which the lips meet: the embouchure presses
  // them together, so the mouth must exceed the threshold p0 before the
  // slit opens. y_p = A p0 / k. Fletcher and Tarnopolsky, JASA 105,
  // 874-881 (1999), Eq. (1): p0 = K f, K about 7 Pa/Hz for their
  // professional player GC (4 to 8 Pa/Hz across three players).
  [[nodiscard]] double lipPreload() const {
    if (!(lipThresholdPa_ > 0.0) || !(trumpet_.lipStiffnessNewtonsPerMeter > 0.0)) return 0.0;
    return trumpet_.lipPressureAreaM2 * lipThresholdPa_ / trumpet_.lipStiffnessNewtonsPerMeter;
  }

  void stopLip() {
    if (!(slitWidth_ > 0.0)) return;
    const double yMin = lipPreload() - restOpening_ / slitWidth_;
    const double yMax = lipPreload() + (maximumOpening_ - restOpening_) / slitWidth_;
    // With the two-lip contact the closed lips push back elastically
    // instead of stopping dead.
    if (trumpet_.lipCollisionStiffness > 0.0) {
      if (displacement_ > yMax) {
        displacement_ = yMax;
        if (velocity_ > 0.0) velocity_ = 0.0;
      }
      return;
    }
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
  struct AdachiSato {
    bool on = false;
    double m = 0, k = 0, b = 7.0e-3, d = 2.0e-3, jx = 0.0, jy = 4.0e-3;
    double eqX = 1.0e-3, eqY = 0.0, scup = 2.3e-4;
    double x = 0, y = 0, vx = 0, vy = 0;
  } as_;
  // Eq. 1: (m/2) xi'' = -(sqrt(m k)/(2Q)) xi' - (k/2)(xi - xi_eq)
  //        + b (p0 - p) (xi - xi_joint)^perp + b d p_lip e_y [- 3k y e_y].
  void stepAdachiSato(double overpressure, double p, double opening, double dt) {
    const bool closed = as_.y < 0.0;
    const double q = closed ? 0.5 : 3.0;
    const double damp = std::sqrt(as_.m * as_.k) / (2.0 * q);
    double plip = p;
    if (opening > 1.0e-12 && apertureFlow_ != 0.0)
      plip = p + density_ * apertureFlow_ * apertureFlow_ *
                     (1.0 / (as_.scup * opening) - 1.0 / (as_.scup * as_.scup));
    const double ax = as_.x - as_.jx, ay = as_.y - as_.jy;
    double fx = -damp * as_.vx - 0.5 * as_.k * (as_.x - as_.eqX) + as_.b * overpressure * (-ay);
    double fy = -damp * as_.vy - 0.5 * as_.k * (as_.y - as_.eqY) + as_.b * overpressure * ax +
                as_.b * as_.d * plip;
    if (closed) fy -= 3.0 * as_.k * as_.y;
    as_.vx += fx / (0.5 * as_.m) * dt;
    as_.vy += fy / (0.5 * as_.m) * dt;
    as_.x += as_.vx * dt;
    as_.y += as_.vy * dt;
    if (!std::isfinite(as_.x) || !std::isfinite(as_.y)) { as_.x = as_.eqX; as_.y = as_.eqY; as_.vx = as_.vy = 0; }
  }
  double measuredBase_ = 0.0;
  int measuredMask_ = -1;
  long slideKey_ = -1;
  std::map<long, std::pair<std::vector<double>, std::vector<double>>> slideCache_;
  std::pair<std::vector<double>, std::vector<double>> kernelCache_[8];
  bool rfOn_ = false;
  bool steepRfOn_ = false;
  double steepLength_ = 0.0;
  static constexpr int kSteepRfSize = 64;
  double steepRf_[kSteepRfSize] = {};
  int steepRfPos_ = 0;
  // Fractional-delay read at age D - adv, D = 24 samples (clamped).
  [[nodiscard]] double steepenMeasured(double p) {
    steepRf_[steepRfPos_] = p;
    auto at = [&](int ago) {
      int idx = steepRfPos_ - ago;
      while (idx < 0) idx += kSteepRfSize;
      return steepRf_[idx];
    };
    const double denom = density_ * speed_ * speed_ * speed_;
    const double beta = 0.5 * (acoustic_constants::dryAirGamma + 1.0);
    constexpr double kDelay = 24.0;
    double adv = (denom > 0.0 && std::isfinite(p)) ? beta * p * steepLength_ * rate_.value() / denom : 0.0;
    if (adv > kDelay) adv = kDelay;
    if (adv < -kDelay) adv = -kDelay;
    const double age = kDelay - adv;
    const int i0 = static_cast<int>(age);
    const double frac = age - i0;
    const double y = at(i0) * (1.0 - frac) + at(std::min(i0 + 1, kSteepRfSize - 1)) * frac;
    steepRfPos_ = (steepRfPos_ + 1) % kSteepRfSize;
    return std::isfinite(y) ? y : p;
  }
  bool measuredTrombone_ = false;
  mutable std::map<std::tuple<long long, long long, long long>, double> tuneCache_;
  brass_bore::BellRadiation measuredBell_ = brass_bore::BellRadiation::LevineSchwinger;
  std::vector<double> rf_, uf_, hist_, histS_;
  std::size_t hpos_ = 0;
  // Sum over n >= 1 of kernel[n] * p+(t - n); hist_ holds past p+.
  [[nodiscard]] double convolveHistory(const std::vector<double>& kernel,
                                       const std::vector<double>& hist) const {
    const std::size_t n = hist.size();
    double acc = 0.0;
    std::size_t idx = hpos_;
    for (std::size_t j = 1; j < kernel.size() && j <= n; ++j) {
      idx = (idx == 0) ? n - 1 : idx - 1;
      acc += kernel[j] * hist[idx];
    }
    return acc;
  }
  double bellScale_ = 1.0;
  double lipThresholdPa_ = 0.0;
  double lipQ_ = 0.0;
  double lipSide_ = 1.0;  // gap-to-mouth pressure fraction at the lips (series tongue)
  double lastOpening_ = 0.0;
  PlayerAirway* airway_ = nullptr;
  bool lipLocked_ = false;
  double bellFlow_ = 0.0;
  double radiationState_ = 0.0;
  double radiationCorner_ = 1.0;
  double boreThiran_ = 0.0;
  double boreX_ = 0.0;
  double boreY_ = 0.0;
  double nyquist_ = 0.0;
  valve_detail::TripLoss wall_{};
  valve_detail::TripLoss cutoff_{};
  std::uint64_t attackFrames_ = 0;
  std::uint64_t releaseFrames_ = 0;
  std::uint64_t tongueFrames_ = 0;
  double playingWallPole_ = 0.0;
  bool playingWallSaved_ = false;
  double reedTurbulence_ = 0.0;
  double lockedBreath_ = -1.0;
  double brassiness_ = 0.0;
  double soundingLength_ = 0.0;
  BurgersLine burgers_;
  bool burgersOn_ = false;
  double preWall_ = 0.0, burgersWallPole_ = -1.0, burgersWallGain_ = -1.0;
  int burgersLossLumps_ = kBurgersLossSections;
  std::vector<double> burgersReturn_;
  static constexpr int kSteepAtBellDelay = 4;
  double steepReturn_[kSteepAtBellDelay] = {};
  int steepReturnPos_ = 0;
  std::size_t burgersPos_ = 0;
  double burgersFraction_ = 1.0, burgersLength_ = 0.0;
  double prevOutgoing_ = 0.0;
  double steepHist_[16] = {};
  int steepPos_ = 0;
  std::uint32_t noise_ = 1u;
  double breathEnv_ = 0.0;
  double breathState_ = 0.0;
  double flowMean_ = 0.0;
  double apertureFlow_ = 0.0;
  valve_detail::BreakEnvelope break_{};
  bool useBoreThiran_ = false;
  double fundamentalHz_ = 0.0;
  double decay_ = 0.0;
  double apex_ = 0.0;
  double speed_ = 0.0;
  double variableAge_ = 0.0;
  bool variableRead_ = false;
  double closing_ = 0.0;
  double restOpening_ = 0.0;
  double maximumOpening_ = 0.0;
  double slitWidth_ = 0.0;
  bool lip_ = false;
  bool radiating_ = false;
  bool flareOn_ = false;
  bool bellTransmit_ = false;
  double flareMatchHz_ = 0.0;
  double wallRadius_ = 0.0;
  BesselBellProfile bell_{};
  valve_detail::TripLoss flareLoss_{};
  OpenEndReflection openEnd_{};
  ReedCharacteristic reed_{};
  ReedInertia inertia_{};
  Trumpet trumpet_{};
  std::vector<double> line_;
  std::size_t pos_ = 0;
  std::uint64_t frame_ = 0;
};

}  // namespace mmathen
