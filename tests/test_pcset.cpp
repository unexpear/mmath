// D4 — pitch-class set tests (Catch2).
//
// The whole transcribed catalogue is verified here: every row's interval
// vector is recomputed from its prime (catches prime/vector slips), every
// row's prime recomputes to itself through primeForm(), and the per-
// cardinality distinct-number counts match Forte's catalogue structure.
// music21 independently confirmed all 352 rows pre-implementation.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <mmathen/music/forte_table.h>
#include <mmathen/music/pcset.h>

using namespace mmathen;

namespace {
PitchClassSet set(std::initializer_list<int> pcs) {
  auto r = makePitchClassSet(std::vector<int>(pcs));
  if (!r.ok()) throw std::logic_error("bad test set");
  return std::move(r).value();
}

std::string forte(const PitchClassSet& s) {
  auto r = forteNumber(s);
  if (!r.ok()) throw std::logic_error("unknown set class");
  return std::move(r).value();
}

std::vector<int> parsePrime(const std::string_view& s) {
  std::vector<int> out;
  for (char c : s) {
    if (c >= '0' && c <= '9') {
      out.push_back(c - '0');
    } else if (c == 'T') {
      out.push_back(10);
    } else {
      out.push_back(11);
    }
  }
  return out;
}

std::array<int, 6> parseIcv(const std::string_view& s) {
  std::array<int, 6> out = {0, 0, 0, 0, 0, 0};
  for (std::size_t i = 0; i < 6; ++i) {
    const char c = s[i];
    if (c >= '0' && c <= '9') {
      out[i] = c - '0';
    } else if (c == 'T') {
      out[i] = 10;
    } else {
      out[i] = 12;
    }
  }
  return out;
}

std::string baseNumber(const std::string_view& number) {
  std::string base(number);
  if (!base.empty() && (base.back() == 'A' || base.back() == 'B'))
    base.pop_back();
  return base;
}
}  // namespace

TEST_CASE("catalogue rows are internally consistent", "[pcset]") {
  // Prime forms are unique per class: B rows store the inversion spelling,
  // which primeForm() normalizes to the A prime. So the expected prime is
  // the row's own prime for A/symmetric rows, the A prime for B rows.
  std::map<std::string, std::string> aPrimeOf;
  for (const auto& e : kForteTable) {
    const std::string num(e.number);
    if (!num.empty() && num.back() == 'A') aPrimeOf[num] = std::string(e.prime);
  }
  std::size_t rows = 0;
  for (const auto& e : kForteTable) {
    const PitchClassSet s{parsePrime(e.prime)};
    CHECK(intervalClassVector(s) == parseIcv(e.icv));
    std::string expected(e.prime);
    const std::string num(e.number);
    if (!num.empty() && num.back() == 'B') {
      const std::string base = num.substr(0, num.size() - 1);
      expected = aPrimeOf[base + "A"];
    }
    CHECK(primeForm(s) == parsePrime(expected));
    ++rows;
  }
  CHECK(rows == 352);
}

TEST_CASE("catalogue cardinalities match Forte's structure", "[pcset]") {
  std::map<int, std::set<std::string>> numbers;
  for (const auto& e : kForteTable)
    numbers[static_cast<int>(parsePrime(e.prime).size())].insert(
        baseNumber(e.number));
  const std::map<int, std::size_t> expected = {{0, 1},  {1, 1}, {2, 6},
                                               {3, 12}, {4, 29}, {5, 38},
                                               {6, 50}, {7, 38}, {8, 29},
                                               {9, 12}, {10, 6}, {11, 1},
                                               {12, 1}};
  REQUIRE(numbers.size() == expected.size());
  for (const auto& [card, count] : expected) {
    INFO("cardinality " << card);
    CHECK(numbers[card].size() == count);
  }
}

TEST_CASE("famous set classes resolve", "[pcset]") {
  CHECK(forte(set({0, 4, 7})) == "3-11B");  // major triad
  CHECK(forte(set({0, 3, 7})) == "3-11A");  // minor triad
  CHECK(forte(set({0, 3, 6})) == "3-10");   // diminished
  CHECK(forte(set({0, 4, 8})) == "3-12");   // augmented
  CHECK(forte(set({0, 4, 7, 10})) == "4-27B");  // dominant seventh
  CHECK(forte(set({0, 3, 6, 9})) == "4-28");    // diminished seventh
  CHECK(forte(set({0, 2, 4, 5, 7, 9, 11})) == "7-35");  // diatonic
  CHECK(forte(set({0, 2, 4, 6, 8, 10})) == "6-35");     // whole tone
  CHECK(forte(set({0, 2, 4, 7, 9})) == "5-35");         // pentatonic
}

TEST_CASE("equivalence relations behave", "[pcset]") {
  CHECK(isTranspositionOf(set({0, 4, 7}), set({2, 6, 9})));  // C -> D major
  CHECK_FALSE(isTranspositionOf(set({0, 4, 7}), set({0, 3, 7})));
  CHECK(isInversionOf(set({0, 4, 7}), set({0, 3, 7})));  // major/minor
  CHECK(sameSetClass(set({0, 4, 7}), set({0, 3, 7})));   // Forte merges I
  CHECK_FALSE(sameSetClass(set({0, 4, 7}), set({0, 3, 6})));
  CHECK(makePitchClassSet({0, 12}).error() == QuantityError::InvalidArgument);
  CHECK(makePitchClassSet({0, -1}).error() == QuantityError::InvalidArgument);
}
