#pragma once

// D6 — single-cycle wavetable oscillator with bounded interpolation error.
//
// A Wavetable is one period of a waveform sampled at N points. Harmonic
// tables are built from integer partials (spectrum known: highest harmonic
// K, levels a_k); sample tables take an arbitrary cycle (spectrum unknown,
// K conservatively floor((N-1)/2)). Harmonic tables require N > 2K so the
// table itself holds every partial (no aliasing inside the table).
//
// Lookup interpolates on unit phase, which is continuous — so unlike the
// PolyBLEP sources (frame-indexed by necessity) the oscillator IS an
// honest continuous-time Signal, reusing the analytic phase shapes of
// phase.h. Two interpolators, each with a stated error bound for harmonic
// tables, h = 2*pi/N the node spacing in radians:
//   Linear            |e| <= h^2/8     * sum a_k*k^2
//   Cubic (Lagrange,  |e| <= 3/128*h^4 * sum a_k*k^4
//     4-point, nodes -1..2; 9/16 = max |(x+1)x(x-1)(x-2)| on [0,1])
// Both follow from the polynomial interpolation remainder and are ATTAINED
// for a single sine at the interval midpoint (1-cos(h/2) and
// 1-(9/8 cos(h/2)-1/8 cos(3h/2)) expand to exactly these leading terms), so
// the tests pin them from both sides. Table rounding (~1e-16 * sum a_k) is
// below every bound used and is not included.
//
// Bandwidth contract: the table content lies at f..K*f, so the frequency
// must stay strictly below fs/(2K) (wavetableFrequencyLimit; for sweeps, the
// peak instantaneous frequency). Interpolation error is periodic in phase,
// so it can only add components at harmonics of f, each of amplitude at
// most 2*bound — stated, not yet measured (quantitative spectral proof
// awaits the FFT milestone, as for PolyBLEP). No mipmapping or crossfading
// between tables here; scoped follow-up.

#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../math/constants.h"
#include "../time/factories.h"
#include "../time/function.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"
#include "harmonics.h"
#include "phase.h"
#include "signal.h"

namespace mmathen {

enum class WavetableInterpolation { Linear, Cubic };

// Size bounds: 4 points is the minimum the cubic stencil needs; the upper
// cap rejects absurd allocations as InvalidArgument (same policy as the
// renderer's frame cap).
inline constexpr std::size_t kMinWavetableSize = 4;
inline constexpr std::size_t kMaxWavetableSize = std::size_t{1} << 24;

class Wavetable {
 public:
  // Table from integer harmonics: sample j = sum level*sin(2*pi*k*j/N).
  // Non-integer ratios (a single cycle cannot hold them), N outside
  // [kMinWavetableSize, kMaxWavetableSize], or N <= 2K are InvalidArgument.
  [[nodiscard]] static Result<Wavetable, QuantityError> fromHarmonics(
      const HarmonicSeries& series, std::size_t size) {
    using R = Result<Wavetable, QuantityError>;
    if (size < kMinWavetableSize || size > kMaxWavetableSize)
      return R::failure(QuantityError::InvalidArgument);
    std::vector<Partial> spectrum;
    std::size_t maxK = 0;
    for (const auto& h : series.partials()) {
      const double r = h.ratio.value();
      if (r != std::floor(r) || r > static_cast<double>(kMaxWavetableSize))
        return R::failure(QuantityError::InvalidArgument);
      const auto k = static_cast<std::size_t>(r);
      spectrum.push_back(Partial{k, h.level.value()});
      if (k > maxK) maxK = k;
    }
    if (size <= 2 * maxK) return R::failure(QuantityError::InvalidArgument);
    std::vector<double> samples(size, 0.0);
    const double n = static_cast<double>(size);
    for (std::size_t j = 0; j < size; ++j) {
      double v = 0.0;
      for (const auto& p : spectrum)
        v += p.level * std::sin(constants::twoPi * static_cast<double>(p.k) *
                                static_cast<double>(j) / n);
      samples[j] = v;
    }
    std::string formula = "table[N=" + std::to_string(size) +
                          ", harmonics<=" + std::to_string(maxK) + "]";
    return R::success(Wavetable{std::move(samples), std::move(spectrum), maxK,
                                true, std::move(formula)});
  }

  // Table from one arbitrary cycle. Spectrum unknown: maxHarmonic() is the
  // conservative floor((N-1)/2) and no interpolation bound is available.
  [[nodiscard]] static Result<Wavetable, QuantityError> fromSamples(
      std::vector<double> cycle) {
    using R = Result<Wavetable, QuantityError>;
    if (cycle.size() < kMinWavetableSize || cycle.size() > kMaxWavetableSize)
      return R::failure(QuantityError::InvalidArgument);
    for (const double v : cycle)
      if (!std::isfinite(v)) return R::failure(QuantityError::InvalidArgument);
    // Count before the move: argument evaluation order is unspecified.
    const std::size_t count = cycle.size();
    return R::success(Wavetable{std::move(cycle), {}, (count - 1) / 2, false,
                                "table[N=" + std::to_string(count) +
                                    ", samples]"});
  }

  [[nodiscard]] const std::vector<double>& samples() const noexcept {
    return samples_;
  }
  [[nodiscard]] std::size_t size() const noexcept { return samples_.size(); }
  [[nodiscard]] std::size_t maxHarmonic() const noexcept { return maxK_; }
  [[nodiscard]] bool spectrumKnown() const noexcept { return known_; }
  [[nodiscard]] const std::string& formula() const noexcept {
    return formula_;
  }

  // Sum of level*k^power over the known spectrum (bound ingredients).
  [[nodiscard]] double weightedHarmonicSum(int power) const {
    double s = 0.0;
    for (const auto& p : spectrum_)
      s += p.level * std::pow(static_cast<double>(p.k), power);
    return s;
  }

 private:
  struct Partial {
    std::size_t k;
    double level;
  };
  Wavetable(std::vector<double> samples, std::vector<Partial> spectrum,
            std::size_t maxK, bool known, std::string formula)
      : samples_(std::move(samples)),
        spectrum_(std::move(spectrum)),
        maxK_(maxK),
        known_(known),
        formula_(std::move(formula)) {}

  std::vector<double> samples_;
  std::vector<Partial> spectrum_;
  std::size_t maxK_;
  bool known_;
  std::string formula_;
};

namespace detail {
// Unchecked lookup; phase01 must lie in [0, 1).
inline double wavetableLookup(const std::vector<double>& s, double phase01,
                              WavetableInterpolation interp) {
  const std::size_t n = s.size();
  const double pos = phase01 * static_cast<double>(n);
  auto i = static_cast<std::size_t>(pos);
  double x = pos - static_cast<double>(i);
  if (i >= n) {  // phase01 just below 1 can round pos up to n
    i = 0;
    x = 0.0;
  }
  const double y0 = s[i];
  const double y1 = s[(i + 1) % n];
  if (interp == WavetableInterpolation::Linear) return y0 + x * (y1 - y0);
  const double ym = s[(i + n - 1) % n];
  const double y2 = s[(i + 2) % n];
  // Lagrange weights for nodes -1, 0, 1, 2 evaluated at x in [0, 1).
  const double wm = -x * (x - 1.0) * (x - 2.0) / 6.0;
  const double w0 = (x + 1.0) * (x - 1.0) * (x - 2.0) / 2.0;
  const double w1 = -(x + 1.0) * x * (x - 2.0) / 2.0;
  const double w2 = (x + 1.0) * x * (x - 1.0) / 6.0;
  return wm * ym + w0 * y0 + w1 * y1 + w2 * y2;
}

inline const char* interpolationName(WavetableInterpolation interp) {
  return interp == WavetableInterpolation::Linear ? "linear" : "cubic";
}

template <typename Shape>
[[nodiscard]] inline Signal<SignedAudioValue> wavetableFromShape(
    Wavetable table, Shape shape, Function1D<AmplitudeLinear> amp,
    PhaseRadians phase0, WavetableInterpolation interp, const char* kind) {
  std::string formula = std::string("wavetable_") + kind + "(" +
                        table.formula() + ", " + interpolationName(interp) +
                        ")";
  return Signal<SignedAudioValue>{wrap<SignedAudioValue>(
      [table = std::move(table), shape, amp = std::move(amp), phase0,
       interp](TimePointSeconds t) {
        const double a = amp.at(t).value();
        double p = shape.phaseAt(t, phase0).value() / constants::twoPi;
        p -= std::floor(p);
        // Non-finite phase propagates as NaN for tryAt()/render stats to
        // flag, never silently corrected.
        if (!std::isfinite(p)) return SignedAudioValue{p};
        if (p >= 1.0) p = 0.0;  // -tiny - floor(-tiny) rounds to 1.0
        return SignedAudioValue{
            a * wavetableLookup(table.samples(), p, interp)};
      },
      std::move(formula))};
}
}  // namespace detail

// Checked lookup at unit phase in [0, 1); anything else is InvalidArgument.
[[nodiscard]] inline Result<double, QuantityError> wavetableValue(
    const Wavetable& table, double phase01, WavetableInterpolation interp) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(phase01) || phase01 < 0.0 || phase01 >= 1.0)
    return R::failure(QuantityError::InvalidArgument);
  return R::success(detail::wavetableLookup(table.samples(), phase01, interp));
}

// Worst-case interpolation error for a harmonic table (formulas above).
// Sample tables have no known spectrum: DomainError.
[[nodiscard]] inline Result<double, QuantityError> interpolationErrorBound(
    const Wavetable& table, WavetableInterpolation interp) {
  using R = Result<double, QuantityError>;
  if (!table.spectrumKnown()) return R::failure(QuantityError::DomainError);
  const double h = constants::twoPi / static_cast<double>(table.size());
  const double bound =
      interp == WavetableInterpolation::Linear
          ? h * h / 8.0 * table.weightedHarmonicSum(2)
          : 3.0 / 128.0 * h * h * h * h * table.weightedHarmonicSum(4);
  if (!std::isfinite(bound)) return R::failure(QuantityError::RangeError);
  return R::success(bound);
}

// Exclusive upper frequency limit fs/(2K): at or above it the table's
// highest harmonic reaches Nyquist.
[[nodiscard]] inline Result<FrequencyHz, QuantityError>
wavetableFrequencyLimit(const Wavetable& table, SampleRateHz rate) {
  using R = Result<FrequencyHz, QuantityError>;
  const double fs = rate.value();
  if (!std::isfinite(fs) || !(fs > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double k =
      static_cast<double>(table.maxHarmonic() > 0 ? table.maxHarmonic() : 1);
  return R::success(FrequencyHz{fs / (2.0 * k)});
}

// Constant-frequency wavetable oscillator: A(t) * table(frac(phi(t)/2pi)).
[[nodiscard]] inline Result<Signal<SignedAudioValue>, QuantityError>
wavetableTone(Wavetable table, FrequencyHz f,
              Function1D<AmplitudeLinear> amp = constant(AmplitudeLinear{1.0}),
              PhaseRadians phase0 = PhaseRadians{0.0},
              WavetableInterpolation interp = WavetableInterpolation::Cubic) {
  using R = Result<Signal<SignedAudioValue>, QuantityError>;
  if (!std::isfinite(f.value()) || !(f.value() > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (!std::isfinite(phase0.value()))
    return R::failure(QuantityError::InvalidArgument);
  return R::success(detail::wavetableFromShape(
      std::move(table), ConstantFrequency{f}, std::move(amp), phase0, interp,
      "tone"));
}

// Linear-sweep wavetable oscillator: f(t) = f0 + slope*t, analytic phase.
[[nodiscard]] inline Result<Signal<SignedAudioValue>, QuantityError>
wavetableSweep(Wavetable table, FrequencyHz f0, FrequencySlopeHzPerSec slope,
               Function1D<AmplitudeLinear> amp = constant(AmplitudeLinear{1.0}),
               PhaseRadians phase0 = PhaseRadians{0.0},
               WavetableInterpolation interp = WavetableInterpolation::Cubic) {
  using R = Result<Signal<SignedAudioValue>, QuantityError>;
  if (!std::isfinite(f0.value()) || !std::isfinite(slope.value()) ||
      !std::isfinite(phase0.value()))
    return R::failure(QuantityError::InvalidArgument);
  return R::success(detail::wavetableFromShape(
      std::move(table), LinearSweep{f0, slope}, std::move(amp), phase0, interp,
      "sweep"));
}

}  // namespace mmathen
