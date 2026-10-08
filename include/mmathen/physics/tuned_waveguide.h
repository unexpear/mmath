#pragma once

// D14/D15 — tuned, lossy, dispersive digital waveguide string.
//
// D8's Waveguide1D is exact but rigid: its pitch is whatever integer loop
// length the grid allows, and it cannot represent loss or stiffness. This
// waveguide keeps D8's two rails of M intervals (dt = 1/fs) and adds a
// filter chain F(z) where the right-going wave reflects at the bridge end:
//   F(z) = T(z) * D(z)^K * H(z)
//   T(z) = (a + z^-1)/(1 + a z^-1)   first-order Thiran allpass: fractional
//                                    delay d, a = (1-d)/(1+d)  [fine tuning]
//   D(z) = (q + z^-1)/(1 + q z^-1)   K identical allpasses, q in (-1, 0)
//                                    [stiffness dispersion]
//   H(z) = g (1-b)/(1 - b z^-1)      one-pole lowpass, 0 <= b < 1, g <= 1
//                                    [frequency-dependent loss]
// Both ends reflect with -1 (a string: Fixed-Fixed), so the loop is
// G(z) = F(z) z^{-2M} and every partial is a root of 1 - F(z) z^{-2M} = 0.
//
// THE LOOP'S OWN PARTIALS ARE COMPUTED EXACTLY: loopPole(k) solves that
// characteristic equation by complex Newton from the lossless phase
// condition 2M w - arg F(e^{iw}) = 2 pi k, giving each partial's frequency
// arg(z) fs/2pi and decay -ln|z| fs. FFT measurements of the rendered
// waveguide are checked against these poles. Separately, the DESIGN tries
// to make those poles match the continuous model (wave1d.h):
//   - fundamental: exact by construction (Thiran delay solved at w_1),
//   - loss: two chosen partials' decay rates exact (b, g solved against the
//     exact poles by fixed-point iteration),
//   - stiffness: q chosen by search to minimise the worst partial cents
//     error over the fitted partials (an approximation; residual reported).
// The design report lists, per fitted partial, the remaining cents and
// decay error against the model, so accuracy is stated rather than implied.
//
// Scope: hinged or non-stiff Fixed-Fixed strings (closed-form model
// partials to fit). Filters are lumped at one termination, so pickup nodes
// are positions on the M-interval rail grid, not exact physical points of
// the tuned length. Stateful processor: allocation-free step, exact reset.

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../math/constants.h"
#include "../units/physical_units.h"
#include "fdtd1d.h"
#include "wave1d.h"

namespace mmathen {

struct LoopFilters {
  std::size_t intervals = 0;  // M (loop = 2M samples + filters)
  double thiran = 0.0;        // a
  int dispersionStages = 0;   // K
  double dispersion = 0.0;    // q
  double lossGain = 1.0;      // g
  double lossPole = 0.0;      // b
};

struct LoopPartial {
  int n;
  FrequencyHz frequency;  // exact loop pole
  double decayRate;       // exact loop pole, 1/s
  double centsError;      // vs continuous model partial n
  double decayError;      // relative, vs model (0 when the model is lossless)
};

namespace detail {
using cplx = std::complex<double>;

inline cplx loopFilterResponse(const LoopFilters& f, cplx zinv) {
  cplx h = (f.thiran + zinv) / (1.0 + f.thiran * zinv);
  for (int i = 0; i < f.dispersionStages; ++i)
    h *= (f.dispersion + zinv) / (1.0 + f.dispersion * zinv);
  h *= f.lossGain * (1.0 - f.lossPole) / (1.0 - f.lossPole * zinv);
  return h;
}

// d/dz ln F(z).
inline cplx loopFilterLogDerivative(const LoopFilters& f, cplx z) {
  const cplx zinv = 1.0 / z;
  const cplx zinv2 = zinv * zinv;
  auto allpass = [&](double a) {
    return -zinv2 / (a + zinv) + a * zinv2 / (1.0 + a * zinv);
  };
  cplx d = allpass(f.thiran);
  for (int i = 0; i < f.dispersionStages; ++i) d += allpass(f.dispersion);
  d += -f.lossPole * zinv2 / (1.0 - f.lossPole * zinv);
  return d;
}

// Lossless-phase estimate of partial k: solve 2M w - arg_unwrapped F = 2 pi k
// by bisection on w in (0, pi), using the continuous phase of F along the
// unit circle (sum of section phases, each continuous for |coef| < 1).
inline double loopPhaseFrequency(const LoopFilters& f, int k) {
  const auto m2 = static_cast<double>(2 * f.intervals);
  auto sectionPhase = [](double a, double w) {
    // arg of (a + e^{-iw})/(1 + a e^{-iw}) = -w + 2 atan(a sin w/(1 + a cos w))
    // for |a| < 1 (continuous in w).
    return -w + 2.0 * std::atan2(a * std::sin(w), 1.0 + a * std::cos(w));
  };
  auto phase = [&](double w) {
    double ph = sectionPhase(f.thiran, w);
    for (int i = 0; i < f.dispersionStages; ++i)
      ph += sectionPhase(f.dispersion, w);
    ph += -std::atan2(f.lossPole * std::sin(w), 1.0 - f.lossPole * std::cos(w));
    return m2 * w - ph;  // total phase lag, increasing in w
  };
  const double target = constants::twoPi * static_cast<double>(k);
  double lo = 1e-12;
  double hi = constants::pi;
  if (phase(hi) < target) return -1.0;
  for (int it = 0; it < 200; ++it) {
    const double mid = 0.5 * (lo + hi);
    if (!(mid > lo) || !(mid < hi)) break;
    if (phase(mid) < target)
      lo = mid;
    else
      hi = mid;
  }
  return 0.5 * (lo + hi);
}

// Exact loop pole of partial k (complex Newton on 1 - F(z) z^{-2M}).
inline bool loopPoleZ(const LoopFilters& f, int k, cplx& z) {
  const double w0 = loopPhaseFrequency(f, k);
  if (!(w0 > 0.0)) return false;
  const auto m2 = static_cast<double>(2 * f.intervals);
  const double mag0 = std::abs(loopFilterResponse(f, cplx{std::cos(w0), -std::sin(w0)}));
  z = std::polar(std::pow(mag0, 1.0 / m2), w0);
  for (int it = 0; it < 100; ++it) {
    const cplx fz = loopFilterResponse(f, 1.0 / z);
    const cplx zm = std::exp(-m2 * std::log(z));
    const cplx g = 1.0 - fz * zm;
    const cplx dg = -fz * zm * (loopFilterLogDerivative(f, z) - m2 / z);
    const cplx step = g / dg;
    z -= step;
    if (std::abs(step) < 1e-15 * std::abs(z)) break;
  }
  const cplx residual =
      1.0 - loopFilterResponse(f, 1.0 / z) * std::exp(-m2 * std::log(z));
  return std::abs(residual) < 1e-9 && std::fabs(std::arg(z) - w0) < 0.5 *
                                          constants::pi / m2;
}
}  // namespace detail

// Exact frequency and decay of loop partial k >= 1 at sample rate fs.
[[nodiscard]] inline Result<LoopPartial, QuantityError> loopPole(
    const LoopFilters& filters, SampleRateHz rate, int k) {
  using R = Result<LoopPartial, QuantityError>;
  const double fs = rate.value();
  if (k < 1 || !std::isfinite(fs) || !(fs > 0.0) || filters.intervals < 2)
    return R::failure(QuantityError::InvalidArgument);
  detail::cplx z;
  if (!detail::loopPoleZ(filters, k, z))
    return R::failure(QuantityError::DomainError);
  return R::success(LoopPartial{k,
                                FrequencyHz{std::arg(z) * fs / constants::twoPi},
                                -std::log(std::abs(z)) * fs, 0.0, 0.0});
}

struct LoopDesign {
  LoopFilters filters;
  std::vector<LoopPartial> partials;  // fitted partials with model errors
  double worstCents = 0.0;
  double worstDecayError = 0.0;
};

namespace detail {
// Solve the Thiran coefficient (and M) so partial 1's exact pole frequency
// equals w1 (rad/sample). Returns false if no d in [0.25, 1.75] works.
inline bool tuneFundamental(LoopFilters& f, double w1) {
  // Phase delay (samples) of every section except the Thiran, at w1.
  auto restDelay = [&]() {
    double ph = 0.0;
    for (int i = 0; i < f.dispersionStages; ++i)
      ph += -w1 + 2.0 * std::atan2(f.dispersion * std::sin(w1),
                                   1.0 + f.dispersion * std::cos(w1));
    ph += -std::atan2(f.lossPole * std::sin(w1), 1.0 - f.lossPole * std::cos(w1));
    return -ph / w1;
  };
  const double needed = constants::twoPi / w1 - restDelay();
  const double mReal = std::floor((needed - 0.75) / 2.0);
  if (!(mReal >= 2.0)) return false;
  f.intervals = static_cast<std::size_t>(mReal);
  // Solve Thiran phase delay = needed - 2M at w1 by bisection on a.
  const double want = needed - 2.0 * mReal;
  auto thiranDelay = [&](double a) {
    return -(-w1 + 2.0 * std::atan2(a * std::sin(w1), 1.0 + a * std::cos(w1))) /
           w1;
  };
  // Delay decreases as a increases (a in (-1, 1)).
  double lo = -0.95, hi = 0.95;
  if (!(thiranDelay(lo) >= want && thiranDelay(hi) <= want)) return false;
  for (int it = 0; it < 200; ++it) {
    const double mid = 0.5 * (lo + hi);
    if (!(mid > lo) || !(mid < hi)) break;
    if (thiranDelay(mid) > want)
      lo = mid;
    else
      hi = mid;
  }
  f.thiran = 0.5 * (lo + hi);
  return true;
}
}  // namespace detail

// Design the loop for a Fixed-Fixed (hinged if stiff) string at rate fs,
// fitting partials 1..fitPartials (2..64). dispersionStages 0..16 (0 means no
// dispersion filter; must be >= 1 when the system is stiff). Loss (if any)
// is matched exactly at partials 1 and fitPartials. InvalidArgument for bad
// counts/rate or a non-string system; DomainError if no stable design
// exists (e.g. the loop would be shorter than 2 intervals).
[[nodiscard]] inline Result<LoopDesign, QuantityError> designTunedLoop(
    const WaveSystem1D& system, SampleRateHz rate, int fitPartials,
    int dispersionStages) {
  using R = Result<LoopDesign, QuantityError>;
  const double fs = rate.value();
  if (!std::isfinite(fs) || !(fs > 0.0) || fitPartials < 2 ||
      fitPartials > 64 || dispersionStages < 0 || dispersionStages > 16)
    return R::failure(QuantityError::InvalidArgument);
  if (system.left != Boundary::Fixed || system.right != Boundary::Fixed ||
      !detail::closedFormModes(system))
    return R::failure(QuantityError::InvalidArgument);
  const bool stiff = system.stiffness > 0.0;
  if (stiff != (dispersionStages > 0))
    return R::failure(QuantityError::InvalidArgument);
  std::vector<double> fModel(static_cast<std::size_t>(fitPartials));
  std::vector<double> sModel(static_cast<std::size_t>(fitPartials));
  for (int n = 1; n <= fitPartials; ++n) {
    auto f = modeFrequency(system, n);
    auto s = modeDecayRate(system, n);
    if (!f.ok() || !s.ok()) return R::failure(QuantityError::DomainError);
    if (!(f.value().value() < fs / 2.0))
      return R::failure(QuantityError::DomainError);
    fModel[static_cast<std::size_t>(n - 1)] = f.value().value();
    sModel[static_cast<std::size_t>(n - 1)] = s.value();
  }
  const bool lossy = !system.loss.lossless();
  const double w1 = constants::twoPi * fModel[0] / fs;

  // Build a design for a given dispersion coefficient q: tune, fit loss,
  // retune (loss filter phase shifts the pitch), repeat.
  auto build = [&](double q, LoopFilters& f) {
    f = LoopFilters{};
    f.dispersionStages = dispersionStages;
    f.dispersion = q;
    for (int round = 0; round < 12; ++round) {
      if (!detail::tuneFundamental(f, w1)) return false;
      if (!lossy) return true;
      // Fixed-point: adjust per-round-trip magnitudes so the exact poles'
      // decay at partials 1 and P match the model.
      const std::size_t last = static_cast<std::size_t>(fitPartials - 1);
      auto poleDecay = [&](int k) {
        detail::cplx z;
        if (!detail::loopPoleZ(f, k, z)) return -1.0;
        return -std::log(std::abs(z)) * fs;
      };
      double s1 = poleDecay(1);
      double sp = poleDecay(fitPartials);
      if (!(s1 >= 0.0) || !(sp >= 0.0)) return false;
      // Desired filter magnitudes scaled by the ratio of targets to actual.
      const double wP = constants::twoPi * fModel[last] / fs;
      auto mag = [&](double w) {
        const detail::cplx e{std::cos(w), -std::sin(w)};
        return std::abs(f.lossGain * (1.0 - f.lossPole) / (1.0 - f.lossPole * e));
      };
      const double loop1 = constants::twoPi / w1;
      const double loopP = constants::twoPi * static_cast<double>(fitPartials) / wP;
      double r1 = mag(w1) * std::exp(-(sModel[0] - s1) * loop1 / fs);
      double rp = mag(wP) * std::exp(-(sModel[last] - sp) * loopP / fs);
      if (round == 0) {
        r1 = std::exp(-sModel[0] * loop1 / fs);
        rp = std::exp(-sModel[last] * loopP / fs);
      }
      // One-pole magnitudes: |H|^2 = g^2 (1-b)^2 / (1 - 2b cos w + b^2).
      // r1^2 (1 - 2b c1 + b^2) = rp^2 (1 - 2b cp + b^2)  ->  quadratic in b.
      const double c1 = std::cos(w1);
      const double cp = std::cos(wP);
      const double A = r1 * r1 - rp * rp;
      const double Bq = -2.0 * (r1 * r1 * c1 - rp * rp * cp);
      const double C = r1 * r1 - rp * rp;
      double b = 0.0;
      if (std::fabs(A) < 1e-300) {
        b = 0.0;
      } else {
        const double disc = Bq * Bq - 4.0 * A * C;
        if (disc < 0.0) return false;
        const double b1 = (-Bq - std::sqrt(disc)) / (2.0 * A);
        const double b2 = (-Bq + std::sqrt(disc)) / (2.0 * A);
        b = (b1 >= 0.0 && b1 < 1.0) ? b1 : b2;
      }
      if (!(b >= 0.0) || !(b < 1.0)) return false;
      const double g = r1 * std::sqrt(1.0 - 2.0 * b * c1 + b * b) / (1.0 - b);
      if (!(g > 0.0) || g > 1.0) return false;
      f.lossPole = b;
      f.lossGain = g;
    }
    if (!detail::tuneFundamental(f, w1)) return false;
    if (!lossy) return true;
    // With loss the exact pole sits slightly off the lossless phase
    // condition: correct the tuning target until the EXACT pole of partial 1
    // lands on w1 (a first version stopped at the phase condition and left
    // the lossy fundamental 6e-4 cents off).
    double aim = w1;
    for (int it = 0; it < 20; ++it) {
      detail::cplx z;
      if (!detail::loopPoleZ(f, 1, z)) return false;
      const double got = std::arg(z);
      if (std::fabs(got - w1) < 1e-15 * w1) break;
      aim *= w1 / got;
      if (!detail::tuneFundamental(f, aim)) return false;
    }
    return true;
  };

  auto evaluate = [&](const LoopFilters& f, LoopDesign& d) {
    d.filters = f;
    d.partials.clear();
    d.worstCents = 0.0;
    d.worstDecayError = 0.0;
    for (int n = 1; n <= fitPartials; ++n) {
      auto pole = loopPole(f, rate, n);
      if (!pole.ok()) return false;
      LoopPartial p = pole.value();
      const auto i = static_cast<std::size_t>(n - 1);
      p.centsError = 1200.0 * std::log2(p.frequency.value() / fModel[i]);
      p.decayError = sModel[i] > 0.0 ? p.decayRate / sModel[i] - 1.0
                                     : p.decayRate;
      d.worstCents = (std::max)(d.worstCents, std::fabs(p.centsError));
      d.worstDecayError =
          (std::max)(d.worstDecayError, std::fabs(p.decayError));
      d.partials.push_back(p);
    }
    return true;
  };

  LoopDesign best;
  bool found = false;
  if (!stiff) {
    LoopFilters f;
    if (build(0.0, f) && evaluate(f, best)) found = true;
  } else {
    // Coarse scan of q, then golden refinement of the best bracket.
    double bestQ = 0.0;
    double bestCost = 1e300;
    for (int i = 1; i < 100; ++i) {
      const double q = -0.99 * static_cast<double>(i) / 100.0;
      LoopFilters f;
      LoopDesign d;
      if (build(q, f) && evaluate(f, d) && d.worstCents < bestCost) {
        bestCost = d.worstCents;
        bestQ = q;
        best = d;
        found = true;
      }
    }
    if (found) {
      double lo = bestQ - 0.0099, hi = bestQ + 0.0099;
      lo = (std::max)(lo, -0.995);
      hi = (std::min)(hi, -1e-6);
      const double invPhi = (std::sqrt(5.0) - 1.0) / 2.0;
      auto cost = [&](double q, LoopDesign& d) {
        LoopFilters f;
        if (!build(q, f) || !evaluate(f, d)) return 1e300;
        return d.worstCents;
      };
      for (int it = 0; it < 40; ++it) {
        const double c = hi - invPhi * (hi - lo);
        const double e = lo + invPhi * (hi - lo);
        LoopDesign dc, de;
        const double vc = cost(c, dc);
        const double ve = cost(e, de);
        if (vc < best.worstCents) best = dc;
        if (ve < best.worstCents) best = de;
        if (vc < ve)
          hi = e;
        else
          lo = c;
      }
    }
  }
  if (!found) return R::failure(QuantityError::DomainError);
  return R::success(std::move(best));
}

class TunedWaveguide {
 public:
  // shape's system must match the design's (Fixed-Fixed string); the rail
  // grid is the design's M intervals over the string length.
  [[nodiscard]] static Result<TunedWaveguide, QuantityError> create(
      const InitialShape& shape, const LoopFilters& filters) {
    using R = Result<TunedWaveguide, QuantityError>;
    const WaveSystem1D& sys = shape.system();
    if (sys.left != Boundary::Fixed || sys.right != Boundary::Fixed ||
        filters.intervals < 2 || filters.intervals > kMaxGridIntervals ||
        !(std::fabs(filters.thiran) < 1.0) ||
        !(std::fabs(filters.dispersion) < 1.0) || !(filters.lossPole >= 0.0) ||
        !(filters.lossPole < 1.0) || !(filters.lossGain > 0.0) ||
        filters.lossGain > 1.0 || filters.dispersionStages < 0 ||
        filters.dispersionStages > 16)
      return R::failure(QuantityError::InvalidArgument);
    return R::success(TunedWaveguide{shape, filters});
  }

  void step() {
    const std::size_t m = right_.size() - 1;
    for (std::size_t j = m; j > 0; --j) right_[j] = right_[j - 1];
    for (std::size_t j = 0; j < m; ++j) left_[j] = left_[j + 1];
    right_[0] = -left_[0];
    // Bridge end: filter the arriving right-going sample, reflect with -1.
    double x = right_[m];
    // Thiran: y = a x + x1 - a y1
    {
      const double y = f_.thiran * x + thX_ - f_.thiran * thY_;
      thX_ = x;
      thY_ = y;
      x = y;
    }
    for (int i = 0; i < f_.dispersionStages; ++i) {
      const auto ii = static_cast<std::size_t>(i);
      const double y = f_.dispersion * x + dsX_[ii] - f_.dispersion * dsY_[ii];
      dsX_[ii] = x;
      dsY_[ii] = y;
      x = y;
    }
    {
      const double y = f_.lossGain * (1.0 - f_.lossPole) * x + f_.lossPole * lsY_;
      lsY_ = y;
      x = y;
    }
    left_[m] = -x;
    ++steps_;
  }

  // Same rail shift and bridge filter as step(), but the bridge-end
  // displacement is the caller's metres instead of a rigid zero.
  // Zero reproduces step()'s reflection. Returns the filtered arrival.
  [[nodiscard]] double stepYieldingBridge(double endDisplacement) {
    const std::size_t m = right_.size() - 1;
    for (std::size_t j = m; j > 0; --j) right_[j] = right_[j - 1];
    for (std::size_t j = 0; j < m; ++j) left_[j] = left_[j + 1];
    right_[0] = -left_[0];
    double x = right_[m];
    {
      const double y = f_.thiran * x + thX_ - f_.thiran * thY_;
      thX_ = x;
      thY_ = y;
      x = y;
    }
    for (int i = 0; i < f_.dispersionStages; ++i) {
      const auto ii = static_cast<std::size_t>(i);
      const double y = f_.dispersion * x + dsX_[ii] - f_.dispersion * dsY_[ii];
      dsX_[ii] = x;
      dsY_[ii] = y;
      x = y;
    }
    {
      const double y = f_.lossGain * (1.0 - f_.lossPole) * x + f_.lossPole * lsY_;
      lsY_ = y;
      x = y;
    }
    left_[m] = endDisplacement - x;
    ++steps_;
    return x;
  }

  void reset() {
    right_ = initialRail_;
    left_ = initialRail_;
    thX_ = thY_ = lsY_ = 0.0;
    std::fill(dsX_.begin(), dsX_.end(), 0.0);
    std::fill(dsY_.begin(), dsY_.end(), 0.0);
    steps_ = 0;
  }

  [[nodiscard]] Result<double, QuantityError> valueAt(std::size_t node) const {
    using R = Result<double, QuantityError>;
    if (node >= right_.size()) return R::failure(QuantityError::InvalidArgument);
    return R::success(right_[node] + left_[node]);
  }
  [[nodiscard]] std::uint64_t steps() const noexcept { return steps_; }
  [[nodiscard]] const LoopFilters& filters() const noexcept { return f_; }

 private:
  TunedWaveguide(const InitialShape& shape, const LoopFilters& filters)
      : f_(filters) {
    const std::size_t m = filters.intervals;
    const double l = shape.system().length.value();
    const double dx = l / static_cast<double>(m);
    initialRail_.resize(m + 1);
    for (std::size_t j = 0; j <= m; ++j)
      initialRail_[j] =
          0.5 * shape.valueAt(j == m ? l : static_cast<double>(j) * dx);
    right_ = initialRail_;
    left_ = initialRail_;
    dsX_.assign(static_cast<std::size_t>(filters.dispersionStages), 0.0);
    dsY_.assign(static_cast<std::size_t>(filters.dispersionStages), 0.0);
  }

  LoopFilters f_;
  std::vector<double> initialRail_;
  std::vector<double> right_;
  std::vector<double> left_;
  double thX_ = 0.0, thY_ = 0.0, lsY_ = 0.0;
  std::vector<double> dsX_, dsY_;
  std::uint64_t steps_ = 0;
};

}  // namespace mmathen
