// Unsourced physical numbers stay arguments.
//
// A zero end correction is the caller's zero, not Levine and Schwinger's
// 0.6133 a. A non-positive radius is refused. ValveBlow's trailing zeros
// select the constant-magnitude reflection; they are not a bell.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <utility>

#include <mmathen/instruments/bore_valve.h>
#include <mmathen/instruments/drum.h>
#include <mmathen/instruments/electric_guitar.h>
#include <mmathen/physics/medium.h>
#include <mmathen/physics/radiation.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
}  // namespace

TEST_CASE("an open end does not invent an end correction", "[defaults]") {
  const auto air = unwrap(makeAir(TemperatureKelvin{293.15}));
  const double speed = unwrap(speedOfSound(air)).value();
  const SampleRateHz rate{48000.0};
  const double radius = 0.02;
  CHECK(OpenEndReflection::create(0.0, 0.0, speed, rate).error() ==
        QuantityError::InvalidArgument);
  CHECK(OpenEndReflection::create(-radius, radius, speed, rate).error() ==
        QuantityError::InvalidArgument);
  auto zero = unwrap(OpenEndReflection::create(radius, 0.0, speed, rate));
  const double unflanged = 0.6133 * radius;
  auto cited = unwrap(OpenEndReflection::create(radius, unflanged, speed, rate));
  const int delay = 126;
  const double flat = unwrap(zero.resonanceHz(delay, false));
  const double longer = unwrap(cited.resonanceHz(delay, false));
  CHECK(std::isfinite(flat));
  CHECK(std::isfinite(longer));
  CHECK(longer < flat - 1.0);
  CHECK(std::fabs(flat - longer) > 1.0);
}

TEST_CASE("a valve blow's zero radius is not a bell", "[defaults]") {
  const ValveBlow omitted{1000.0, 16, 0.98, 0.0};
  CHECK(omitted.radiusMeters == 0.0);
  CHECK(omitted.endCorrectionMeters == 0.0);
  CHECK(kCatalogInchMeters == 0.0254);
  CHECK(kElectricGuitarInchMeters == 0.0254);
  CHECK(kAvoirdupoisPoundKg == 0.45359237);
  CHECK(acoustic_constants::dryAirGamma == 1.4);
  CHECK(acoustic_constants::dryAirGasConstant == 287.058);
}
