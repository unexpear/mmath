#pragma once

// Sample-rate voice: a stateful stepper that fills blocks, then a sink.
//
// renderOffline evaluates a continuous Signal at t = t0 + n/fs. A voice
// does not. It keeps the state of a physics stepper and advances one
// sample at a time. Block size only batches those samples. Two block
// sizes of the same voice, from the same reset, are the same sequence.
//
// Nothing here multiplies by an envelope. Decay, if the samples have
// any, is the stepper's. This header wires no instrument. A waveguide
// voice is a separate object in waveguide_voice.h so the path can be
// heard before an instrument meets the audible-physics bar.

#include <cmath>
#include <cstdint>
#include <vector>

#include "../core/result.h"
#include "../sink/sink.h"
#include "../units/physical_units.h"

namespace mmathen {

// The voice owns its sample rate. The caller opens the sink at that
// rate. frames is the number of samples to pull. blockSize is the
// batch handed to the sink; the final batch may be shorter.
struct VoiceRenderConfig {
  FrameCount frames;
  std::uint64_t blockSize = 512;
};

class IVoice {
 public:
  virtual ~IVoice() = default;

  [[nodiscard]] virtual SampleRateHz sampleRate() const = 0;

  // Write n samples into out and advance the stepper by n. n == 0
  // writes nothing and leaves the state. A null out with n > 0 is
  // InvalidArgument.
  [[nodiscard]] virtual Result<FrameCount, QuantityError> renderBlock(
      double* out, FrameCount n) = 0;

  virtual void reset() = 0;
};

// Pulls blocks from the voice into the sink. InvalidConfig: zero
// block, too many frames, or a non-positive voice rate. VoiceFailure
// if a block returns an error; samples already consumed stay consumed.
// SinkFailure if the sink rejects a block. Non-finite samples are
// counted and still written, the same rule as renderOffline.
[[nodiscard]] inline Result<RenderStats, RenderError> renderVoice(
    IVoice& voice, const VoiceRenderConfig& cfg, IAudioSink& sink) {
  using R = Result<RenderStats, RenderError>;
  const double rate = voice.sampleRate().value();
  const std::uint64_t total = static_cast<std::uint64_t>(cfg.frames.value());
  if (!std::isfinite(rate) || !(rate > 0.0))
    return R::failure(RenderError::InvalidConfig);
  if (cfg.blockSize == 0) return R::failure(RenderError::InvalidConfig);
  if (cfg.blockSize > kMaxRenderFrames + 1)
    return R::failure(RenderError::InvalidConfig);
  if (total > kMaxRenderFrames) return R::failure(RenderError::InvalidConfig);

  RenderStats stats;
  std::vector<double> block(static_cast<std::size_t>(cfg.blockSize));
  std::uint64_t done = 0;
  while (done < total) {
    std::uint64_t n = cfg.blockSize;
    if (n > total - done) n = total - done;
    auto produced = voice.renderBlock(block.data(), FrameCount{n});
    if (!produced.ok()) return R::failure(RenderError::VoiceFailure);
    if (produced.value().value() != n)
      return R::failure(RenderError::VoiceFailure);
    accumulateStats(stats, block.data(), FrameCount{n});
    if (!sink.consume(block.data(), FrameCount{n}))
      return R::failure(RenderError::SinkFailure);
    done += n;
  }
  return R::success(stats);
}

}  // namespace mmathen
