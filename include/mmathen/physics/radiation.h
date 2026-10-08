#pragma once

// Open-end radiation of an unflanged circular pipe (docs/48).
//
// Levine and Schwinger, Physical Review 73 (1948), the low-frequency
// end of an unflanged pipe:
//
//   Z_rad / Z_c = (1/4) (k a)^2 + j k Δ
//
// a and Δ are arguments. 0.6133 a is not stored. The pressure reflection
// of that load is (z − 1)/(z + 1). For small k a and k Δ its magnitude is
// 1 − (1/2)(k a)^2 and its phase is π − 2 k Δ: an open-end sign, a
// round-trip delay 2Δ/c, and a loss that vanishes at k = 0.
//
// The stepper is that factorization, not a fitted one-pole. The delay
// 2Δ/c is an integer line plus a first-order Thiran allpass for the
// fraction. The magnitude is the unique first-order lowpass of unity DC
// gain whose curvature matches |R|^2 = 1 − (k a)^2:
//
//   b / (1 − b)^2 = (a f_s / c)^2,
//   b = 2q / (2q + 1 + sqrt(4q + 1)),  q = (a f_s / c)^2,
//   H(z) = (1 − b) / (1 − b z^{−1}).
//
// The sample is then multiplied by −1. No coefficient is stored.

#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include "../core/result.h"
#include "../math/constants.h"
#include "../units/physical_units.h"

namespace mmathen {

struct OpenEndRatio {
  double real = 0.0;
  double imag = 0.0;
};

struct OpenEndReflectionValue {
  double real = 0.0;
  double imag = 0.0;
};

// InvalidArgument: a non-finite wave number, radius, or end correction,
// or a negative wave number, radius, or end correction.
[[nodiscard]] inline Result<OpenEndRatio, QuantityError> openEndImpedanceRatio(
    double waveNumberPerMeter, double radiusMeters, double endCorrectionMeters) {
  using R = Result<OpenEndRatio, QuantityError>;
  if (!std::isfinite(waveNumberPerMeter) || waveNumberPerMeter < 0.0 ||
      !std::isfinite(radiusMeters) || !(radiusMeters > 0.0) ||
      !std::isfinite(endCorrectionMeters) || endCorrectionMeters < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  const double ka = waveNumberPerMeter * radiusMeters;
  OpenEndRatio ratio;
  ratio.real = 0.25 * ka * ka;
  ratio.imag = waveNumberPerMeter * endCorrectionMeters;
  if (!std::isfinite(ratio.real) || !std::isfinite(ratio.imag))
    return R::failure(QuantityError::RangeError);
  return R::success(ratio);
}

// (z−1)/(z+1) by a real quotient. The denominator is |z+1|^2.
[[nodiscard]] inline Result<OpenEndReflectionValue, QuantityError>
openEndPressureReflection(OpenEndRatio load) {
  using R = Result<OpenEndReflectionValue, QuantityError>;
  if (!std::isfinite(load.real) || !std::isfinite(load.imag) || load.real < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  const double denRe = load.real + 1.0;
  const double denIm = load.imag;
  const double den = denRe * denRe + denIm * denIm;
  if (!std::isfinite(den) || !(den > 0.0))
    return R::failure(QuantityError::RangeError);
  const double numRe = load.real - 1.0;
  const double numIm = load.imag;
  OpenEndReflectionValue value;
  value.real = (numRe * denRe + numIm * denIm) / den;
  value.imag = (numIm * denRe - numRe * denIm) / den;
  if (!std::isfinite(value.real) || !std::isfinite(value.imag))
    return R::failure(QuantityError::RangeError);
  return R::success(value);
}

// Far-end reflection stepper. radius and end correction are arguments.
class OpenEndReflection {
 public:
  OpenEndReflection() = default;

  [[nodiscard]] static Result<OpenEndReflection, QuantityError> create(
      double radiusMeters, double endCorrectionMeters, double speedMetersPerSecond,
      SampleRateHz rate) {
    using R = Result<OpenEndReflection, QuantityError>;
    const double fs = rate.value();
    if (!std::isfinite(radiusMeters) || !(radiusMeters > 0.0) ||
        !std::isfinite(endCorrectionMeters) || endCorrectionMeters < 0.0 ||
        !std::isfinite(speedMetersPerSecond) || !(speedMetersPerSecond > 0.0) ||
        !std::isfinite(fs) || !(fs > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    const double q = (radiusMeters * fs / speedMetersPerSecond) *
                     (radiusMeters * fs / speedMetersPerSecond);
    if (!std::isfinite(q) || !(q > 0.0))
      return R::failure(QuantityError::RangeError);
    const double root = std::sqrt(4.0 * q + 1.0);
    const double poleDenom = 2.0 * q + 1.0 + root;
    if (!std::isfinite(root) || !std::isfinite(poleDenom) || !(poleDenom > 0.0))
      return R::failure(QuantityError::RangeError);
    const double pole = (2.0 * q) / poleDenom;
    if (!std::isfinite(pole) || !(pole > 0.0) || !(pole < 1.0))
      return R::failure(QuantityError::RangeError);
    const double delay = fs * 2.0 * endCorrectionMeters / speedMetersPerSecond;
    if (!std::isfinite(delay) || delay < 0.0 ||
        delay > static_cast<double>(std::numeric_limits<int>::max() / 4))
      return R::failure(QuantityError::RangeError);
    int integer = static_cast<int>(std::floor(delay));
    double fraction = delay - static_cast<double>(integer);
    if (fraction > 1.0 - 1.0e-9) {
      ++integer;
      fraction = 0.0;
    }
    double thiran = 0.0;
    if (fraction > 1.0e-12) {
      thiran = (1.0 - fraction) / (1.0 + fraction);
      if (!std::isfinite(thiran) || !(std::fabs(thiran) < 1.0))
        return R::failure(QuantityError::RangeError);
    }
    return R::success(OpenEndReflection{pole, thiran, fraction > 1.0e-12, integer, fs});
  }

  void reset() {
    std::fill(delay_.begin(), delay_.end(), 0.0);
    pos_ = 0;
    thX_ = 0.0;
    thY_ = 0.0;
    lowpass_ = 0.0;
  }

  // Reflected traveling pressure for one incident sample.
  [[nodiscard]] double step(double incident) {
    double x = incident;
    if (!delay_.empty()) {
      const double y = delay_[pos_];
      delay_[pos_] = x;
      pos_ = (pos_ + 1) % delay_.size();
      x = y;
    }
    if (useThiran_) {
      const double y = thiran_ * x + thX_ - thiran_ * thY_;
      thX_ = x;
      thY_ = y;
      x = y;
    }
    const double y = (1.0 - pole_) * x + pole_ * lowpass_;
    lowpass_ = y;
    return -y;
  }

  // Complex value of the reflection at this frequency, including −1.
  // The quotient is |denominator|^2 of each section. No complex division.
  [[nodiscard]] Result<OpenEndReflectionValue, QuantityError> response(
      double frequencyHz) const {
    using R = Result<OpenEndReflectionValue, QuantityError>;
    if (!std::isfinite(frequencyHz) || frequencyHz < 0.0 ||
        frequencyHz > sampleRate_ / 2.0)
      return R::failure(QuantityError::InvalidArgument);
    const double omega = constants::twoPi * frequencyHz / sampleRate_;
    const double cosine = std::cos(omega);
    const double sine = std::sin(omega);
    double re = -1.0;
    double im = 0.0;
    if (!delay_.empty()) {
      const double angle = omega * static_cast<double>(delay_.size());
      multiply(re, im, std::cos(angle), -std::sin(angle));
    }
    if (useThiran_) {
      const double numRe = thiran_ + cosine;
      const double numIm = -sine;
      const double denRe = 1.0 + thiran_ * cosine;
      const double denIm = -thiran_ * sine;
      const double den = denRe * denRe + denIm * denIm;
      if (!(den > 0.0)) return R::failure(QuantityError::RangeError);
      const double hRe = (numRe * denRe + numIm * denIm) / den;
      const double hIm = (numIm * denRe - numRe * denIm) / den;
      multiply(re, im, hRe, hIm);
    }
    const double denRe = 1.0 - pole_ * cosine;
    const double denIm = pole_ * sine;
    const double den = denRe * denRe + denIm * denIm;
    if (!(den > 0.0)) return R::failure(QuantityError::RangeError);
    const double hRe = ((1.0 - pole_) * denRe) / den;
    const double hIm = (-(1.0 - pole_) * denIm) / den;
    multiply(re, im, hRe, hIm);
    if (!std::isfinite(re) || !std::isfinite(im))
      return R::failure(QuantityError::RangeError);
    return R::success(OpenEndReflectionValue{re, im});
  }

  [[nodiscard]] double pole() const noexcept { return pole_; }
  [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }

  // Lowest frequency at which the bore loop gain is +1.
  // boreDelaySamples is the geometric round trip. openOpen adds the
  // embouchure's −1; a closed mouth does not. The search upper bound is
  // the ideal pipe with this delay and no end correction.
  [[nodiscard]] Result<double, QuantityError> resonanceHz(int boreDelaySamples,
                                                         bool openOpen) const {
    using R = Result<double, QuantityError>;
    if (boreDelaySamples < 1) return R::failure(QuantityError::InvalidArgument);
    const double upper = openOpen ? sampleRate_ / static_cast<double>(boreDelaySamples)
                                  : sampleRate_ / (2.0 * static_cast<double>(boreDelaySamples));
    if (!std::isfinite(upper) || !(upper > 0.0) || upper > sampleRate_ / 2.0)
      return R::failure(QuantityError::RangeError);
    auto gain = [&](double frequency, double& re, double& im) -> bool {
      auto reflected = response(frequency);
      if (!reflected.ok()) return false;
      re = reflected.value().real;
      im = reflected.value().imag;
      if (openOpen) multiply(re, im, -1.0, 0.0);
      const double angle =
          constants::twoPi * frequency / sampleRate_ * static_cast<double>(boreDelaySamples);
      multiply(re, im, std::cos(angle), -std::sin(angle));
      return std::isfinite(re) && std::isfinite(im);
    };
    double previous = upper / 64.0;
    double previousIm = 0.0;
    double ignoredRe = 0.0;
    if (!gain(previous, ignoredRe, previousIm))
      return R::failure(QuantityError::RangeError);
    for (int i = 2; i <= 64; ++i) {
      const double frequency = upper * static_cast<double>(i) / 64.0;
      double re = 0.0;
      double im = 0.0;
      if (!gain(frequency, re, im)) return R::failure(QuantityError::RangeError);
      if (previousIm > 0.0 && im <= 0.0) {
        double lo = previous;
        double hi = frequency;
        double loIm = previousIm;
        for (int step = 0; step < 60; ++step) {
          const double mid = 0.5 * (lo + hi);
          double midRe = 0.0;
          double midIm = 0.0;
          if (!gain(mid, midRe, midIm)) return R::failure(QuantityError::RangeError);
          if (loIm > 0.0 && midIm <= 0.0) {
            hi = mid;
          } else {
            lo = mid;
            loIm = midIm;
          }
        }
        const double root = 0.5 * (lo + hi);
        double rootRe = 0.0;
        double rootIm = 0.0;
        if (!gain(root, rootRe, rootIm)) return R::failure(QuantityError::RangeError);
        if (!(rootRe > 0.0) || !std::isfinite(root))
          return R::failure(QuantityError::DomainError);
        return R::success(root);
      }
      previous = frequency;
      previousIm = im;
    }
    return R::failure(QuantityError::DomainError);
  }

 private:
  OpenEndReflection(double pole, double thiran, bool useThiran, int integer,
                    double sampleRate)
      : pole_(pole),
        thiran_(thiran),
        useThiran_(useThiran),
        sampleRate_(sampleRate),
        delay_(static_cast<std::size_t>(integer), 0.0) {}

  static void multiply(double& re, double& im, double a, double b) {
    const double nextRe = re * a - im * b;
    const double nextIm = re * b + im * a;
    re = nextRe;
    im = nextIm;
  }

  double pole_ = 0.0;
  double thiran_ = 0.0;
  bool useThiran_ = false;
  double sampleRate_ = 0.0;
  std::vector<double> delay_;
  std::size_t pos_ = 0;
  double thX_ = 0.0;
  double thY_ = 0.0;
  double lowpass_ = 0.0;
};

// Piecewise-cylindrical Bessel horn a(x) = b (x + x0)^{−γ}, x from the
// mouth, ended by the Levine–Schwinger load of an unflanged pipe of
// mouthRadiusMeters. The return is the pressure-reflection magnitude
// at the throat. The horn's own travel stays in the caller's bore
// length: this magnitude is not a second delay. Invalid geometry or a
// non-positive frequency returns 1, a sealed lossless end.
struct BesselBellProfile {
  double lengthMeters = 0.0;
  double gamma = 0.0;
  double coefficient = 0.0;
  double mouthOffsetMeters = 0.0;
  double mouthRadiusMeters = 0.0;
};

[[nodiscard]] inline double besselBellReflectionMagnitude(BesselBellProfile bell,
                                                         double frequencyHz,
                                                         double speedMetersPerSecond) {
  if (!(bell.lengthMeters > 0.0) || !(bell.gamma > 0.0) || !(bell.coefficient > 0.0) ||
      !(bell.mouthOffsetMeters > 0.0) || !(bell.mouthRadiusMeters > 0.0) ||
      !(frequencyHz > 0.0) || !(speedMetersPerSecond > 0.0))
    return 1.0;
  constexpr int kSlices = 160;
  const double wave = constants::twoPi * frequencyHz / speedMetersPerSecond;
  if (!std::isfinite(wave) || !(wave > 0.0)) return 1.0;
  const double step = bell.lengthMeters / static_cast<double>(kSlices);
  double radius[kSlices];
  for (int i = 0; i < kSlices; ++i) {
    const double fromMouth =
        bell.lengthMeters * (1.0 - (static_cast<double>(i) + 0.5) / static_cast<double>(kSlices));
    const double arg = fromMouth + bell.mouthOffsetMeters;
    if (!(arg > 0.0)) return 1.0;
    radius[i] = bell.coefficient * std::pow(arg, -bell.gamma);
    if (!(radius[i] > 0.0) || !std::isfinite(radius[i])) return 1.0;
  }
  const double ka = wave * bell.mouthRadiusMeters;
  const double load = 0.25 * ka * ka;
  if (!std::isfinite(load) || load < 0.0) return 1.0;
  double real = (load - 1.0) / (load + 1.0);
  double imag = 0.0;
  const double theta = 2.0 * wave * step;
  const double cosine = std::cos(theta);
  const double sine = std::sin(theta);
  for (int i = kSlices - 1; i >= 0; --i) {
    const double nextReal = real * cosine + imag * sine;
    const double nextImag = imag * cosine - real * sine;
    real = nextReal;
    imag = nextImag;
    if (i == 0) break;
    const double here = radius[i] * radius[i];
    const double previous = radius[i - 1] * radius[i - 1];
    if (!(here > 0.0) || !(previous > 0.0)) return 1.0;
    const double zHere = 1.0 / here;
    const double zPrev = 1.0 / previous;
    const double dRe = 1.0 - real;
    const double dIm = -imag;
    const double den = dRe * dRe + dIm * dIm;
    if (!(den > 0.0) || !std::isfinite(den)) return 1.0;
    const double nRe = 1.0 + real;
    const double nIm = imag;
    const double qRe = (nRe * dRe + nIm * dIm) / den;
    const double qIm = (nIm * dRe - nRe * dIm) / den;
    const double inRe = zHere * qRe;
    const double inIm = zHere * qIm;
    const double aRe = inRe - zPrev;
    const double aIm = inIm;
    const double bRe = inRe + zPrev;
    const double bIm = inIm;
    const double bDen = bRe * bRe + bIm * bIm;
    if (!(bDen > 0.0) || !std::isfinite(bDen)) return 1.0;
    real = (aRe * bRe + aIm * bIm) / bDen;
    imag = (aIm * bRe - aRe * bIm) / bDen;
    if (!std::isfinite(real) || !std::isfinite(imag)) return 1.0;
  }
  const double magnitude = std::hypot(real, imag);
  if (!std::isfinite(magnitude) || !(magnitude > 0.0) || magnitude > 1.0) return 1.0;
  return magnitude;
}

}  // namespace mmathen
