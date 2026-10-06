#pragma once

// Free circular plate, Kirchhoff thin-plate theory.
//
// A cymbal is not a membrane: the restoring force is bending stiffness,
// not a tension. The ideal plate is flat, isotropic, and free at r = a
// (zero radial moment and zero Kirchhoff shear). The mode in nodal
// diameters n is
//   R(r) = J_n(k r) + C I_n(k r),   k = λ / a,
// with λ a positive root of the boundary determinant derived below.
// Rigid-body roots (λ = 0 for n = 0 and n = 1) are not elastic modes
// and are not returned.
//
//   D = E h^3 / (12 (1 - ν^2)),
//   f = (λ^2 / (2 π a^2)) sqrt(D / (ρ h))
//     = (λ^2 h / (2 π a^2)) sqrt(E / (12 ρ (1 - ν^2))).
//
// E, ρ, h, and ν are arguments. No alloy table is read. Eigenvalues are
// computed; they are not a copied frequency list. The search stays below
// λ = 40, where the modified Bessel values still fit in a double.

#include <cmath>
#include <limits>

#include "../core/result.h"
#include "../math/constants.h"
#include "../units/physical_units.h"

namespace mmathen {

struct FreePlateMode {
  double lambda = 0.0;
  double coefficient = 0.0;
  int nodalDiameters = 0;
  int radialIndex = 0;
};

namespace plate_detail {

struct BesselPair {
  double j = 0.0;
  double jp = 0.0;
  double i = 0.0;
  double ip = 0.0;
};

[[nodiscard]] inline double besselJPrime(int n, double x) {
  if (n == 0) return -std::cyl_bessel_j(1.0, x);
  return std::cyl_bessel_j(static_cast<double>(n - 1), x) -
         static_cast<double>(n) / x * std::cyl_bessel_j(static_cast<double>(n), x);
}

[[nodiscard]] inline double besselIPrime(int n, double x) {
  if (n == 0) return std::cyl_bessel_i(1.0, x);
  return std::cyl_bessel_i(static_cast<double>(n - 1), x) -
         static_cast<double>(n) / x * std::cyl_bessel_i(static_cast<double>(n), x);
}

[[nodiscard]] inline BesselPair pairAt(int n, double lambda) {
  BesselPair values;
  values.j = std::cyl_bessel_j(static_cast<double>(n), lambda);
  values.jp = besselJPrime(n, lambda);
  values.i = std::cyl_bessel_i(static_cast<double>(n), lambda);
  values.ip = besselIPrime(n, lambda);
  return values;
}

// Moment row M and shear row S, so the nontrivial kernel requires
// det = M_J * S_I + S_J * M_I = 0, and C = B/A = M_J / M_I.
struct PlateRows {
  double momentJ = 0.0;
  double momentI = 0.0;
  double shearJ = 0.0;
  double shearI = 0.0;
};

[[nodiscard]] inline PlateRows boundaryRows(int n, double lambda, double poisson,
                                            const BesselPair& v) {
  const double oneMinus = 1.0 - poisson;
  const double n2 = static_cast<double>(n) * static_cast<double>(n);
  PlateRows rows;
  rows.momentJ = lambda * lambda * v.j + oneMinus * (lambda * v.jp - n2 * v.j);
  rows.momentI = lambda * lambda * v.i - oneMinus * (lambda * v.ip - n2 * v.i);
  rows.shearJ = -(lambda * lambda * lambda + oneMinus * n2 * lambda) * v.jp +
                oneMinus * n2 * v.j;
  rows.shearI = (lambda * lambda * lambda - oneMinus * n2 * lambda) * v.ip +
                oneMinus * n2 * v.i;
  return rows;
}

[[nodiscard]] inline bool poissonUsable(double poisson) {
  return std::isfinite(poisson) && poisson > -1.0 && poisson < 0.5;
}

}  // namespace plate_detail

// Boundary determinant at one λ. A root is a sign change of this value.
// InvalidArgument: n < 0, λ not strictly positive, or ν outside (-1, 1/2).
[[nodiscard]] inline Result<double, QuantityError> freePlateCharacteristic(
    int nodalDiameters, double lambda, double poisson) {
  using R = Result<double, QuantityError>;
  if (nodalDiameters < 0 || !std::isfinite(lambda) || !(lambda > 0.0) ||
      !plate_detail::poissonUsable(poisson))
    return R::failure(QuantityError::InvalidArgument);
  const auto values = plate_detail::pairAt(nodalDiameters, lambda);
  if (!std::isfinite(values.j) || !std::isfinite(values.jp) ||
      !std::isfinite(values.i) || !std::isfinite(values.ip))
    return R::failure(QuantityError::RangeError);
  const auto rows =
      plate_detail::boundaryRows(nodalDiameters, lambda, poisson, values);
  const double det = rows.momentJ * rows.shearI + rows.shearJ * rows.momentI;
  if (!std::isfinite(det)) return R::failure(QuantityError::RangeError);
  return R::success(det);
}

// s-th elastic free-edge eigenvalue, s >= 1, in increasing λ. The scan
// starts at 0.5 so the rigid-body root at the origin is not returned.
// RangeError if that root is not found below λ = 40.
[[nodiscard]] inline Result<FreePlateMode, QuantityError> freeCircularPlateMode(
    int nodalDiameters, int radialIndex, double poisson) {
  using R = Result<FreePlateMode, QuantityError>;
  if (nodalDiameters < 0 || radialIndex < 1 || !plate_detail::poissonUsable(poisson))
    return R::failure(QuantityError::InvalidArgument);
  constexpr double step = 0.05;
  constexpr double limit = 40.0;
  double x = 0.5;
  auto previous = freePlateCharacteristic(nodalDiameters, x, poisson);
  if (!previous.ok()) return R::failure(previous.error());
  int seen = 0;
  while (x + step <= limit) {
    const double nextX = x + step;
    auto next = freePlateCharacteristic(nodalDiameters, nextX, poisson);
    if (!next.ok()) return R::failure(next.error());
    if (previous.value() == 0.0 || previous.value() * next.value() < 0.0) {
      double lo = x;
      double hi = nextX;
      double flo = previous.value();
      if (previous.value() != 0.0) {
        for (int iteration = 0; iteration < 80; ++iteration) {
          const double mid = 0.5 * (lo + hi);
          auto fmid = freePlateCharacteristic(nodalDiameters, mid, poisson);
          if (!fmid.ok()) return R::failure(fmid.error());
          if (flo * fmid.value() <= 0.0) {
            hi = mid;
          } else {
            lo = mid;
            flo = fmid.value();
          }
        }
      } else {
        hi = x;
        lo = x;
      }
      ++seen;
      if (seen == radialIndex) {
        const double lambda = 0.5 * (lo + hi);
        const auto values = plate_detail::pairAt(nodalDiameters, lambda);
        const auto rows = plate_detail::boundaryRows(nodalDiameters, lambda,
                                                     poisson, values);
        if (!(std::fabs(rows.momentI) > 0.0) || !std::isfinite(rows.momentJ) ||
            !std::isfinite(rows.momentI))
          return R::failure(QuantityError::RangeError);
        const double coefficient = rows.momentJ / rows.momentI;
        if (!std::isfinite(coefficient))
          return R::failure(QuantityError::RangeError);
        return R::success(
            FreePlateMode{lambda, coefficient, nodalDiameters, radialIndex});
      }
    }
    x = nextX;
    previous = next;
  }
  return R::failure(QuantityError::RangeError);
}

// f = (λ^2 h / (2 π a^2)) sqrt(E / (12 ρ (1-ν^2))). Thickness, Young's
// modulus, density, and Poisson's ratio are arguments.
[[nodiscard]] inline Result<FrequencyHz, QuantityError> freeCircularPlateFrequency(
    DistanceMeters radius, double thicknessMeters, double youngsModulusPa,
    double densityKgPerM3, double poisson, int nodalDiameters, int radialIndex) {
  using R = Result<FrequencyHz, QuantityError>;
  const double a = radius.value();
  if (!std::isfinite(a) || !(a > 0.0) || !std::isfinite(thicknessMeters) ||
      !(thicknessMeters > 0.0) || !std::isfinite(youngsModulusPa) ||
      !(youngsModulusPa > 0.0) || !std::isfinite(densityKgPerM3) ||
      !(densityKgPerM3 > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  auto mode = freeCircularPlateMode(nodalDiameters, radialIndex, poisson);
  if (!mode.ok()) return R::failure(mode.error());
  const double stiffnessFactor =
      youngsModulusPa / (12.0 * densityKgPerM3 * (1.0 - poisson * poisson));
  if (!(stiffnessFactor > 0.0) || !std::isfinite(stiffnessFactor))
    return R::failure(QuantityError::RangeError);
  const double lambda = mode.value().lambda;
  const double frequency = (lambda * lambda * thicknessMeters) /
                           (constants::twoPi * a * a) * std::sqrt(stiffnessFactor);
  if (!std::isfinite(frequency) || !(frequency > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(FrequencyHz{frequency});
}

// R(r) = J_n(λ r/a) + C I_n(λ r/a). r outside [0, a] is InvalidArgument.
[[nodiscard]] inline Result<double, QuantityError> freePlateRadialShape(
    const FreePlateMode& mode, double radiusMeters, double positionMeters) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(radiusMeters) || !(radiusMeters > 0.0) ||
      !std::isfinite(positionMeters) || positionMeters < 0.0 ||
      positionMeters > radiusMeters || !std::isfinite(mode.lambda) ||
      !(mode.lambda > 0.0) || !std::isfinite(mode.coefficient))
    return R::failure(QuantityError::InvalidArgument);
  const double argument = mode.lambda * positionMeters / radiusMeters;
  const double order = static_cast<double>(mode.nodalDiameters);
  const double shape = std::cyl_bessel_j(order, argument) +
                       mode.coefficient * std::cyl_bessel_i(order, argument);
  if (!std::isfinite(shape)) return R::failure(QuantityError::RangeError);
  return R::success(shape);
}

// ∫ φ^2 dA for φ = R(r) cos(n (θ-θ0)), over the disk. Simpson's rule
// on the radial integral. The angular factor is 2π for n = 0 and π
// otherwise.
[[nodiscard]] inline Result<double, QuantityError> freePlateModeNorm(
    const FreePlateMode& mode, double radiusMeters) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(radiusMeters) || !(radiusMeters > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  constexpr int panels = 512;
  const double h = radiusMeters / static_cast<double>(panels);
  auto integrand = [&](double r) {
    auto shape = freePlateRadialShape(mode, radiusMeters, r);
    if (!shape.ok()) return std::numeric_limits<double>::quiet_NaN();
    return shape.value() * shape.value() * r;
  };
  double sum = integrand(0.0) + integrand(radiusMeters);
  if (!std::isfinite(sum)) return R::failure(QuantityError::RangeError);
  for (int i = 1; i < panels; ++i) {
    const double weight = (i % 2 == 0) ? 2.0 : 4.0;
    const double sample = integrand(h * static_cast<double>(i));
    if (!std::isfinite(sample)) return R::failure(QuantityError::RangeError);
    sum += weight * sample;
  }
  const double radial = sum * h / 3.0;
  const double angular = mode.nodalDiameters == 0 ? constants::twoPi : constants::pi;
  const double norm = angular * radial;
  if (!std::isfinite(norm) || !(norm > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(norm);
}

}  // namespace mmathen
