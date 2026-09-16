// Header self-containment: mmathen/core/result.h compiles standalone.
#include <mmathen/core/result.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("result.h is self-contained", "[headers]") {
  auto ok =
      mmathen::Result<int, mmathen::QuantityError>::success(3);
  CHECK(ok.ok());
  CHECK(ok.value() == 3);
  auto err =
      mmathen::Result<int, mmathen::QuantityError>::failure(
          mmathen::QuantityError::DomainError);
  CHECK_FALSE(err.ok());
  CHECK(err.error() == mmathen::QuantityError::DomainError);
}
