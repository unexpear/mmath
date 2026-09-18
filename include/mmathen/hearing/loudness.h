#pragma once

// D18 — loudness: ISO 226:2023 equal-loudness-level contours, sones, SPL.
//
// A phon value is MEANINGLESS without frequency: 60 dB SPL at 1 kHz is 60
// phon; 60 dB at 100 Hz is roughly 50 phon. This module converts both
// directions through the standard's analytical formulation (Formulas 1, 2):
//   Lf = (10/af) lg{(4e-10)^(ar-af) [10^(ar LN/10) - 10^(ar Tr/10)]
//        + 10^(af (Tf+LU)/10)} - LU
//   LN = (10/ar) lg{[10^(af (Lf+LU)/10) - 10^(af (Tf+LU)/10)]
//        / (4e-10)^(ar-af) + 10^(ar Tr/10)}
// with ar = 0.300, Tr = 2.4 dB, and per-frequency (af, LU, Tf) from the
// standard's 29 third-octave rows (20 Hz..12.5 kHz), log-frequency
// interpolated between rows. PROVENANCE: the ISO primary is paywalled;
// table transcribed from an open implementation of the standard, with an
// independent second transcription checked mechanically (see D18 record).
// Fletcher-Munson (1933) and Robinson-Dadson (1956) curves are SUPERSEDED
// and are not used anywhere here.
// Validated scope (standard's stated scope, enforced): f in [20, 12500] Hz;
// phon in [20, 90] (f <= 4000 Hz) or [20, 80] (above); below 20 phon the
// standard is informative-only, so we refuse (InvalidArgument) rather than
// compute unclaimed values. SPL inputs below the 20-phon contour have no
// defined phon: DomainError.
// SPL values use Decibels (reference 20 uPa stated at conversion sites).

#include <array>
#include <cmath>
#include <cstddef>
#include <utility>

#include "../core/result.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"
#include "../units/quantity.h"

namespace mmathen {

inline constexpr double kSplReferencePa = 20e-6;
inline constexpr double kIso226AlphaR = 0.300;
inline constexpr double kIso226ThresholdRefDb = 2.4;

namespace detail {
struct Iso226Row {
  double frequency;
  double alpha;
  double transfer;
  double threshold;
};
// Table 1, ISO 226:2023 (see header provenance note).
inline constexpr Iso226Row kIso226Table[] = {
    {20.0, 0.635, -31.5, 78.1},     {25.0, 0.602, -27.2, 68.7},
    {31.5, 0.569, -23.1, 59.5},     {40.0, 0.537, -19.3, 51.1},
    {50.0, 0.509, -16.1, 44.0},     {63.0, 0.482, -13.1, 37.5},
    {80.0, 0.456, -10.4, 31.5},     {100.0, 0.433, -8.2, 26.5},
    {125.0, 0.412, -6.3, 22.1},     {160.0, 0.391, -4.6, 17.9},
    {200.0, 0.373, -3.2, 14.4},     {250.0, 0.357, -2.1, 11.4},
    {315.0, 0.343, -1.2, 8.6},      {400.0, 0.330, -0.5, 6.2},
    {500.0, 0.320, 0.0, 4.4},       {630.0, 0.311, 0.4, 3.0},
    {800.0, 0.303, 0.5, 2.2},       {1000.0, 0.300, 0.0, 2.4},
    {1250.0, 0.295, -2.7, 3.5},     {1600.0, 0.292, -4.2, 1.7},
    {2000.0, 0.290, -1.2, -1.3},    {2500.0, 0.290, 1.4, -4.2},
    {3150.0, 0.289, 2.3, -6.0},     {4000.0, 0.289, 1.0, -5.4},
    {5000.0, 0.289, -2.3, -1.5},    {6300.0, 0.293, -7.2, 6.0},
    {8000.0, 0.303, -11.2, 12.6},   {10000.0, 0.323, -10.9, 13.9},
    {12500.0, 0.354, -3.5, 12.3},
};
inline constexpr std::size_t kIso226Rows = 29;

struct Iso226Params {
  double alpha;
  double transfer;
  double threshold;
  bool exact;  // true when f hit a table row (no interpolation)
};

// Log-frequency interpolation; exact table hits return table values.
inline Iso226Params iso226Params(double f) {
  if (f <= kIso226Table[0].frequency)
    return {kIso226Table[0].alpha, kIso226Table[0].transfer,
            kIso226Table[0].threshold, f == kIso226Table[0].frequency};
  for (std::size_t i = 1; i < kIso226Rows; ++i) {
    if (f <= kIso226Table[i].frequency) {
      if (f == kIso226Table[i].frequency)
        return {kIso226Table[i].alpha, kIso226Table[i].transfer,
                kIso226Table[i].threshold, true};
      const double x0 = std::log10(kIso226Table[i - 1].frequency);
      const double x1 = std::log10(kIso226Table[i].frequency);
      const double t = (std::log10(f) - x0) / (x1 - x0);
      const auto mix = [&](double a, double b) { return a + t * (b - a); };
      return {mix(kIso226Table[i - 1].alpha, kIso226Table[i].alpha),
              mix(kIso226Table[i - 1].transfer, kIso226Table[i].transfer),
              mix(kIso226Table[i - 1].threshold, kIso226Table[i].threshold),
              false};
    }
  }
  const auto& last = kIso226Table[kIso226Rows - 1];
  return {last.alpha, last.transfer, last.threshold,
          f == last.frequency};
}
}  // namespace detail

// SPL (dB) of a pure tone at frequency f with loudness level Ln (phon).
[[nodiscard]] inline Result<Decibels, QuantityError> splFromPhon(
    FrequencyHz frequency, Phons loudness) {
  using R = Result<Decibels, QuantityError>;
  const double f = frequency.value();
  const double ln = loudness.value();
  if (!std::isfinite(f) || !std::isfinite(ln))
    return R::failure(QuantityError::InvalidArgument);
  if (f < 20.0 || f > 12500.0)
    return R::failure(QuantityError::InvalidArgument);
  const double hi = f <= 4000.0 ? 90.0 : 80.0;
  if (ln < 20.0 || ln > hi)
    return R::failure(QuantityError::InvalidArgument);
  const auto p = detail::iso226Params(f);
  const double base = std::pow(4e-10, kIso226AlphaR - p.alpha);
  const double loudnessTerm =
      std::pow(10.0, kIso226AlphaR * ln / 10.0) -
      std::pow(10.0, kIso226AlphaR * kIso226ThresholdRefDb / 10.0);
  const double thresholdTerm =
      std::pow(10.0, p.alpha * (p.threshold + p.transfer) / 10.0);
  const double argument = base * loudnessTerm + thresholdTerm;
  if (!(argument > 0.0) || !std::isfinite(argument))
    return R::failure(QuantityError::DomainError);
  const double spl = (10.0 / p.alpha) * std::log10(argument) - p.transfer;
  if (!std::isfinite(spl)) return R::failure(QuantityError::RangeError);
  return R::success(Decibels{spl});
}

// Loudness level (phon) of a pure tone at f with level Lf (dB SPL).
// Inputs below the validated 20-phon contour have no defined phon.
[[nodiscard]] inline Result<Phons, QuantityError> phonFromSpl(
    FrequencyHz frequency, Decibels spl) {
  using R = Result<Phons, QuantityError>;
  const double f = frequency.value();
  const double lf = spl.value();
  if (!std::isfinite(f) || !std::isfinite(lf))
    return R::failure(QuantityError::InvalidArgument);
  if (f < 20.0 || f > 12500.0)
    return R::failure(QuantityError::InvalidArgument);
  const double hi = f <= 4000.0 ? 90.0 : 80.0;
  auto floor = splFromPhon(frequency, Phons{20.0});
  if (!floor.ok()) return R::failure(floor.error());
  auto ceiling = splFromPhon(frequency, Phons{hi});
  if (!ceiling.ok()) return R::failure(ceiling.error());
  if (lf < floor.value().value() || lf > ceiling.value().value() + 1e-9)
    return R::failure(QuantityError::DomainError);
  const auto p = detail::iso226Params(f);
  const double numerator =
      std::pow(10.0, p.alpha * (lf + p.transfer) / 10.0) -
      std::pow(10.0, p.alpha * (p.threshold + p.transfer) / 10.0);
  const double argument =
      numerator / std::pow(4e-10, kIso226AlphaR - p.alpha) +
      std::pow(10.0, kIso226AlphaR * kIso226ThresholdRefDb / 10.0);
  if (!(argument > 0.0) || !std::isfinite(argument))
    return R::failure(QuantityError::DomainError);
  const double phon = (10.0 / kIso226AlphaR) * std::log10(argument);
  if (!std::isfinite(phon)) return R::failure(QuantityError::RangeError);
  return R::success(Phons{phon});
}

// Stevens loudness in sones: 40 phon = 1 sone, doubling per 10 phon.
[[nodiscard]] inline Result<Sones, QuantityError> sonesFromPhon(Phons loudness) {
  using R = Result<Sones, QuantityError>;
  const double p = loudness.value();
  if (!std::isfinite(p)) return R::failure(QuantityError::InvalidArgument);
  const double s = std::pow(2.0, (p - 40.0) / 10.0);
  if (!std::isfinite(s) || !(s > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(Sones{s});
}

[[nodiscard]] inline Result<Phons, QuantityError> phonFromSones(Sones loudness) {
  using R = Result<Phons, QuantityError>;
  const double s = loudness.value();
  if (!std::isfinite(s) || !(s > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  const double p = 40.0 + 10.0 * std::log2(s);
  if (!std::isfinite(p)) return R::failure(QuantityError::RangeError);
  return R::success(Phons{p});
}

// SPL in dB re 20 uPa from RMS pressure in Pa.
[[nodiscard]] inline Result<Decibels, QuantityError> splDbFromPressure(
    PressurePascals pressure) {
  using R = Result<Decibels, QuantityError>;
  const double p = pressure.value();
  if (!std::isfinite(p) || !(p > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  return R::success(Decibels{20.0 * std::log10(p / kSplReferencePa)});
}

[[nodiscard]] inline Result<PressurePascals, QuantityError> pressureFromSplDb(
    Decibels spl) {
  using R = Result<PressurePascals, QuantityError>;
  const double db = spl.value();
  if (!std::isfinite(db)) return R::failure(QuantityError::InvalidArgument);
  const double p = kSplReferencePa * std::pow(10.0, db / 20.0);
  if (!std::isfinite(p) || !(p > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(PressurePascals{p});
}

}  // namespace mmathen
