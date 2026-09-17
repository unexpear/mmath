#pragma once

// D16 — sourced material and instrument data, with a validation annex
// (VALIDATION.md: "each physical model gets a validation annex").
//
// EVERY NUMBER HERE HAS A CITED SOURCE; nothing is a convenient default.
//
// 1. Steel piano wire: E = 2.0e11 Pa, rho = 7850 kg/m^3 — Chabassier,
//    Chaigne & Joly, "Modeling and simulation of a grand piano", J. Acoust.
//    Soc. Am. 134(1), 648-665 (2013), Table III (string parameters).
//    Cross-check: R. W. Young, "Inharmonicity of Plain Wire Piano Strings",
//    J. Acoust. Soc. Am. 24(3), 267-273 (1952), DOI 10.1121/1.1906888,
//    abstract: "A value of Q/rho = 25.5 x 10^10 (cm/sec)^2 was assumed for the
//    steel wire" (Q = Young's modulus). E/rho above = 2.548e7 m^2/s^2, within
//    0.09% of Young's 2.55e7.
// 2. Concert-grand string scale data for five notes (L, d, winding density
//    factor F, tension T0, fundamental f0), same Table III. These are the
//    paper's MODEL INPUTS for a real instrument, not per-string measurements
//    of inharmonicity. Winding convention of that table: A = pi d^2/4,
//    I = pi d^4/64 from the given (core) d, density rho*F.
// 3. Measured inharmonicity trend, Young 1952 abstract, plain wire strings
//    in situ in six pianos: "about 1.2 cents for the second mode of vibration
//    of the middle C string. Above this point every eight semitones it is
//    doubled. Below middle C the inharmonicity is consistently less in large
//    pianos than in small ones."
//
// Not available: an openly retrievable table of MEASURED string damping
// (decay times per partial) could not be obtained in this milestone, so loss
// parameters remain caller-supplied data (lossFromT60) and unvalidated.

#include <cmath>
#include <string>
#include <vector>

#include "../core/result.h"
#include "../math/constants.h"
#include "../units/physical_units.h"
#include "wave1d.h"

namespace mmathen {

struct WireMaterial {
  double youngsModulusPa;
  double densityKgPerM3;
  const char* source;
};

[[nodiscard]] inline WireMaterial steelPianoWireChabassier2013() {
  return WireMaterial{2.0e11, 7850.0,
                      "Chabassier, Chaigne & Joly 2013, JASA 134(1) 648, "
                      "Table III"};
}

// Young 1952: Q/rho = 25.5e10 (cm/s)^2, converted to m^2/s^2.
inline constexpr double kYoung1952SteelSpecificModulus = 25.5e10 * 1e-4;

// Young 1952 cents formula constant (cgs: d, l in cm): basic inharmonicity
// cents = 3.4e13 n^2 d^2 / (v0^2 l^4).
inline constexpr double kYoung1952CentsConstantCgs = 3.4e13;

struct PianoStringSpec {
  const char* note;
  DistanceMeters length;
  DistanceMeters diameter;  // steel (core) diameter
  double densityFactor;     // F: 1 for plain wire, > 1 for wound strings
  TensionNewtons tension;
  FrequencyHz publishedF0;
  int semitonesFromC4;      // middle C = C4
  bool plainWire;
};

// Chabassier, Chaigne & Joly 2013, Table III, all five columns as printed
// (f0 as printed: 38.9, 65.4, 174.6, 555.6, 1571.4 Hz).
[[nodiscard]] inline std::vector<PianoStringSpec> chabassier2013Strings() {
  return {
      {"D#1", DistanceMeters{1.945}, DistanceMeters{1.48e-3}, 5.73,
       TensionNewtons{1781.0}, FrequencyHz{38.9}, -33, false},
      {"C2", DistanceMeters{1.600}, DistanceMeters{0.9502e-3}, 3.55,
       TensionNewtons{865.0}, FrequencyHz{65.4}, -24, false},
      {"F3", DistanceMeters{0.961}, DistanceMeters{1.0525e-3}, 1.0,
       TensionNewtons{774.0}, FrequencyHz{174.6}, -7, true},
      {"C#5", DistanceMeters{0.326}, DistanceMeters{0.921e-3}, 1.0,
       TensionNewtons{684.0}, FrequencyHz{555.6}, 13, true},
      {"G6", DistanceMeters{0.124}, DistanceMeters{0.794e-3}, 1.0,
       TensionNewtons{587.0}, FrequencyHz{1571.4}, 31, true},
  };
}

// The string as a D10 wave system: hinged stiff Fixed-Fixed (the paper
// states simply supported ends, Table II), c = sqrt(T/mu) with
// mu = rho F pi d^2/4, kappa^2 = E I / (rho F A) = E d^2 / (16 rho F).
// Invalid geometry/material (non-finite or non-positive) is InvalidArgument.
[[nodiscard]] inline Result<WaveSystem1D, QuantityError> pianoString(
    const PianoStringSpec& spec, const WireMaterial& material) {
  using R = Result<WaveSystem1D, QuantityError>;
  const double d = spec.diameter.value();
  const double f = spec.densityFactor;
  const double e = material.youngsModulusPa;
  const double rho = material.densityKgPerM3;
  for (const double v : {d, f, e, rho})
    if (!std::isfinite(v) || !(v > 0.0))
      return R::failure(QuantityError::InvalidArgument);
  const double mu = rho * f * constants::pi * d * d / 4.0;
  auto sys = idealString(spec.length, spec.tension, LinearDensityKgPerM{mu});
  if (!sys.ok()) return R::failure(QuantityError::InvalidArgument);
  const double kappa = 0.25 * d * std::sqrt(e / (rho * f));
  return withStiffness(sys.value(), kappa);
}

// Young 1952's measured trend for PLAIN wire, mode 2, in cents: 1.2 at middle
// C, doubling every 8 semitones. It summarizes six pianos ("about"), so it is
// a trend reference, not a per-instrument value.
[[nodiscard]] inline double young1952Mode2Cents(int semitonesFromC4) {
  return 1.2 * std::pow(2.0, static_cast<double>(semitonesFromC4) / 8.0);
}

// Inharmonicity of mode n in cents from a system's partials:
// 1200 log2(f_n / (n f_1^ideal)), where n f_1^ideal = n c/(2L) — the same
// "departure from the harmonic modes of an ideal flexible string" Young
// measured. Requires a Fixed-Fixed system and n >= 1.
[[nodiscard]] inline Result<double, QuantityError> inharmonicityCents(
    const WaveSystem1D& system, int n) {
  using R = Result<double, QuantityError>;
  if (n < 1 || system.left != Boundary::Fixed ||
      system.right != Boundary::Fixed)
    return R::failure(QuantityError::InvalidArgument);
  auto fn = modeFrequency(system, n);
  if (!fn.ok()) return R::failure(fn.error());
  const double ideal = static_cast<double>(n) * system.speed.value() /
                       (2.0 * system.length.value());
  if (!std::isfinite(ideal) || !(ideal > 0.0))
    return R::failure(QuantityError::RangeError);
  const double cents = 1200.0 * std::log2(fn.value().value() / ideal);
  if (!std::isfinite(cents)) return R::failure(QuantityError::RangeError);
  return R::success(cents);
}

}  // namespace mmathen
