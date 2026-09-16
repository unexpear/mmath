#pragma once

// Phase 2.5 — offline renderer: the single path every sink consumes.
//
//   Signal -> Rendered buffer (owned here, loaned to the sink) -> Sink.
//
// Frame count N is authoritative: N frames cover t_0 .. t_0+(N-1)/fs.
// Evaluation times derive from integer indices (never accumulated t += dt),
// with the identical formula as timeAt(); rate/t_0 are validated once before
// the loop. Block size affects only batching, never values: the
// block-invariance test renders 64/127/512-frame blocks and demands identical
// sequences. The renderer completes on non-finite/overload (counted in
// stats for certification to judge); only sink I/O failure aborts.

#include <cmath>
#include <cstdint>
#include <vector>

#include "../core/result.h"
#include "../signal/signal.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"
#include "../sink/sink.h"

namespace mmathen {

struct RenderConfig {
  SampleRateHz rate;
  FrameCount frames;
  TimePointSeconds t0{0.0};
  std::uint64_t blockSize = 512;
};

[[nodiscard]] inline Result<RenderStats, RenderError> renderOffline(
    const Signal<SignedAudioValue>& sig, const RenderConfig& cfg,
    IAudioSink& sink) {
  using R = Result<RenderStats, RenderError>;
  const double rate = cfg.rate.value();
  const double t0 = cfg.t0.value();
  const std::uint64_t total = static_cast<std::uint64_t>(cfg.frames.value());
  if (!(rate > 0.0) || !std::isfinite(rate))
    return R::failure(RenderError::InvalidConfig);
  if (!std::isfinite(t0)) return R::failure(RenderError::InvalidConfig);
  if (cfg.blockSize == 0) return R::failure(RenderError::InvalidConfig);
  // Blocks are an internal batching detail: anything beyond one full-length
  // block is meaningless, and an unbounded blockSize would die in the
  // buffer allocation below instead of reporting (audit F2).
  if (cfg.blockSize > kMaxRenderFrames + 1)
    return R::failure(RenderError::InvalidConfig);
  if (total > kMaxRenderFrames) return R::failure(RenderError::InvalidConfig);

  RenderStats stats;
  std::vector<double> block(static_cast<std::size_t>(cfg.blockSize));
  std::uint64_t done = 0;
  while (done < total) {
    std::uint64_t n = cfg.blockSize;
    if (n > total - done) n = total - done;  // final partial block
    for (std::uint64_t i = 0; i < n; ++i) {
      // t = t_0 + (done + i)/fs — integer-derived, same as timeAt().
      const double t = t0 + static_cast<double>(done + i) / rate;
      block[static_cast<std::size_t>(i)] = sig.at(TimePointSeconds{t}).value();
    }
    accumulateStats(stats, block.data(), FrameCount{n});
    if (!sink.consume(block.data(), FrameCount{n}))
      return R::failure(RenderError::SinkFailure);
    done += n;
  }
  return R::success(stats);
}

}  // namespace mmathen
