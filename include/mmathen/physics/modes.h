#pragma once

// D2 — resonant modes from boundary conditions (derived, not quoted).
//
// Fixed-fixed string: separation of variables on the 1-D wave equation with
// Dirichlet ends gives k_n*L = n*pi, so lambda_n = 2L/n and (with v = f*λ)
// f_n = n*v/2L with v = sqrt(T/mu) (Feynman Lectures I Ch. 49; UMass/BU/UNCW
// lab derivations; FHWA cable report). Open-open pipes share the series;
// stopped (closed-open) pipes impose a pressure antinode at the closed end,
// keeping odd quarter wavelengths only: f_n = n*c/4L for odd n (UNSW,
// Rochester, LibreTexts, UCF, UCSB). End corrections (~0.61r per open end
// per LibreTexts) are deferred with citation, not fudged into L.
//
// Ideal circular membrane, fixed at r = a (Rayleigh, The Theory of Sound,
// vol. 1): c = sqrt(T/σ) with T the tension per unit length and σ the
// areal density, and f_{m n} = α_{m n} c / (2 π a). α_{m n} is the n-th
// positive zero of the Bessel function J_m, computed here, not copied
// from a drum table. Air loading is not in this frequency.

#include <cmath>

#include "../core/result.h"
#include "../math/constants.h"
#include "../units/physical_units.h"

namespace mmathen {

// Wave speed on a string: v = sqrt(T/mu). Tension and linear density must
// be finite and > 0.
[[nodiscard]] inline Result<SpeedMetersPerSecond, QuantityError> stringWaveSpeed(
    TensionNewtons tension, LinearDensityKgPerM density) {
  using R = Result<SpeedMetersPerSecond, QuantityError>;
  const double t = tension.value();
  const double m = density.value();
  if (!std::isfinite(t) || !std::isfinite(m))
    return R::failure(QuantityError::InvalidArgument);
  if (!(t > 0.0) || !(m > 0.0))
    return R::failure(QuantityError::DomainError);
  const double v = std::sqrt(t / m);
  if (!std::isfinite(v)) return R::failure(QuantityError::RangeError);
  return R::success(SpeedMetersPerSecond{v});
}

// Fixed-fixed string mode n (1-based): f_n = n*v/2L.
[[nodiscard]] inline Result<FrequencyHz, QuantityError> stringMode(
    DistanceMeters length, TensionNewtons tension, LinearDensityKgPerM density,
    int n) {
  using R = Result<FrequencyHz, QuantityError>;
  if (n < 1) return R::failure(QuantityError::InvalidArgument);
  const double l = length.value();
  if (!std::isfinite(l) || !(l > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  auto v = stringWaveSpeed(tension, density);
  if (!v.ok()) return R::failure(v.error());
  const double f = static_cast<double>(n) * v.value().value() / (2.0 * l);
  if (!std::isfinite(f)) return R::failure(QuantityError::RangeError);
  return R::success(FrequencyHz{f});
}

// Open-open pipe mode n (1-based): f_n = n*c/2L (all integers).
[[nodiscard]] inline Result<FrequencyHz, QuantityError> openPipeMode(
    DistanceMeters length, SpeedMetersPerSecond speed, int n) {
  using R = Result<FrequencyHz, QuantityError>;
  if (n < 1) return R::failure(QuantityError::InvalidArgument);
  const double l = length.value();
  const double c = speed.value();
  if (!std::isfinite(l) || !(l > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(c) || !(c > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double f = static_cast<double>(n) * c / (2.0 * l);
  if (!std::isfinite(f)) return R::failure(QuantityError::RangeError);
  return R::success(FrequencyHz{f});
}

// Stopped (closed-open) pipe mode: f_n = n*c/4L for ODD n only. Even n are
// not resonances of this geometry: DomainError, not a number.
[[nodiscard]] inline Result<FrequencyHz, QuantityError> stoppedPipeMode(
    DistanceMeters length, SpeedMetersPerSecond speed, int n) {
  using R = Result<FrequencyHz, QuantityError>;
  if (n < 1) return R::failure(QuantityError::InvalidArgument);
  if (n % 2 == 0) return R::failure(QuantityError::DomainError);
  const double l = length.value();
  const double c = speed.value();
  if (!std::isfinite(l) || !(l > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(c) || !(c > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double f = static_cast<double>(n) * c / (4.0 * l);
  if (!std::isfinite(f)) return R::failure(QuantityError::RangeError);
  return R::success(FrequencyHz{f});
}

// c = sqrt(T/σ) on an ideal membrane. Both arguments strictly positive.
[[nodiscard]] inline Result<SpeedMetersPerSecond, QuantityError>
membraneWaveSpeed(SurfaceTensionNewtonsPerMeter tension,
                  ArealDensityKgPerSquareMeter density) {
  using R = Result<SpeedMetersPerSecond, QuantityError>;
  const double t = tension.value();
  const double s = density.value();
  if (!std::isfinite(t) || !std::isfinite(s))
    return R::failure(QuantityError::InvalidArgument);
  if (!(t > 0.0) || !(s > 0.0)) return R::failure(QuantityError::DomainError);
  const double v = std::sqrt(t / s);
  if (!std::isfinite(v)) return R::failure(QuantityError::RangeError);
  return R::success(SpeedMetersPerSecond{v});
}

// n-th positive zero of J_m (m = order >= 0, n = index >= 1). Bisection
// on std::cyl_bessel_j. The zeros are about π apart, so a step of 0.05
// cannot skip a sign change. RangeError if no zero is found.
[[nodiscard]] inline Result<double, QuantityError> besselJZero(int order,
                                                              int index) {
  using R = Result<double, QuantityError>;
  if (order < 0 || index < 1) return R::failure(QuantityError::InvalidArgument);
  auto value = [](int m, double x) {
    return std::cyl_bessel_j(static_cast<double>(m), x);
  };
  double x = 1.0e-8;
  double previous = value(order, x);
  int seen = 0;
  constexpr double step = 0.05;
  for (int guard = 0; guard < 20000; ++guard) {
    if (!std::isfinite(previous)) return R::failure(QuantityError::RangeError);
    if (previous == 0.0) {
      ++seen;
      if (seen == index) return R::success(x);
    }
    const double nextX = x + step;
    const double next = value(order, nextX);
    if (!std::isfinite(next)) return R::failure(QuantityError::RangeError);
    if (previous != 0.0 && previous * next < 0.0) {
      double lo = x;
      double hi = nextX;
      double flo = previous;
      for (int iteration = 0; iteration < 80; ++iteration) {
        const double mid = 0.5 * (lo + hi);
        const double fmid = value(order, mid);
        if (!std::isfinite(fmid)) return R::failure(QuantityError::RangeError);
        if (flo * fmid <= 0.0) {
          hi = mid;
        } else {
          lo = mid;
          flo = fmid;
        }
      }
      ++seen;
      if (seen == index) return R::success(0.5 * (lo + hi));
    }
    x = nextX;
    previous = next;
  }
  return R::failure(QuantityError::RangeError);
}

// Fixed circular membrane. nodalDiameters is m >= 0. radialIndex is the
// n-th positive Bessel zero, n >= 1. f = α c / (2 π a).
[[nodiscard]] inline Result<FrequencyHz, QuantityError> circularMembraneMode(
    DistanceMeters radius, SurfaceTensionNewtonsPerMeter tension,
    ArealDensityKgPerSquareMeter density, int nodalDiameters, int radialIndex) {
  using R = Result<FrequencyHz, QuantityError>;
  if (nodalDiameters < 0) return R::failure(QuantityError::InvalidArgument);
  const double radiusM = radius.value();
  if (!std::isfinite(radiusM) || !(radiusM > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  auto alpha = besselJZero(nodalDiameters, radialIndex);
  if (!alpha.ok()) return R::failure(alpha.error());
  auto speed = membraneWaveSpeed(tension, density);
  if (!speed.ok()) return R::failure(speed.error());
  const double frequency =
      alpha.value() * speed.value().value() / (constants::twoPi * radiusM);
  if (!std::isfinite(frequency) || !(frequency > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(FrequencyHz{frequency});
}

}  // namespace mmathen
