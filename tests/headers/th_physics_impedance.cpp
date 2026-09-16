// Header self-containment: mmathen/physics/impedance.h compiles standalone.
#include <mmathen/physics/impedance.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("physics/impedance.h is self-contained", "[headers]") {
  CHECK(mmathen::waterImpedance() ==
        mmathen::ImpedancePasPerMs{1480000.0});
}
