// Header self-containment: mmathen/instruments/bowed_note.h compiles standalone.
#include <mmathen/instruments/bowed_note.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/bowed_note.h is self-contained", "[headers]") {
  CHECK(mmathen::BowedPlayerHabits{}.bowForceScale == 1.0);
}
