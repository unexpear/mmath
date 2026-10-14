#pragma once

// A bowed course, stepped at the sample rate (docs/48).
//
// The resonator is the tuned loop of tuned_waveguide.h: integer delays
// whose lengths sum to 2M, and the same Thiran, dispersion, and one-pole
// loss, applied once on the bridge round trip and then reflected with −1.
// The nut reflects with −1 and no filter. With the bow up, the junction
// is transparent and the loop is exactly G(z) = F(z) z^{-2M}, so the
// ringing frequency and decay are loopPole of that design.
//
// While the bow is down, the junction is stickSlipJunction (Woodhouse,
// Euphonics 9.2.1). The sample is the bow-point string velocity in m/s.
// sustainFrames is the caller saying when the bow lifts: after that the
// force is zero. Lifting the bow is not an amplitude envelope.
//
// β is the bow's distance from the bridge. The delays are the nearest
// positive split of 2M, so the realized fraction can differ from the
// request by less than one sample. Schelleng's window is not enforced;
// captureStickSlip does not enforce it either.
//
// Pass a BowBodyDrive to close the bridge on a modal soundboard. The
// bridge force Z (2 v+ − v_bridge) drives the modes, and their velocity
// replaces the rigid −1. The sample is then the compact monopole of
// the bridge acceleration. With no BowBodyDrive the sample stays the
// bow-point velocity and the bridge stays rigid.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
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
#include "bow.h"
#include "plucked_string.h"

namespace mmathen {

// Caller modes, area, distance, and air. Nothing here is a stored
// violin admittance or an A0.
struct BowBodyDrive {
  std::span<const BodyModeSpec> modes;
  double radiatingAreaM2 = 0.0;
  double distanceMeters = 0.0;
  double airPressurePa = 0.0;
  double temperatureKelvin = 0.0;
};

// Player expression on one bow stroke. Zero fields switch a feature off.
// Vibrato is a finger rocking along the string: the loop length moves
// sinusoidally, realized as a short linearly interpolated delay taken
// from the nut segment, so the mean loop length is unchanged.
//   Rate: violinists' vibrato sits near 5.5-7 Hz (Seashore, Psychology
//   of Music, 1938; Mellody and Wakefield, JASA 107, 2000). Extent about
//   +/-0.1-0.2 semitone, i.e. +/-10-20 cents (same sources; VERIFY exact
//   figure). Onset delay and ramp time are arguments (UNSOURCED).
// Bow envelope: normal force and bow speed rise on a raised cosine from
// attackFraction to 1 over attackSeconds, and fall to zero over
// releaseSeconds before the bow lifts. Shape and times are arguments
// (UNSOURCED); Guettler (Acta Acustica 88, 2002) shows a gentle force and
// acceleration onset is what gives a clean Helmholtz attack.
struct BowExpression {
  double vibratoRateHz = 0.0;
  double vibratoCents = 0.0;
  double vibratoOnsetSeconds = 0.0;
  double vibratoRampSeconds = 0.0;
  double attackSeconds = 0.0;
  double attackFraction = 1.0;
  double releaseSeconds = 0.0;
};

// Per-note bow articulation, applied from the call to articulate().
// accentGain scales bow force and speed at the onset and relaxes to 1
// with time constant accentSeconds. onsetSeconds is a raised-cosine bow
// landing. contactSeconds > 0 lifts the bow that long after the onset
// (off-string strokes: spiccato / sautille); 0 keeps it down (detache).
struct BowArticulation {
  double accentGain = 1.0;
  double accentSeconds = 0.0;
  double onsetSeconds = 0.0;
  double contactSeconds = 0.0;
  // Off-string bounce: bow speed and force follow a half sine over the
  // contact (the bounce cycle of Askenfelt and Guettler 1998).
  bool bounce = false;
  // After the lift the string is damped toward T60 restDampT60 seconds
  // (bow hair returning to rest on the string between bounces, and
  // left-hand finger lift). 0 lets it ring freely.
  double restDampT60 = 0.0;
  // Note dynamic: bow speed and force scale together (Schelleng ratio kept).
  double level = 1.0;
};

// Off-string strokes: the bow bounces, touching the string for part of
// each cycle (Askenfelt and Guettler, "The bouncing bow", JASA 103,
// 1998; Guettler and Askenfelt, KTH STL-QPSR 1998). Notes shorter than
// kSpiccatoMaxSeconds are played off the string; sautille rates of about
// 8-12 notes/s are reported there (threshold value VERIFY).
inline constexpr double kSpiccatoMaxSeconds = 0.15;     // VERIFY
inline constexpr double kSpiccatoContactFraction = 0.5; // VERIFY (part of bounce cycle on string)
inline constexpr double kSpiccatoLandingSeconds = 0.008; // UNSOURCED
inline constexpr double kDetacheLandingSeconds = 0.03;   // UNSOURCED
inline constexpr double kAccentSeconds = 0.06;           // UNSOURCED
// Rest damping after a spiccato bounce. Guettler and Askenfelt describe
// the hair settling on the string between bounces; this T60 is UNSOURCED
// (chosen, not measured).
inline constexpr double kSpiccatoRestT60Seconds = 0.2;  // VERIFY (0.15-0.3 s range chosen; 0.04 s silenced the gaps)
// Score velocity (1-127) to accent: gain 1 at velocity <= 80 rising to
// 1.6 at 127. UNSOURCED mapping.
[[nodiscard]] inline BowArticulation articulationForNote(double seconds, int velocity) {
  BowArticulation a;
  const double v = std::clamp((static_cast<double>(velocity) - 80.0) / 47.0, 0.0, 1.0);
  a.accentGain = 1.0 + 0.6 * v;
  a.accentSeconds = kAccentSeconds;
  if (seconds > 0.0 && seconds < kSpiccatoMaxSeconds) {
    a.onsetSeconds = kSpiccatoLandingSeconds;
    a.contactSeconds = kSpiccatoContactFraction * seconds;
    a.bounce = true;
    a.restDampT60 = kSpiccatoRestT60Seconds;
  } else {
    a.onsetSeconds = kDetacheLandingSeconds;
  }
  return a;
}

class BowedStringVoice final : public IVoice {
 public:
  // sustainFrames is how long the bow stays down. Zero lifts it before
  // the first sample. fitPartials and dispersionStages follow
  // designTunedLoop. κ = 0 requires zero dispersion stages. body is
  // optional; null keeps the rigid bridge and the velocity sample.
  [[nodiscard]] static Result<BowedStringVoice, QuantityError> create(
      const StringCourse& course, const BowStroke& stroke, LossModel loss,
      double stiffnessKappa, int fitPartials, int dispersionStages,
      SampleRateHz rate, std::uint64_t sustainFrames,
      const BowBodyDrive* body = nullptr) {
    using R = Result<BowedStringVoice, QuantityError>;
    const double fs = rate.value();
    const double beta = stroke.bridgeFraction;
    if (!std::isfinite(fs) || !(fs > 0.0) || !std::isfinite(beta) ||
        !(beta > 0.0) || !(beta < 1.0))
      return R::failure(QuantityError::InvalidArgument);
    const double muS = stroke.friction.staticCoefficient;
    const double muD = stroke.friction.slidingCoefficient;
    if (!std::isfinite(stroke.speedMetersPerSecond) ||
        stroke.speedMetersPerSecond == 0.0 ||
        !std::isfinite(stroke.normalForceNewtons) ||
        !(stroke.normalForceNewtons > 0.0) || !std::isfinite(muS) ||
        !std::isfinite(muD) || !(muD >= 0.0) || !(muS > muD))
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
    int bridge = static_cast<int>(std::llround(beta * static_cast<double>(loop)));
    if (bridge < 1) bridge = 1;
    if (bridge > loop - 1) bridge = loop - 1;
    const int nut = loop - bridge;
    const double density = course.material.linearDensity.value();
    const double tension = course.tension.value();
    if (!std::isfinite(density) || !(density > 0.0) || !std::isfinite(tension) ||
        !(tension > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    const double waveSpeed = std::sqrt(tension / density);
    const double impedance = density * waveSpeed;
    if (!std::isfinite(impedance) || !(impedance > 0.0))
      return R::failure(QuantityError::RangeError);
    auto pole = loopPole(filters, rate, 1);
    if (!pole.ok()) return R::failure(pole.error());
    BowedStringVoice voice{filters, rate, stroke, impedance, bridge, nut,
                           sustainFrames, pole.value().frequency.value(),
                           pole.value().decayRate};
    voice.retuneStore_.emplace(RetuneSpec{course, loss, stiffnessKappa, fitPartials, dispersionStages});
    if (body != nullptr) {
      auto attached = voice.attachBody(*body);
      if (!attached.ok()) return R::failure(attached.error());
    }
    return R::success(std::move(voice));
  }

  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }

  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(
      double* out, FrameCount n) override {
    using R = Result<FrameCount, QuantityError>;
    const std::uint64_t count = static_cast<std::uint64_t>(n.value());
    if (count == 0) return R::success(n);
    if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
    const double twoZ = 2.0 * impedance_;
    const double fs = rate_.value();
    for (std::uint64_t i = 0; i < count; ++i) {
      const double delayedBridge = bridge_[bridgePos_];
      double delayedNut = nut_[nutPos_];
      const double t = static_cast<double>(frame_) / fs;
      if (!vib_.empty()) {
        vib_[vibPos_] = delayedNut;
        double depth = 0.0;
        if (t > expression_.vibratoOnsetSeconds) {
          const double since = t - expression_.vibratoOnsetSeconds;
          const double ramp = expression_.vibratoRampSeconds > 0.0
                                  ? std::min(1.0, since / expression_.vibratoRampSeconds)
                                  : 1.0;
          // vibSamples_ is the swing for the open-string period; a stopped
          // note's loop is shorter, so scale to keep the depth in cents.
          const double stopScale =
              stoppedHz_ > fundamentalHz_ ? fundamentalHz_ / stoppedHz_ : 1.0;
          depth = vibSamples_ * stopScale * ramp *
                  std::sin(constants::twoPi * expression_.vibratoRateHz * since);
        }
        const double d = static_cast<double>(vibMean_) + fingerShift_ + depth;
        const auto whole = static_cast<std::size_t>(std::floor(d));
        const double frac = d - std::floor(d);
        const std::size_t size = vib_.size();
        const double a = vib_[(vibPos_ + size - whole) % size];
        const double b = vib_[(vibPos_ + size - whole - 1) % size];
        delayedNut = (1.0 - frac) * a + frac * b;
        vibPos_ = (vibPos_ + 1) % size;
      }
      double envelope = 1.0;
      if (expression_.attackSeconds > 0.0 && t < expression_.attackSeconds)
        envelope = expression_.attackFraction +
                   (1.0 - expression_.attackFraction) * 0.5 *
                       (1.0 - std::cos(constants::pi * t / expression_.attackSeconds));
      double reversal = 1.0;
      if (changeEnd_ > 0) {
        if (frame_ >= changeEnd_) {
          stroke_.speedMetersPerSecond = -stroke_.speedMetersPerSecond;
          changeEnd_ = 0;
        } else {
          const double tau = static_cast<double>(frame_ - changeStart_) /
                             static_cast<double>(changeEnd_ - changeStart_);
          reversal = std::cos(constants::pi * tau);
          if (std::fabs(reversal) < 1.0e-6) reversal = 1.0e-6;
        }
      }
      if (expression_.releaseSeconds > 0.0 && frame_ < sustainFrames_) {
        const double left = static_cast<double>(sustainFrames_ - frame_) / fs;
        if (left < expression_.releaseSeconds)
          envelope *= std::max(left / expression_.releaseSeconds, 1.0e-3);
      }
      {
        const double tn = static_cast<double>(frame_ - noteStart_) / fs;
        envelope *= art_.level;
        if (art_.accentSeconds > 0.0 && art_.accentGain != 1.0)
          envelope *= 1.0 + (art_.accentGain - 1.0) * std::exp(-tn / art_.accentSeconds);
        if (art_.bounce && art_.contactSeconds > 0.0 && tn < art_.contactSeconds)
          envelope *= std::max(0.05, std::sin(constants::pi * tn / art_.contactSeconds));
        else if (art_.onsetSeconds > 0.0 && tn < art_.onsetSeconds)
          envelope *= std::max(0.05, 0.5 * (1.0 - std::cos(constants::pi * tn / art_.onsetSeconds)));
      }
      const bool bowOn = frame_ < sustainFrames_ && !(liftAt_ > 0 && frame_ >= liftAt_);
      const double filtered = applyFilter(delayedBridge);
      if (!std::isfinite(filtered)) return R::failure(QuantityError::RangeError);
      const double incomingBridge =
          radiating_ ? bridgeVelocity_ - filtered : -filtered;
      const double incomingNut = -delayedNut;
      const double history = incomingBridge + incomingNut;
      double outgoingBridge = 0.0;
      double outgoingNut = 0.0;
      double velocity = 0.0;
      if (bowOn) {
        auto junction = stickSlipJunction(
            history, incomingBridge, incomingNut, stroke_.speedMetersPerSecond * envelope * reversal,
            impedance_, stroke_.normalForceNewtons * envelope, stroke_.friction, wasStuck_);
        if (!junction.ok()) return R::failure(junction.error());
        const double force = junction.value().frictionForce;
        outgoingBridge = incomingNut + force / twoZ;
        outgoingNut = incomingBridge + force / twoZ;
        velocity = junction.value().stringVelocity;
        wasStuck_ = junction.value().stuck;
      } else {
        outgoingBridge = incomingNut;
        outgoingNut = incomingBridge;
        velocity = history;
      }
      if (!std::isfinite(outgoingBridge) || !std::isfinite(outgoingNut) ||
          !std::isfinite(velocity))
        return R::failure(QuantityError::RangeError);
      if (!bowOn && liftAt_ > 0 && frame_ >= liftAt_ && art_.restDampT60 > 0.0 &&
          fundamentalHz_ > 0.0) {
        // A wave passes the bow point twice per period: g^(2 f0 T60) = 10^-3.
        const double g = std::pow(10.0, -3.0 / (2.0 * art_.restDampT60 * fundamentalHz_));
        outgoingBridge *= g;
        outgoingNut *= g;
      }
      bridge_[bridgePos_] = outgoingBridge;
      nut_[nutPos_] = outgoingNut;
      bridgePos_ = (bridgePos_ + 1) % bridge_.size();
      nutPos_ = (nutPos_ + 1) % nut_.size();
      if (radiating_) {
        const double force = impedance_ * (2.0 * filtered - bridgeVelocity_);
        if (!std::isfinite(force)) return R::failure(QuantityError::RangeError);
        const double dt = 1.0 / fs;
        double bridgeVelocity = 0.0;
        for (BodyMode& mode : modes_) {
          auto stepped = stepConstantForceOscillator(
              mode.state, mode.omega, mode.sigma, force / mode.mass, dt);
          if (!stepped.ok()) return R::failure(stepped.error());
          mode.state = stepped.value();
          bridgeVelocity += mode.state.velocity;
        }
        if (!std::isfinite(bridgeVelocity))
          return R::failure(QuantityError::RangeError);
        const double acceleration = (bridgeVelocity - bridgeVelocity_) * fs;
        bridgeVelocity_ = bridgeVelocity;
        const double delayed = radiation_[radiationPos_];
        radiation_[radiationPos_] = acceleration;
        radiationPos_ = (radiationPos_ + 1) % radiation_.size();
        out[i] = scale_ * delayed;
      } else {
        out[i] = velocity;
      }
      ++frame_;
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
    wasStuck_ = true;
    frame_ = 0;
    bridgeVelocity_ = 0.0;
    for (BodyMode& mode : modes_) mode.state = {};
    std::fill(radiation_.begin(), radiation_.end(), 0.0);
    radiationPos_ = 0;
    std::fill(vib_.begin(), vib_.end(), 0.0);
    vibPos_ = 0;
  }

  // Bow change: over transitionSeconds the bow velocity swings on a half
  // cosine from +v through zero to -v; normal force is held, so the
  // Helmholtz motion re-forms on the new stroke (bow-change transients:
  // Guettler and Askenfelt, JASA 101, 1997; the cosine is UNSOURCED).
  // A violin bow has about 0.65 m of hair (VERIFY), so at speed v a
  // stroke lasts about 0.65 / v seconds. False while a change runs.
  [[nodiscard]] bool changeBow(double transitionSeconds) {
    if (!std::isfinite(transitionSeconds) || !(transitionSeconds > 0.0) || changeEnd_ > 0)
      return false;
    changeStart_ = frame_;
    changeEnd_ = frame_ + std::max<std::uint64_t>(
        2, static_cast<std::uint64_t>(std::llround(transitionSeconds * rate_.value())));
    return true;
  }
  // Keep the bow down for frames more samples from now (0 lifts it now).
  // While bowed inside Schelleng's range the Helmholtz motion is
  // self-sustaining; once lifted the string rings down freely.
  void sustainFor(std::uint64_t frames) { sustainFrames_ = frame_ + frames; }
  // Move the bow point (beta of the open string from the bridge) and set
  // the matching normal force, e.g. at a bow change. Players move toward
  // the bridge for louder notes, with more force (Schoonderwaldt 2009).
  // The bridge and nut segments trade whole samples, keeping the loop
  // length (pitch); the newest samples on each side are kept. This is an
  // instantaneous jump of the contact point (approximation; VERIFY).
  [[nodiscard]] bool setBowPosition(double beta, double normalForce) {
    if (!std::isfinite(beta) || !(beta > 0.0) || !(beta < 0.5) || !std::isfinite(normalForce) ||
        !(normalForce > 0.0))
      return false;
    const long long loop = static_cast<long long>(bridge_.size() + nut_.size()) +
                           (vib_.empty() ? 0 : vibMean_);
    const long long nb = std::llround(beta * static_cast<double>(loop));
    const long long nn = static_cast<long long>(bridge_.size() + nut_.size()) - nb;
    if (nb < 1 || nn < 1) return false;
    auto resize = [](std::vector<double>& line, std::size_t& pos, std::size_t target) {
      const std::size_t n = line.size();
      std::vector<double> kept(target, 0.0);
      for (std::size_t i = 0; i < target; ++i) {
        const long long from = static_cast<long long>(n) - static_cast<long long>(target) +
                               static_cast<long long>(i);
        if (from >= 0) kept[i] = line[(pos + static_cast<std::size_t>(from)) % n];
      }
      line = std::move(kept);
      pos = 0;
    };
    resize(bridge_, bridgePos_, static_cast<std::size_t>(nb));
    resize(nut_, nutPos_, static_cast<std::size_t>(nn));
    stroke_.bridgeFraction = beta;
    stroke_.normalForceNewtons = normalForce;
    return true;
  }
  // Start a note articulation now (see BowArticulation). False on a
  // negative or non-finite field or accentGain <= 0.
  [[nodiscard]] bool articulate(const BowArticulation& a) {
    if (!std::isfinite(a.accentGain) || !(a.accentGain > 0.0) || !std::isfinite(a.accentSeconds) ||
        a.accentSeconds < 0.0 || !std::isfinite(a.onsetSeconds) || a.onsetSeconds < 0.0 ||
        !std::isfinite(a.contactSeconds) || a.contactSeconds < 0.0 || !std::isfinite(a.level) ||
        !(a.level > 0.0))
      return false;
    art_ = a;
    noteStart_ = frame_;
    liftAt_ = a.contactSeconds > 0.0
                  ? frame_ + std::max<std::uint64_t>(1, static_cast<std::uint64_t>(std::llround(
                                                            a.contactSeconds * rate_.value())))
                  : 0;
    return true;
  }
  // Finger change on the sounding string: retune the loop to hz (at or
  // above the created pitch) by moving the stop, keeping string state.
  // Whole samples come out of the nut segment, the fraction from the
  // interpolated line. Loss and dispersion filters stay the created
  // course's (an approximation; VERIFY over large shifts). False when
  // out of range.
  [[nodiscard]] bool setStoppedFrequency(double hz) {
    if (!std::isfinite(hz) || !(hz > 0.0)) return false;
    if (retuneFilters_ && retuneStore_.has_value()) return refinger(hz);
    const double fs = rate_.value();
    const double shorter = fs / fundamentalHz_ - fs / hz;
    if (shorter < -1.0e-9) return false;
    if (vib_.empty()) {
      if (nut_.size() < 4) return false;
      std::vector<double> kept(nut_.size() - 2);
      for (std::size_t i = 0; i < kept.size(); ++i)
        kept[i] = nut_[(nutPos_ + 2 + i) % nut_.size()];
      nut_ = std::move(kept);
      nutPos_ = 0;
      vibMean_ = 2;
      vib_.assign(6, 0.0);
      vibPos_ = 0;
      openNut_ = nut_.size();
    }
    const int whole = static_cast<int>(std::floor(shorter));
    if (whole > static_cast<int>(openNut_) - 1) return false;
    const std::size_t target = openNut_ - static_cast<std::size_t>(whole);
    const std::size_t n = nut_.size();
    if (target != n) {
      std::vector<double> kept(target, 0.0);
      for (std::size_t i = 0; i < target; ++i) {
        // Keep the newest samples when shortening, pad the oldest end when lengthening.
        const long long from = static_cast<long long>(n) - static_cast<long long>(target) +
                               static_cast<long long>(i);
        kept[i] = from >= 0 ? nut_[(nutPos_ + static_cast<std::size_t>(from)) % n] : 0.0;
      }
      nut_ = std::move(kept);
      nutPos_ = 0;
    }
    fingerShift_ = -(shorter - static_cast<double>(whole));
    stoppedHz_ = hz;
    return true;
  }
  [[nodiscard]] double stoppedHz() const noexcept {
    return stoppedHz_ > 0.0 ? stoppedHz_ : fundamentalHz_;
  }

  // Finger change mode. true (default): the loop is redesigned for the
  // stopped course (fingerStop of the created course, same loss model and
  // stiffness), so loss, dispersion and the Thiran fraction match the new
  // length, as a fresh voice on that stop would be (designTunedLoop). The
  // bridge segment keeps its length (the bow stays where it is); the nut
  // segment gives up or gains samples at its far (finger) end. false: the
  // older shift of the created loop.
  void setFingerRetune(bool on) noexcept { retuneFilters_ = on; }

  // Replace the stroke between blocks (crescendo, bow speed). The bridge
  // fraction is fixed at create; a different one returns false.
  [[nodiscard]] bool setStroke(const BowStroke& stroke) {
    if (stroke.bridgeFraction != stroke_.bridgeFraction ||
        !std::isfinite(stroke.speedMetersPerSecond) || stroke.speedMetersPerSecond == 0.0 ||
        !std::isfinite(stroke.normalForceNewtons) || !(stroke.normalForceNewtons > 0.0) ||
        !(stroke.friction.staticCoefficient > stroke.friction.slidingCoefficient))
      return false;
    stroke_ = stroke;
    return true;
  }

  // Set the player expression before rendering. False (and no change) on
  // a negative or non-finite field, an attack fraction outside (0, 1], or
  // vibrato deeper than the nut segment can lend.
  [[nodiscard]] bool setExpression(const BowExpression& e) {
    const double fields[] = {e.vibratoRateHz, e.vibratoCents, e.vibratoOnsetSeconds,
                             e.vibratoRampSeconds, e.attackSeconds, e.releaseSeconds};
    for (double v : fields)
      if (!std::isfinite(v) || v < 0.0) return false;
    if (!std::isfinite(e.attackFraction) || !(e.attackFraction > 0.0) || e.attackFraction > 1.0)
      return false;
    const double period = rate_.value() / fundamentalHz_;
    const double swing = period * (std::pow(2.0, e.vibratoCents / 1200.0) - 1.0);
    if (e.vibratoRateHz > 0.0 && e.vibratoCents > 0.0) {
      const int mean = static_cast<int>(std::ceil(swing)) + 2;
      if (vib_.empty()) {
        if (static_cast<int>(nut_.size()) - mean < 1) return false;
        nut_.assign(nut_.size() - static_cast<std::size_t>(mean), 0.0);
        nutPos_ = 0;
        vibMean_ = mean;
        // The read point swings to mean + swing samples back, so the ring must
        // hold that plus the interpolation neighbour. (It was mean + 3: deeper
        // vibrato wrapped the read, shortening the loop - a sharp offset.)
        vib_.assign(static_cast<std::size_t>(mean) + static_cast<std::size_t>(std::ceil(swing)) + 3, 0.0);
        vibPos_ = 0;
        openNut_ = nut_.size();
      } else if (mean > vibMean_) {
        return false;
      }
      vibSamples_ = swing;
    }
    expression_ = e;
    return true;
  }

  [[nodiscard]] double fundamentalHz() const noexcept { return fundamentalHz_; }
  [[nodiscard]] double fundamentalDecay() const noexcept {
    return fundamentalDecay_;
  }
  [[nodiscard]] double realizedBridgeFraction() const noexcept {
    const double loop =
        static_cast<double>(bridge_.size() + nut_.size());
    return static_cast<double>(bridge_.size()) / loop;
  }
  [[nodiscard]] int bridgeDelaySamples() const noexcept {
    return static_cast<int>(bridge_.size());
  }
  [[nodiscard]] int nutDelaySamples() const noexcept {
    return static_cast<int>(nut_.size());
  }
  [[nodiscard]] double impedance() const noexcept { return impedance_; }
  [[nodiscard]] bool radiating() const noexcept { return radiating_; }

 private:
  struct BodyMode {
    double omega = 0.0;
    double sigma = 0.0;
    double mass = 0.0;
    OscillatorState state;
  };

  // Modal mass, Q, area, distance, and air are the caller's. A heavy
  // mass leaves the bridge nearly rigid. The delay is r/c, at least one
  // sample. Q must keep the mode underdamped.
  [[nodiscard]] Result<int, QuantityError> attachBody(const BowBodyDrive& body) {
    using R = Result<int, QuantityError>;
    const double fs = rate_.value();
    if (body.modes.empty() || !std::isfinite(body.radiatingAreaM2) ||
        !(body.radiatingAreaM2 > 0.0) || !std::isfinite(body.distanceMeters) ||
        !(body.distanceMeters > 0.0) || !std::isfinite(body.airPressurePa) ||
        !(body.airPressurePa > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    std::vector<BodyMode> built;
    built.reserve(body.modes.size());
    for (const BodyModeSpec& spec : body.modes) {
      auto resonator = makeBodyResonator(spec);
      if (!resonator.ok()) return R::failure(resonator.error());
      const double omega = resonator.value().omega0;
      const double sigma = omega / (2.0 * resonator.value().qualityFactor);
      if (!(sigma < omega)) return R::failure(QuantityError::DomainError);
      built.push_back(BodyMode{omega, sigma, resonator.value().modalMassKg, {}});
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
    const double scale = rho.value().value() * body.radiatingAreaM2 /
                         (4.0 * constants::pi * body.distanceMeters);
    if (!std::isfinite(scale) || !(scale > 0.0))
      return R::failure(QuantityError::RangeError);
    modes_ = std::move(built);
    scale_ = scale;
    radiation_.assign(static_cast<std::size_t>(delay), 0.0);
    radiationPos_ = 0;
    bridgeVelocity_ = 0.0;
    radiating_ = true;
    return R::success(delay);
  }

  BowedStringVoice(LoopFilters filters, SampleRateHz rate, BowStroke stroke,
                   double impedance, int bridge, int nut,
                   std::uint64_t sustainFrames, double fundamentalHz,
                   double fundamentalDecay)
      : filters_(filters),
        rate_(rate),
        stroke_(stroke),
        impedance_(impedance),
        sustainFrames_(sustainFrames),
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
  BowStroke stroke_;
  double impedance_;
  std::uint64_t sustainFrames_;
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
  bool wasStuck_ = true;
  BowArticulation art_{};
  std::uint64_t noteStart_ = 0;
  std::uint64_t liftAt_ = 0;
  std::uint64_t frame_ = 0;
  bool radiating_ = false;
  double scale_ = 0.0;
  double bridgeVelocity_ = 0.0;
  std::vector<BodyMode> modes_;
  std::vector<double> radiation_;
  std::size_t radiationPos_ = 0;
  BowExpression expression_{};
  std::vector<double> vib_;
  std::size_t vibPos_ = 0;
  int vibMean_ = 0;
  double vibSamples_ = 0.0;
  double fingerShift_ = 0.0;
  double stoppedHz_ = 0.0;
  std::uint64_t changeStart_ = 0;
  std::uint64_t changeEnd_ = 0;
  std::size_t openNut_ = 0;
  struct RetuneSpec {
    StringCourse course;
    LossModel loss;
    double kappa;
    int fit;
    int disp;
  };
  std::optional<RetuneSpec> retuneStore_;
  bool retuneFilters_ = true;
  [[nodiscard]] bool refinger(double hz) {
    const RetuneSpec& r = *retuneStore_;
    const double open = r.course.tuning.frequency.value();
    if (!(hz >= open * (1.0 - 1e-6))) return false;
    StringCourse stopped = r.course;
    if (hz > open * (1.0 + 1e-6)) {
      auto s = fingerStop(r.course, 1.0 - open / hz);
      if (!s.ok()) return false;
      stopped = s.value();
    }
    auto system = pluckedCourseSystem(stopped, r.loss, r.kappa);
    if (!system.ok()) return false;
    auto design = designTunedLoop(system.value(), rate_, r.fit, r.disp);
    if (!design.ok()) return false;
    const LoopFilters filters = design.value().filters;
    if (filters.dispersionStages != filters_.dispersionStages) return false;
    const long long loop = static_cast<long long>(filters.intervals) * 2;
    const long long nutLen = loop - static_cast<long long>(bridge_.size()) -
                             static_cast<long long>(vib_.empty() ? 0 : vibMean_);
    if (nutLen < 1) return false;
    const auto target = static_cast<std::size_t>(nutLen);
    const std::size_t n = nut_.size();
    if (target != n) {
      std::vector<double> kept(target, 0.0);
      for (std::size_t i = 0; i < target; ++i) {
        const long long from = static_cast<long long>(n) - static_cast<long long>(target) +
                               static_cast<long long>(i);
        kept[i] = from >= 0 ? nut_[(nutPos_ + static_cast<std::size_t>(from)) % n] : 0.0;
      }
      nut_ = std::move(kept);
      nutPos_ = 0;
    }
    filters_ = filters;
    fingerShift_ = 0.0;
    stoppedHz_ = hz;
    return true;
  }
};

// In-range bow stroke for the stock violin strings. Force is 0.2 of
// Schelleng's maximum 2 Z v / (beta (mu_s - mu_d)) (Woodhouse 9.3.1).
// Swept on this loop (beta 0.1, 0.2 m/s, 4x oversampled, T60 2 s / 0.25 s
// at 6 f0), each open string G3 D4 A4 E5:
//   0.05: dies away on every string (below this loop's minimum);
//   0.075-0.10: a weak non-Helmholtz regime at about 0.07 rms that a bow
//     change kicks up 3-6 dB into full Helmholtz motion (the +5 dB seen
//     after a bow change);
//   0.15: locks a different periodicity (-144 cents);
//   0.20: full Helmholtz, steady, bow change within 1.6 dB; pitch G3 -11,
//     D4 -8, A4 -6, E5 -4 cents.
// The fraction is a swept value of this model, not a published player
// force (VERIFY against Schoonderwaldt's measured bowing ranges).
inline constexpr double kViolinBowForceFraction = 0.2;
// Playing bow position. Players keep beta between about 0.05 and 0.2,
// nearer the bridge for loud playing (Schoonderwaldt, "Mechanics and
// acoustics of violin bowing", KTH PhD thesis 2009; value VERIFY).
// 0.15 is a mezzo choice; 0.1 left harmonics 9-15 near -15 dB.
inline constexpr double kViolinPlayingBeta = 0.15;  // VERIFY
// Dynamic bow position: beta from kViolinBetaForte (loud) to
// kViolinBetaPiano (soft), Schoonderwaldt's measured range is roughly
// 0.05-0.2 (VERIFY); level is the note level in dB re mezzo.
inline constexpr double kViolinBetaForte = 0.08;   // VERIFY
inline constexpr double kViolinBetaPiano = 0.18;   // VERIFY
[[nodiscard]] inline double violinBetaForLevelDb(double levelDb) {
  const double x = std::clamp(0.5 - levelDb / 24.0, 0.0, 1.0);  // +-12 dB spans the range (UNSOURCED)
  return kViolinBetaForte + x * (kViolinBetaPiano - kViolinBetaForte);
}
// String loss for the stock violin loop: T60 2 s at f0 and 0.08 s at
// 6 f0. Measured violin-string damping rises steeply with frequency
// (Woodhouse, Acta Acustica 90, 2004, string loss factors) but these two
// T60 values are a fit to the Madoyan comparison spectrum, not measured:
// UNSOURCED. No body filter is applied: the body resonances (A0 ~280 Hz,
// B1 ~450-550 Hz, the 2-3 kHz bridge hill) are absent here.
inline constexpr double kViolinT60LowSeconds = 2.0;    // UNSOURCED
// 0.08 s was tried (v2-v4): it darkened the dry render but shifted pitch
// +2-5 cents sharp (loss-filter phase at f0); with a room stage the dark
// top end of the reference came from the room, so 0.25 s is kept.
inline constexpr double kViolinT60HighSeconds = 0.25;  // UNSOURCED
[[nodiscard]] inline Result<LossModel, QuantityError> violinStringLoss(const StringCourse& c) {
  using R = Result<LossModel, QuantityError>;
  const double f = c.tuning.frequency.value();
  auto ideal = courseString(c);
  if (!ideal.ok()) return R::failure(ideal.error());
  return lossFromT60(ideal.value().speed, FrequencyHz{f}, DurationSeconds{kViolinT60LowSeconds},
                     FrequencyHz{6.0 * f}, DurationSeconds{kViolinT60HighSeconds});
}
// Violin vibrato. Mellody and Wakefield (JASA 107, 2000) analyse violin
// vibrato at roughly 5-7 Hz with depths of tens of cents (VERIFY exact
// values). The Madoyan reference measured here (audiodiag) shows ~7 Hz
// and ~20 cents. Onset 0.1 s and ramp 0.15 s are UNSOURCED, chosen so
// 0.2-0.3 s notes carry vibrato.
inline constexpr double kViolinVibratoRateHz = 6.5;        // VERIFY
inline constexpr double kViolinVibratoCents = 20.0;        // VERIFY
inline constexpr double kViolinVibratoOnsetSeconds = 0.1;  // UNSOURCED
inline constexpr double kViolinVibratoRampSeconds = 0.15;  // UNSOURCED
inline constexpr double kViolinHyperbolicV0 = 0.1;  // m/s, VERIFY
[[nodiscard]] inline BowStroke violinBowStroke(const StringCourse& course, double beta,
                                               double speed) {
  const double mu = course.material.linearDensity.value();
  const double impedance = mu * std::sqrt(course.tension.value() / mu);
  // Hyperbolic rosin curve (Smith and Woodhouse 2000), v0 0.1 m/s VERIFY.
  // Measured on the A string at 0.2 of maximum: fingered notes +1.3 to
  // +3.7 cents against -4 to -11 with the Coulomb law.
  const BowFriction friction{0.8, 0.3, kViolinHyperbolicV0};
  const double maximum = 2.0 * impedance * std::fabs(speed) /
                         (beta * (friction.staticCoefficient - friction.slidingCoefficient));
  return BowStroke{beta, speed, kViolinBowForceFraction * maximum, friction};
}
// The stick-slip switch happens on sample instants, so the Helmholtz
// period of the sampled loop locks to whole samples: measured here at
// 48 kHz, A4 (109.09-sample period) read -14.2 cents (110 samples) or
// -35.5 cents at 0.10-0.50 of Schelleng's maximum, the same at every bow
// speed. Published Helmholtz flattening is a few cents (McIntyre,
// Schumacher and Woodhouse, JASA 69, 1981), so this is a discretization
// error. Running the voice at K times the rate makes the lock 1/K sample
// (about 14/K cents at A4). The output is the mean of each K samples
// (boxcar decimation; its aliasing above fs/2 is not filtered further).
class OversampledBowedStringVoice final : public IVoice {
 public:
  [[nodiscard]] static Result<OversampledBowedStringVoice, QuantityError> create(
      const StringCourse& course, const BowStroke& stroke, LossModel loss,
      double stiffnessKappa, int fitPartials, int dispersionStages, SampleRateHz rate,
      std::uint64_t sustainFrames, int factor) {
    using R = Result<OversampledBowedStringVoice, QuantityError>;
    if (factor < 1 || factor > 32) return R::failure(QuantityError::InvalidArgument);
    auto inner = BowedStringVoice::create(course, stroke, loss, stiffnessKappa, fitPartials,
                                          dispersionStages, SampleRateHz{rate.value() * factor},
                                          sustainFrames * static_cast<std::uint64_t>(factor));
    if (!inner.ok()) return R::failure(inner.error());
    return R::success(OversampledBowedStringVoice{std::move(inner).value(), rate, factor});
  }
  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }
  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(double* out, FrameCount n) override {
    using R = Result<FrameCount, QuantityError>;
    const auto count = static_cast<std::size_t>(n.value());
    if (count == 0) return R::success(n);
    if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
    buffer_.resize(count * static_cast<std::size_t>(factor_));
    auto done = inner_.renderBlock(buffer_.data(), FrameCount{buffer_.size()});
    if (!done.ok()) return R::failure(done.error());
    for (std::size_t i = 0; i < count; ++i) {
      double sum = 0.0;
      for (int k = 0; k < factor_; ++k) sum += buffer_[i * static_cast<std::size_t>(factor_) + static_cast<std::size_t>(k)];
      out[i] = sum / factor_;
    }
    return R::success(n);
  }
  void reset() override { inner_.reset(); }
  [[nodiscard]] BowedStringVoice& inner() noexcept { return inner_; }
  [[nodiscard]] bool changeBow(double seconds) { return inner_.changeBow(seconds); }
  void sustainFor(std::uint64_t frames) { inner_.sustainFor(frames * static_cast<std::uint64_t>(factor_)); }
  [[nodiscard]] bool articulate(const BowArticulation& a) { return inner_.articulate(a); }
  [[nodiscard]] bool setBowPosition(double beta, double force) { return inner_.setBowPosition(beta, force); }
  [[nodiscard]] bool setStoppedFrequency(double hz) { return inner_.setStoppedFrequency(hz); }
  [[nodiscard]] bool setExpression(const BowExpression& e) { return inner_.setExpression(e); }
  [[nodiscard]] bool setStroke(const BowStroke& s) { return inner_.setStroke(s); }
  [[nodiscard]] double fundamentalHz() const noexcept { return inner_.fundamentalHz(); }

 private:
  OversampledBowedStringVoice(BowedStringVoice inner, SampleRateHz rate, int factor)
      : inner_(std::move(inner)), rate_(rate), factor_(factor) {}
  BowedStringVoice inner_;
  SampleRateHz rate_;
  int factor_;
  std::vector<double> buffer_;
};
}  // namespace mmathen
