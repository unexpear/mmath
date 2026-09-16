#pragma once

// D3 — equal-temperament error metrics: what an N-EDO actually buys you.
//
// For a just ratio r and N divisions, the nearest degree and its SIGNED
// error (nearest − just, in cents) are computed, never asserted. Reports
// aggregate worst/mean absolute error over a target set, so rankings like
// 12 < 19 < 31 on the major triad come out of arithmetic the tests pin,
// not prose. Full regular-temperament theory (mappings, generators as
// matrices) is explicitly deferred; this module measures, it does not map.

#include <cmath>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../units/music_units.h"

namespace mmathen {

struct DegreeError {
  int degree = 0;        // nearest degree in [0, N]
  double errorCents = 0.0;  // signed: nearest-degree cents minus just cents
};

// Nearest N-EDO degree to a just ratio with its signed cent error.
// Divisions >= 1; ratio finite and > 0.
[[nodiscard]] inline Result<DegreeError, QuantityError> nearestDegreeError(
    int divisions, FrequencyRatio justRatio) {
  using R = Result<DegreeError, QuantityError>;
  if (divisions < 1)
    return R::failure(QuantityError::InvalidArgument);
  const double rv = justRatio.value();
  if (!std::isfinite(rv) || !(rv > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double step = 1200.0 / static_cast<double>(divisions);
  const double justCents = 1200.0 * std::log2(rv);
  const double layers = justCents / step;
  if (!std::isfinite(layers))
    return R::failure(QuantityError::RangeError);
  const long long degree = std::llround(layers);
  // Degree fits int for all musical inputs; extreme ratio/huge-N corners
  // are RangeError, never narrowed silently (cents of finite doubles is
  // bounded, so this triggers only past ~2e9 steps with extreme ratios).
  if (degree < -2147483648LL || degree > 2147483647LL)
    return R::failure(QuantityError::RangeError);
  const double err = static_cast<double>(degree) * step - justCents;
  if (!std::isfinite(err)) return R::failure(QuantityError::RangeError);
  return R::success(DegreeError{static_cast<int>(degree), err});
}

struct TemperamentReport {
  double worstAbsCents = 0.0;
  double meanAbsCents = 0.0;
  std::size_t targets = 0;
};

[[nodiscard]] inline Result<TemperamentReport, QuantityError> errorReport(
    int divisions, const std::vector<FrequencyRatio>& targets) {
  using R = Result<TemperamentReport, QuantityError>;
  if (targets.empty())
    return R::failure(QuantityError::InvalidArgument);
  TemperamentReport out;
  out.targets = targets.size();
  for (const auto& t : targets) {
    auto e = nearestDegreeError(divisions, t);
    if (!e.ok()) return R::failure(e.error());
    const double a = e.value().errorCents < 0.0 ? -e.value().errorCents
                                                : e.value().errorCents;
    if (a > out.worstAbsCents) out.worstAbsCents = a;
    out.meanAbsCents += a;
  }
  out.meanAbsCents /= static_cast<double>(targets.size());
  return R::success(out);
}

}  // namespace mmathen
