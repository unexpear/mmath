// Header self-containment: mmathen/time/tempo.h compiles standalone.
#include <mmathen/time/tempo.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("time/tempo.h is self-contained", "[headers]") {
  const auto tempo = mmathen::ConstantTempo::create(mmathen::TempoBPM{120.0});
  CHECK(tempo.ok());
}
