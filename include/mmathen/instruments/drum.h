#pragma once

// Drum kit shells and the ideal circular membrane on each head (docs/48).
//
// The shells are the Yamaha Tour Custom series as printed on
// https://usa.yamaha.com/products/musical_instruments/drums/ac_drums/drum_sets/tour_custom2/specs.html
// TMP2F4 is the 22"×16" bass, 10"×7" and 12"×8" mounted toms, and
// 16"×15" floor tom. The same page lists snares TMS-1455 (14"×5.5") and
// TMS-1465 (14"×6.5") as components; they are not inside the TMP2F4
// column. Shell thickness 5.6 mm and the 6-ply maple description are
// that page's shell row. Bearing edge 45° is printed for TT/FT/BD only,
// so the snares do not store one. Head names (Remo Ambassador, Powerstroke)
// are printed; areal density and tension are not, so tuneMembraneHead
// takes both ranges as arguments.
//
// A head is a fixed circular membrane, not a string and not a rectangular
// box. Frequency is circularMembraneMode. Shell depth and shell thickness
// are geometry of the cylinder; they are not the membrane radius and they
// do not enter f. Snare wires, the air cavity, and head-to-head coupling
// are not evaluated: the page does not give wire mass or a port radius.
//
// A strike is an ideal impulse of momentum P at one point, expanded on
// the Bessel modes. The fixed rim is not a strike point. Felt contact
// reuses feltContactRate (force and mallet acceleration). It does not
// apply the string law F/(2Z).

#include <cmath>
#include <string>

#include "../math/constants.h"
#include "../physics/modes.h"
#include "hammer.h"

namespace mmathen {

// International inch, exactly 0.0254 m. The catalog prints inches.
inline constexpr double kCatalogInchMeters = 0.0254;

inline constexpr const char* kTourCustomSource =
    "Yamaha Tour Custom drum kit specs, "
    "https://usa.yamaha.com/products/musical_instruments/drums/ac_drums/"
    "drum_sets/tour_custom2/specs.html . "
    "TMP2F4: bass 22 x 16 in (TMB-2216), rack toms 10 x 7 in (TMT-1007) "
    "and 12 x 8 in (TMT-1208), floor tom 16 x 15 in (TMF-1615). "
    "Snares on the same page, not in the TMP2F4 column: 14 x 5.5 in "
    "(TMS-1455) and 14 x 6.5 in (TMS-1465). Shells 5.6 mm, maple 6-ply. "
    "Bearing edge 45 degrees for TT/FT/BD. Heads are named Remo; the page "
    "prints no areal density and no membrane tension. Radii are half the "
    "printed diameter times the international inch 0.0254 m.";

struct MembraneShell {
  const char* name;
  const char* model;
  double diameterInches;
  double depthInches;
  DistanceMeters headRadius;
  DistanceMeters shellDepth;
  double shellThicknessMeters;
  bool bearingEdgePrinted;
  double bearingEdgeDegrees;
  const char* source;
};

struct MembraneHead {
  const char* name;
  DistanceMeters radius;
  ArealDensityKgPerSquareMeter arealDensity;
  SurfaceTensionNewtonsPerMeter tension;
  TabulatedRange publishedArealDensityKgPerM2;
  TabulatedRange publishedTensionNPerM;
  double requestedFrequencyHz;
  const char* source;
};

struct TourCustomKit {
  const char* name;
  MembraneShell bassDrum;
  MembraneShell rackTom10;
  MembraneShell rackTom12;
  MembraneShell floorTom;
  MembraneShell snare55;
  MembraneShell snare65;
  const char* source;
};

namespace drum_detail {

[[nodiscard]] inline MembraneShell shell(const char* name, const char* model,
                                         double diameterInches, double depthInches,
                                         bool bearingEdge) {
  const double radius = 0.5 * diameterInches * kCatalogInchMeters;
  const double depth = depthInches * kCatalogInchMeters;
  return MembraneShell{name,
                       model,
                       diameterInches,
                       depthInches,
                       DistanceMeters{radius},
                       DistanceMeters{depth},
                       0.0056,
                       bearingEdge,
                       bearingEdge ? 45.0 : 0.0,
                       kTourCustomSource};
}

[[nodiscard]] inline bool shellSane(const MembraneShell& drum) {
  if (!detail::text(drum.name) || !detail::text(drum.model) ||
      !detail::text(drum.source))
    return false;
  if (!(drum.diameterInches > 0.0) || !(drum.depthInches > 0.0)) return false;
  const double radius = 0.5 * drum.diameterInches * kCatalogInchMeters;
  const double depth = drum.depthInches * kCatalogInchMeters;
  if (drum.headRadius.value() != radius || drum.shellDepth.value() != depth)
    return false;
  if (!(drum.shellThicknessMeters > 0.0)) return false;
  if (drum.shellThicknessMeters >= radius) return false;
  if (drum.bearingEdgePrinted) {
    if (drum.bearingEdgeDegrees != 45.0) return false;
  } else if (drum.bearingEdgeDegrees != 0.0) {
    return false;
  }
  return true;
}

}  // namespace drum_detail

[[nodiscard]] inline Result<TourCustomKit, QuantityError> yamahaTourCustomKit() {
  using R = Result<TourCustomKit, QuantityError>;
  TourCustomKit kit{
      "Yamaha Tour Custom",
      drum_detail::shell("bass drum", "TMB-2216", 22.0, 16.0, true),
      drum_detail::shell("rack tom", "TMT-1007", 10.0, 7.0, true),
      drum_detail::shell("rack tom", "TMT-1208", 12.0, 8.0, true),
      drum_detail::shell("floor tom", "TMF-1615", 16.0, 15.0, true),
      drum_detail::shell("snare", "TMS-1455", 14.0, 5.5, false),
      drum_detail::shell("snare", "TMS-1465", 14.0, 6.5, false),
      kTourCustomSource};
  const MembraneShell* drums[] = {&kit.bassDrum, &kit.rackTom10, &kit.rackTom12,
                                  &kit.floorTom, &kit.snare55, &kit.snare65};
  for (const MembraneShell* drum : drums) {
    if (!drum_detail::shellSane(*drum))
      return R::failure(QuantityError::InvalidArgument);
  }
  if (!(kit.bassDrum.headRadius.value() > kit.floorTom.headRadius.value()) ||
      !(kit.floorTom.headRadius.value() > kit.rackTom12.headRadius.value()) ||
      !(kit.rackTom12.headRadius.value() > kit.rackTom10.headRadius.value()))
    return R::failure(QuantityError::RangeError);
  if (kit.snare55.headRadius.value() != kit.snare65.headRadius.value())
    return R::failure(QuantityError::RangeError);
  if (!(kit.snare65.shellDepth.value() > kit.snare55.shellDepth.value()))
    return R::failure(QuantityError::RangeError);
  if (kit.snare55.bearingEdgePrinted || kit.snare65.bearingEdgePrinted)
    return R::failure(QuantityError::InvalidArgument);
  return R::success(kit);
}

// Nominal areal density is the midpoint of the caller's kg/m² range.
// Tension is the ideal-membrane tension that puts the (0,1) mode on
// `frequencyHz`: T = σ (2 π a f / α_{01})^2. RangeError if that tension
// leaves the caller's interval. The catalog prints no pitch, so
// frequencyHz is the caller's request, not a second published frequency.
[[nodiscard]] inline Result<MembraneHead, QuantityError> tuneMembraneHead(
    const MembraneShell& shell, double frequencyHz, double densityMinKgPerM2,
    double densityMaxKgPerM2, double tensionMinNPerM, double tensionMaxNPerM) {
  using R = Result<MembraneHead, QuantityError>;
  if (!drum_detail::shellSane(shell))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(frequencyHz) || !(frequencyHz > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (!detail::usableRange(densityMinKgPerM2, densityMaxKgPerM2) ||
      !detail::usableRange(tensionMinNPerM, tensionMaxNPerM))
    return R::failure(QuantityError::InvalidArgument);
  const double density = 0.5 * (densityMinKgPerM2 + densityMaxKgPerM2);
  auto alpha = besselJZero(0, 1);
  if (!alpha.ok()) return R::failure(alpha.error());
  const double wave =
      constants::twoPi * shell.headRadius.value() * frequencyHz / alpha.value();
  const double tension = density * wave * wave;
  if (!std::isfinite(tension) || tension < tensionMinNPerM ||
      tension > tensionMaxNPerM)
    return R::failure(QuantityError::RangeError);
  const SurfaceTensionNewtonsPerMeter tensionQ{tension};
  const ArealDensityKgPerSquareMeter densityQ{density};
  auto fundamental =
      circularMembraneMode(shell.headRadius, tensionQ, densityQ, 0, 1);
  if (!fundamental.ok()) return R::failure(fundamental.error());
  if (!(std::fabs(fundamental.value().value() - frequencyHz) <=
        1.0e-9 * frequencyHz))
    return R::failure(QuantityError::RangeError);
  return R::success(MembraneHead{
      shell.name, shell.headRadius, densityQ, tensionQ,
      TabulatedRange{densityMinKgPerM2, densityMaxKgPerM2},
      TabulatedRange{tensionMinNPerM, tensionMaxNPerM}, frequencyHz,
      kTourCustomSource});
}

// Point impulse. radialFraction is 0 at the center and increases toward
// the rim. The rim itself is the fixed boundary, so the fraction is in
// [0, 1). Momentum is kg·m/s and must be strictly positive.
struct MembraneImpulse {
  double radialFraction;
  double angleRadians;
  double momentumKgMetersPerSecond;
};

struct MembraneModalCoefficient {
  int nodalDiameters = 0;
  int radialIndex = 0;
  double besselZero = 0.0;
  double angularFrequency = 0.0;
  double velocityCoefficient = 0.0;
};

namespace drum_detail {

[[nodiscard]] inline double membraneNorm(int nodalDiameters, double radius,
                                         double alpha) {
  const double radial =
      std::cyl_bessel_j(static_cast<double>(nodalDiameters + 1), alpha);
  const double disk = 0.5 * radius * radius * radial * radial;
  const double angular =
      nodalDiameters == 0 ? constants::twoPi : constants::pi;
  return angular * disk;
}

[[nodiscard]] inline bool impulseSane(const MembraneImpulse& impulse) {
  return std::isfinite(impulse.radialFraction) && impulse.radialFraction >= 0.0 &&
         impulse.radialFraction < 1.0 && std::isfinite(impulse.angleRadians) &&
         std::isfinite(impulse.momentumKgMetersPerSecond) &&
         impulse.momentumKgMetersPerSecond > 0.0;
}

}  // namespace drum_detail

// Velocity coefficient b of one mode for an impulse at (r, θ).
// φ = J_m(α r/a) cos(m (θ-θ_s)), N = ∫ φ^2 dA, b = (P/σ) φ_s / N.
// A center strike has J_m(0) = 0 for m > 0, so those coefficients are 0.
[[nodiscard]] inline Result<MembraneModalCoefficient, QuantityError>
membraneStrikeCoefficient(const MembraneHead& head, const MembraneImpulse& impulse,
                          int nodalDiameters, int radialIndex) {
  using R = Result<MembraneModalCoefficient, QuantityError>;
  if (nodalDiameters < 0 || radialIndex < 1 || !drum_detail::impulseSane(impulse))
    return R::failure(QuantityError::InvalidArgument);
  const double radius = head.radius.value();
  const double density = head.arealDensity.value();
  const double tension = head.tension.value();
  if (!std::isfinite(radius) || !(radius > 0.0) || !std::isfinite(density) ||
      !(density > 0.0) || !std::isfinite(tension) || !(tension > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  auto alpha = besselJZero(nodalDiameters, radialIndex);
  if (!alpha.ok()) return R::failure(alpha.error());
  auto speed = membraneWaveSpeed(head.tension, head.arealDensity);
  if (!speed.ok()) return R::failure(speed.error());
  const double norm = drum_detail::membraneNorm(nodalDiameters, radius, alpha.value());
  if (!(norm > 0.0) || !std::isfinite(norm))
    return R::failure(QuantityError::RangeError);
  const double strikeR = impulse.radialFraction * radius;
  const double shape = std::cyl_bessel_j(static_cast<double>(nodalDiameters),
                                         alpha.value() * strikeR / radius);
  const double velocity =
      impulse.momentumKgMetersPerSecond / density * shape / norm;
  const double omega = alpha.value() * speed.value().value() / radius;
  if (!std::isfinite(shape) || !std::isfinite(velocity) || !std::isfinite(omega) ||
      !(omega > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(MembraneModalCoefficient{nodalDiameters, radialIndex,
                                             alpha.value(), omega, velocity});
}

namespace drum_detail {

[[nodiscard]] inline Result<double, QuantityError> strikeSum(
    const MembraneHead& head, const MembraneImpulse& impulse,
    double observeRadialFraction, double observeAngle, double timeSeconds,
    int maxNodalDiameters, int maxRadial, bool velocity) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(observeRadialFraction) || observeRadialFraction < 0.0 ||
      observeRadialFraction > 1.0 || !std::isfinite(observeAngle) ||
      !std::isfinite(timeSeconds) || timeSeconds < 0.0 || maxNodalDiameters < 0 ||
      maxRadial < 1)
    return R::failure(QuantityError::InvalidArgument);
  double sum = 0.0;
  for (int m = 0; m <= maxNodalDiameters; ++m) {
    for (int n = 1; n <= maxRadial; ++n) {
      auto coefficient = membraneStrikeCoefficient(head, impulse, m, n);
      if (!coefficient.ok()) return R::failure(coefficient.error());
      const double argument =
          coefficient.value().besselZero * observeRadialFraction;
      const double radial =
          std::cyl_bessel_j(static_cast<double>(m), argument);
      const double angle =
          std::cos(static_cast<double>(m) * (observeAngle - impulse.angleRadians));
      const double shape = radial * angle;
      const double omega = coefficient.value().angularFrequency;
      const double time = velocity ? std::cos(omega * timeSeconds)
                                  : std::sin(omega * timeSeconds) / omega;
      const double term = coefficient.value().velocityCoefficient * time * shape;
      if (!std::isfinite(term)) return R::failure(QuantityError::RangeError);
      sum += term;
    }
  }
  if (!std::isfinite(sum)) return R::failure(QuantityError::RangeError);
  return R::success(sum);
}

}  // namespace drum_detail

// Modal displacement (m) and velocity (m/s) after the impulse. Initial
// displacement is zero. maxNodalDiameters and maxRadial bound the sum.
// The rim (observe fraction 1) is included and sits at 0 because every
// J_m(α) is 0.
[[nodiscard]] inline Result<double, QuantityError> membraneStrikeDisplacement(
    const MembraneHead& head, const MembraneImpulse& impulse,
    double observeRadialFraction, double observeAngle, double timeSeconds,
    int maxNodalDiameters, int maxRadial) {
  return drum_detail::strikeSum(head, impulse, observeRadialFraction, observeAngle,
                                timeSeconds, maxNodalDiameters, maxRadial, false);
}

[[nodiscard]] inline Result<double, QuantityError> membraneStrikeVelocity(
    const MembraneHead& head, const MembraneImpulse& impulse,
    double observeRadialFraction, double observeAngle, double timeSeconds,
    int maxNodalDiameters, int maxRadial) {
  return drum_detail::strikeSum(head, impulse, observeRadialFraction, observeAngle,
                                timeSeconds, maxNodalDiameters, maxRadial, true);
}

[[nodiscard]] inline std::string formula(const TourCustomKit& kit) {
  std::string text = kit.name == nullptr ? std::string{} : std::string{kit.name};
  const MembraneShell* drums[] = {&kit.bassDrum, &kit.rackTom10, &kit.rackTom12,
                                  &kit.floorTom, &kit.snare55, &kit.snare65};
  for (const MembraneShell* drum : drums) {
    text.push_back(' ');
    if (drum->model != nullptr) text += drum->model;
  }
  return text;
}

}  // namespace mmathen
