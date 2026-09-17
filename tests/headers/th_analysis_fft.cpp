// Header self-containment: mmathen/analysis/fft.h compiles standalone.
#include <mmathen/analysis/fft.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("analysis/fft.h is self-contained", "[headers]") {
  CHECK(mmathen::isPowerOfTwo(1024));
  CHECK(mmathen::windowCoefficients(mmathen::Window::Hann, 8).ok());
}
