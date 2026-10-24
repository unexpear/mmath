// Header self-containment: mmathen/instruments/hammer.h compiles standalone.
#include <mmathen/instruments/hammer.h>
#include <mmathen/instruments/violin.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/hammer.h is self-contained", "[headers]") {
  const auto violinInstrument = mmathen::violin();
  REQUIRE(violinInstrument.ok());
  const auto blow = mmathen::makeHardHammerBlow(
      violinInstrument.value().courses[0], mmathen::HammerStrike{0.5, 0.01, 1.0});
  REQUIRE(blow.ok());
  CHECK(mmathen::hardHammerForce(blow.value(), 0.0).ok());
  CHECK(mmathen::reflectedHammerForce(blow.value(), 0.0).ok());
  CHECK(mmathen::feltHammerForce(1.0e-4, 1.0e6, 2.0).ok());
  const mmathen::FeltHammerState state{1.0e-4, 1.0, 0.0, 0.0};
  CHECK(mmathen::feltHammerDerivative(blow.value(), state, 1.0e6, 2.0).ok());
}
