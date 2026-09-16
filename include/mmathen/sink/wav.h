#pragma once

// Phase 2.5 — minimal deterministic WAV sink (stdlib only).
//
// Fixed format: mono, signed 16-bit PCM, little-endian. Declared policies:
//   - rounding: round-half-away-from-zero (llround) of clamp(x)*32767;
//   - range: inputs outside [-1,1] are counted as overload and clamped
//     (certification renders must have zero); non-finite inputs encode as 0
//     (the render is already flagged unclean via stats; the encoder never
//     emits conversion garbage);
//   - scale: +1.0 -> +32767, -1.0 -> -32767 (symmetric; -32768 never emitted);
//   - layout: canonical 44-byte header (RIFF/WAVE, fmt , data); the total
//     frame count N is known upfront (frame count is authoritative), so the
//     header is complete before the first sample and data streams in blocks;
//   - header fields are 32-bit: the rate must be a positive integer with
//     2*rate fitting uint32 (byte rate), and frames are capped by
//     kMaxRenderFrames; violations are InvalidConfig, never silent wrap;
//   - failure: open() validates rate/frames; consume() records I/O errors and
//     returns false to abort; close() verifies bytes-written == declared and
//     reports SinkFailure otherwise. No success message on a partial file.

#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>

#include "../core/result.h"
#include "../units/physical_units.h"
#include "sink.h"

namespace mmathen {

class WavSink : public IAudioSink {
 public:
  WavSink() = default;
  WavSink(const WavSink&) = delete;
  WavSink& operator=(const WavSink&) = delete;
  WavSink(WavSink&&) = default;
  WavSink& operator=(WavSink&&) = default;

  // Testable in isolation: the exact PCM mapping, no file involved.
  [[nodiscard]] static std::int16_t encodeSample(double x) {
    if (!std::isfinite(x)) return 0;
    double c = x;
    if (c > 1.0) c = 1.0;
    if (c < -1.0) c = -1.0;
    return static_cast<std::int16_t>(std::llround(c * 32767.0));
  }

  [[nodiscard]] static Result<WavSink, RenderError> open(
      const std::string& path, SampleRateHz rate, FrameCount totalFrames) {
    using R = Result<WavSink, RenderError>;
    const double r = rate.value();
    const std::uint64_t n =
        static_cast<std::uint64_t>(totalFrames.value());
    if (!std::isfinite(r) || !(r > 0.0) || r != std::floor(r) ||
        r > 2147483647.0)
      return R::failure(RenderError::InvalidConfig);
    if (n > kMaxRenderFrames) return R::failure(RenderError::InvalidConfig);

    WavSink sink;
    sink.out_.open(path, std::ios::binary | std::ios::trunc);
    if (!sink.out_.is_open()) return R::failure(RenderError::SinkFailure);
    sink.totalFrames_ = n;
    sink.rate_ = static_cast<std::uint32_t>(r);
    sink.writeHeader();
    if (!sink.out_.good()) return R::failure(RenderError::SinkFailure);
    return R::success(std::move(sink));
  }

  bool consume(const double* frames, FrameCount n) override {
    if (!out_.good()) return false;
    const auto count = static_cast<std::uint64_t>(n.value());
    for (std::uint64_t i = 0; i < count; ++i) {
      const std::int16_t s = encodeSample(frames[i]);
      putU16(static_cast<std::uint16_t>(s));
      ++writtenFrames_;
    }
    return static_cast<bool>(out_.good());
  }

  [[nodiscard]] Result<FrameCount, RenderError> close() {
    using R = Result<FrameCount, RenderError>;
    out_.flush();
    const bool ok =
        out_.good() && writtenFrames_ == totalFrames_;
    out_.close();
    if (!ok) return R::failure(RenderError::SinkFailure);
    return R::success(FrameCount{writtenFrames_});
  }

 private:
  void putU16(std::uint16_t v) {
    out_.put(static_cast<char>(v & 0xFF));
    out_.put(static_cast<char>((v >> 8) & 0xFF));
  }
  void putU32(std::uint32_t v) {
    putU16(static_cast<std::uint16_t>(v & 0xFFFF));
    putU16(static_cast<std::uint16_t>((v >> 16) & 0xFFFF));
  }
  void putBytes(const char* s, int n) { out_.write(s, n); }

  void writeHeader() {
    const std::uint32_t dataSize =
        static_cast<std::uint32_t>(totalFrames_ * 2U);
    putBytes("RIFF", 4);
    putU32(36U + dataSize);
    putBytes("WAVE", 4);
    putBytes("fmt ", 4);
    putU32(16U);          // fmt chunk size
    putU16(1U);           // PCM
    putU16(1U);           // mono
    putU32(rate_);        // sample rate
    putU32(rate_ * 2U);   // byte rate
    putU16(2U);           // block align
    putU16(16U);          // bits per sample
    putBytes("data", 4);
    putU32(dataSize);
  }

  std::ofstream out_;
  std::uint64_t totalFrames_ = 0;
  std::uint64_t writtenFrames_ = 0;
  std::uint32_t rate_ = 0;
};

}  // namespace mmathen
