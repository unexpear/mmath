// D2 — modes, impedance, absorption tests (Catch2).
//
// Absorption anchors: ISO 9613-1 Table 1 spot values (dB/km, fetched from
// the standard-derived CSV) with 2% relative tolerance — the table carries
// 3 significant figures from 1993-era computation, so tighter would test
// history, not physics. Exact anchors (relaxation frequencies at zero
// humidity, energy identity, matched/rigid limits) pin the implementation.

#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include <mmathen/math/comparison.h>
#include <mmathen/physics/absorption.h>
#include <mmathen/physics/impedance.h>
#include <mmathen/physics/medium.h>
#include <mmathen/physics/modes.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

HumidAir humidAir(double celsius, double rhFraction) {
  return unwrap(makeHumidAir(TemperatureKelvin{celsius + 273.15},
                             unwrap(makeRelativeHumidity(rhFraction)),
                             PressurePascals{101325.0}));
}
}  // namespace

TEST_CASE("string modes from wave speed and length", "[d2]") {
  // v = sqrt(100/0.01) = 100 m/s exactly; L = 1 m.
  CHECK(unwrap(stringWaveSpeed(TensionNewtons{100.0},
                               LinearDensityKgPerM{0.01})) ==
        SpeedMetersPerSecond{100.0});
  CHECK(unwrap(stringMode(DistanceMeters{1.0}, TensionNewtons{100.0},
                          LinearDensityKgPerM{0.01}, 1)) ==
        FrequencyHz{50.0});
  CHECK(unwrap(stringMode(DistanceMeters{1.0}, TensionNewtons{100.0},
                          LinearDensityKgPerM{0.01}, 3)) ==
        FrequencyHz{150.0});
  // A110 string: v = sqrt(484/0.01) = 220 m/s exactly.
  CHECK(unwrap(stringMode(DistanceMeters{1.0}, TensionNewtons{484.0},
                          LinearDensityKgPerM{0.01}, 1)) ==
        FrequencyHz{110.0});
  CHECK(unwrap(stringMode(DistanceMeters{1.0}, TensionNewtons{484.0},
                          LinearDensityKgPerM{0.01}, 2)) ==
        FrequencyHz{220.0});
  CHECK(stringMode(DistanceMeters{1.0}, TensionNewtons{100.0},
                   LinearDensityKgPerM{0.01}, 0)
            .error() == QuantityError::InvalidArgument);
  CHECK(stringMode(DistanceMeters{0.0}, TensionNewtons{100.0},
                   LinearDensityKgPerM{0.01}, 1)
            .error() == QuantityError::InvalidArgument);
  CHECK(stringMode(DistanceMeters{1.0}, TensionNewtons{0.0},
                   LinearDensityKgPerM{0.01}, 1)
            .error() == QuantityError::DomainError);
}

TEST_CASE("open pipes keep all harmonics, stopped pipes odd only", "[d2]") {
  const SpeedMetersPerSecond c{340.0};
  CHECK(unwrap(openPipeMode(DistanceMeters{1.0}, c, 1)) ==
        FrequencyHz{170.0});
  CHECK(unwrap(openPipeMode(DistanceMeters{1.0}, c, 2)) ==
        FrequencyHz{340.0});
  CHECK(unwrap(stoppedPipeMode(DistanceMeters{1.0}, c, 1)) ==
        FrequencyHz{85.0});
  CHECK(unwrap(stoppedPipeMode(DistanceMeters{1.0}, c, 3)) ==
        FrequencyHz{255.0});
  CHECK(unwrap(stoppedPipeMode(DistanceMeters{1.0}, c, 5)) ==
        FrequencyHz{425.0});
  // Even modes do not exist in stopped geometry: DomainError, not a number.
  CHECK(stoppedPipeMode(DistanceMeters{1.0}, c, 2).error() ==
        QuantityError::DomainError);
  CHECK(stoppedPipeMode(DistanceMeters{1.0}, c, 0).error() ==
        QuantityError::InvalidArgument);
  // Real-air integration: open C-pipe, L = 0.5 m sounds exactly c Hz.
  const auto airC =
      unwrap(speedOfSound(unwrap(makeAir(TemperatureKelvin{293.15}))));
  CHECK(unwrap(openPipeMode(DistanceMeters{0.5}, airC, 1)) ==
        FrequencyHz{airC.value()});
}

TEST_CASE("impedance, reflection, transmission, energy", "[d2]") {
  CHECK(unwrap(airDensity(PressurePascals{101325.0},
                          TemperatureKelvin{293.15}))
            .value() == 1.2040847588826422);
  CHECK(nearlyEqual(unwrap(airImpedanceAt(TemperatureKelvin{293.15})).value(),
                    413.2861520439527, 1e-9, 1e-12));
  CHECK(waterImpedance() == ImpedancePasPerMs{1480000.0});
  // Matched interface: no reflection, full transmission (exact).
  const ImpedancePasPerMs z{413.0};
  CHECK(unwrap(reflectionCoefficient(z, z)).value() == 0.0);
  CHECK(unwrap(transmissionCoefficient(z, z)).value() == 1.0);
  // Air -> water: near-total reflection. Anchors use the computed air
  // impedance (same expression both sides would hide a bias here).
  const auto zAir =
      unwrap(airImpedanceAt(TemperatureKelvin{293.15}));
  const auto r =
      unwrap(reflectionCoefficient(zAir, waterImpedance())).value();
  const auto t =
      unwrap(transmissionCoefficient(zAir, waterImpedance())).value();
  CHECK(nearlyEqual(r, 0.9994416611146228, 1e-12, 1e-12));
  CHECK(nearlyEqual(t, 1.9994416611146228, 1e-12, 1e-12));
  CHECK(nearlyEqual(r * r + (zAir.value() / waterImpedance().value()) * t * t,
                    1.0, 1e-12, 1e-12));  // R^2 + (Z1/Z2)T^2 = 1
  CHECK(reflectionCoefficient(ImpedancePasPerMs{0.0}, z).error() ==
        QuantityError::DomainError);
}

TEST_CASE("ISO 9613-1 chain: saturation, relaxation, tables", "[d2]") {
  // Exact anchors at zero humidity (definitional, no table involved).
  CHECK(nearlyEqual(
      unwrap(saturationVapourPressure(TemperatureKelvin{293.15})).value(),
      2336.630452976146, 1e-6, 1e-9));
  const auto dry = humidAir(20.0, 0.0);
  CHECK(unwrap(relaxationFrequencyNitrogen(dry)) == FrequencyHz{9.0});
  CHECK(unwrap(relaxationFrequencyOxygen(dry)) == FrequencyHz{24.0});
  // ISO Table 1 spots (dB/km / 1000); 2% relative covers table precision
  // plus 1993-era computation — the equations, not the table, are exact.
  CHECK(nearlyEqual(
      unwrap(attenuationCoefficient(humidAir(25.0, 0.8),
                                    FrequencyHz{1000.0}))
          .value(),
      0.00635, 1e-9, 0.02));
  CHECK(nearlyEqual(
      unwrap(attenuationCoefficient(humidAir(25.0, 0.8),
                                    FrequencyHz{4000.0}))
          .value(),
      0.0211, 1e-9, 0.02));
  CHECK(nearlyEqual(
      unwrap(attenuationCoefficient(humidAir(-15.0, 0.3),
                                    FrequencyHz{1000.0}))
          .value(),
      0.00944, 1e-9, 0.02));
  CHECK(nearlyEqual(
      unwrap(attenuationCoefficient(humidAir(-20.0, 0.1),
                                    FrequencyHz{1000.0}))
          .value(),
      0.00165, 1e-9, 0.02));
  // Non-monotonic physics: at 4 kHz absorption FALLS as humidity rises.
  CHECK(unwrap(attenuationCoefficient(humidAir(20.0, 0.4),
                                      FrequencyHz{4000.0}))
            .value() >
        unwrap(attenuationCoefficient(humidAir(20.0, 0.7),
                                      FrequencyHz{4000.0}))
            .value());
  // Scope enforcement (ISO stated scope).
  CHECK(makeHumidAir(TemperatureKelvin{200.0},
                     unwrap(makeRelativeHumidity(0.5)),
                     PressurePascals{101325.0})
            .error() == QuantityError::InvalidArgument);
  CHECK(makeRelativeHumidity(1.5).error() == QuantityError::InvalidArgument);
  CHECK(attenuationCoefficient(humidAir(20.0, 0.5), FrequencyHz{0.0})
            .error() == QuantityError::InvalidArgument);
  CHECK(attenuationCoefficient(humidAir(20.0, 0.5), FrequencyHz{20001.0})
            .error() == QuantityError::InvalidArgument);
  // Total over distance: 0.6345 dB per 100 m at 25 C / 80% / 1 kHz.
  const auto a = unwrap(
      attenuationCoefficient(humidAir(25.0, 0.8), FrequencyHz{1000.0}));
  CHECK(nearlyEqual(
      unwrap(attenuationOverDistance(a, DistanceMeters{100.0})).value(),
      a.value() * 100.0, 0.0, 1e-12));
  CHECK(attenuationOverDistance(a, DistanceMeters{-1.0}).error() ==
        QuantityError::InvalidArgument);
}
