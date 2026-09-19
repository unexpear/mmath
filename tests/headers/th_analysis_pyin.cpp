// Header self-containment: mmathen/analysis/pyin.h compiles standalone.
#include <mmathen/analysis/pyin.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("analysis/pyin.h is self-contained", "[headers]") {
  CHECK(mmathen::PyinOptions{}.binsPerSemitone == 10);
}
