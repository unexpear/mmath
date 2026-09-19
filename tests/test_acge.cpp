// D38 — Välimäki-Liski accurate cascade graphic EQ, and the cross-check
// against D25 (Catch2).
//
// The Orfanidis band filter hits its peak gain at fc and c*g at the lower
// neighbour. With one iteration the design stays within 1 dB of the target
// at the centres AND the midpoints for all four test targets (measured
// worst 0.76 dB), matching the paper's claim. The cross-check pins what it
// revealed about D25: exact at the centres but up to 5.3 dB off between
// them (all +12 dB), and not converged on a +/-12 dB zigzag (its reported
// residual says so), where ACGE stays within 1 dB.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include <mmathen/analysis/spectrum.h>
#include <mmathen/dsp/acge.h>
#include <mmathen/dsp/equalizer.h>
#include <mmathen/dsp/filters.h>
#include <mmathen/dsp/match_eq.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}
const SampleRateHz kRate{48000.0};

std::vector<FrequencyBand> octaves() {
  return unwrap(fractionalOctaveBands(1, FrequencyHz{22.0}, FrequencyHz{17000.0}));
}
// Worst |response - target| at the centres, or at the geometric midpoints
// (target = mean of neighbours).
double worstErr(const std::vector<BiquadCoeffs>& s, const std::vector<FrequencyBand>& b,
                const std::vector<double>& t, bool mids) {
  double w = 0.0;
  for (std::size_t i = 0; i < b.size(); ++i) {
    double f = b[i].center.value(), tt = t[i];
    if (mids) {
      if (i + 1 >= b.size()) break;
      f = std::sqrt(f * b[i + 1].center.value());
      tt = 0.5 * (t[i] + t[i + 1]);
    }
    w = std::max(w, std::fabs(unwrap(cascadeResponseDb(s, FrequencyHz{f}, kRate)) - tt));
  }
  return w;
}
std::vector<BiquadCoeffs> d25Sections(const std::vector<FrequencyBand>& b,
                                      const std::vector<double>& g) {
  std::vector<BiquadCoeffs> s;
  for (std::size_t i = 0; i < b.size(); ++i)
    s.push_back(unwrap(designEqSection(
        EqBand{EqShape::Peaking, b[i].center, g[i], bandEdgeQ(b[i])}, kRate)));
  return s;
}
}  // namespace

TEST_CASE("Orfanidis band filter: peak gain and neighbour gain", "[d38acge]") {
  const auto b = octaves();
  const auto d = unwrap(designAcge(b, std::vector<double>(b.size(), 0.0), kRate));
  for (std::size_t m = 0; m < b.size(); ++m) {
    const auto k = unwrap(makeOrfanidisPeak(b[m].center, 17.0, 0.3 * 17.0, d.bandwidthRad[m], kRate));
    const double atC = 20.0 * std::log10(unwrap(transferMagnitude(k, b[m].center, kRate)));
    CHECK(std::fabs(atC - 17.0) < 1e-9);
    const double lower = m > 0 ? b[m - 1].center.value()
                               : b[0].center.value() * b[0].center.value() / b[1].center.value();
    const double atN = 20.0 * std::log10(unwrap(transferMagnitude(k, FrequencyHz{lower}, kRate)));
    CHECK(std::fabs(atN - 0.3 * 17.0) < 1e-6);
  }
  // Low bands reproduce the paper's B = 1.5 wc (no warping there).
  const double wc = 2.0 * 3.141592653589793 * b[2].center.value() / 48000.0;
  CHECK(std::fabs(d.bandwidthRad[2] / (1.5 * wc) - 1.0) < 0.01);
  // Zero gain is the identity.
  const auto id = unwrap(makeOrfanidisPeak(FrequencyHz{1000.0}, 0.0, 0.0, 0.5, kRate));
  CHECK(id.b0 == 1.0);
  CHECK(id.b1 == id.a1);
  CHECK(id.b2 == id.a2);
}

TEST_CASE("ACGE within 1 dB at centres and midpoints; cross-check D25",
          "[d38acge]") {
  const auto b = octaves();
  REQUIRE(b.size() == 10);
  const std::vector<std::vector<double>> targets = {
      std::vector<double>(10, 12.0),
      {12, -12, 12, -12, 12, -12, 12, -12, 12, -12},
      {8, 6, 3, 0, -3, -5, -3, 0, 4, 8},
      {0, 0, 0, 0, 12, 0, 0, 0, 0, 0}};
  for (const auto& t : targets) {
    const auto d = unwrap(designAcge(b, t, kRate));
    CHECK(worstErr(d.sections, b, t, false) < 1.0);  // measured <= 0.59
    CHECK(worstErr(d.sections, b, t, true) < 1.0);   // measured <= 0.76
  }
  // The iteration helps (zigzag, centres: 0.41 -> 0.25 dB measured).
  const auto& zig = targets[1];
  CHECK(worstErr(unwrap(designAcge(b, zig, kRate, 1)).sections, b, zig, false) <
        worstErr(unwrap(designAcge(b, zig, kRate, 0)).sections, b, zig, false));
  // D25 on a smooth target: exact at centres, much worse between them.
  const auto& smooth = targets[2];
  const auto sol = unwrap(solveGraphicEqGains(b, smooth, kRate, 15.0));
  const auto s25 = d25Sections(b, sol.gainsDb);
  CHECK(worstErr(s25, b, smooth, false) < 0.001);
  CHECK(worstErr(s25, b, smooth, true) > 2.0);  // measured 3.0 dB
  // D25 on all +12: the midpoint ripple reaches 5.3 dB (measured).
  const auto& up = targets[0];
  const auto s25up = d25Sections(b, unwrap(solveGraphicEqGains(b, up, kRate, 15.0)).gainsDb);
  CHECK(worstErr(s25up, b, up, true) > 4.0);
  // D25 on the zigzag does not converge, and says so.
  const auto zsol = unwrap(solveGraphicEqGains(b, zig, kRate, 15.0));
  CHECK(zsol.residualDb > 1.0);  // measured 3.0 dB, reported honestly
}

TEST_CASE("ACGE refusals", "[d38acge]") {
  const auto b = octaves();
  CHECK(designAcge(b, std::vector<double>(3, 0.0), kRate).error() ==
        QuantityError::InvalidArgument);
  CHECK(designAcge(b, std::vector<double>(10, 30.0), kRate).error() ==
        QuantityError::InvalidArgument);
  CHECK(makeOrfanidisPeak(FrequencyHz{30000.0}, 6.0, 1.8, 0.5, kRate).error() ==
        QuantityError::InvalidArgument);
}
