#pragma once

// D22 — WAV reader (stdlib only): the inverse of wav.h, and enough of the
// format to open files made elsewhere.
//
// Accepted: RIFF/WAVE with a "fmt " chunk and a "data" chunk, in any order,
// other chunks (LIST, fact, cue, ...) skipped with RIFF's even-byte padding;
// format tag 1 (integer PCM: 16, 24 or 32 bits), 3 (IEEE float, 32 bits),
// or 0xFFFE (WAVE_FORMAT_EXTENSIBLE) whose SubFormat GUID is PCM or float;
// 1..32 channels; interleaved frames.
//
// Decoding scale is the exact inverse of wav.h's symmetric encoder:
//   16-bit v -> v / 32767,  24-bit v -> v / 8388607,  32-bit v -> v / 2147483647
// so a file written by WavSink reads back as exactly encodeSample(x)/32767.
// The most negative code (e.g. -32768, which WavSink never writes) maps to
// just below -1 (-1.0000305...) and is reported, not clamped: WavData
// records how many samples exceeded [-1, 1]. Float samples are taken as
// stored; non-finite float samples are rejected (InvalidArgument), never
// passed on.
//
// Errors: a buffer that is not RIFF/WAVE, has no fmt/data chunk, or has
// truncated chunks is InvalidArgument; a valid file in an unsupported
// encoding (ADPCM, 8-bit, 64-bit float, ...) is DomainError; a declared
// data size larger than what is present is RangeError (never a partial
// silent read).

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../units/physical_units.h"

namespace mmathen {

struct WavData {
  SampleRateHz rate{0.0};
  int channels = 0;
  int bitsPerSample = 0;
  bool isFloat = false;
  std::size_t frames = 0;
  std::vector<double> samples;  // interleaved, frames * channels
  std::size_t outOfRange = 0;   // samples with |x| > 1

  // One channel, 0-based.
  [[nodiscard]] std::vector<double> channel(int c) const {
    std::vector<double> out(frames);
    for (std::size_t i = 0; i < frames; ++i)
      out[i] = samples[i * static_cast<std::size_t>(channels) +
                       static_cast<std::size_t>(c)];
    return out;
  }
  // Mean of all channels per frame (a documented mixdown, not a pan law).
  [[nodiscard]] std::vector<double> mono() const {
    std::vector<double> out(frames, 0.0);
    const auto ch = static_cast<std::size_t>(channels);
    for (std::size_t i = 0; i < frames; ++i) {
      double s = 0.0;
      for (std::size_t c = 0; c < ch; ++c) s += samples[i * ch + c];
      out[i] = s / static_cast<double>(ch);
    }
    return out;
  }
};

namespace detail {
inline std::uint32_t le32(const unsigned char* p) {
  return static_cast<std::uint32_t>(p[0]) |
         (static_cast<std::uint32_t>(p[1]) << 8) |
         (static_cast<std::uint32_t>(p[2]) << 16) |
         (static_cast<std::uint32_t>(p[3]) << 24);
}
inline std::uint16_t le16(const unsigned char* p) {
  return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}
}  // namespace detail

[[nodiscard]] inline Result<WavData, QuantityError> parseWav(
    const std::vector<unsigned char>& bytes) {
  using R = Result<WavData, QuantityError>;
  const std::size_t size = bytes.size();
  const unsigned char* b = bytes.data();
  if (size < 12 || std::memcmp(b, "RIFF", 4) != 0 ||
      std::memcmp(b + 8, "WAVE", 4) != 0)
    return R::failure(QuantityError::InvalidArgument);
  std::size_t pos = 12;
  bool haveFmt = false;
  std::uint16_t format = 0, channels = 0, bits = 0;
  std::uint32_t rate = 0;
  const unsigned char* data = nullptr;
  std::size_t dataSize = 0;
  bool dataTruncated = false;
  while (pos + 8 <= size) {
    const unsigned char* id = b + pos;
    const std::uint32_t len = detail::le32(b + pos + 4);
    const std::size_t body = pos + 8;
    if (std::memcmp(id, "fmt ", 4) == 0) {
      if (len < 16 || body + len > size)
        return R::failure(QuantityError::InvalidArgument);
      format = detail::le16(b + body);
      channels = detail::le16(b + body + 2);
      rate = detail::le32(b + body + 4);
      bits = detail::le16(b + body + 14);
      if (format == 0xFFFE) {
        // WAVE_FORMAT_EXTENSIBLE: cbSize(2) validBits(2) mask(4) GUID(16);
        // the GUID's first two bytes carry the real format tag.
        if (len < 40) return R::failure(QuantityError::InvalidArgument);
        format = detail::le16(b + body + 24);
      }
      haveFmt = true;
    } else if (std::memcmp(id, "data", 4) == 0) {
      data = b + body;
      if (body + len > size) {
        dataTruncated = true;
        dataSize = size - body;
      } else {
        dataSize = len;
      }
    }
    // Advance with RIFF even-byte padding; stop if the chunk overruns.
    const std::size_t next = body + len + (len & 1U);
    if (next <= pos) break;
    pos = next;
  }
  if (!haveFmt || data == nullptr)
    return R::failure(QuantityError::InvalidArgument);
  if (dataTruncated) return R::failure(QuantityError::RangeError);
  if (channels < 1 || channels > 32 || rate == 0)
    return R::failure(QuantityError::InvalidArgument);
  const bool pcm = format == 1 && (bits == 16 || bits == 24 || bits == 32);
  const bool flt = format == 3 && bits == 32;
  if (!pcm && !flt) return R::failure(QuantityError::DomainError);
  const std::size_t bytesPer = bits / 8U;
  const std::size_t frameBytes = bytesPer * channels;
  WavData w;
  w.rate = SampleRateHz{static_cast<double>(rate)};
  w.channels = channels;
  w.bitsPerSample = bits;
  w.isFloat = flt;
  w.frames = dataSize / frameBytes;
  w.samples.resize(w.frames * channels);
  for (std::size_t i = 0; i < w.samples.size(); ++i) {
    const unsigned char* p = data + i * bytesPer;
    double v = 0.0;
    if (flt) {
      float f;
      std::uint32_t u = detail::le32(p);
      std::memcpy(&f, &u, 4);
      if (!std::isfinite(f)) return R::failure(QuantityError::InvalidArgument);
      v = static_cast<double>(f);
    } else if (bits == 16) {
      v = static_cast<double>(static_cast<std::int16_t>(detail::le16(p))) /
          32767.0;
    } else if (bits == 24) {
      std::int32_t s = static_cast<std::int32_t>(p[0] | (p[1] << 8) |
                                                 (p[2] << 16));
      if (s & 0x800000) s -= 0x1000000;  // sign-extend 24 -> 32
      v = static_cast<double>(s) / 8388607.0;
    } else {
      v = static_cast<double>(static_cast<std::int32_t>(detail::le32(p))) /
          2147483647.0;
    }
    if (std::fabs(v) > 1.0) ++w.outOfRange;
    w.samples[i] = v;
  }
  return R::success(std::move(w));
}

// Read and parse a file. Unopenable path: RangeError.
[[nodiscard]] inline Result<WavData, QuantityError> readWavFile(
    const std::string& path) {
  using R = Result<WavData, QuantityError>;
  std::ifstream f(path, std::ios::binary);
  if (!f) return R::failure(QuantityError::RangeError);
  std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(f)),
                                   std::istreambuf_iterator<char>());
  return parseWav(bytes);
}

}  // namespace mmathen
