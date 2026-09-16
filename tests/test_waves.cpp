// M6 — wave kinematics and air medium tests (Catch2).
//
// Anchors are Python-computed literals (ideal-gas dry air), never the
// implementation's own sqrt/pow output alone. Round trips supplement them.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>

#include <mmathen/math/comparison.h>
#include <mmathen/physics/medium.h>
#include <mmathen/physics/waves.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

SpeedMetersPerSecond air20() {
  return unwrap(
      speedOfSound(unwrap(makeAir(TemperatureKelvin{293.15}))));
}
}  // namespace

TEST_CASE("dry-air speed of sound follows c = sqrt(gRT)", "[physics]") {
  CHECK(nearlyEqual(air20().value(), 343.2367605312694, 1e-9, 1e-12));
  const auto cold = unwrap(
      speedOfSound(unwrap(makeAir(TemperatureKelvin{273.15}))));
  CHECK(nearlyEqual(cold.value(), 331.3213693379888, 1e-9, 1e-12));
  const auto warm = unwrap(
      speedOfSound(unwrap(makeAir(TemperatureKelvin{303.15}))));
  CHECK(nearlyEqual(warm.value(), 349.0419541831612, 1e-9, 1e-12));
  // A guitar is (slightly, correctly) different in cold vs warm air.
  CHECK(cold < warm);
  CHECK(makeAir(TemperatureKelvin{0.0}).error() ==
        QuantityError::InvalidArgument);
  CHECK(makeAir(TemperatureKelvin{-10.0}).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("wavelength and frequency invert through the medium", "[physics]") {
  const auto lam =
      unwrap(wavelengthFromFrequency(FrequencyHz{440.0}, air20()));
  CHECK(nearlyEqual(lam.value(), 0.7800835466619759, 1e-12, 1e-12));
  CHECK(nearlyEqual(
      unwrap(frequencyFromWavelength(lam, air20())).value(), 440.0, 1e-9,
      1e-9));
  // Phase velocity closes the loop: v = f*lambda recovers c.
  CHECK(nearlyEqual(
      unwrap(phaseVelocity(FrequencyHz{440.0}, lam)).value(), air20().value(),
      1e-9, 1e-9));
  CHECK(wavelengthFromFrequency(FrequencyHz{0.0}, air20()).error() ==
        QuantityError::DomainError);
  CHECK(frequencyFromWavelength(DistanceMeters{0.0}, air20()).error() ==
        QuantityError::DomainError);
}

TEST_CASE("wavenumber is explicit radians per meter", "[physics]") {
  CHECK(nearlyEqual(unwrap(wavenumber(DistanceMeters{1.0})).value(),
                    2.0 * std::numbers::pi, 1e-12, 1e-12));
  CHECK(nearlyEqual(
      unwrap(wavenumber(DistanceMeters{0.7800835466619759})).value(),
      8.054503051712489, 1e-9, 1e-9));
  CHECK(wavenumber(DistanceMeters{0.0}).error() ==
        QuantityError::DomainError);
  CHECK(wavenumber(DistanceMeters{-2.0}).error() ==
        QuantityError::DomainError);
}

TEST_CASE("traveling wave hits its analytic nodes", "[physics]") {
  // A = 0.01 m, lambda = 1 m (k = 2pi), f = 1 Hz (w = 2pi), phi = 0.
  const DisplacementMeters amp{0.01};
  const auto k = unwrap(wavenumber(DistanceMeters{1.0}));
  const AngularFrequencyRadPerSec w{2.0 * std::numbers::pi};
  const auto at = [&](double x, double t) {
    return unwrap(travelingWave(amp, k, w, PhaseRadians{0.0},
                                DistanceMeters{x}, TimePointSeconds{t}))
        .value();
  };
  CHECK(nearlyEqual(at(0.0, 0.0), 0.0, 1e-12, 0.0));
  CHECK(nearlyEqual(at(0.25, 0.0), 0.01, 1e-12, 0.0));   // sin(pi/2)
  CHECK(nearlyEqual(at(0.0, 0.25), -0.01, 1e-12, 0.0));  // sin(-pi/2)
  CHECK(nearlyEqual(at(0.5, 0.0), 0.0, 1e-12, 0.0));     // sin(pi)
  CHECK(unwrap(travelTime(DistanceMeters{10.0}, air20())).value() > 0.029);
  CHECK(unwrap(travelTime(DistanceMeters{10.0}, air20())).value() < 0.030);
  CHECK(nearlyEqual(
      unwrap(travelTime(DistanceMeters{10.0}, air20())).value(),
      0.02913440851883633, 1e-12, 1e-12));
  CHECK(travelTime(DistanceMeters{10.0}, SpeedMetersPerSecond{0.0}).error() ==
        QuantityError::InvalidArgument);
}
