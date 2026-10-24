// Header self-containment: mmathen/instruments/drum.h compiles standalone.
#include <mmathen/instruments/drum.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/drum.h is self-contained", "[headers]") {
  const auto kit = mmathen::yamahaTourCustomKit();
  REQUIRE(kit.ok());
  const auto head = mmathen::tuneMembraneHead(kit.value().snare55, 150.0, 0.2,
                                              0.4, 1.0, 1.0e6);
  REQUIRE(head.ok());
  CHECK(mmathen::membraneStrikeVelocity(head.value(), {0.2, 0.0, 0.01}, 0.2, 0.0,
                                        0.0, 0, 1)
            .ok());
}
