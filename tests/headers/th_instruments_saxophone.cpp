// Header self-containment: mmathen/instruments/saxophone.h compiles standalone.
#include <mmathen/instruments/saxophone.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/saxophone.h is self-contained", "[headers]") {
  const auto saxophone = mmathen::makeSaxophone(mmathen::SaxophoneSpec{
      0.6, 0.2, 2.0e-4, 101325.0, 293.15, 1.5e-5, 6.0e3, 2.0e-5, 1.0e-4,
      1.0e-3, 0.0});
  REQUIRE(saxophone.ok());
  CHECK(mmathen::saxophoneConeMode(saxophone.value(), 1).ok());
  CHECK(mmathen::saxophoneConeMode(saxophone.value(), 2).ok());
  CHECK(mmathen::saxophoneConeImpedance(saxophone.value(),
                                        saxophone.value().fundamentalHz)
            .error() == mmathen::QuantityError::RangeError);
  CHECK(mmathen::saxophoneEmbouchure(saxophone.value(), 2000.0, 0.0, 0.0)
            .ok());
}
