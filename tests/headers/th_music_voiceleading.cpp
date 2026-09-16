// Header self-containment: mmathen/music/voiceleading.h compiles standalone.
#include <mmathen/music/voiceleading.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("music/voiceleading.h is self-contained", "[headers]") {
  const auto r = mmathen::voiceLeadingCost({60}, {62});
  CHECK(r.ok());
}
