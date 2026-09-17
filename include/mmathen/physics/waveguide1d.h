#pragma once

// D8 — digital waveguide rung for the 1-D wave system of wave1d.h.
//
// d'Alembert made discrete: two rails of samples, one right-going and one
// left-going, advanced one node per step (dt = dx/c, so Courant = 1 by
// construction). The field is their sum. Ends reflect with a coefficient:
// -1 at a Fixed end (u = r + l = 0), +1 at a Free end (even reflection).
// Zero initial velocity splits the initial shape equally onto both rails.
// Exact at grid points for any initial data (same argument as FDTD at
// lam = 1, to which it is equivalent — Smith, "Physical Audio Signal
// Processing"; verified against both in the tests).
//
// Lossless and fixed-length: no loop filter, no fractional delay — the
// pitch is whatever L and c give on the grid (see gridIntervalsForRate in
// fdtd1d.h). Stateful processor: buffers allocated at creation, step()
// allocation-free and O(M), reset() restores the initial state exactly.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../units/physical_units.h"
#include "fdtd1d.h"
#include "wave1d.h"

namespace mmathen {

class Waveguide1D {
 public:
  // intervals M in [2, kMaxGridIntervals]. Lossy and stiff systems need loop
  // filters matched to the model; those live in TunedWaveguide
  // (tuned_waveguide.h, D14/D15), whose partials are approximations with
  // reported errors. This exact rung refuses them: DomainError, rather than
  // silently running the ideal object.
  [[nodiscard]] static Result<Waveguide1D, QuantityError> create(
      InitialShape shape, std::size_t intervals) {
    using R = Result<Waveguide1D, QuantityError>;
    if (!shape.system().loss.lossless() || shape.system().stiffness != 0.0)
      return R::failure(QuantityError::DomainError);
    if (intervals < 2 || intervals > kMaxGridIntervals)
      return R::failure(QuantityError::InvalidArgument);
    return R::success(Waveguide1D{std::move(shape), intervals});
  }

  void step() {
    const std::size_t m = right_.size() - 1;
    for (std::size_t j = m; j > 0; --j) right_[j] = right_[j - 1];
    for (std::size_t j = 0; j < m; ++j) left_[j] = left_[j + 1];
    right_[0] = leftReflection_ * left_[0];
    left_[m] = rightReflection_ * right_[m];
    ++steps_;
  }

  void reset() {
    right_ = initialRail_;
    left_ = initialRail_;
    steps_ = 0;
  }

  [[nodiscard]] Result<double, QuantityError> valueAt(std::size_t node) const {
    using R = Result<double, QuantityError>;
    if (node >= right_.size())
      return R::failure(QuantityError::InvalidArgument);
    return R::success(right_[node] + left_[node]);
  }
  [[nodiscard]] std::uint64_t steps() const noexcept { return steps_; }
  [[nodiscard]] std::size_t intervals() const noexcept {
    return right_.size() - 1;
  }
  [[nodiscard]] DistanceMeters gridSpacing() const noexcept {
    return DistanceMeters{dx_};
  }
  [[nodiscard]] DurationSeconds timeStep() const noexcept {
    return DurationSeconds{dt_};
  }
  [[nodiscard]] TimePointSeconds time() const noexcept {
    return TimePointSeconds{static_cast<double>(steps_) * dt_};
  }

 private:
  Waveguide1D(InitialShape shape, std::size_t intervals) {
    const WaveSystem1D& sys = shape.system();
    const double l = sys.length.value();
    dx_ = l / static_cast<double>(intervals);
    dt_ = dx_ / sys.speed.value();
    leftReflection_ = sys.left == Boundary::Fixed ? -1.0 : 1.0;
    rightReflection_ = sys.right == Boundary::Fixed ? -1.0 : 1.0;
    initialRail_.resize(intervals + 1);
    for (std::size_t j = 0; j <= intervals; ++j)
      initialRail_[j] =
          0.5 * shape.valueAt(j == intervals ? l : static_cast<double>(j) * dx_);
    right_ = initialRail_;
    left_ = initialRail_;
  }

  double dx_ = 0.0;
  double dt_ = 0.0;
  double leftReflection_ = -1.0;
  double rightReflection_ = -1.0;
  std::vector<double> initialRail_;
  std::vector<double> right_;
  std::vector<double> left_;
  std::uint64_t steps_ = 0;
};

}  // namespace mmathen
