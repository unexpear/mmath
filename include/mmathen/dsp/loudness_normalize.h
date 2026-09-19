#pragma once

// D33 — loudness normalization for delivery: bring a programme to a target
// integrated loudness (BS.1770-5, D31) under a true-peak ceiling (D31's
// true-peak limiter mode), and report what was actually achieved.
//
//   1. measure the integrated loudness L_in (silence: DomainError);
//   2. gain g = target - L_in;
//   3. pass: y = x * 10^(g/20); if the true peak of y exceeds the ceiling,
//      run the look-ahead limiter in true-peak mode at the ceiling (latency
//      compensated: the output is sample-aligned, same length); measure
//      L(y); if |target - L(y)| <= tolerance stop, else g += target - L(y)
//      and repeat (limiting costs loudness, so the gain is raised to make
//      up for it), at most maxPasses times.
// Guarantees and reports: sample peaks never exceed the ceiling whenever
// the limiter ran (D24's guarantee); the true peak is re-measured with the
// 4x detector and reported (within D31's limiter residual); the achieved
// loudness is reported and `reachedTarget` says honestly whether the
// target was met: a target too loud for the ceiling cannot be met without
// distortion, and this function does not pretend otherwise. A programme
// that only needs attenuation (or boost below the ceiling) is a pure gain,
// bit-for-bit x * 10^(g/20).
//
// Common targets (streaming services per AES TD1008; broadcast): -14 LUFS
// (Spotify, YouTube, Tidal, Amazon), -16 LUFS (Apple Music), -23 LUFS (EBU
// R128), -24 LKFS (ATSC A/85); a -1 dBTP ceiling is the usual delivery
// margin. These are data for callers, not built-in policy.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "../analysis/bs1770.h"
#include "../core/result.h"
#include "../units/physical_units.h"
#include "dynamics.h"

namespace mmathen {

struct NormalizeTarget {
  double lufs = -14.0;          // [-60, 0]
  double truePeakDb = -1.0;     // ceiling, [-20, 0]
  double toleranceLu = 0.1;     // (0, 2]
  int maxPasses = 5;            // 1..20
  double releaseMs = 50.0;      // limiter release
};

struct NormalizeReport {
  double inputLufs = kLoudnessFloor;
  double gainDb = 0.0;          // final gain before limiting
  double outputLufs = kLoudnessFloor;
  double outputTruePeakDb = kLoudnessFloor;
  double outputSamplePeakDb = kLoudnessFloor;
  int passes = 0;
  bool limited = false;         // the limiter ran on the final pass
  bool reachedTarget = false;
};

namespace detail {
inline Result<LoudnessMeter, QuantityError> measure(
    const std::vector<double>& x, int channels, SampleRateHz rate) {
  using R = Result<LoudnessMeter, QuantityError>;
  auto m = LoudnessMeter::create(rate, channels);
  if (!m.ok()) return m;
  LoudnessMeter meter = std::move(m).value();
  meter.push(x.data(), x.size() / static_cast<std::size_t>(channels));
  return R::success(std::move(meter));
}
}  // namespace detail

// In place on interleaved samples (1 or 2 channels).
[[nodiscard]] inline Result<NormalizeReport, QuantityError> normalizeLoudness(
    std::vector<double>& interleaved, int channels, SampleRateHz rate,
    const NormalizeTarget& t = {}) {
  using R = Result<NormalizeReport, QuantityError>;
  if ((channels != 1 && channels != 2) || interleaved.empty() ||
      interleaved.size() % static_cast<std::size_t>(channels) != 0 ||
      !std::isfinite(t.lufs) || t.lufs > 0.0 || t.lufs < -60.0 ||
      !std::isfinite(t.truePeakDb) || t.truePeakDb > 0.0 ||
      t.truePeakDb < -20.0 || !std::isfinite(t.toleranceLu) ||
      !(t.toleranceLu > 0.0) || t.toleranceLu > 2.0 || t.maxPasses < 1 ||
      t.maxPasses > 20 || !std::isfinite(t.releaseMs) || !(t.releaseMs > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  for (double v : interleaved)
    if (!std::isfinite(v)) return R::failure(QuantityError::InvalidArgument);
  const auto ch = static_cast<std::size_t>(channels);
  const std::size_t frames = interleaved.size() / ch;
  auto in = detail::measure(interleaved, channels, rate);
  if (!in.ok()) return R::failure(in.error());
  NormalizeReport rep;
  rep.inputLufs = in.value().integratedLufs();
  if (rep.inputLufs <= kLoudnessFloor + 1.0)
    return R::failure(QuantityError::DomainError);  // nothing to measure
  const std::vector<double> source = interleaved;
  double g = t.lufs - rep.inputLufs;
  std::vector<double> y(source.size());
  for (int pass = 1; pass <= t.maxPasses; ++pass) {
    rep.passes = pass;
    rep.gainDb = g;
    const double lin = std::pow(10.0, g / 20.0);
    for (std::size_t i = 0; i < source.size(); ++i) y[i] = source[i] * lin;
    auto tpR = TruePeakDetector::create(channels);
    if (!tpR.ok()) return R::failure(tpR.error());
    TruePeakDetector tp = std::move(tpR).value();
    tp.push(y.data(), frames);
    rep.limited = tp.truePeakDb() > t.truePeakDb;
    if (rep.limited) {
      auto limR = Limiter::create(
          LimiterParams{t.truePeakDb, 5.0, t.releaseMs, true}, rate);
      if (!limR.ok()) return R::failure(limR.error());
      Limiter lim = std::move(limR).value();
      const std::size_t lat = lim.latencyFrames();
      // Planar buffers with `lat` trailing zeros; drop the first `lat`.
      std::vector<double> l(frames + lat, 0.0), r(frames + lat, 0.0);
      for (std::size_t f = 0; f < frames; ++f) {
        l[f] = y[f * ch];
        r[f] = y[f * ch + ch - 1];
      }
      const FrameCount n{static_cast<std::uint64_t>(frames + lat)};
      if (channels == 1)
        lim.processBlock(l.data(), l.data(), n);
      else
        lim.processStereo(l.data(), r.data(), l.data(), r.data(), n);
      for (std::size_t f = 0; f < frames; ++f) {
        y[f * ch] = l[f + lat];
        if (channels == 2) y[f * ch + 1] = r[f + lat];
      }
    }
    auto out = detail::measure(y, channels, rate);
    if (!out.ok()) return R::failure(out.error());
    rep.outputLufs = out.value().integratedLufs();
    rep.outputTruePeakDb = out.value().truePeakDbtp();
    rep.outputSamplePeakDb = out.value().samplePeakDbfs();
    const double err = t.lufs - rep.outputLufs;
    rep.reachedTarget = std::fabs(err) <= t.toleranceLu;
    if (rep.reachedTarget) break;
    g += err;
  }
  interleaved = std::move(y);
  return R::success(rep);
}

}  // namespace mmathen
