// Header self-containment: mmathen/instruments/player_profile.h compiles standalone.
#include <mmathen/instruments/player_profile.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/player_profile.h is self-contained", "[headers]") {
  CHECK(mmathen::PlayerFeel{}.isDefault());
}
