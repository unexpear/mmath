#pragma once
// Room stage: offline convolution of a dry render with a measured room
// impulse response (one FFT of the full length). The engine stores no
// room: the caller supplies a measured IR and must cite it. Output length
// is dry + ir - 1.
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

#include "../analysis/fft.h"

namespace mmathen {

[[nodiscard]] inline std::vector<double> convolveRoom(const std::vector<double>& dry,
                                                      const std::vector<double>& ir) {
  if (dry.empty() || ir.empty()) return {};
  const std::size_t len = dry.size() + ir.size() - 1;
  std::size_t n = 1;
  while (n < len) n <<= 1;
  std::vector<std::complex<double>> a(n), b(n);
  for (std::size_t i = 0; i < dry.size(); ++i) a[i] = dry[i];
  for (std::size_t i = 0; i < ir.size(); ++i) b[i] = ir[i];
  detail::fftInPlace(a, false);
  detail::fftInPlace(b, false);
  for (std::size_t i = 0; i < n; ++i) a[i] *= b[i];
  detail::fftInPlace(a, true);
  // Normalise by the delta response so either inverse convention works.
  std::vector<std::complex<double>> d(n);
  d[0] = 1.0;
  detail::fftInPlace(d, false);
  detail::fftInPlace(d, true);
  const double scale = 1.0 / d[0].real();
  std::vector<double> out(len);
  for (std::size_t i = 0; i < len; ++i) out[i] = a[i].real() * scale;
  return out;
}

// Partial-wet room: (1 - wet) dry + wet (dry * ir), trimmed to the dry
// length. wet is the caller's choice (no published value implied).
[[nodiscard]] inline std::vector<double> mixRoom(const std::vector<double>& dry,
                                                 const std::vector<double>& ir, double wet) {
  std::vector<double> out = convolveRoom(dry, ir);
  out.resize(dry.size());
  for (std::size_t i = 0; i < dry.size(); ++i) out[i] = (1.0 - wet) * dry[i] + wet * out[i];
  return out;
}

// Studio environment: re-place the microphone of a measured room IR.
// The IR was measured at irDistanceM. The direct sound (the first
// directMs after the IR's peak, plus pre-roll) follows spherical spreading,
// pressure ~ 1/r, so it is scaled by irDistanceM / micDistanceM; the
// reverberant remainder is diffuse and is kept unchanged (Kuttruff, "Room
// Acoustics", diffuse-field theory). The direct-to-reverb ratio therefore
// follows from the distances alone; no wet fraction is chosen. Source
// directivity is taken as omnidirectional (UNSOURCED for the violin).
[[nodiscard]] inline std::vector<double> studioRoom(const std::vector<double>& dry, std::vector<double> ir,
                                                    double irDistanceM, double micDistanceM,
                                                    double sampleRate, double directMs = 2.5) {
  if (ir.empty() || !(irDistanceM > 0.0) || !(micDistanceM > 0.0)) return dry;
  std::size_t peak = 0;
  for (std::size_t i = 1; i < ir.size(); ++i) if (std::abs(ir[i]) > std::abs(ir[peak])) peak = i;
  const std::size_t end = std::min(ir.size(), peak + static_cast<std::size_t>(directMs * 1e-3 * sampleRate));
  const double g = irDistanceM / micDistanceM;
  for (std::size_t i = 0; i < end; ++i) ir[i] *= g;
  std::vector<double> out = convolveRoom(dry, ir);
  out.resize(dry.size());
  return out;
}

}  // namespace mmathen
