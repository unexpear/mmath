#pragma once

// Wound-string bending stiffness on one assembled course (docs/48).
//
// Chabassier, Chaigne & Joly, JASA 134(1), 648–665 (2013), Table III,
// the same convention as pianoString:
//   the core alone supplies I = π d^4 / 64,
//   the winding adds mass and does not add bending stiffness,
//   μ = ρ F π d^2 / 4,
//   κ = (d/4) √(E / (ρ F)) = √(E I / μ).
//
// The course already carries the total linear density, the length, and
// the tension. Core diameter, Young's modulus, and core density are
// arguments. F = μ / (ρ A_core) is computed. F below 1 by more than
// rounding is DomainError: the named core would weigh more than the
// whole string, which is what an outer diameter passed as a core
// produces. F = 1 is a plain core and is accepted; a one-ulp miss of
// that product is still a plain core.
//
// No harp, violin, viola, cello, or bass core diameter is read. The
// Atlantide table's printed diameter is an outer diameter. Young's 1952
// specific modulus is plain wire and is not substituted. Chabassier's
// E and ρ are not substituted either.
//
// The result is the course's ideal string with hinged stiffness κ.
// Modes stay f_n = n f0 √(1 + B n^2), f0 = c/(2L),
// B = κ^2 π^2 / (c^2 L^2).

#include <cmath>
#include <limits>

#include "instrument.h"

namespace mmathen {

inline constexpr const char* kWoundStringSource =
    "Wound-string bending stiffness, Chabassier, Chaigne & Joly 2013, "
    "JASA 134(1) Table III: I = pi d^4/64 from the core diameter, mass "
    "from the course linear density, kappa = (d/4) sqrt(E/(rho F)), "
    "F = mu/(rho A_core). Core diameter, Young's modulus, and core "
    "density are caller arguments. No instrument core or modulus is "
    "stored. Young 1952 is plain wire and is not used.";

struct WoundString {
  WaveSystem1D system;
  double coreDiameterMeters = 0.0;
  double youngsModulusPa = 0.0;
  double coreDensityKgPerM3 = 0.0;
  double windingFactor = 0.0;
  double stiffness = 0.0;
  double inharmonicity = 0.0;
  const char* source = nullptr;
};

// Hinged stiff string for `course`. Core diameter (m), Young's modulus
// (Pa), and core density (kg/m^3) are the caller's.
// InvalidArgument: a non-positive or non-finite diameter, modulus,
// density, length, tension, or linear density.
// DomainError: winding factor below 1 by more than rounding. A plain
// core, F = 1, is accepted; the product ρ A can miss 1 by an ulp.
// RangeError: the factor or κ overflows.
[[nodiscard]] inline Result<WoundString, QuantityError> makeWoundString(
    const StringCourse& course, double coreDiameterMeters,
    double youngsModulusPa, double coreDensityKgPerM3) {
  using R = Result<WoundString, QuantityError>;
  const double length = course.geometry.vibratingLength.value();
  const double tension = course.tension.value();
  const double mu = course.material.linearDensity.value();
  if (!std::isfinite(coreDiameterMeters) || !(coreDiameterMeters > 0.0) ||
      !std::isfinite(youngsModulusPa) || !(youngsModulusPa > 0.0) ||
      !std::isfinite(coreDensityKgPerM3) || !(coreDensityKgPerM3 > 0.0) ||
      !std::isfinite(length) || !(length > 0.0) || !std::isfinite(tension) ||
      !(tension > 0.0) || !std::isfinite(mu) || !(mu > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double area = constants::pi * coreDiameterMeters * coreDiameterMeters /
                      4.0;
  const double coreMass = coreDensityKgPerM3 * area;
  if (!std::isfinite(coreMass) || !(coreMass > 0.0))
    return R::failure(QuantityError::RangeError);
  const double factor = mu / coreMass;
  if (!std::isfinite(factor)) return R::failure(QuantityError::RangeError);
  // 16 ulps: ρ·F·A and ρ·A are the same core when F is 1, and are not
  // when an outer diameter was passed as the core.
  const double plainCore =
      1.0 - 16.0 * std::numeric_limits<double>::epsilon();
  if (factor < plainCore) return R::failure(QuantityError::DomainError);
  const double kappa =
      0.25 * coreDiameterMeters *
      std::sqrt(youngsModulusPa / (coreDensityKgPerM3 * factor));
  if (!std::isfinite(kappa)) return R::failure(QuantityError::RangeError);
  auto ideal = courseString(course);
  if (!ideal.ok()) return R::failure(ideal.error());
  auto stiff = withStiffness(ideal.value(), kappa);
  if (!stiff.ok()) return R::failure(stiff.error());
  const double b = inharmonicityB(stiff.value());
  if (!std::isfinite(b)) return R::failure(QuantityError::RangeError);
  return R::success(WoundString{stiff.value(), coreDiameterMeters,
                                youngsModulusPa, coreDensityKgPerM3, factor,
                                kappa, b, kWoundStringSource});
}

}  // namespace mmathen
