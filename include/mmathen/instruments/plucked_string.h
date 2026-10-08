#pragma once

// A plucked course, stepped at the sample rate (docs/48).
//
// The resonator is the tuned waveguide: two rails, a Thiran fine tune,
// optional stiffness allpasses, and the one-pole loss already in
// tuned_waveguide.h. The pluck is InitialShape::pluck at the caller's
// fraction of the vibrating length. There is no gain envelope. Decay
// is the loop filter.
//
// Frequency-dependent loss and bending stiffness are arguments. This
// header stores no T60, no inharmonicity B, and no pickup location.
// kappa == 0 is the non-stiff string and requires zero dispersion
// stages. kappa > 0 is hinged stiffness and requires at least one
// dispersion stage, the same rule as designTunedLoop.
//
// The listen fraction is one interior rail node. A partial with a node
// on that node is missing from the sample, which is the position comb.
// Displacement is the string displacement in metres. CoilVelocity is
// the transverse velocity in m/s, (u[n] - u[n-1]) * fs, with u[-1]
// equal to the initial displacement so a pluck starts at rest. A
// magnetic pickup's open-circuit voltage is proportional to that
// velocity (Faraday). Turns, magnet strength, and inductance are not
// a stored gain, so the sample is not in volts. Julius O. Smith,
// Physical Audio Signal Processing, CCRMA,
// https://ccrma.stanford.edu/jos/pasp/ .
//
// The same pluck is also an Fdtd1D initial field. That grid's time
// step is the CFL step, not a requested sample rate, and its pitch is
// the grid's. The WAV voice is the tuned loop, whose fundamental is
// the course's.
//
// A bow, a hammer, and a reed are not this pluck. A soundboard is not
// in the loop. None of those absences is filled with an envelope.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "../physics/fdtd1d.h"
#include "../physics/tuned_waveguide.h"
#include "../render/voice.h"
#include "instrument.h"

namespace mmathen {

enum class StringReadout {
  Displacement,  // metres at the listen node
  CoilVelocity,  // m/s, first difference of that displacement
};

// Loss replaces the ideal course's lossless model. stiffnessKappa is
// κ in m^2/s (0 is non-stiff). InvalidArgument if the course, the
// loss, or κ is unusable.
[[nodiscard]] inline Result<WaveSystem1D, QuantityError> pluckedCourseSystem(
    const StringCourse& course, LossModel loss, double stiffnessKappa) {
  using R = Result<WaveSystem1D, QuantityError>;
  auto ideal = courseString(course);
  if (!ideal.ok()) return R::failure(ideal.error());
  auto lossy = withLoss(ideal.value(), loss);
  if (!lossy.ok()) return R::failure(lossy.error());
  return withStiffness(lossy.value(), stiffnessKappa);
}

// Triangular pluck on `system`. positionFraction is measured from the
// nut, in (0, 1). Bow, hammer, and reed are DomainError.
[[nodiscard]] inline Result<InitialShape, QuantityError> pluckedCourseShape(
    const WaveSystem1D& system, const Excitation& excitation) {
  using R = Result<InitialShape, QuantityError>;
  if (excitation.kind != ExcitationKind::Pluck)
    return R::failure(QuantityError::DomainError);
  const double position = excitation.positionFraction;
  const double amplitude = excitation.amplitudeMeters;
  const double length = system.length.value();
  if (!std::isfinite(position) || !std::isfinite(amplitude) ||
      !std::isfinite(length) || !(position > 0.0) || !(position < 1.0) ||
      !(length > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  return InitialShape::pluck(system, DistanceMeters{position * length},
                             amplitude);
}

// The same pluck on the FDTD grid. `courant` is the CFL number in
// (0, 1]. The sample period is courant * L / (intervals * c).
[[nodiscard]] inline Result<Fdtd1D, QuantityError> pluckedCourseFdtd(
    const StringCourse& course, const Excitation& pluck, LossModel loss,
    double stiffnessKappa, std::size_t intervals, double courant) {
  using R = Result<Fdtd1D, QuantityError>;
  auto system = pluckedCourseSystem(course, loss, stiffnessKappa);
  if (!system.ok()) return R::failure(system.error());
  auto shape = pluckedCourseShape(system.value(), pluck);
  if (!shape.ok()) return R::failure(shape.error());
  return Fdtd1D::create(std::move(shape).value(), intervals, courant);
}

// One course, stepped into an IVoice. listenFraction is the position
// comb, in (0, 1). fitPartials is 2..64. dispersionStages is 0..16
// and must be 0 exactly when κ is 0.
class PluckedStringVoice final : public IVoice {
 public:
  [[nodiscard]] static Result<PluckedStringVoice, QuantityError> create(
      const StringCourse& course, const Excitation& pluck, LossModel loss,
      double stiffnessKappa, int fitPartials, int dispersionStages,
      SampleRateHz rate, double listenFraction, StringReadout readout) {
    using R = Result<PluckedStringVoice, QuantityError>;
    const double fs = rate.value();
    if (!std::isfinite(fs) || !(fs > 0.0) || !std::isfinite(listenFraction) ||
        !(listenFraction > 0.0) || !(listenFraction < 1.0))
      return R::failure(QuantityError::InvalidArgument);
    auto system = pluckedCourseSystem(course, loss, stiffnessKappa);
    if (!system.ok()) return R::failure(system.error());
    auto shape = pluckedCourseShape(system.value(), pluck);
    if (!shape.ok()) return R::failure(shape.error());
    auto design = designTunedLoop(system.value(), rate, fitPartials,
                                  dispersionStages);
    if (!design.ok()) return R::failure(design.error());
    auto guide = TunedWaveguide::create(shape.value(), design.value().filters);
    if (!guide.ok()) return R::failure(guide.error());
    const std::size_t intervals = design.value().filters.intervals;
    if (intervals < 2) return R::failure(QuantityError::InvalidArgument);
    const double scaled = listenFraction * static_cast<double>(intervals);
    auto node = static_cast<std::size_t>(std::llround(scaled));
    if (node < 1) node = 1;
    if (node >= intervals) node = intervals - 1;
    auto moved = std::move(guide).value();
    auto initial = moved.valueAt(node);
    if (!initial.ok()) return R::failure(initial.error());
    const LoopPartial fundamental = design.value().partials.front();
    return R::success(PluckedStringVoice{std::move(moved), rate, node, readout,
                                         initial.value(),
                                         fundamental.frequency.value(),
                                         fundamental.decayRate});
  }

  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }

  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(
      double* out, FrameCount n) override {
    using R = Result<FrameCount, QuantityError>;
    const std::uint64_t count = static_cast<std::uint64_t>(n.value());
    if (count == 0) return R::success(n);
    if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
    const double fs = rate_.value();
    for (std::uint64_t i = 0; i < count; ++i) {
      auto sample = guide_.valueAt(node_);
      if (!sample.ok()) return R::failure(sample.error());
      const double displacement = sample.value();
      if (readout_ == StringReadout::Displacement) {
        out[i] = displacement;
      } else {
        out[i] = (displacement - previous_) * fs;
      }
      previous_ = displacement;
      guide_.step();
    }
    return R::success(n);
  }

  void reset() override {
    guide_.reset();
    auto initial = guide_.valueAt(node_);
    previous_ = initial.ok() ? initial.value() : 0.0;
  }

  [[nodiscard]] StringReadout readout() const noexcept { return readout_; }
  [[nodiscard]] std::size_t listenNode() const noexcept { return node_; }
  [[nodiscard]] std::size_t intervals() const noexcept {
    return guide_.filters().intervals;
  }
  [[nodiscard]] const LoopFilters& filters() const noexcept {
    return guide_.filters();
  }
  [[nodiscard]] double fundamentalHz() const noexcept { return fundamentalHz_; }
  [[nodiscard]] double fundamentalDecay() const noexcept {
    return fundamentalDecay_;
  }

 private:
  PluckedStringVoice(TunedWaveguide guide, SampleRateHz rate, std::size_t node,
                     StringReadout readout, double previous,
                     double fundamentalHz, double fundamentalDecay)
      : guide_(std::move(guide)),
        rate_(rate),
        node_(node),
        readout_(readout),
        previous_(previous),
        fundamentalHz_(fundamentalHz),
        fundamentalDecay_(fundamentalDecay) {}

  TunedWaveguide guide_;
  SampleRateHz rate_;
  std::size_t node_;
  StringReadout readout_;
  double previous_;
  double fundamentalHz_;
  double fundamentalDecay_;
};

}  // namespace mmathen
