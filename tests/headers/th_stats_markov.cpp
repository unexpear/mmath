// Header self-containment: mmathen/stats/markov.h compiles standalone.
#include <mmathen/stats/markov.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("stats/markov.h is self-contained", "[headers]") {
  CHECK(mmathen::makeMarkovChain({}).error() == mmathen::QuantityError::InvalidArgument);
}
