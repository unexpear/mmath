#pragma once

// D32 — multichannel WAV writer: 1..8 interleaved channels, signed 16- or
// 24-bit PCM, little-endian. The certified mono sink (wav.h) is left
// untouched. This writer uses the same declared policies, so a mono 16-bit
// file is byte-identical to WavSink's (tested):
//   rounding  round-half-away-from-zero (llround) of clamp(x) * full scale,
//             full scale 32767 (16-bit) or 8388607 (24-bit), symmetric;
//   range     |x| > 1 is clamped and COUNTED (overloads()); non-finite
//             samples encode as 0 and are counted too;
//   layout    canonical 44-byte header written first (the frame count is
//             declared up front); close() verifies that every declared
//             frame was written, otherwise SinkFailure (no partial success).
// No dither is applied (a stated choice: 24-bit output makes it moot; for
// 16-bit, the quantisation error is plain rounding, <= 1/2 LSB).
// Round trip through wav_reader.h's parseWav is exact: parse(write(x)) =
// round(clamp(x) * fs) / fs for either depth (tested).

#include <cmath>
#include <cstdint>
#include <fstream>
#include <string>

#include "../core/result.h"
#include "../units/physical_units.h"
#include "sink.h"

namespace mmathen {

class WavWriter {
 public:
  WavWriter() = default;
  WavWriter(const WavWriter&) = delete;
  WavWriter& operator=(const WavWriter&) = delete;
  WavWriter(WavWriter&&) = default;
  WavWriter& operator=(WavWriter&&) = default;

  // channels 1..8, bits 16 or 24; rate a positive integer whose byte rate
  // fits 32 bits; frames <= kMaxRenderFrames and the data size < 4 GiB.
  [[nodiscard]] static Result<WavWriter, RenderError> open(
      const std::string& path, SampleRateHz rate, FrameCount totalFrames,
      int channels, int bits = 16) {
    using R = Result<WavWriter, RenderError>;
    const double r = rate.value();
    const auto n = static_cast<std::uint64_t>(totalFrames.value());
    if (channels < 1 || channels > 8 || (bits != 16 && bits != 24) ||
        !std::isfinite(r) || !(r > 0.0) || r != std::floor(r))
      return R::failure(RenderError::InvalidConfig);
    const std::uint64_t block =
        static_cast<std::uint64_t>(channels) * static_cast<std::uint64_t>(bits / 8);
    if (r * static_cast<double>(block) > 4294967295.0 || n > kMaxRenderFrames ||
        n * block + 36U > 4294967295ULL)
      return R::failure(RenderError::InvalidConfig);
    WavWriter w;
    w.out_.open(path, std::ios::binary | std::ios::trunc);
    if (!w.out_.is_open()) return R::failure(RenderError::SinkFailure);
    w.total_ = n;
    w.rate_ = static_cast<std::uint32_t>(r);
    w.channels_ = channels;
    w.bytes_ = bits / 8;
    w.writeHeader();
    if (!w.out_.good()) return R::failure(RenderError::SinkFailure);
    return R::success(std::move(w));
  }

  // The exact integer code for x at this depth (no file involved).
  [[nodiscard]] static std::int32_t encode(double x, int bits,
                                           bool* overload = nullptr) {
    if (!std::isfinite(x)) {
      if (overload) *overload = true;
      return 0;
    }
    double c = x;
    if (c > 1.0 || c < -1.0) {
      if (overload) *overload = true;
      c = c > 1.0 ? 1.0 : -1.0;
    }
    const double fs = bits == 24 ? 8388607.0 : 32767.0;
    return static_cast<std::int32_t>(std::llround(c * fs));
  }

  // n interleaved frames (n * channels values).
  bool write(const double* interleaved, std::size_t n) {
    if (!out_.good()) return false;
    const std::size_t count = n * static_cast<std::size_t>(channels_);
    for (std::size_t i = 0; i < count; ++i) {
      bool over = false;
      const std::int32_t v = encode(interleaved[i], bytes_ * 8, &over);
      if (over) ++overloads_;
      const auto u = static_cast<std::uint32_t>(v);
      for (int b = 0; b < bytes_; ++b)
        out_.put(static_cast<char>((u >> (8 * b)) & 0xFF));
    }
    written_ += n;
    return static_cast<bool>(out_.good());
  }

  [[nodiscard]] Result<FrameCount, RenderError> close() {
    using R = Result<FrameCount, RenderError>;
    out_.flush();
    const bool ok = out_.good() && written_ == total_;
    out_.close();
    if (!ok) return R::failure(RenderError::SinkFailure);
    return R::success(FrameCount{written_});
  }

  [[nodiscard]] std::uint64_t overloads() const noexcept { return overloads_; }

 private:
  void putU16(std::uint16_t v) {
    out_.put(static_cast<char>(v & 0xFF));
    out_.put(static_cast<char>((v >> 8) & 0xFF));
  }
  void putU32(std::uint32_t v) {
    putU16(static_cast<std::uint16_t>(v & 0xFFFF));
    putU16(static_cast<std::uint16_t>((v >> 16) & 0xFFFF));
  }
  void writeHeader() {
    const auto block = static_cast<std::uint32_t>(channels_ * bytes_);
    const auto dataSize = static_cast<std::uint32_t>(total_ * block);
    out_.write("RIFF", 4);
    putU32(36U + dataSize);
    out_.write("WAVE", 4);
    out_.write("fmt ", 4);
    putU32(16U);
    putU16(1U);  // PCM
    putU16(static_cast<std::uint16_t>(channels_));
    putU32(rate_);
    putU32(rate_ * block);
    putU16(static_cast<std::uint16_t>(block));
    putU16(static_cast<std::uint16_t>(bytes_ * 8));
    out_.write("data", 4);
    putU32(dataSize);
  }

  std::ofstream out_;
  std::uint64_t total_ = 0;
  std::uint64_t written_ = 0;
  std::uint64_t overloads_ = 0;
  std::uint32_t rate_ = 0;
  int channels_ = 1;
  int bytes_ = 2;
};

}  // namespace mmathen
