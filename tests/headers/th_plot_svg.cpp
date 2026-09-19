// Header self-containment: mmathen/plot/svg.h compiles standalone.
#include <mmathen/plot/svg.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("plot/svg.h is self-contained", "[headers]") {
  mmathen::PlotStyle style;
  CHECK(style.dbMin < style.dbMax);
}
