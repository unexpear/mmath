#pragma once

// Phase 0 — musical/acoustic domain quantities.
//
// Canonical representations: FrequencyRatio is a positive dimensionless
// ratio; Cents are signed cents (negative accepted); Beat is a signed beat
// count; TempoBPM is beats per minute; AmplitudeLinear/GainLinear are linear
// (>= 0 by convention, not by type); SignedAudioValue is a finite digital
// sample — values outside [-1,1] are ACCEPTED (overload is counted at the
// export boundary, never silently clamped here).
//
// Dimensionless does not mean interchangeable: each is a distinct tag.
// Scalar scaling is enabled for Cents, Beat, AmplitudeLinear, GainLinear,
// SignedAudioValue. FrequencyRatio and TempoBPM are not scaled.

#include <cmath>

#include "../core/result.h"
#include "dimension.h"
#include "quantity.h"

namespace mmathen {

namespace tags {
struct FrequencyRatio {};
struct Cents {};
struct Beat {};
struct TempoBPM {};
struct AmplitudeLinear {};
struct GainLinear {};
struct SignedAudioValue {};
}  // namespace tags

using FrequencyRatio = Quantity<double, DimDimensionless, tags::FrequencyRatio>;
using Cents = Quantity<double, DimDimensionless, tags::Cents>;
using Beat = Quantity<double, DimDimensionless, tags::Beat>;
using TempoBPM = Quantity<double, DimDimensionless, tags::TempoBPM>;
using AmplitudeLinear = Quantity<double, DimDimensionless, tags::AmplitudeLinear>;
using GainLinear = Quantity<double, DimDimensionless, tags::GainLinear>;
using SignedAudioValue =
    Quantity<double, DimDimensionless, tags::SignedAudioValue>;

template <>
struct allows_scalar_scaling<tags::Cents> : std::true_type {};
template <>
struct allows_scalar_scaling<tags::Beat> : std::true_type {};
template <>
struct allows_scalar_scaling<tags::AmplitudeLinear> : std::true_type {};
template <>
struct allows_scalar_scaling<tags::GainLinear> : std::true_type {};
template <>
struct allows_scalar_scaling<tags::SignedAudioValue> : std::true_type {};

// FrequencyRatio <= 0 is rejected at creation (it has no real-valued cents).
[[nodiscard]] inline Result<FrequencyRatio, QuantityError> makeFrequencyRatio(
    double v) {
  if (!std::isfinite(v) || !(v > 0.0))
    return Result<FrequencyRatio, QuantityError>::failure(
        QuantityError::InvalidArgument);
  return Result<FrequencyRatio, QuantityError>::success(FrequencyRatio{v});
}

}  // namespace mmathen
