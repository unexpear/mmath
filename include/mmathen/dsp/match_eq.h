#pragma once

// D25 — match EQ, and the graphic-EQ gain solver it needs.
//
// Graphic-EQ gain solver. D21's graphic EQ puts each band's gain on its own
// section, and the neighbours' skirts add to it (reported by
// graphicEqCentreErrorDb, 1.6-2.8 dB for octave bands all at +6 dB). Here
// the gains are solved so the cascade's EXACT response at every band centre
// equals a target:
//   g_{k+1} = clamp(g_k + (t - r(g_k)), +/- limit),
// where r(g) is the exact cascade response at the centres (responseDb).
// The fixed-point step converges because each band dominates its own centre
// (every neighbour's influence is well under 1 dB per dB); the achieved
// residual max |t - r| is returned, never assumed, and the tests pin it.
//
// Match EQ. The long-term average spectrum (Welch: mean of window-corrected
// power over half-overlapping frames, spectrum.h) of a source and of a
// reference are reduced to fractional-octave band levels; the correction
// per band is reference minus source. Bands where either signal is below
// `floorDb`, more than `relativeFloorDb` below its own loudest band, or
// holds no FFT bin are left at 0 dB (nothing to match). With
// `normalize` the mean correction over the matched bands is removed, so the
// EQ matches tonal balance, not loudness. `amount` in [0, 1] scales the
// correction, which is clamped to +/- maxGainDb, then solved into exact
// graphic-EQ gains and refined in closed loop on the band levels the source
// is predicted to have through the EQ (bin power times |H|^2, exact for a
// linear filter on the measured spectrum). A band-level match is a smooth
// match by design: it does not reproduce detail narrower than one band.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "../analysis/fft.h"
#include "../analysis/spectrum.h"
#include "../core/result.h"
#include "../units/physical_units.h"
#include "equalizer.h"

namespace mmathen {

struct GraphicEqSolution {
  std::vector<double> gainsDb;
  double residualDb = 0.0;  // max |target - response| at the centres
  int iterations = 0;
};

// Solve graphic-EQ gains whose exact centre responses equal targetDb.
// targets and gains limited to +/- limitDb (<= 24). Stops when the residual
// is below toleranceDb or after maxIterations (1..500).
[[nodiscard]] inline Result<GraphicEqSolution, QuantityError>
solveGraphicEqGains(const std::vector<FrequencyBand>& bands,
                    const std::vector<double>& targetDb, SampleRateHz rate,
                    double limitDb = 15.0, double toleranceDb = 1e-3,
                    int maxIterations = 100) {
  using R = Result<GraphicEqSolution, QuantityError>;
  if (bands.empty() || bands.size() != targetDb.size() ||
      !std::isfinite(limitDb) || !(limitDb > 0.0) || limitDb > 24.0 ||
      !std::isfinite(toleranceDb) || !(toleranceDb > 0.0) ||
      maxIterations < 1 || maxIterations > 500)
    return R::failure(QuantityError::InvalidArgument);
  for (double t : targetDb)
    if (!std::isfinite(t) || std::fabs(t) > limitDb)
      return R::failure(QuantityError::InvalidArgument);
  GraphicEqSolution s;
  s.gainsDb = targetDb;
  for (int it = 1; it <= maxIterations; ++it) {
    auto eq = makeGraphicEq(bands, s.gainsDb, rate);
    if (!eq.ok()) return R::failure(eq.error());
    double worst = 0.0;
    std::vector<double> err(bands.size());
    for (std::size_t i = 0; i < bands.size(); ++i) {
      auto r = eq.value().responseDb(bands[i].center);
      if (!r.ok()) return R::failure(r.error());
      err[i] = targetDb[i] - r.value();
      worst = std::max(worst, std::fabs(err[i]));
    }
    s.residualDb = worst;
    s.iterations = it;
    if (worst < toleranceDb) break;
    for (std::size_t i = 0; i < bands.size(); ++i)
      s.gainsDb[i] = std::clamp(s.gainsDb[i] + err[i], -limitDb, limitDb);
  }
  return R::success(std::move(s));
}

// Welch long-term average spectrum: mean power (and amplitude) over frames
// of frameSize (power of two >= 16) at hop frameSize/2. The input must hold
// at least one frame.
[[nodiscard]] inline Result<Spectrum, QuantityError> longTermSpectrum(
    const std::vector<double>& samples, SampleRateHz rate,
    std::size_t frameSize, Window window = Window::Hann) {
  using R = Result<Spectrum, QuantityError>;
  if (frameSize < 16 || !isPowerOfTwo(frameSize) ||
      samples.size() < frameSize)
    return R::failure(QuantityError::InvalidArgument);
  Spectrum mean;
  std::size_t frames = 0;
  std::vector<double> frame(frameSize);
  for (std::size_t start = 0; start + frameSize <= samples.size();
       start += frameSize / 2) {
    std::copy(samples.begin() + static_cast<std::ptrdiff_t>(start),
              samples.begin() + static_cast<std::ptrdiff_t>(start + frameSize),
              frame.begin());
    auto s = computeSpectrum(frame, rate, window);
    if (!s.ok()) return R::failure(s.error());
    if (frames == 0) {
      mean = s.value();
    } else {
      for (std::size_t k = 0; k < mean.power.size(); ++k) {
        mean.power[k] += s.value().power[k];
        mean.amplitude[k] += s.value().amplitude[k];
      }
    }
    ++frames;
  }
  for (std::size_t k = 0; k < mean.power.size(); ++k) {
    mean.power[k] /= static_cast<double>(frames);
    mean.amplitude[k] /= static_cast<double>(frames);
  }
  return R::success(std::move(mean));
}

struct MatchEqOptions {
  double amount = 1.0;      // [0, 1]
  double maxGainDb = 12.0;  // (0, 15]
  double floorDb = -90.0;   // bands below this (either side) are not matched
  // Bands more than this far below the same signal's loudest band are not
  // matched either: in sparse (tonal) material they hold only window
  // leakage, and "matching" leakage means large, meaningless boosts.
  double relativeFloorDb = 50.0;  // > 0
  bool normalize = true;    // remove the mean correction (tone, not level)
  int refinements = 4;      // closed-loop passes on predicted levels, 0..20
};

struct MatchEqDesign {
  std::vector<FrequencyBand> bands;
  std::vector<double> correctionDb;  // desired band change (after options)
  std::vector<bool> matched;         // false: band left alone
  GraphicEqSolution solution;        // graphic-EQ gains, exact at centres
  // Predicted band change of the source through the solved EQ, from its
  // spectrum: sum_k P_src(k) |H(f_k)|^2 per band (exact for the bins).
  std::vector<double> predictedDb;
  double predictedErrorDb = 0.0;  // max |correction - predicted|, matched
};

namespace detail {
// Band levels of a spectrum after an EQ: each bin's power times |H|^2.
inline Result<std::vector<BandLevel>, QuantityError> levelsThrough(
    const Spectrum& s, const std::vector<FrequencyBand>& bands,
    const ParametricEq& eq, SampleRateHz rate) {
  using R = Result<std::vector<BandLevel>, QuantityError>;
  Spectrum out = s;
  for (std::size_t k = 0; k < out.power.size(); ++k) {
    const double f = static_cast<double>(k) * s.binHz;
    if (f > rate.value() / 2.0) break;
    auto db = eq.responseDb(FrequencyHz{f});
    if (!db.ok()) return R::failure(db.error());
    const double g = std::pow(10.0, db.value() / 20.0);
    out.power[k] *= g * g;
    out.amplitude[k] *= g;
  }
  return R::success(bandLevels(out, bands));
}
}  // namespace detail

// Match EQ from the long-term spectra of a source and a reference (same
// rate and FFT size, e.g. longTermSpectrum of each). The desired change per
// band is reference - source (options applied). Graphic-EQ gains are solved
// for it, then refined in closed loop: the band levels the source WOULD
// have through the EQ are predicted exactly from its spectrum, and the
// centre targets are corrected by what is still missing (band levels
// integrate over the band, so centre values alone undershoot on slopes and
// at the top/bottom band). The final prediction error is reported.
// The reference enters only through its band levels, so it may come from a
// different sample rate or FFT size (same band list: InvalidArgument if a
// centre differs).
[[nodiscard]] inline Result<MatchEqDesign, QuantityError> designMatchEq(
    const Spectrum& source, const std::vector<BandLevel>& referenceLevels,
    const std::vector<FrequencyBand>& bands, SampleRateHz rate,
    const MatchEqOptions& o = {}) {
  using R = Result<MatchEqDesign, QuantityError>;
  if (bands.empty() || source.power.empty() ||
      referenceLevels.size() != bands.size() || !std::isfinite(o.amount) ||
      o.amount < 0.0 || o.amount > 1.0 || !std::isfinite(o.maxGainDb) ||
      !(o.maxGainDb > 0.0) || o.maxGainDb > 15.0 || !std::isfinite(o.floorDb) ||
      !std::isfinite(o.relativeFloorDb) || !(o.relativeFloorDb > 0.0) ||
      o.refinements < 0 || o.refinements > 20)
    return R::failure(QuantityError::InvalidArgument);
  for (std::size_t i = 0; i < bands.size(); ++i)
    if (referenceLevels[i].band.center.value() != bands[i].center.value())
      return R::failure(QuantityError::InvalidArgument);
  const auto ls = bandLevels(source, bands);
  const auto& lr = referenceLevels;
  double maxS = -1e300, maxR = -1e300;
  for (std::size_t i = 0; i < bands.size(); ++i) {
    if (ls[i].bins > 0) maxS = std::max(maxS, ls[i].levelDb);
    if (lr[i].bins > 0) maxR = std::max(maxR, lr[i].levelDb);
  }
  MatchEqDesign d;
  d.bands = bands;
  double sum = 0.0;
  std::size_t used = 0;
  for (std::size_t i = 0; i < bands.size(); ++i) {
    const bool ok = ls[i].bins > 0 && lr[i].bins > 0 &&
                    ls[i].levelDb > o.floorDb && lr[i].levelDb > o.floorDb &&
                    ls[i].levelDb > maxS - o.relativeFloorDb &&
                    lr[i].levelDb > maxR - o.relativeFloorDb;
    d.matched.push_back(ok);
    const double diff = ok ? lr[i].levelDb - ls[i].levelDb : 0.0;
    d.correctionDb.push_back(diff);
    if (ok) {
      sum += diff;
      ++used;
    }
  }
  const double offset =
      (o.normalize && used > 0) ? sum / static_cast<double>(used) : 0.0;
  for (std::size_t i = 0; i < bands.size(); ++i) {
    const double t = d.matched[i] ? (d.correctionDb[i] - offset) * o.amount : 0.0;
    d.correctionDb[i] = std::clamp(t, -o.maxGainDb, o.maxGainDb);
  }
  std::vector<double> target = d.correctionDb;
  for (int pass = 0;; ++pass) {
    auto sol = solveGraphicEqGains(bands, target, rate, 15.0);
    if (!sol.ok()) return R::failure(sol.error());
    d.solution = std::move(sol).value();
    auto eq = makeGraphicEq(bands, d.solution.gainsDb, rate);
    if (!eq.ok()) return R::failure(eq.error());
    auto lo = detail::levelsThrough(source, bands, eq.value(), rate);
    if (!lo.ok()) return R::failure(lo.error());
    d.predictedDb.assign(bands.size(), 0.0);
    d.predictedErrorDb = 0.0;
    for (std::size_t i = 0; i < bands.size(); ++i) {
      if (!d.matched[i]) continue;
      d.predictedDb[i] = lo.value()[i].levelDb - ls[i].levelDb;
      d.predictedErrorDb = std::max(
          d.predictedErrorDb, std::fabs(d.correctionDb[i] - d.predictedDb[i]));
    }
    if (pass >= o.refinements) break;
    for (std::size_t i = 0; i < bands.size(); ++i)
      if (d.matched[i])
        target[i] = std::clamp(
            target[i] + (d.correctionDb[i] - d.predictedDb[i]), -15.0, 15.0);
  }
  return R::success(std::move(d));
}

// Both long-term spectra given: same rate and FFT size required.
[[nodiscard]] inline Result<MatchEqDesign, QuantityError> designMatchEq(
    const Spectrum& source, const Spectrum& reference,
    const std::vector<FrequencyBand>& bands, SampleRateHz rate,
    const MatchEqOptions& o = {}) {
  using R = Result<MatchEqDesign, QuantityError>;
  if (source.power.size() != reference.power.size() ||
      source.binHz != reference.binHz)
    return R::failure(QuantityError::InvalidArgument);
  return designMatchEq(source, bandLevels(reference, bands), bands, rate, o);
}

}  // namespace mmathen
