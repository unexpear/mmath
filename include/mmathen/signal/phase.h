#pragma once

// Phase 2 — frequency shapes with analytic phase.
//
// Phase is always the time integral of frequency, never the naive product:
//   constant: phi(t) = phi0 + 2*pi*f*t
//   sweep:    phi(t) = phi0 + 2*pi*(f0*t + 1/2*a*t^2), a in Hz/s
//   FM:       phi(t) = phi0 + 2*pi*f0*t + (d/rm)*(1 - cos(2*pi*rm*t))
// A general numerical integrator (adaptive Simpson with tolerance) covers
// arbitrary frequency functions; the closed forms stay preferred as exact
// and free.

#include <cmath>
#include <functional>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../math/constants.h"
#include "../time/function.h"
#include "../units/dimension.h"
#include "../units/physical_units.h"
#include "../units/quantity.h"

namespace mmathen {

namespace tags {
struct FrequencySlope {};
}  // namespace tags

// Canonical representation: hertz per second.
using FrequencySlopeHzPerSec =
    Quantity<double, DimInverseTimeSquared, tags::FrequencySlope>;

struct ConstantFrequency {
  FrequencyHz f;
  [[nodiscard]] FrequencyHz frequencyAt(TimePointSeconds) const { return f; }
  [[nodiscard]] PhaseRadians phaseAt(TimePointSeconds t,
                                     PhaseRadians p0) const {
    return PhaseRadians{p0.value() + constants::twoPi * f.value() * t.value()};
  }
};

struct LinearSweep {
  FrequencyHz f0;
  FrequencySlopeHzPerSec slope;
  [[nodiscard]] FrequencyHz frequencyAt(TimePointSeconds t) const {
    return FrequencyHz{f0.value() + slope.value() * t.value()};
  }
  [[nodiscard]] PhaseRadians phaseAt(TimePointSeconds t,
                                     PhaseRadians p0) const {
    const double tt = t.value();
    return PhaseRadians{p0.value() + constants::twoPi *
                                         (f0.value() * tt +
                                          0.5 * slope.value() * tt * tt)};
  }
};

struct SinusoidalFM {
  FrequencyHz carrier;    // f0, finite
  FrequencyHz deviation;  // d, peak frequency deviation in Hz, finite
  FrequencyHz rate;       // rm, modulation rate in Hz: finite and NONZERO
  // (rm = 0 divides by zero; the resulting non-finite output is detected
  // downstream by tryAt()/render stats, never silently corrected).
  [[nodiscard]] FrequencyHz frequencyAt(TimePointSeconds t) const {
    return FrequencyHz{carrier.value() +
                       deviation.value() *
                           std::sin(constants::twoPi * rate.value() *
                                    t.value())};
  }
  [[nodiscard]] PhaseRadians phaseAt(TimePointSeconds t,
                                     PhaseRadians p0) const {
    const double tt = t.value();
    const double rm = rate.value();
    return PhaseRadians{
        p0.value() + constants::twoPi * carrier.value() * tt +
        (deviation.value() / rm) *
            (1.0 - std::cos(constants::twoPi * rm * tt))};
  }
};

namespace detail {
// Adaptive Simpson integration of g over [a,b] to absolute tolerance.
// Returns {value, converged}. Standard formulation; maxDepth bounds cost.
inline std::pair<double, bool> adaptiveSimpson(const std::function<double(double)>& g,
                                               double a, double b, double tol,
                                               int maxDepth) {
  const double m = 0.5 * (a + b);
  const double fa = g(a);
  const double fm = g(m);
  const double fb = g(b);
  if (!std::isfinite(fa) || !std::isfinite(fm) || !std::isfinite(fb))
    return {0.0, false};
  struct Frame {
    double a, m, b, fa, fm, fb, whole, tol;
    int depth;
  };
  std::vector<Frame> stack;
  const double whole = (b - a) / 6.0 * (fa + 4.0 * fm + fb);
  stack.push_back(Frame{a, m, b, fa, fm, fb, whole, tol, 0});
  double total = 0.0;
  while (!stack.empty()) {
    Frame fr = stack.back();
    stack.pop_back();
    const double lm = 0.5 * (fr.a + fr.m);
    const double rm = 0.5 * (fr.m + fr.b);
    const double flm = g(lm);
    const double frm = g(rm);
    if (!std::isfinite(flm) || !std::isfinite(frm)) return {0.0, false};
    const double left =
        (fr.m - fr.a) / 6.0 * (fr.fa + 4.0 * flm + fr.fm);
    const double right =
        (fr.b - fr.m) / 6.0 * (fr.fm + 4.0 * frm + fr.fb);
    const double delta = left + right - fr.whole;
    if (fr.depth >= maxDepth) return {0.0, false};
    if (delta == 0.0 || std::fabs(delta) <= 15.0 * fr.tol) {
      total += left + right + delta / 15.0;
    } else {
      stack.push_back(Frame{fr.a, lm, fr.m, fr.fa, flm, fr.fm,
                            left, fr.tol * 0.5, fr.depth + 1});
      stack.push_back(Frame{fr.m, rm, fr.b, fr.fm, frm, fr.fb,
                            right, fr.tol * 0.5, fr.depth + 1});
    }
  }
  return {total, true};
}
}  // namespace detail

// General phase integrator: phi = 2*pi*integral(f, t0..t1) for ARBITRARY
// frequency functions (the closed forms above stay preferred: exact and
// free). Adaptive Simpson to absolute tolerance tol on the phase integral;
// t1 < t0 and non-positive/non-finite tol are InvalidArgument; non-finite
// integrand samples or non-convergence (depth 20) are DomainError.
// Per-call cost is unbounded in the adversarial case — documented, and the
// reason closed forms remain the hot path.
[[nodiscard]] inline Result<PhaseRadians, QuantityError> integratePhase(
    const Function1D<FrequencyHz>& f, TimePointSeconds t0, TimePointSeconds t1,
    double tol) {
  using R = Result<PhaseRadians, QuantityError>;
  const double a = t0.value();
  const double b = t1.value();
  if (!std::isfinite(a) || !std::isfinite(b))
    return R::failure(QuantityError::InvalidArgument);
  if (b < a) return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(tol) || !(tol > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (b == a) return R::success(PhaseRadians{0.0});
  auto g = [&](double t) {
    return constants::twoPi * f.at(TimePointSeconds{t}).value();
  };
  const auto [value, converged] = detail::adaptiveSimpson(g, a, b, tol, 20);
  if (!converged) return R::failure(QuantityError::DomainError);
  if (!std::isfinite(value)) return R::failure(QuantityError::RangeError);
  return R::success(PhaseRadians{value});
}

}  // namespace mmathen
