// Audio-only experiment: extended arithmetic vs a change to spectral features.
// The public detector and desktop are unchanged until independent validation.
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include "../../app/analysis_profiles.h"
#include <mmathen/sink/wav_reader.h>

using namespace mmathen;

template<class Real> class Transform {
  std::vector<std::vector<std::complex<Real>>> roots_;
public:
  explicit Transform(std::size_t n) {
    for (std::size_t len = 2; len <= n; len *= 2) {
      std::vector<std::complex<Real>> roots(len / 2);
      for (std::size_t k = 0; k < roots.size(); ++k) {
        const Real angle = -2 * std::numbers::pi_v<Real> * static_cast<Real>(k) / static_cast<Real>(len);
        roots[k] = {std::cos(angle), std::sin(angle)};
      }
      roots_.push_back(std::move(roots));
    }
  }
  void apply(std::vector<std::complex<Real>>& a) const {
    const auto n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
      std::size_t bit = n >> 1;
      for (; j & bit; bit >>= 1) j ^= bit;
      j ^= bit;
      if (i < j) std::swap(a[i], a[j]);
    }
    std::size_t stage = 0;
    for (std::size_t len = 2; len <= n; len *= 2, ++stage)
      for (std::size_t start = 0; start < n; start += len)
        for (std::size_t k = 0; k < len / 2; ++k) {
          const auto u = a[start + k], v = a[start + k + len / 2] * roots_[stage][k];
          a[start + k] = u + v;
          a[start + k + len / 2] = u - v;
        }
  }
};

template<class Real> Chromagram features(const WavData& w, int padding, bool peaks) {
  const auto o = app::guitarChordOptions().chroma;
  const std::size_t n = o.frameSize, fftSize = n * static_cast<std::size_t>(padding);
  const Real fs = static_cast<Real>(w.rate.value());
  std::vector<Real> window(n), power(fftSize / 2 + 1);
  Real energy = 0;
  for (std::size_t i = 0; i < n; ++i) {
    window[i] = Real(.5) - Real(.5) * std::cos(2 * std::numbers::pi_v<Real> * static_cast<Real>(i) / static_cast<Real>(n));
    energy += window[i] * window[i];
  }
  const Real ref = static_cast<Real>(fftSize) * energy / 4;
  std::vector<std::complex<Real>> buf(fftSize);
  Transform<Real> fft(fftSize);
  Chromagram out; out.hopSeconds = static_cast<double>(o.hop) / w.rate.value();
  const auto count = w.frames <= n ? 1 : (w.frames - n) / o.hop + 1;
  for (std::size_t frame = 0; frame < count; ++frame) {
    std::fill(buf.begin(), buf.end(), std::complex<Real>{});
    for (std::size_t i = 0; i < n && frame * o.hop + i < w.frames; ++i)
      buf[i] = {static_cast<Real>(w.samples[frame * o.hop + i]) * window[i], 0};
    fft.apply(buf);
    for (std::size_t k = 0; k < power.size(); ++k) power[k] = std::norm(buf[k]) / ref;
    Real total = 0, maximum = 0;
    for (std::size_t k = 1; k < power.size(); ++k) {
      const Real hz = static_cast<Real>(k) * fs / static_cast<Real>(fftSize);
      if (hz < o.fMinHz || hz > o.fMaxHz) continue;
      total += power[k]; maximum = std::max(maximum, power[k]);
    }
    const bool gate = !(total > 0) || 10 * std::log10(total) < o.energyGateDb;
    std::array<Real, 12> c{};
    for (std::size_t k = 1; k + 1 < power.size(); ++k) {
      Real hz = static_cast<Real>(k) * fs / static_cast<Real>(fftSize), weight = power[k];
      if (peaks) {
        if (!(power[k] > power[k-1] && power[k] >= power[k+1] && power[k] > maximum * Real(.0001))) continue;
        const Real a = std::log(std::max(power[k-1], std::numeric_limits<Real>::min()));
        const Real b = std::log(power[k]);
        const Real d = std::log(std::max(power[k+1], std::numeric_limits<Real>::min()));
        const Real den = a - 2 * b + d;
        const Real offset = den < 0 ? std::clamp(Real(.5) * (a - d) / den, Real(-.5), Real(.5)) : 0;
        hz = (static_cast<Real>(k) + offset) * fs / static_cast<Real>(fftSize);
        weight = std::exp(b - Real(.25) * (a - d) * offset);
      }
      if (hz < o.fMinHz || hz > o.fMaxHz) continue;
      const auto midi = std::lround(69 + 12 * std::log2(hz / static_cast<Real>(o.tuningHz)));
      c[static_cast<std::size_t>((midi % 12 + 12) % 12)] += weight;
    }
    const auto mx = *std::max_element(c.begin(), c.end());
    Real norm = 0;
    for (auto& v : c) { v = mx > 0 ? std::log1p(static_cast<Real>(o.compression) * v / mx) : 0; norm += v*v; }
    std::array<double, 12> result{};
    for (std::size_t i = 0; i < 12; ++i) result[i] = !gate && norm > 0 ? static_cast<double>(c[i] / std::sqrt(norm)) : 0;
    out.frames.push_back(result); out.gated.push_back(gate);
  }
  return out;
}

std::vector<std::size_t> decode(const Chromagram& cg) {
  const auto o = app::guitarChordOptions();
  std::vector<std::vector<double>> transitions(kChordCount, std::vector<double>(kChordCount, (1-o.selfTransition)/(kChordCount-1)));
  for (std::size_t i = 0; i < kChordCount; ++i) transitions[i][i] = o.selfTransition;
  auto hmm = makeHmm(std::vector<double>(kChordCount, 1.0/kChordCount), transitions);
  LogMatrix emissions;
  for (std::size_t i = 0; i < cg.frames.size(); ++i) {
    const auto scores = chordScores(cg.frames[i], cg.gated[i], o.noChordPenalty);
    std::vector<double> row;
    for (double v : scores) row.push_back(o.sharpness * (v-1));
    emissions.push_back(std::move(row));
  }
  return viterbi(hmm.value(), emissions).value().states;
}

int main(int argc, char** argv) {
  if (argc < 3) return 2;
  const auto wr = readWavFile(argv[1]); if (!wr.ok() || wr.value().channels != 1) return 1;
  const auto& w = wr.value();
  const auto baseline = chromagram(w.samples, w.rate, app::guitarChordOptions().chroma);
  if (!baseline.ok()) return 1;
  const auto states = decode(baseline.value());
  std::ofstream out(argv[2]); if (!out) return 1;
  out << std::setprecision(17) << "{\"sampleRate\":" << w.rate.value() << ",\"samples\":" << w.frames
      << ",\"extendedMantissaBits\":" << std::numeric_limits<long double>::digits << ",\"variants\":{";
  bool first = true;
  const auto write = [&](const char* name, const Chromagram& cg) {
    const auto labels = decode(cg);
    double delta = 0; std::size_t changed = 0;
    for (std::size_t i = 0; i < labels.size(); ++i) {
      if (labels[i] != states[i]) ++changed;
      for (std::size_t k = 0; k < 12; ++k) delta = std::max(delta, std::abs(cg.frames[i][k] - baseline.value().frames[i][k]));
    }
    if (!first) out << ',';
    first = false;
    out << '"' << name << "\":{\"maxFeatureDelta\":" << delta << ",\"changedFrames\":" << changed << ",\"chords\":[";
    for (std::size_t i = 0; i < labels.size(); ++i) {
      if (i) out << ',';
      out << '[' << (static_cast<double>(i*2048)+4096)/w.rate.value() << ',' << labels[i] << ',' << labels[i] << ']';
    }
    out << "]}";
  };
  write("baseline", baseline.value());
  if (argc == 3) {
    write("double-control", features<double>(w, 1, false));
    write("extended", features<long double>(w, 1, false));
  }
  if (argc == 3 || std::string(argv[3]) == "peaks") write("peaks", features<double>(w, 1, true));
  if (argc == 3 || std::string(argv[3]) == "peaks2") write("peaks2", features<double>(w, 2, true));
  out << "}}\n";
  return out ? 0 : 1;
}
