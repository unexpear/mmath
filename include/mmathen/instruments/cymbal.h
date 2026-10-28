#pragma once

// Cymbal plates (docs/48). A cymbal is a free circular plate, not a
// drum head and not a string.
//
// The set is the SABIAN XSR Complete Set XSR5006B:
// https://sabian.com/product/xsr5006b-xsr-complete-set/
// "10″ XSR Splash, 14″ XSR Hats, 16/18″ XSR Fast Crash, 18″ XSR Chinese
// and 20″ XSR Ride." The page names the alloy B20 and does not print a
// density, a Young's modulus, a Poisson ratio, a thickness, or a
// frequency. Those stay arguments of the plate frequency. Splash and
// china are in this set, so they are included. A chinese cymbal's flange
// is not given a height here, so the china is the same ideal flat plate
// as any other plate of that radius.
//
// The hi-hat is two plates of the printed 14″ diameter. No clutch gap is
// printed, and no contact stiffness between the plates is invented. A
// strike is an impulse on one plate, expanded on the free-plate modes.
// Felt contact reuses feltContactRate and does not apply F/(2Z).

#include <cmath>
#include <cstring>
#include <string>

#include "../physics/circular_plate.h"
#include "hammer.h"

namespace mmathen {

inline constexpr const char* kXsrCymbalSource =
    "SABIAN XSR Complete Set XSR5006B, "
    "https://sabian.com/product/xsr5006b-xsr-complete-set/ . "
    "10 in splash, 14 in hats, 16 in and 18 in fast crash, 18 in chinese, "
    "20 in ride. Material is named B20. The page does not print thickness, "
    "density, Young's modulus, Poisson's ratio, or a frequency. Radii are "
    "half the printed diameter times the international inch 0.0254 m.";

inline constexpr const char* kXsrAlloyName = "B20";

struct CymbalPlate {
  const char* name;
  double diameterInches;
  DistanceMeters radius;
  const char* source;
};

struct XsrCymbalSet {
  const char* name;
  CymbalPlate splash;
  CymbalPlate hiHatTop;
  CymbalPlate hiHatBottom;
  CymbalPlate crash16;
  CymbalPlate crash18;
  CymbalPlate china;
  CymbalPlate ride;
  const char* alloyName;
  const char* source;
};

// Thickness (m), Young's modulus (Pa), density (kg/m³), and Poisson's
// ratio. None of the four is read off the cymbal.
struct CymbalMaterial {
  double thicknessMeters;
  double youngsModulusPa;
  double densityKgPerM3;
  double poisson;
};

struct CymbalImpulse {
  double radialFraction;
  double angleRadians;
  double momentumKgMetersPerSecond;
};

namespace cymbal_detail {

[[nodiscard]] inline CymbalPlate plate(const char* name, double diameterInches) {
  return CymbalPlate{name, diameterInches,
                     DistanceMeters{0.5 * diameterInches * 0.0254},
                     kXsrCymbalSource};
}

[[nodiscard]] inline bool plateSane(const CymbalPlate& plate) {
  if (!detail::text(plate.name) || !detail::text(plate.source)) return false;
  if (!(plate.diameterInches > 0.0)) return false;
  const double radius = 0.5 * plate.diameterInches * 0.0254;
  return plate.radius.value() == radius;
}

[[nodiscard]] inline bool materialSane(const CymbalMaterial& material) {
  return std::isfinite(material.thicknessMeters) && material.thicknessMeters > 0.0 &&
         std::isfinite(material.youngsModulusPa) && material.youngsModulusPa > 0.0 &&
         std::isfinite(material.densityKgPerM3) && material.densityKgPerM3 > 0.0 &&
         std::isfinite(material.poisson) && material.poisson > -1.0 &&
         material.poisson < 0.5;
}

[[nodiscard]] inline bool impulseSane(const CymbalImpulse& impulse) {
  return std::isfinite(impulse.radialFraction) && impulse.radialFraction >= 0.0 &&
         impulse.radialFraction <= 1.0 && std::isfinite(impulse.angleRadians) &&
         std::isfinite(impulse.momentumKgMetersPerSecond) &&
         impulse.momentumKgMetersPerSecond > 0.0;
}

}  // namespace cymbal_detail

[[nodiscard]] inline Result<XsrCymbalSet, QuantityError> sabianXsrCompleteSet() {
  using R = Result<XsrCymbalSet, QuantityError>;
  XsrCymbalSet set{
      "SABIAN XSR Complete Set XSR5006B",
      cymbal_detail::plate("splash", 10.0),
      cymbal_detail::plate("hi-hat top", 14.0),
      cymbal_detail::plate("hi-hat bottom", 14.0),
      cymbal_detail::plate("fast crash", 16.0),
      cymbal_detail::plate("fast crash", 18.0),
      cymbal_detail::plate("chinese", 18.0),
      cymbal_detail::plate("ride", 20.0),
      kXsrAlloyName,
      kXsrCymbalSource};
  const CymbalPlate* plates[] = {&set.splash,    &set.hiHatTop, &set.hiHatBottom,
                                 &set.crash16,   &set.crash18,  &set.china,
                                 &set.ride};
  for (const CymbalPlate* plate : plates) {
    if (!cymbal_detail::plateSane(*plate))
      return R::failure(QuantityError::InvalidArgument);
  }
  if (set.hiHatTop.radius.value() != set.hiHatBottom.radius.value())
    return R::failure(QuantityError::RangeError);
  if (set.crash18.radius.value() != set.china.radius.value())
    return R::failure(QuantityError::RangeError);
  if (!(set.splash.radius.value() < set.hiHatTop.radius.value()) ||
      !(set.hiHatTop.radius.value() < set.crash16.radius.value()) ||
      !(set.crash16.radius.value() < set.crash18.radius.value()) ||
      !(set.crash18.radius.value() < set.ride.radius.value()))
    return R::failure(QuantityError::RangeError);
  if (set.alloyName == nullptr || std::strcmp(set.alloyName, "B20") != 0)
    return R::failure(QuantityError::InvalidArgument);
  return R::success(set);
}

[[nodiscard]] inline Result<FrequencyHz, QuantityError> cymbalModeFrequency(
    const CymbalPlate& plate, const CymbalMaterial& material, int nodalDiameters,
    int radialIndex) {
  using R = Result<FrequencyHz, QuantityError>;
  if (!cymbal_detail::plateSane(plate) || !cymbal_detail::materialSane(material))
    return R::failure(QuantityError::InvalidArgument);
  return freeCircularPlateFrequency(plate.radius, material.thicknessMeters,
                                   material.youngsModulusPa, material.densityKgPerM3,
                                   material.poisson, nodalDiameters, radialIndex);
}

struct CymbalModalCoefficient {
  int nodalDiameters = 0;
  int radialIndex = 0;
  double lambda = 0.0;
  double angularFrequency = 0.0;
  double velocityCoefficient = 0.0;
};

// b = (P / (ρ h)) R(r_s) / N, with N the L2 norm of the free-plate mode.
// A center strike does not excite n > 0, because R(0) = 0 for those modes.
[[nodiscard]] inline Result<CymbalModalCoefficient, QuantityError>
cymbalStrikeCoefficient(const CymbalPlate& plate, const CymbalMaterial& material,
                        const CymbalImpulse& impulse, int nodalDiameters,
                        int radialIndex) {
  using R = Result<CymbalModalCoefficient, QuantityError>;
  if (!cymbal_detail::plateSane(plate) || !cymbal_detail::materialSane(material) ||
      !cymbal_detail::impulseSane(impulse) || nodalDiameters < 0 || radialIndex < 1)
    return R::failure(QuantityError::InvalidArgument);
  auto mode = freeCircularPlateMode(nodalDiameters, radialIndex, material.poisson);
  if (!mode.ok()) return R::failure(mode.error());
  auto frequency = freeCircularPlateFrequency(
      plate.radius, material.thicknessMeters, material.youngsModulusPa,
      material.densityKgPerM3, material.poisson, nodalDiameters, radialIndex);
  if (!frequency.ok()) return R::failure(frequency.error());
  auto norm = freePlateModeNorm(mode.value(), plate.radius.value());
  if (!norm.ok()) return R::failure(norm.error());
  const double strikeR = impulse.radialFraction * plate.radius.value();
  auto shape = freePlateRadialShape(mode.value(), plate.radius.value(), strikeR);
  if (!shape.ok()) return R::failure(shape.error());
  const double areal = material.densityKgPerM3 * material.thicknessMeters;
  const double velocity =
      impulse.momentumKgMetersPerSecond / areal * shape.value() / norm.value();
  const double omega = constants::twoPi * frequency.value().value();
  if (!std::isfinite(velocity) || !std::isfinite(omega) || !(omega > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(CymbalModalCoefficient{nodalDiameters, radialIndex,
                                           mode.value().lambda, omega, velocity});
}

namespace cymbal_detail {

[[nodiscard]] inline Result<double, QuantityError> strikeSum(
    const CymbalPlate& plate, const CymbalMaterial& material,
    const CymbalImpulse& impulse, double observeRadialFraction, double observeAngle,
    double timeSeconds, int maxNodalDiameters, int maxRadial, bool velocity) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(observeRadialFraction) || observeRadialFraction < 0.0 ||
      observeRadialFraction > 1.0 || !std::isfinite(observeAngle) ||
      !std::isfinite(timeSeconds) || timeSeconds < 0.0 || maxNodalDiameters < 0 ||
      maxRadial < 1)
    return R::failure(QuantityError::InvalidArgument);
  double sum = 0.0;
  for (int n = 0; n <= maxNodalDiameters; ++n) {
    for (int s = 1; s <= maxRadial; ++s) {
      auto coefficient =
          cymbalStrikeCoefficient(plate, material, impulse, n, s);
      if (!coefficient.ok()) return R::failure(coefficient.error());
      auto mode = freeCircularPlateMode(n, s, material.poisson);
      if (!mode.ok()) return R::failure(mode.error());
      const double observeR = observeRadialFraction * plate.radius.value();
      auto shape = freePlateRadialShape(mode.value(), plate.radius.value(), observeR);
      if (!shape.ok()) return R::failure(shape.error());
      const double angle = std::cos(static_cast<double>(n) *
                                    (observeAngle - impulse.angleRadians));
      const double omega = coefficient.value().angularFrequency;
      const double time = velocity ? std::cos(omega * timeSeconds)
                                  : std::sin(omega * timeSeconds) / omega;
      const double term =
          coefficient.value().velocityCoefficient * time * shape.value() * angle;
      if (!std::isfinite(term)) return R::failure(QuantityError::RangeError);
      sum += term;
    }
  }
  if (!std::isfinite(sum)) return R::failure(QuantityError::RangeError);
  return R::success(sum);
}

}  // namespace cymbal_detail

[[nodiscard]] inline Result<double, QuantityError> cymbalStrikeDisplacement(
    const CymbalPlate& plate, const CymbalMaterial& material,
    const CymbalImpulse& impulse, double observeRadialFraction, double observeAngle,
    double timeSeconds, int maxNodalDiameters, int maxRadial) {
  return cymbal_detail::strikeSum(plate, material, impulse, observeRadialFraction,
                                  observeAngle, timeSeconds, maxNodalDiameters,
                                  maxRadial, false);
}

[[nodiscard]] inline Result<double, QuantityError> cymbalStrikeVelocity(
    const CymbalPlate& plate, const CymbalMaterial& material,
    const CymbalImpulse& impulse, double observeRadialFraction, double observeAngle,
    double timeSeconds, int maxNodalDiameters, int maxRadial) {
  return cymbal_detail::strikeSum(plate, material, impulse, observeRadialFraction,
                                  observeAngle, timeSeconds, maxNodalDiameters,
                                  maxRadial, true);
}

[[nodiscard]] inline std::string formula(const XsrCymbalSet& set) {
  std::string text = set.name == nullptr ? std::string{} : std::string{set.name};
  const CymbalPlate* plates[] = {&set.splash,  &set.hiHatTop, &set.hiHatBottom,
                                 &set.crash16, &set.crash18,  &set.china,
                                 &set.ride};
  for (const CymbalPlate* plate : plates) {
    text.push_back(' ');
    if (plate->name != nullptr) text += plate->name;
  }
  return text;
}

}  // namespace mmathen
