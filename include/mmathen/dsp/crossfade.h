#pragma once

// D26 — smooth cuts: crossfade curves, gain ramps, and a crossfading
// playback reader.
//
// Curves, fade position t in [0, 1] (gi fades in, go fades out):
//   Linear        gi = t,              go = 1 - t         gi + go = 1
//   RaisedCosine  gi = (1 - cos pi t)/2, go = 1 - gi       gi + go = 1,
//                 zero slope at both ends
//   EqualPower    gi = sin(pi t / 2),  go = cos(pi t / 2)  gi^2 + go^2 = 1
// Constant-sum curves keep the level of CORRELATED material (the same
// signal, e.g. a loop whose ends match); equal power keeps the level of
// UNCORRELATED material (a cut between unrelated positions). Block
// crossfades sample t at frame centres, t = (i + 1/2)/n, so a fade is
// symmetric and never includes the exact end points.
//
// GainRamp: a linear ramp to a target over a stated number of frames (for
// de-clicking start/stop, mute and dry/wet switches); reaches the target
// exactly on the last frame of the ramp.
//
// SpliceReader<T>: plays an interleaved buffer (caller-owned, float or
// double) and turns every discontinuity into a crossfade of `fadeFrames`:
//   jumpTo(target): the current position keeps playing as an outgoing voice
//     and fades out while the target fades in;
//   loop region [start, end): when the playing position reaches end - F it
//     jumps (crossfaded) to start, so the last F frames of the loop overlap
//     the first F — the loop seam is a crossfade, not a cut.
// A jump during a fade keeps the louder of the two voices as the outgoing
// one and drops the quieter (a bounded step of at most the dropped voice's
// gain, stated). Reads are allocation-free and bounded, suitable for an
// audio callback. After a fade completes the output is the source exactly.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>

#include "../core/result.h"

namespace mmathen {

enum class FadeCurve { Linear, RaisedCosine, EqualPower };

[[nodiscard]] inline double fadeInGain(FadeCurve c, double t) noexcept {
  t = std::clamp(t, 0.0, 1.0);
  switch (c) {
    case FadeCurve::Linear:
      return t;
    case FadeCurve::RaisedCosine:
      return 0.5 - 0.5 * std::cos(std::numbers::pi * t);
    case FadeCurve::EqualPower:
      return std::sin(0.5 * std::numbers::pi * t);
  }
  return t;
}

[[nodiscard]] inline double fadeOutGain(FadeCurve c, double t) noexcept {
  t = std::clamp(t, 0.0, 1.0);
  switch (c) {
    case FadeCurve::Linear:
      return 1.0 - t;
    case FadeCurve::RaisedCosine:
      return 0.5 + 0.5 * std::cos(std::numbers::pi * t);
    case FadeCurve::EqualPower:
      return std::cos(0.5 * std::numbers::pi * t);
  }
  return 1.0 - t;
}

// out[i] = go(t_i) a[i] + gi(t_i) b[i], t_i = (i + 1/2)/n. Buffers may alias.
inline void crossfadeBlocks(const double* a, const double* b, double* out,
                            std::size_t n, FadeCurve curve) noexcept {
  for (std::size_t i = 0; i < n; ++i) {
    const double t =
        (static_cast<double>(i) + 0.5) / static_cast<double>(n);
    out[i] = fadeOutGain(curve, t) * a[i] + fadeInGain(curve, t) * b[i];
  }
}

class GainRamp {
 public:
  explicit GainRamp(double initial = 1.0) noexcept
      : value_(initial), target_(initial) {}

  // Ramp linearly to target over `frames` frames (0: jump at once).
  void setTarget(double target, std::size_t frames) noexcept {
    target_ = target;
    if (frames == 0) {
      value_ = target;
      left_ = 0;
      return;
    }
    step_ = (target - value_) / static_cast<double>(frames);
    left_ = frames;
  }

  // Gain for the next frame.
  double next() noexcept {
    if (left_ > 0) {
      --left_;
      value_ = left_ == 0 ? target_ : value_ + step_;
    }
    return value_;
  }

  [[nodiscard]] double value() const noexcept { return value_; }
  [[nodiscard]] double target() const noexcept { return target_; }
  [[nodiscard]] bool ramping() const noexcept { return left_ > 0; }

 private:
  double value_;
  double target_;
  double step_ = 0.0;
  std::size_t left_ = 0;
};

template <typename T>
class SpliceReader {
 public:
  // channels 1..32, fadeFrames 1..2^20.
  [[nodiscard]] static Result<SpliceReader, QuantityError> create(
      int channels, std::size_t fadeFrames,
      FadeCurve curve = FadeCurve::EqualPower) {
    using R = Result<SpliceReader, QuantityError>;
    if (channels < 1 || channels > 32 || fadeFrames < 1 ||
        fadeFrames > (std::size_t{1} << 20))
      return R::failure(QuantityError::InvalidArgument);
    return R::success(SpliceReader{channels, fadeFrames, curve});
  }

  // New source (interleaved, `frames` frames, caller keeps it alive):
  // position 0, no fade, loop region = whole buffer, looping unchanged.
  void setSource(const T* data, std::size_t frames) noexcept {
    data_ = data;
    frames_ = frames;
    main_ = 0;
    old_ = 0;
    fading_ = false;
    fadePos_ = 0;
    loopStart_ = 0;
    loopEnd_ = frames;
  }

  void setLooping(bool on) noexcept { loop_ = on; }
  [[nodiscard]] bool looping() const noexcept { return loop_; }

  // Loop region [start, end): end <= frames, and at least two fades long
  // (so the seam crossfade fits). InvalidArgument otherwise.
  [[nodiscard]] Result<std::size_t, QuantityError> setLoopRegion(
      std::size_t start, std::size_t end) noexcept {
    using R = Result<std::size_t, QuantityError>;
    if (!(start < end) || end > frames_ || end - start < 2 * fade_)
      return R::failure(QuantityError::InvalidArgument);
    loopStart_ = start;
    loopEnd_ = end;
    return R::success(end - start);
  }

  // Crossfaded jump; target clamped to the buffer.
  void jumpTo(std::size_t target) noexcept {
    if (frames_ == 0) return;
    target = std::min(target, frames_ - 1);
    if (fading_) {
      const double t = (static_cast<double>(fadePos_) + 0.5) /
                       static_cast<double>(fade_);
      // Keep the louder voice as the outgoing one.
      if (fadeInGain(curve_, t) >= fadeOutGain(curve_, t)) old_ = main_;
    } else {
      old_ = main_;
    }
    main_ = target;
    fading_ = true;
    fadePos_ = 0;
  }

  // Write up to `frames` interleaved frames; returns frames written, fewer
  // only when a non-looping source ends.
  std::size_t read(double* out, std::size_t frames) noexcept {
    const auto ch = static_cast<std::size_t>(channels_);
    for (std::size_t i = 0; i < frames; ++i) {
      if (loop_ && !fading_ && loopEnd_ - loopStart_ >= 2 * fade_ &&
          main_ + fade_ >= loopEnd_ && main_ < loopEnd_)
        jumpTo(loopStart_);
      if (!fading_ && main_ >= frames_) return i;
      if (fading_) {
        const double t = (static_cast<double>(fadePos_) + 0.5) /
                         static_cast<double>(fade_);
        const double gi = fadeInGain(curve_, t), go = fadeOutGain(curve_, t);
        for (std::size_t c = 0; c < ch; ++c)
          out[i * ch + c] = go * at(old_, c) + gi * at(main_, c);
        ++old_;
        if (++fadePos_ == fade_) fading_ = false;
      } else {
        for (std::size_t c = 0; c < ch; ++c) out[i * ch + c] = at(main_, c);
      }
      ++main_;
    }
    return frames;
  }

  // Hard position set, no crossfade (a cut): for use while the output is
  // silent (paused, or a freshly created reader). Cancels any fade.
  void setPosition(std::size_t frame) noexcept {
    main_ = frames_ == 0 ? 0 : std::min(frame, frames_ - 1);
    fading_ = false;
    fadePos_ = 0;
  }

  [[nodiscard]] std::size_t position() const noexcept { return main_; }
  [[nodiscard]] bool fading() const noexcept { return fading_; }
  [[nodiscard]] std::size_t fadeFrames() const noexcept { return fade_; }
  [[nodiscard]] int channels() const noexcept { return channels_; }

 private:
  SpliceReader(int channels, std::size_t fade, FadeCurve curve)
      : channels_(channels), fade_(fade), curve_(curve) {}

  double at(std::size_t frame, std::size_t c) const noexcept {
    if (data_ == nullptr || frame >= frames_) return 0.0;
    return static_cast<double>(
        data_[frame * static_cast<std::size_t>(channels_) + c]);
  }

  int channels_;
  std::size_t fade_;
  FadeCurve curve_;
  const T* data_ = nullptr;
  std::size_t frames_ = 0;
  std::size_t main_ = 0;
  std::size_t old_ = 0;
  bool fading_ = false;
  std::size_t fadePos_ = 0;
  bool loop_ = false;
  std::size_t loopStart_ = 0;
  std::size_t loopEnd_ = 0;
};

}  // namespace mmathen
