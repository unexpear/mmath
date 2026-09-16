// D3 — MOS construction tests (Catch2).
//
// Diatonic (5 large + 2 small) and pentatonic (3 + 2) step multiset anchors;
// degenerate stacks rejected; tolerance clustering is structural (stacked
// floating-point sums need not be bitwise identical).

#include <catch2/catch_test_macros.hpp>

#include <utility>
#include <vector>

#include <mmathen/music/mos.h>
#include <mmathen/units/music_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

int multiplicity(const MosScale& m, std::size_t which) {
  return m.sizes[which].second;
}
}  // namespace

TEST_CASE("fifth-stacked heptatonic is a 5+2 MOS", "[d3mos]") {
  const auto scale = unwrap(mosWord(Cents{701.9550008653874}, 7));
  CHECK(scale.isMOS);
  REQUIRE(scale.sizes.size() == 2);
  // Five large steps, two small — order-independent multiset check.
  const int a = multiplicity(scale, 0);
  const int b = multiplicity(scale, 1);
  CHECK(((a == 5 && b == 2) || (a == 2 && b == 5)));
  CHECK(scale.pitchClasses.size() == 7);
  CHECK(scale.steps.size() == 7);
}

TEST_CASE("fifth-stacked pentatonic is a 3+2 MOS", "[d3mos]") {
  const auto scale = unwrap(mosWord(Cents{701.9550008653874}, 5));
  CHECK(scale.isMOS);
  REQUIRE(scale.sizes.size() == 2);
  const int a = multiplicity(scale, 0);
  const int b = multiplicity(scale, 1);
  CHECK(((a == 3 && b == 2) || (a == 2 && b == 3)));
}

TEST_CASE("degenerate stacks are rejected, trivial ones are not MOS",
          "[d3mos]") {
  // Generator 0 mod octave collapses every pitch class: not a scale.
  CHECK(mosWord(Cents{0.0}, 7).error() == QuantityError::InvalidArgument);
  CHECK(mosWord(Cents{1200.0}, 7).error() == QuantityError::InvalidArgument);
  // Tritone generator duplicates classes: rejected, not silently merged.
  CHECK(mosWord(Cents{600.0}, 4).error() == QuantityError::InvalidArgument);
  // Single note: well-defined but not a MOS (one step size).
  const auto one = unwrap(mosWord(Cents{701.9550008653874}, 1));
  CHECK_FALSE(one.isMOS);
  CHECK(mosWord(Cents{701.9550008653874}, 0).error() ==
        QuantityError::InvalidArgument);
}
