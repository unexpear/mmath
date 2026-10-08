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
// captureStickSlip does not enforce it either. A soundboard is not in
// the loop.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "../physics/tuned_waveguide.h"
#include "../render/voice.h"
#include "bow.h"
#include "plucked_string.h"

namespace mmathen {

class BowedStringVoice final : public IVoice {
 public:
  // sustainFrames is how long the bow stays down. Zero lifts it before
  // the first sample. fitPartials and dispersionStages follow
  // designTunedLoop. κ = 0 requires zero dispersion stages.
  [[nodiscard]] static Result<BowedStringVoice, QuantityError> create(
      const StringCourse& course, const BowStroke& stroke, LossModel loss,
      double stiffnessKappa, int fitPartials, int dispersionStages,
      SampleRateHz rate, std::uint64_t sustainFrames) {
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
    return R::success(BowedStringVoice{
        filters, rate, stroke, impedance, bridge, nut, sustainFrames,
        pole.value().frequency.value(), pole.value().decayRate});
  }

  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }

  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(
      double* out, FrameCount n) override {
    using R = Result<FrameCount, QuantityError>;
    const std::uint64_t count = static_cast<std::uint64_t>(n.value());
    if (count == 0) return R::success(n);
    if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
    const double twoZ = 2.0 * impedance_;
    for (std::uint64_t i = 0; i < count; ++i) {
      const double delayedBridge = bridge_[bridgePos_];
      const double delayedNut = nut_[nutPos_];
      const double filtered = applyFilter(delayedBridge);
      if (!std::isfinite(filtered)) return R::failure(QuantityError::RangeError);
      const double incomingBridge = -filtered;
      const double incomingNut = -delayedNut;
      const double history = incomingBridge + incomingNut;
      double outgoingBridge = 0.0;
      double outgoingNut = 0.0;
      double velocity = 0.0;
      if (frame_ < sustainFrames_) {
        auto junction = stickSlipJunction(
            history, incomingBridge, incomingNut, stroke_.speedMetersPerSecond,
            impedance_, stroke_.normalForceNewtons, stroke_.friction, wasStuck_);
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
      bridge_[bridgePos_] = outgoingBridge;
      nut_[nutPos_] = outgoingNut;
      bridgePos_ = (bridgePos_ + 1) % bridge_.size();
      nutPos_ = (nutPos_ + 1) % nut_.size();
      out[i] = velocity;
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

 private:
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
  std::uint64_t frame_ = 0;
};

}  // namespace mmathen
