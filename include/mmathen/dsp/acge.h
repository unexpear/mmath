#pragma once

// D38 — the accurate cascade graphic equalizer of Välimäki & Liski (IEEE
// Signal Processing Letters 24(2), 2017; method as restated in Liski &
// Välimäki, "The quest for the best graphic equalizer", DAFx-17, sec. 4),
// implemented as an independent cross-check of D25's gain solver.
//
// Band filter (Orfanidis peak/notch, unit gain at DC), per octave band m:
//   H(z) = (1 + G b - 2 cos(wc) z^-1 + (1 - G b) z^-2)
//        / (1 + b   - 2 cos(wc) z^-1 + (1 - b)   z^-2),
//   b = tan(B/2)                                   when G = 1,
//     = sqrt(|GB^2 - 1| / |G^2 - GB^2|) tan(B/2)   otherwise,
//   G = 10^(g/20) the peak gain, GB = 10^(c g / 20) the gain at the
//   bandwidth edges, c = 0.3 (the paper's choice).
// Bandwidth: the paper uses B = 1.5 wc (the neighbours' distance) and, for
// the three highest octaves where warping makes the response asymmetric,
// a B that puts the c*g point on the LOWER neighbour (its numbers are for
// 44.1 kHz). Here B is solved numerically by that same lower-neighbour rule
// for every band, so the design works at any rate (a stated
// generalisation; for low bands it reproduces B ~= 1.5 wc).
// Gains: an interaction matrix over 2K-1 design points (the K centres and
// the K-1 geometric midpoints), column k = the dB response of band filter k
// designed at the prototype gain g_p = 17 dB, divided by g_p. The command
// gains are the least-squares solution g = (B^T B)^-1 B^T t, t = targets at
// the centres and the mean of neighbouring targets at the midpoints; then
// one iteration: rebuild B with each filter at its solved gain and solve
// again (the paper: one iteration suffices; worst error below 1 dB).
// Measured here against D25 (see tests): ACGE is accurate to about a dB at
// centres and midpoints with one iteration; D25 is exact at the centres
// (<= 0.001 dB) but unconstrained between them.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>
#include <vector>

#include "../analysis/spectrum.h"
#include "../core/result.h"
#include "../units/physical_units.h"
#include "biquad.h"
#include "filters.h"

namespace mmathen {

// Orfanidis peaking section: peak gain gDb at fc, gbDb at the edges of
// bandwidth bRad (radians). gDb == 0 gives the identity.
[[nodiscard]] inline Result<BiquadCoeffs, QuantityError> makeOrfanidisPeak(
    FrequencyHz fc, double gDb, double gbDb, double bRad, SampleRateHz rate) {
  using R = Result<BiquadCoeffs, QuantityError>;
  const double fs = rate.value();
  const double wc = 2.0 * std::numbers::pi * fc.value() / fs;
  if (!(wc > 0.0) || !(wc < std::numbers::pi) || !(bRad > 0.0) ||
      !(bRad < std::numbers::pi) || !std::isfinite(gDb) || !std::isfinite(gbDb))
    return R::failure(QuantityError::InvalidArgument);
  const double g = std::pow(10.0, gDb / 20.0), gb = std::pow(10.0, gbDb / 20.0);
  const double t = std::tan(bRad / 2.0);
  double beta = t;
  if (gDb != 0.0) {
    const double den = std::fabs(g * g - gb * gb);
    if (!(den > 0.0)) return R::failure(QuantityError::DomainError);
    beta = std::sqrt(std::fabs(gb * gb - 1.0) / den) * t;
  }
  const double c = std::cos(wc);
  const double a0 = 1.0 + beta;
  BiquadCoeffs k{(1.0 + g * beta) / a0, -2.0 * c / a0, (1.0 - g * beta) / a0,
                 -2.0 * c / a0, (1.0 - beta) / a0};
  return R::success(k);
}

struct AcgeDesign {
  std::vector<BiquadCoeffs> sections;
  std::vector<double> gainsDb;      // command gains after the iterations
  std::vector<double> bandwidthRad;
};

namespace detail {
inline double sectionDb(const BiquadCoeffs& k, double f, SampleRateHz rate) {
  auto m = transferMagnitude(k, FrequencyHz{f}, rate);
  return m.ok() && m.value() > 0.0 ? 20.0 * std::log10(m.value()) : -300.0;
}
// Solve A x = y (small dense, partial pivoting); false if singular.
inline bool solveDense(std::vector<std::vector<double>> a, std::vector<double> y,
                       std::vector<double>& x) {
  const std::size_t n = y.size();
  for (std::size_t c = 0; c < n; ++c) {
    std::size_t p = c;
    for (std::size_t r = c + 1; r < n; ++r)
      if (std::fabs(a[r][c]) > std::fabs(a[p][c])) p = r;
    if (!(std::fabs(a[p][c]) > 1e-300)) return false;
    std::swap(a[p], a[c]);
    std::swap(y[p], y[c]);
    for (std::size_t r = c + 1; r < n; ++r) {
      const double f = a[r][c] / a[c][c];
      for (std::size_t k = c; k < n; ++k) a[r][k] -= f * a[c][k];
      y[r] -= f * y[c];
    }
  }
  x.assign(n, 0.0);
  for (std::size_t c = n; c-- > 0;) {
    double s = y[c];
    for (std::size_t k = c + 1; k < n; ++k) s -= a[c][k] * x[k];
    x[c] = s / a[c][c];
  }
  return true;
}
}  // namespace detail

// Design for K octave bands (centres from `bands`), targets in dB (|t| <=
// 24), `iterations` >= 0 extra passes (the paper: 1).
[[nodiscard]] inline Result<AcgeDesign, QuantityError> designAcge(
    const std::vector<FrequencyBand>& bands, const std::vector<double>& targetDb,
    SampleRateHz rate, int iterations = 1, double c = 0.3, double prototypeDb = 17.0) {
  using R = Result<AcgeDesign, QuantityError>;
  const std::size_t k = bands.size();
  const double fs = rate.value();
  if (k < 2 || targetDb.size() != k || iterations < 0 || iterations > 10 ||
      !(c > 0.0) || !(c < 1.0) || !(prototypeDb > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  for (double t : targetDb)
    if (!std::isfinite(t) || std::fabs(t) > 24.0) return R::failure(QuantityError::InvalidArgument);
  for (const auto& b : bands)
    if (!(b.center.value() < 0.49 * fs)) return R::failure(QuantityError::DomainError);
  AcgeDesign d;
  // Bandwidth per band: bisection on B so the prototype's response at the
  // lower neighbour equals c * g_p.
  for (std::size_t m = 0; m < k; ++m) {
    const double fc = bands[m].center.value();
    const double lower = m > 0 ? bands[m - 1].center.value()
                               : fc * fc / bands[1].center.value();
    double lo = 1e-4, hi = std::numbers::pi * 0.999;
    for (int it = 0; it < 100; ++it) {
      const double mid = 0.5 * (lo + hi);
      auto s = makeOrfanidisPeak(FrequencyHz{fc}, prototypeDb, c * prototypeDb, mid, rate);
      if (!s.ok()) return R::failure(s.error());
      if (detail::sectionDb(s.value(), lower, rate) < c * prototypeDb)
        lo = mid;  // too narrow: widen
      else
        hi = mid;
    }
    d.bandwidthRad.push_back(0.5 * (lo + hi));
  }
  // Design points: centres and geometric midpoints; targets.
  std::vector<double> pts, t;
  for (std::size_t m = 0; m < k; ++m) {
    pts.push_back(bands[m].center.value());
    t.push_back(targetDb[m]);
    if (m + 1 < k) {
      pts.push_back(std::sqrt(bands[m].center.value() * bands[m + 1].center.value()));
      t.push_back(0.5 * (targetDb[m] + targetDb[m + 1]));
    }
  }
  const std::size_t np = pts.size();
  std::vector<double> gains(k, prototypeDb);
  auto solve = [&](const std::vector<double>& designGains) -> bool {
    // Interaction matrix, normalised by each filter's design gain.
    std::vector<std::vector<double>> b(np, std::vector<double>(k));
    for (std::size_t j = 0; j < k; ++j) {
      double g = designGains[j];
      if (std::fabs(g) < 1e-3) g = prototypeDb;  // a 0 dB filter has no shape
      auto s = makeOrfanidisPeak(bands[j].center, g, c * g, d.bandwidthRad[j], rate);
      if (!s.ok()) return false;
      for (std::size_t i = 0; i < np; ++i) b[i][j] = detail::sectionDb(s.value(), pts[i], rate) / g;
    }
    std::vector<std::vector<double>> btb(k, std::vector<double>(k, 0.0));
    std::vector<double> bty(k, 0.0);
    for (std::size_t i = 0; i < np; ++i)
      for (std::size_t a = 0; a < k; ++a) {
        bty[a] += b[i][a] * t[i];
        for (std::size_t bb = 0; bb < k; ++bb) btb[a][bb] += b[i][a] * b[i][bb];
      }
    return detail::solveDense(btb, bty, gains);
  };
  if (!solve(std::vector<double>(k, prototypeDb))) return R::failure(QuantityError::RangeError);
  for (int it = 0; it < iterations; ++it) {
    const auto prev = gains;
    if (!solve(prev)) return R::failure(QuantityError::RangeError);
  }
  d.gainsDb = gains;
  for (std::size_t j = 0; j < k; ++j) {
    auto s = makeOrfanidisPeak(bands[j].center, gains[j], c * gains[j], d.bandwidthRad[j], rate);
    if (!s.ok()) return R::failure(s.error());
    d.sections.push_back(s.value());
  }
  return R::success(std::move(d));
}

}  // namespace mmathen
