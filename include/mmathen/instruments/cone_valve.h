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
//
// A positive ValveBlow::radiusMeters replaces the ideal open-end −1
// with OpenEndReflection. The bell radius and the end correction are
// arguments. The geometric bell area is not used as that radius. A
// non-conical Bessel exponent is not discretized, and no bell curve
// is stored. The mouthpiece cup is not in this junction.

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
                  saxophone.fundamentalHz, blow, rate, false, Trumpet{});
  }

  // The lip on a cone of the trumpet's length. Apex distance, bell
  // radius, and end correction are the blow's arguments: apex is not
  // stored on the trumpet. The cup is not read.
  [[nodiscard]] static Result<ConeValveVoice, QuantityError> fromTrombone(
      const Trombone& trombone, double apexDistanceMeters, const ValveBlow& blow,
      SampleRateHz rate) {
    return fromTrumpet(trombone.bore, apexDistanceMeters, blow, rate);
  }

  [[nodiscard]] static Result<ConeValveVoice, QuantityError> fromTrumpet(
      const Trumpet& trumpet, double apexDistanceMeters, const ValveBlow& blow,
      SampleRateHz rate) {
    using R = Result<ConeValveVoice, QuantityError>;
    if (!std::isfinite(apexDistanceMeters) || !(apexDistanceMeters > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    ReedCharacteristic unused{};
    ReedInertia inertia{};
    return create(unused, inertia, trumpet.lengthMeters, apexDistanceMeters,
                  trumpet.crossSectionM2, trumpet.soundSpeedMetersPerSecond,
                  trumpet.airDensityKgPerM3, trumpet.acousticImpedance, 0.0, blow,
                  rate, true, trumpet);
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
      const double incoming =
          radiating_ ? openEnd_.step(line_[pos_]) : -line_[pos_];
      if (!std::isfinite(incoming)) return R::failure(QuantityError::RangeError);
      const bool open = frame_ < blowFrames_;
      const double mouth = open ? pressure_ : 0.0;
      if (!open) {
        if (lip_) {
          displacement_ = -restOpening_ / slitWidth_;
          velocity_ = 0.0;
        } else {
          displacement_ = closing_;
          velocity_ = 0.0;
        }
      }
      double opening = 0.0;
      if (lip_) {
        opening = restOpening_ + slitWidth_ * displacement_;
        if (opening < 0.0) opening = 0.0;
        if (opening > maximumOpening_) opening = maximumOpening_;
      } else {
        auto area = reedAreaAtDisplacement(reed_, inertia_, displacement_);
        if (!area.ok()) return R::failure(area.error());
        opening = area.value();
      }
      const double known = 2.0 * incoming - integrator_ - beta * previousPressure_;
      auto solved = solvePressure(opening, mouth, known, beta);
      if (!solved.ok()) return R::failure(solved.error());
      const double pressure = solved.value();
      const double outgoing = pressure - incoming;
      if (!std::isfinite(pressure) || !std::isfinite(outgoing))
        return R::failure(QuantityError::RangeError);
      integrator_ += beta * (pressure + previousPressure_);
      previousPressure_ = pressure;
      if (open) {
        const double delta = mouth - pressure;
        double acceleration = 0.0;
        if (lip_) {
          auto stepped =
              trumpetLipAcceleration(trumpet_, displacement_, velocity_, delta);
          if (!stepped.ok()) return R::failure(stepped.error());
          acceleration = stepped.value();
        } else {
          auto stepped =
              reedTipAcceleration(inertia_, displacement_, velocity_, delta);
          if (!stepped.ok()) return R::failure(stepped.error());
          acceleration = stepped.value();
        }
        velocity_ += acceleration * dt;
        displacement_ += velocity_ * dt;
        if (!std::isfinite(velocity_) || !std::isfinite(displacement_))
          return R::failure(QuantityError::RangeError);
      }
      line_[pos_] = outgoing;
      pos_ = (pos_ + 1) % line_.size();
      out[i] = pressure;
      ++frame_;
    }
    return R::success(n);
  }

  void reset() override {
    std::fill(line_.begin(), line_.end(), 0.0);
    pos_ = 0;
    openEnd_.reset();
    integrator_ = 0.0;
    previousPressure_ = 0.0;
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
        fundamentalHz_(fundamentalHz),
        lip_(lip),
        radiating_(radiating),
        line_(static_cast<std::size_t>(delay), 0.0) {}

  [[nodiscard]] static Result<ConeValveVoice, QuantityError> create(
      const ReedCharacteristic& reed, const ReedInertia& inertia, double length,
      double apex, double area, double speed, double density, double impedance,
      double coneHz, const ValveBlow& blow, SampleRateHz rate, bool lip,
      const Trumpet& trumpet) {
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
        !std::isfinite(blow.endCorrectionMeters) || blow.endCorrectionMeters < 0.0)
      return R::failure(QuantityError::InvalidArgument);
    const double samples = fs * 2.0 * length / speed;
    if (!std::isfinite(samples) ||
        samples > static_cast<double>(std::numeric_limits<int>::max() / 4))
      return R::failure(QuantityError::RangeError);
    int delay = static_cast<int>(std::llround(samples));
    if (delay < 1) delay = 1;
    const double alpha = speed / apex;
    if (!std::isfinite(alpha) || !(alpha > 0.0))
      return R::failure(QuantityError::RangeError);
    const bool radiating = blow.radiusMeters > 0.0;
    OpenEndReflection openEnd;
    double decay = 0.0;
    if (radiating) {
      auto filter = OpenEndReflection::create(blow.radiusMeters,
                                              blow.endCorrectionMeters, speed, rate);
      if (!filter.ok()) return R::failure(filter.error());
      openEnd = std::move(filter).value();
      auto response = openEnd.response(coneHz > 0.0 ? coneHz : speed / (2.0 * length));
      if (!response.ok()) return R::failure(response.error());
      const double magnitude =
          std::hypot(response.value().real, response.value().imag);
      if (!(magnitude > 0.0) || magnitude > 1.0)
        return R::failure(QuantityError::RangeError);
      const double trip = static_cast<double>(delay) / fs;
      decay = -std::log(magnitude) / trip;
      if (!std::isfinite(decay)) return R::failure(QuantityError::RangeError);
    }
    double hertz = coneHz;
    if (!(hertz > 0.0)) {
      const double thetaLeft = 0.5 * constants::pi;
      const double thetaRight = constants::pi;
      double lo = thetaLeft;
      double hi = thetaRight;
      const double ratio = apex / length;
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
      hertz = (0.5 * (lo + hi)) * speed / (constants::twoPi * length);
    }
    if (!std::isfinite(hertz) || !(hertz > 0.0))
      return R::failure(QuantityError::RangeError);
    ConeValveVoice voice(rate, blow, alpha, impedance, density, delay, hertz, lip,
                         radiating);
    voice.decay_ = decay;
    voice.openEnd_ = std::move(openEnd);
    voice.reed_ = reed;
    voice.inertia_ = inertia;
    voice.closing_ = inertia.closingDisplacementMeters;
    voice.trumpet_ = trumpet;
    if (lip) {
      voice.restOpening_ = trumpet.restOpeningAreaM2;
      voice.maximumOpening_ = trumpet.maximumOpeningAreaM2;
      voice.slitWidth_ = trumpet.slitWidthMeters;
      if (!(voice.slitWidth_ > 0.0)) return R::failure(QuantityError::InvalidArgument);
    }
    return R::success(std::move(voice));
  }

  [[nodiscard]] double bernoulli(double opening, double deltaP) const {
    if (!(opening > 0.0) || deltaP == 0.0) return 0.0;
    const double speed = std::sqrt(2.0 * std::fabs(deltaP) / density_);
    return std::copysign(opening * speed, deltaP);
  }

  [[nodiscard]] Result<double, QuantityError> solvePressure(double opening,
                                                           double mouth,
                                                           double known,
                                                           double beta) const {
    using R = Result<double, QuantityError>;
    const double scale = 1.0 + beta;
    if (!(opening > 0.0)) return R::success(known / scale);
    auto residual = [&](double pressure) {
      return scale * pressure - impedance_ * bernoulli(opening, mouth - pressure) -
             known;
    };
    double lo = known / scale;
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
