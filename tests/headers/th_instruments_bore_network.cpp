// Header self-containment: mmathen/instruments/bore_network.h compiles standalone.
#include <mmathen/instruments/bore_network.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("instruments/bore_network.h is self-contained", "[headers]") {
  const double lengths[] = {0.05, 0.0};
  const bool engaged[] = {true, false};
  const auto sum = mmathen::sumEngagedLengths(lengths, engaged);
  REQUIRE(sum.ok());
  CHECK(sum.value() == 0.05);
  const auto branch = mmathen::toneHoleBranch(mmathen::ToneHoleModel::Dalmont2002, 0.008,
                                              0.004, 0.006, 0.0, 0.0);
  REQUIRE(branch.ok());
  CHECK(branch.value().seriesMeters < 0.0);
}
