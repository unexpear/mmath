// Header self-containment: mmathen/instruments/trombone.h compiles standalone.
#include <mmathen/instruments/trombone.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/trombone.h is self-contained", "[headers]") {
  const auto trombone = mmathen::makeTrombone(mmathen::TromboneSpec{
      1.2, 0.15, 1.2e-4, 101325.0, 293.15, 2.0e-6, 1.0e-5, 1.5e-5, 1.0e-2,
      2.0e-5, 500.0, 1.0e-4, 0.0});
  REQUIRE(trombone.ok());
  CHECK(mmathen::tromboneCylinderMode(trombone.value(), 1).ok());
  CHECK(mmathen::tromboneCylinderMode(trombone.value(), 2).error() ==
        mmathen::QuantityError::DomainError);
  const auto extended = mmathen::tromboneAtSlide(trombone.value(), 0.3);
  REQUIRE(extended.ok());
  CHECK(extended.value().bore.fundamentalHz <
        trombone.value().bore.fundamentalHz);
  CHECK(mmathen::tromboneEmbouchure(trombone.value(), 1000.0, 1.0, 0.0, 0.0,
                                    0.0)
            .ok());
}
