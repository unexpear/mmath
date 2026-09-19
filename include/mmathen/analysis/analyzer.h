#pragma once

// D22 — streaming band analyzer for live displays ("EQ bars" that move).
//
// Samples are pushed in any block size; every `hop` new samples the last
// `frameSize` samples are analysed (spectrum.h: window-corrected power,
// IEC 61260-1 band levels) and the display is updated with meter
// ballistics, all per analysis frame of duration dt = hop / fs:
//   display_i = max(raw_i, display_i - release * dt)
//       instant attack, linear fall of `release` dB/s
//   peak_i    = raw_i if raw_i >= peak_i (hold timer reset), else held for
//       `peakHold` seconds, then falls at `release` dB/s (never below the
//       display)
// Levels start at the floor (kSpectrumFloorDb) until the first full frame.
// This class allocates at creation only; push()/update() are bounded work,
// but they belong on the UI thread (the audio thread hands samples over
// through SpscRing). Pure and deterministic: identical input gives
// identical levels, which the tests pin, including exact release slopes.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../units/physical_units.h"
#include "fft.h"
#include "spectrum.h"

namespace mmathen {

struct MeterBallistics {
  double releaseDbPerSecond = 24.0;
  double peakHoldSeconds = 1.0;
};

class BandAnalyzer {
 public:
  // frameSize: power of two >= 16; 1 <= hop <= frameSize; bands as in
  // fractionalOctaveBands; release > 0, hold >= 0 (finite). InvalidArgument
  // otherwise.
  [[nodiscard]] static Result<BandAnalyzer, QuantityError> create(
      SampleRateHz rate, std::size_t frameSize, std::size_t hop,
      int bandsPerOctave, FrequencyHz fMin, FrequencyHz fMax, Window window,
      MeterBallistics ballistics) {
    using R = Result<BandAnalyzer, QuantityError>;
    const double fs = rate.value();
    if (!std::isfinite(fs) || !(fs > 0.0) || frameSize < 16 ||
        !isPowerOfTwo(frameSize) || frameSize > kMaxFftSize || hop < 1 ||
        hop > frameSize || !std::isfinite(ballistics.releaseDbPerSecond) ||
        !(ballistics.releaseDbPerSecond > 0.0) ||
        !std::isfinite(ballistics.peakHoldSeconds) ||
        ballistics.peakHoldSeconds < 0.0)
      return R::failure(QuantityError::InvalidArgument);
    auto bands = fractionalOctaveBands(bandsPerOctave, fMin, fMax);
    if (!bands.ok()) return R::failure(bands.error());
    if (bands.value().empty()) return R::failure(QuantityError::InvalidArgument);
    return R::success(BandAnalyzer{rate, frameSize, hop, std::move(bands).value(),
                                   window, ballistics});
  }

  // Append samples; runs one analysis per completed hop. Returns the number
  // of analysis frames computed during this call.
  std::size_t push(const double* x, std::size_t n) {
    std::size_t frames = 0;
    for (std::size_t i = 0; i < n; ++i) {
      history_[write_] = x[i];
      write_ = (write_ + 1) % history_.size();
      ++filled_;
      if (++sinceHop_ == hop_) {
        sinceHop_ = 0;
        if (filled_ >= history_.size()) {
          analyse();
          ++frames;
        }
      }
    }
    return frames;
  }

  void reset() {
    std::fill(history_.begin(), history_.end(), 0.0);
    write_ = 0;
    filled_ = 0;
    sinceHop_ = 0;
    for (auto& v : display_) v = kSpectrumFloorDb;
    for (auto& v : peak_) v = kSpectrumFloorDb;
    for (auto& v : hold_) v = 0.0;
    for (auto& l : raw_) {
      l.power = 0.0;
      l.levelDb = kSpectrumFloorDb;
    }
  }

  [[nodiscard]] const std::vector<BandLevel>& raw() const noexcept {
    return raw_;
  }
  [[nodiscard]] const std::vector<double>& display() const noexcept {
    return display_;
  }
  [[nodiscard]] const std::vector<double>& peaks() const noexcept {
    return peak_;
  }
  [[nodiscard]] const Spectrum& spectrum() const noexcept { return spec_; }
  [[nodiscard]] const std::vector<FrequencyBand>& bands() const noexcept {
    return bands_;
  }
  // Seconds represented by one analysis frame step.
  [[nodiscard]] double frameStepSeconds() const noexcept {
    return static_cast<double>(hop_) / rate_.value();
  }

 private:
  BandAnalyzer(SampleRateHz rate, std::size_t frameSize, std::size_t hop,
               std::vector<FrequencyBand> bands, Window window,
               MeterBallistics ballistics)
      : rate_(rate),
        hop_(hop),
        bands_(std::move(bands)),
        window_(window),
        ballistics_(ballistics),
        history_(frameSize, 0.0),
        frame_(frameSize, 0.0),
        display_(bands_.size(), kSpectrumFloorDb),
        peak_(bands_.size(), kSpectrumFloorDb),
        hold_(bands_.size(), 0.0) {
    for (const auto& b : bands_)
      raw_.push_back(BandLevel{b, 0.0, kSpectrumFloorDb, 0});
  }

  void analyse() {
    const std::size_t n = history_.size();
    for (std::size_t i = 0; i < n; ++i) frame_[i] = history_[(write_ + i) % n];
    auto s = computeSpectrum(frame_, rate_, window_);
    if (!s.ok()) return;  // input is finite by contract; nothing to show
    spec_ = std::move(s).value();
    raw_ = bandLevels(spec_, bands_);
    const double dt = frameStepSeconds();
    const double fall = ballistics_.releaseDbPerSecond * dt;
    for (std::size_t i = 0; i < raw_.size(); ++i) {
      const double r = raw_[i].levelDb;
      display_[i] = (std::max)(r, display_[i] - fall);
      if (r >= peak_[i]) {
        peak_[i] = r;
        hold_[i] = 0.0;
      } else {
        hold_[i] += dt;
        if (hold_[i] > ballistics_.peakHoldSeconds) peak_[i] -= fall;
      }
      peak_[i] = (std::max)(peak_[i], display_[i]);
    }
  }

  SampleRateHz rate_;
  std::size_t hop_;
  std::vector<FrequencyBand> bands_;
  Window window_;
  MeterBallistics ballistics_;
  std::vector<double> history_;
  std::vector<double> frame_;
  std::size_t write_ = 0;
  std::size_t filled_ = 0;
  std::size_t sinceHop_ = 0;
  Spectrum spec_;
  std::vector<BandLevel> raw_;
  std::vector<double> display_;
  std::vector<double> peak_;
  std::vector<double> hold_;
};

}  // namespace mmathen
