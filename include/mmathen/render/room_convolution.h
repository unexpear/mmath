#pragma once
// Room stage: offline convolution of a dry render with a measured room
// impulse response (one FFT of the full length). The engine stores no
// room: the caller supplies a measured IR and must cite it. Output length
// is dry + ir - 1.
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

}  // namespace mmathen
