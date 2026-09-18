#pragma once

// Phase 0 — physical quantities.
//
// Canonical representations (see docs/03-units.md):
//   Duration, TimePoint : seconds | Frequency : cycles/s
//   AngularFrequency    : rad/s               | Phase : radians
//   SampleRate          : frames/s            | FrameCount/Index : uint64 frames
//
// Operator table (unlisted operations do not exist):
//   Duration + Duration -> Duration      Duration - Duration -> Duration
//   -Duration -> Duration
//   TimePoint - TimePoint -> Duration
//   TimePoint + Duration -> TimePoint    TimePoint - Duration -> TimePoint
//   TimePoint + TimePoint               -> forbidden
// Scalar scaling is enabled for Duration, Frequency, AngularFrequency, Phase,
// Pressure only. SampleRate, TimePoint, FrameIndex, FrameCount are not scaled.
//
// Same-representation +/- below perform raw arithmetic without range checks
// (documented IEEE-double behavior; callers needing checked arithmetic use
// the fallible conversions in conversions.h). All boundaries validate via
// make* factories and Result-returning conversions.

#include <cmath>
#include <cstdint>
#include <limits>

#include "../core/result.h"
#include "../math/comparison.h"
#include "dimension.h"
#include "quantity.h"

namespace mmathen {

namespace tags {
struct Frequency {};
struct AngularFrequency {};
struct SampleRate {};
struct TimePoint {};
struct Duration {};
struct FrameIndex {};
struct FrameCount {};
struct Phase {};
struct Pressure {};
struct Length {};
struct Speed {};
struct Temperature {};
struct Displacement {};
struct Wavenumber {};
struct Force {};
struct LinearDensity {};
struct Impedance {};
struct Density {};
struct Humidity {};
struct Attenuation {};
struct Decibel {};
struct Phon {};
struct Sone {};
struct Reflection {};
struct Transmission {};
}  // namespace tags

using FrequencyHz = Quantity<double, DimInverseTime, tags::Frequency>;
using AngularFrequencyRadPerSec =
    Quantity<double, DimInverseTime, tags::AngularFrequency>;
using SampleRateHz = Quantity<double, DimInverseTime, tags::SampleRate>;
using TimePointSeconds = Quantity<double, DimTime, tags::TimePoint>;
using DurationSeconds = Quantity<double, DimTime, tags::Duration>;
using FrameIndex = Quantity<std::uint64_t, DimCount, tags::FrameIndex>;
using FrameCount = Quantity<std::uint64_t, DimCount, tags::FrameCount>;
using PhaseRadians = Quantity<double, DimDimensionless, tags::Phase>;
using PressurePascals = Quantity<double, DimPressure, tags::Pressure>;
// M6 additions (added on demonstrated wave-physics need, not upfront):
using DistanceMeters = Quantity<double, DimLength, tags::Length>;
using DisplacementMeters = Quantity<double, DimLength, tags::Displacement>;
using SpeedMetersPerSecond = Quantity<double, DimSpeed, tags::Speed>;
using TemperatureKelvin = Quantity<double, DimTemperature, tags::Temperature>;
using WavenumberRadPerMeter =
    Quantity<double, DimLength, tags::Wavenumber>;  // rad/m, distinct from 1/m
// D2 additions (added on demonstrated wave-physics need):
using TensionNewtons = Quantity<double, DimForce, tags::Force>;
using LinearDensityKgPerM =
    Quantity<double, DimLinearDensity, tags::LinearDensity>;
using ImpedancePasPerMs =  // Pa*s/m (specific acoustic impedance, rayl)
    Quantity<double, DimImpedance, tags::Impedance>;
using DensityKgPerM3 = Quantity<double, DimDensity, tags::Density>;
// Relative humidity as a FRACTION in [0,1] (never percent: the ISO 9613
// molar-concentration equations take mole percent, converted internally).
using RelativeHumidity = Quantity<double, DimDimensionless, tags::Humidity>;
using AttenuationDbPerM = Quantity<double, DimAttenuation, tags::Attenuation>;
using Decibels = Quantity<double, DimDimensionless, tags::Decibel>;
// D18 additions: loudness level (phons, 1 phon = 1 dB SPL at 1 kHz) and
// perceived loudness (sones, doubling every 10 phon above 40).
using Phons = Quantity<double, DimDimensionless, tags::Phon>;
using Sones = Quantity<double, DimDimensionless, tags::Sone>;
using ReflectionCoefficient =
    Quantity<double, DimDimensionless, tags::Reflection>;
using TransmissionCoefficient =
    Quantity<double, DimDimensionless, tags::Transmission>;

// Relative humidity in [0,1]; percent inputs must convert before calling.
[[nodiscard]] inline Result<RelativeHumidity, QuantityError>
makeRelativeHumidity(double fraction) {
  using R = Result<RelativeHumidity, QuantityError>;
  if (!std::isfinite(fraction) || fraction < 0.0 || fraction > 1.0)
    return R::failure(QuantityError::InvalidArgument);
  return R::success(RelativeHumidity{fraction});
}

template <>
struct allows_scalar_scaling<tags::Duration> : std::true_type {};
template <>
struct allows_scalar_scaling<tags::Frequency> : std::true_type {};
template <>
struct allows_scalar_scaling<tags::AngularFrequency> : std::true_type {};
template <>
struct allows_scalar_scaling<tags::Phase> : std::true_type {};
template <>
struct allows_scalar_scaling<tags::Pressure> : std::true_type {};

template <typename T>
concept FrequencyQuantity = std::same_as<typename T::tag, tags::Frequency>;

// --- Operator table: time arithmetic (std::chrono-style separation) ---

[[nodiscard]] constexpr DurationSeconds operator+(DurationSeconds a,
                                                   DurationSeconds b) {
  return DurationSeconds{a.value() + b.value()};
}

[[nodiscard]] constexpr DurationSeconds operator-(DurationSeconds a,
                                                   DurationSeconds b) {
  return DurationSeconds{a.value() - b.value()};
}

[[nodiscard]] constexpr DurationSeconds operator-(DurationSeconds a) {
  return DurationSeconds{-a.value()};
}

[[nodiscard]] constexpr TimePointSeconds operator+(TimePointSeconds t,
                                                    DurationSeconds d) {
  return TimePointSeconds{t.value() + d.value()};
}

[[nodiscard]] constexpr TimePointSeconds operator-(TimePointSeconds t,
                                                    DurationSeconds d) {
  return TimePointSeconds{t.value() - d.value()};
}

[[nodiscard]] constexpr DurationSeconds operator-(TimePointSeconds a,
                                                   TimePointSeconds b) {
  return DurationSeconds{a.value() - b.value()};
}

// TimePoint absolute tolerance is a Duration, not another TimePoint.
[[nodiscard]] inline bool nearlyEqual(TimePointSeconds a, TimePointSeconds b,
                                      DurationSeconds absTol, double relTol) {
  return nearlyEqual(a.value(), b.value(), absTol.value(), relTol);
}

// --- Checked factories: the creation API for validated values ---

[[nodiscard]] inline Result<SampleRateHz, QuantityError> makeSampleRate(
    double v) {
  if (!std::isfinite(v) || !(v > 0.0))
    return Result<SampleRateHz, QuantityError>::failure(
        QuantityError::InvalidArgument);
  return Result<SampleRateHz, QuantityError>::success(SampleRateHz{v});
}

[[nodiscard]] inline Result<FrequencyHz, QuantityError> makeFrequency(
    double v) {
  if (!std::isfinite(v))
    return Result<FrequencyHz, QuantityError>::failure(
        QuantityError::InvalidArgument);
  return Result<FrequencyHz, QuantityError>::success(FrequencyHz{v});
}

[[nodiscard]] inline Result<DurationSeconds, QuantityError> makeDuration(
    double v) {
  if (!std::isfinite(v))
    return Result<DurationSeconds, QuantityError>::failure(
        QuantityError::InvalidArgument);
  return Result<DurationSeconds, QuantityError>::success(DurationSeconds{v});
}

[[nodiscard]] inline Result<TimePointSeconds, QuantityError> makeTimePoint(
    double v) {
  if (!std::isfinite(v))
    return Result<TimePointSeconds, QuantityError>::failure(
        QuantityError::InvalidArgument);
  return Result<TimePointSeconds, QuantityError>::success(TimePointSeconds{v});
}

// --- Integer counters: uint64 from the start ---

[[nodiscard]] inline Result<FrameCount, QuantityError> makeFrameCount(
    std::uint64_t v) {
  return Result<FrameCount, QuantityError>::success(FrameCount{v});
}

// Negative and fractional inputs are rejected BEFORE unsigned conversion.
[[nodiscard]] inline Result<FrameCount, QuantityError> makeFrameCountFromReal(
    double v) {
  if (!std::isfinite(v) || v < 0.0 || v != std::floor(v))
    return Result<FrameCount, QuantityError>::failure(
        QuantityError::InvalidArgument);
  if (v >= 18446744073709551616.0)  // 2^64, exactly representable
    return Result<FrameCount, QuantityError>::failure(QuantityError::RangeError);
  return Result<FrameCount, QuantityError>::success(
      FrameCount{static_cast<std::uint64_t>(v)});
}

[[nodiscard]] inline Result<FrameCount, QuantityError> tryIncrement(
    FrameCount c) {
  if (c.value() == std::numeric_limits<std::uint64_t>::max())
    return Result<FrameCount, QuantityError>::failure(QuantityError::RangeError);
  return Result<FrameCount, QuantityError>::success(FrameCount{c.value() + 1});
}

[[nodiscard]] inline Result<FrameCount, QuantityError> tryDecrement(
    FrameCount c) {
  if (c.value() == 0)
    return Result<FrameCount, QuantityError>::failure(
        QuantityError::InvalidArgument);
  return Result<FrameCount, QuantityError>::success(FrameCount{c.value() - 1});
}

}  // namespace mmathen
