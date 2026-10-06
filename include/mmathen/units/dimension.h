#pragma once

// Phase 0 — coarse physical dimensionality.
//
// Semantic identity lives in the Tag parameter of Quantity (see quantity.h),
// not here. Two quantities can share a Dimension (e.g. Frequency and
// SampleRate are both inverse-time) while remaining distinct types.

namespace mmathen {

struct DimInverseTime {};
struct DimInverseTimeSquared {};
struct DimTime {};
struct DimDimensionless {};
struct DimCount {};
struct DimPressure {};
struct DimLength {};
struct DimSpeed {};
struct DimTemperature {};
struct DimForce {};
struct DimLinearDensity {};
struct DimImpedance {};
struct DimDensity {};
struct DimSurfaceTension {};
struct DimArealDensity {};
struct DimAttenuation {};

}  // namespace mmathen
