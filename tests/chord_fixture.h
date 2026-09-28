#pragma once
// Shared fixture for the D43 chord tests: a functional-harmony progression
// source (a first-order Markov chain over scale degrees, in any key) and a
// synthesiser for plucked harmonic triads with a bass note, a melody of
// passing (non-chord) tones, noise, and drum-like bursts. Ground truth is
// known per sample.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

#include <mmathen/analysis/chords.h>
#include <mmathen/stats/random.h>

namespace chordfix {

// Degrees I ii iii IV V vi: semitone offsets and quality (true = minor).
inline constexpr int kOffset[6] = {0, 2, 4, 5, 7, 9};
inline constexpr bool kMinor[6] = {false, true, true, false, false, true};
inline const std::vector<std::vector<double>> kDegreeChain = {
    {0.00, 0.15, 0.05, 0.30, 0.30, 0.20},   // I
    {0.20, 0.00, 0.00, 0.10, 0.60, 0.10},   // ii
    {0.00, 0.20, 0.00, 0.30, 0.00, 0.50},   // iii
    {0.35, 0.10, 0.00, 0.00, 0.40, 0.15},   // IV
    {0.60, 0.00, 0.00, 0.15, 0.00, 0.25},   // V
    {0.00, 0.25, 0.15, 0.40, 0.20, 0.00}};  // vi

inline std::size_t chordOf(int key, int degree) {
  const int root = (key + kOffset[degree]) % 12;
  return static_cast<std::size_t>(kMinor[degree] ? 12 + root : root);
}

// A progression of `len` chords (chord indices), starting on I.
inline std::vector<std::size_t> progression(int key, std::size_t len, mmathen::SplitMix64& rng) {
  std::vector<std::size_t> out;
  int d = 0;
  for (std::size_t i = 0; i < len; ++i) {
    out.push_back(chordOf(key, d));
    d = static_cast<int>(rng.categorical(kDegreeChain[static_cast<std::size_t>(d)]));
  }
  return out;
}

inline double midiHz(double m) { return 440.0 * std::pow(2.0, (m - 69.0) / 12.0); }

struct Piece {
  std::vector<double> audio;
  std::vector<std::size_t> truthPerSample;
};

// Chords of 1.0-2.0 s; passing-tone melody every 0.25 s (half of the
// melody notes are outside the chord); noise at about -30 dB relative to
// the chord; a noise burst on every beat.
inline Piece synthesize(const std::vector<std::size_t>& chords, double fs, std::uint64_t seed) {
  mmathen::SplitMix64 rng{seed};
  Piece p;
  auto addNote = [&](std::vector<double>& buf, std::size_t start, std::size_t len, double hz,
                     double amp, double decay) {
    for (int k = 1; k <= 8; ++k) {
      const double f = hz * k;
      if (f > fs / 2.2) break;
      const double a = amp / std::pow(k, 1.2);
      for (std::size_t i = 0; i < len && start + i < buf.size(); ++i) {
        const double t = static_cast<double>(i) / fs;
        buf[start + i] += a * std::exp(-decay * t) * std::sin(2.0 * std::numbers::pi * f * t);
      }
    }
  };
  std::vector<std::size_t> durs;
  std::size_t total = 0;
  for (std::size_t c = 0; c < chords.size(); ++c) {
    durs.push_back(static_cast<std::size_t>((1.0 + rng.uniform()) * fs));
    total += durs.back();
  }
  p.audio.assign(total, 0.0);
  p.truthPerSample.assign(total, 0);
  std::size_t pos = 0;
  for (std::size_t c = 0; c < chords.size(); ++c) {
    const auto pcs = mmathen::chordPitchClasses(chords[c]);
    const std::size_t len = durs[c];
    for (std::size_t i = 0; i < len; ++i) p.truthPerSample[pos + i] = chords[c];
    addNote(p.audio, pos, len, midiHz(36 + pcs[0]), 0.25, 1.5);  // bass
    for (int q : pcs) addNote(p.audio, pos, len, midiHz(60 + q), 0.12, 1.2);
    // Melody: every 0.25 s, a chord tone or a passing tone a step away.
    const auto step = static_cast<std::size_t>(0.25 * fs);
    for (std::size_t m = 0; m + step / 2 < len; m += step) {
      int note = 72 + pcs[static_cast<std::size_t>(rng.uniform() * 3.0) % 3];
      if (rng.uniform() < 0.5) note += rng.uniform() < 0.5 ? 1 : 2;
      addNote(p.audio, pos + m, std::min(step, len - m), midiHz(note), 0.08, 3.0);
    }
    pos += len;
  }
  // Noise and beat bursts (0.5 s beat, 30 ms bursts).
  const auto beat = static_cast<std::size_t>(0.5 * fs), burst = static_cast<std::size_t>(0.03 * fs);
  for (std::size_t i = 0; i < total; ++i) {
    p.audio[i] += 0.004 * rng.normal();
    if (i % beat < burst) p.audio[i] += 0.08 * rng.normal() * std::exp(-static_cast<double>(i % beat) / (0.008 * fs));
  }
  return p;
}

// Truth for frame f: the chord at the frame's centre.
inline std::vector<std::size_t> frameTruth(const Piece& p, std::size_t frames, std::size_t hop,
                                           std::size_t frameSize) {
  std::vector<std::size_t> t;
  for (std::size_t f = 0; f < frames; ++f)
    t.push_back(p.truthPerSample[std::min(f * hop + frameSize / 2, p.truthPerSample.size() - 1)]);
  return t;
}

}  // namespace chordfix
