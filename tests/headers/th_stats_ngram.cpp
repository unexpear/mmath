// Header self-containment: mmathen/stats/ngram.h compiles standalone.
#include <mmathen/stats/ngram.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("stats/ngram.h is self-contained", "[headers]") {
  CHECK(mmathen::Vocabulary{}.size() == 3);
}
