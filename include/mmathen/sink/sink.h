#pragma once

// Phase 2.5 — sink boundary.
//
// One offline rendering path produces buffers; every sink (Null, memory-test,
// WAV, later native) consumes the SAME generated buffers via IAudioSink.
// A null sink never skips calculation: it receives and discards. The
// test-only memory sink exposes samples for inspection; it is tooling, not a
// public subsystem. File I/O lives outside the processing boundary: the
// renderer writes into supplied buffers, sinks handle persistence.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "../core/result.h"
#include "../units/physical_units.h"

namespace mmathen {

// Uniform supported maximum: the largest mono 16-bit PCM frame count whose
// RIFF header still fits 32-bit sizes (36 + 2*N <= UINT32_MAX), so every sink
// shares one documented limit. This is NOT an accuracy claim about
// double-precision time conversion at large indices (see timeAt notes), nor
// a RAM promise: a MemorySink retains ~8 bytes per frame, so full-limit
// renders are format-legal but memory-bound on ordinary machines.
inline constexpr std::uint64_t kMaxRenderFrames = 2147483628ULL;

enum class RenderError {
  None = 0,
  InvalidConfig,  // bad rate / zero block / over-limit frames / bad t0
  SinkFailure,    // I/O or persistence failure; never a partial success
  VoiceFailure,   // a stateful voice refused a block; samples already
                  // consumed stay consumed
};

struct RenderStats {
  std::uint64_t framesRendered = 0;
  double peakAbs = 0.0;  // over finite values only
  std::uint64_t nanCount = 0;
  std::uint64_t infCount = 0;
  std::uint64_t overloadCount = 0;  // finite |x| > 1.0
  [[nodiscard]] bool isClean() const noexcept {
    return nanCount == 0 && infCount == 0 && overloadCount == 0;
  }
};

// Mono frames in Phase 2.5 (channels are named correctly from the start;
// stereo is post-MVP). Return false to abort the render with SinkFailure.
struct IAudioSink {
  virtual ~IAudioSink() = default;
  virtual bool consume(const double* frames, FrameCount n) = 0;
};

// Computes everything, produces nothing: CI, benchmarks, determinism,
// counting, NaN/Inf/overload detection, profiling.
class NullSink : public IAudioSink {
 public:
  bool consume(const double* frames, FrameCount n) override {
    accumulate(frames, n);
    return true;
  }
  [[nodiscard]] const RenderStats& stats() const noexcept { return stats_; }

 protected:
  void accumulate(const double* frames, FrameCount n);
  RenderStats stats_;
};

// Test-only memory sink: same buffers, retained for inspection.
class MemorySink : public IAudioSink {
 public:
  bool consume(const double* frames, FrameCount n) override {
    accumulate(frames, n);
    samples_.insert(samples_.end(), frames,
                    frames + static_cast<std::size_t>(n.value()));
    return true;
  }
  [[nodiscard]] const std::vector<double>& samples() const noexcept {
    return samples_;
  }
  [[nodiscard]] const RenderStats& stats() const noexcept { return stats_; }

 protected:
  void accumulate(const double* frames, FrameCount n);
  RenderStats stats_;
  std::vector<double> samples_;
};

inline void accumulateStats(RenderStats& s, const double* frames,
                            FrameCount n) {
  const auto count = static_cast<std::uint64_t>(n.value());
  for (std::uint64_t i = 0; i < count; ++i) {
    const double x = frames[i];
    if (std::isnan(x)) {
      ++s.nanCount;
    } else if (std::isinf(x)) {
      ++s.infCount;
    } else {
      if (x > 1.0 || x < -1.0) ++s.overloadCount;
      const double a = x < 0.0 ? -x : x;
      if (a > s.peakAbs) s.peakAbs = a;
    }
    ++s.framesRendered;
  }
}

inline void NullSink::accumulate(const double* frames, FrameCount n) {
  accumulateStats(stats_, frames, n);
}

inline void MemorySink::accumulate(const double* frames, FrameCount n) {
  accumulateStats(stats_, frames, n);
}

}  // namespace mmathen
