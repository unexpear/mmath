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

// Genesis 6 IR tail still carries Genelec loudspeaker colouration after the
// direct segment is replaced by an ideal impulse. Measured IR band energy
// sits ~1.5 dB higher in 500-700 Hz than 400-500 Hz, which pulls studio
// 400-500 share below dry (v35). A mild peaking boost at 450 Hz compensates;
// gain TUNED against dry/real share. Physics of the compensation is the
// known loudspeaker-in-tail limitation (see comment above); the dB value is
// not from a Genelec datasheet.
inline constexpr double kStudioIrTailEqHz = 450.0;
inline constexpr double kStudioIrTailEqQ = 1.0;       // UNSOURCED width
inline constexpr double kStudioIrTailEqGainDb = 4.0;  // TUNED (v36: studio 400-500 share)
// Direct-window length from Kuttruff's mean free path, l = 4V/S (Kuttruff,
// "Room Acoustics", ch. 4): energy arriving within one mean free path of
// the direct sound is the source's own response plus at most first-order
// reflections; beyond it the field is treated as reverberant. S from a
// cube of volume V (S = 6 V^(2/3)); room shape UNSOURCED. Genesis 6 is
// 35.2 m^3 (OpenAIR) -> about 6.4 ms. Measured studio (v37): the 2.5 ms
// window left the Genelec 8040's own 3-15 ms response in the "reverb",
// where it interfered with the direct path and raised the 1.95 s
// loudness cycle (0.18 -> about 0.10 with this window).
[[nodiscard]] inline double kuttruffMeanFreePathMs(double volumeM3, double soundSpeed = 343.0) {
  if (!(volumeM3 > 0.0)) return 2.5;
  const double s = 6.0 * std::pow(volumeM3, 2.0 / 3.0);
  return 1000.0 * 4.0 * volumeM3 / (s * soundSpeed);
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
  // The measured direct segment carries the measurement loudspeaker's own
  // response; the violin model already radiates, so the direct path is an
  // ideal impulse with the segment's energy (sign of the peak). The tail
  // still contains the loudspeaker colouration (limitation).
  double e = 0.0;
  for (std::size_t i = 0; i < end; ++i) { e += ir[i] * ir[i]; }
  const double sign = ir[peak] < 0.0 ? -1.0 : 1.0;
  for (std::size_t i = 0; i < end; ++i) ir[i] = 0.0;
  ir[peak] = sign * std::sqrt(e) * g;
  // Mild peaking EQ on the TAIL only (i >= end). The ideal direct impulse at
  // peak is left unchanged so 1/r direct energy is exact; the EQ compensates
  // loudspeaker colouration in the diffuse remainder.
  {
    const double A = std::pow(10.0, kStudioIrTailEqGainDb / 40.0);
    const double w0 = 2.0 * 3.14159265358979323846 * kStudioIrTailEqHz / sampleRate;
    const double alpha = std::sin(w0) / (2.0 * kStudioIrTailEqQ);
    const double b0 = 1.0 + alpha * A, b1 = -2.0 * std::cos(w0), b2 = 1.0 - alpha * A;
    const double a0 = 1.0 + alpha / A, a1 = -2.0 * std::cos(w0), a2 = 1.0 - alpha / A;
    double x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    for (std::size_t i = end; i < ir.size(); ++i) {
      const double x0 = ir[i];
      const double y0 = (b0 * x0 + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0;
      x2 = x1; x1 = x0; y2 = y1; y1 = y0; ir[i] = y0;
    }
  }
  std::vector<double> out = convolveRoom(dry, ir);
  out.resize(dry.size());
  return out;
}

}  // namespace mmathen
