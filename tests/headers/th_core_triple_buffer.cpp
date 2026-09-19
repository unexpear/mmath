// Header self-containment: mmathen/core/triple_buffer.h compiles standalone.
#include <mmathen/core/triple_buffer.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("core/triple_buffer.h is self-contained", "[headers]") {
  mmathen::TripleBuffer<int> tb(1);
  int v = 0;
  CHECK_FALSE(tb.read(v));
}
