// Sustained wind tones.
//
// A blown wind is a limit cycle, not the ring after the player stops.
// These renders use published shapes and example arguments:
//   clarinet — quarter-wave cylinder, Dalmont slit, bore near 15 mm
//     (Benade, Fundamentals of Musical Acoustics). The reflection corner
//     is 600 Hz, under Benade's open-hole cutoff near 1.5 kHz, so the
//     odd partials roll off and the evens are not absent;
//   saxophone — truncated cone, tan(kL) = -k x0, small x0/L so the
//     series is near n c/(2L) (Dalmont; saxophone.h). The reflection
//     corner is 1500 Hz, the high end of the alto tone-hole cutoffs in
//     Petersen, Colinot, Kergomard, and Guillemain, Acta Acustica 4
//     (2020), and inside Chen, Smith, and Wolfe's soprano estimate
//     1340±240 Hz (Acoustics Australia 37, 2009). The radiated pressure
//     then passes Benade and Lutgen's envelope, JASA 83, 1900–1907
//     (1988), with their alto break frequency 837 Hz. A turbulence
//     fraction 0.12 is an argument, not a measured intensity.
//   flute — open pipe, embouchure release, Levine–Schwinger end
//     correction 0.61 a, and a jet delay W/(σ U) with σ = 0.45, inside
//     Coltman's 0.4–0.5 convection range. The jet is tanh with an aim
//     offset. A 400 Hz reflection corner and an aim of 0.4 jet
//     thicknesses keep the fundamental strongest without the overshoot
//     a centered jet leaves in the attack;
//   trumpet and trombone — outward-striking lip below the cone's second
//     root (Fletcher, 1979), near concert Bb3 (233 Hz) and Bb2 (117 Hz).
//     Lip damping, mouth pressure, and apex distance are arguments.
//     No stored alto, tenor, or trumpet length.
// Wall loss, when the corner is positive, is Kirchhoff's wide tube with
// Sutherland's viscosity. A zero corner leaves the ideal bore.
//
// The sample is radiated bell pressure. Every voice uses the same
// listener distance, 1 m, and the same compact monopole
// ρ (dU_bell/dt) / (4 π r). A cone scales that volume flow by
// x_bell / x0 before the radiator; a cylinder does not, because its
// area does not grow. Listening files are loudness-normalized to
// −20 LUFS (ITU-R BS.1770-4, K-weighted over 1–2 s) with a peak
// ceiling of −1 dBFS. That gain is playback only. The assertions read
// the unscaled samples.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/instruments/bore_valve.h>
#include <mmathen/instruments/cone_valve.h>
#include <mmathen/math/constants.h>
#include <mmathen/physics/medium.h>
#include <mmathen/sink/wav.h>
#include <mmathen/sink/wav_path.h>

#include "wind_excerpts.h"

using namespace mmathen;

namespace {

template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

constexpr double kAir = 101325.0;
constexpr double kTemp = 293.15;
constexpr double kFs = 48000.0;
const SampleRateHz kRate{kFs};
constexpr std::uint64_t kFrames = 96000;
// Mouth-pressure ramps. ValveBlow::attackFrames is the performance
// argument; these are the demo values, taken from measured players.
//
// Reed woodwinds — Fuks and Sundberg, KTH TMH-QPSR 3/1996, 41–56
// (Acta Acustica united with Acustica 85, 1999, "Blowing pressures in
// bassoon, clarinet, oboe and saxophone"): a mouth-pressure build-up
// before the tone, then a drop onto the quasi-steady pressure, typically
// lasting 0.35–0.60 s. Clarinet and saxophone both use 0.40 s, inside
// that range. The saxophone blow, 4200 Pa, is about 43 cm H2O, inside
// the alto-saxophone pressures in that paper (about 15–80 cm H2O).
//
// Flute — Terrien, Blanc, and Fabre, ISMA 2014, "Experimental Study of
// Attack Transients in Flute-like Instruments": typical duration about
// 10–30 ms, citing Nolle, Verge, and Castellengo, with an acoustic rise
// near 6 ms + 9.5/f0 (about 40 ms at 280 Hz). The 20 ms ramp is that
// typical duration. Fletcher, Acustica 34, 224–233 (1976), is the
// flue-pipe transient those measurements refer to.
//
// Brass mouth pressure — Bertsch and Hoole, ISMA 2014: tongue-tip
// movement at the start of an attack about 40–128 ms, and about 40–64 ms
// for faster single tonguing. Trumpet 40 ms and trombone 45 ms are that
// fast tongue. tongueReleaseFrames is the moment the tongue lets the
// lip go; the mouth-pressure ramp is separate.
// The sound that follows is compared with Luce and Clark, JASA 42,
// 1232–1243 (1967): trumpet fundamental transient about 10 ms; trombone
// fundamental about seven cycles, higher partials tending toward 70 ms
// above 400 Hz; significant amplitude and frequency change about 50 ms,
// average deviation about 20 ms.
constexpr std::uint64_t kClarinetAttack = 19200;   // 0.40 s
constexpr std::uint64_t kSaxophoneAttack = 19200;  // 0.40 s
constexpr std::uint64_t kFluteAttack = 960;        // 20 ms
constexpr std::uint64_t kTrumpetAttack = 1920;     // 40 ms
constexpr std::uint64_t kTrumpetTongue = 1200;     // 25 ms
constexpr std::uint64_t kTromboneAttack = 2160;    // 45 ms
constexpr std::uint64_t kTromboneTongue = 576;     // 12 ms

std::vector<double> renderOf(IVoice& voice, std::uint64_t frames) {
  std::vector<double> samples(static_cast<std::size_t>(frames));
  REQUIRE(voice.renderBlock(samples.data(), FrameCount{frames}).ok());
  for (double sample : samples) REQUIRE(std::isfinite(sample));
  return samples;
}

double rmsOf(const std::vector<double>& x, std::size_t a, std::size_t b) {
  double energy = 0.0;
  for (std::size_t i = a; i < b; ++i) energy += x[i] * x[i];
  return std::sqrt(energy / static_cast<double>(b - a));
}

double meanOf(const std::vector<double>& x, std::size_t a, std::size_t b) {
  double sum = 0.0;
  for (std::size_t i = a; i < b; ++i) sum += x[i];
  return sum / static_cast<double>(b - a);
}

double goertzel(const std::vector<double>& x, std::size_t a, int n, double frequency,
                double rate) {
  const double omega = constants::twoPi * frequency / rate;
  double real = 0.0;
  double imag = 0.0;
  for (int i = 0; i < n; ++i) {
    const double angle = omega * static_cast<double>(i);
    real += x[a + static_cast<std::size_t>(i)] * std::cos(angle);
    imag -= x[a + static_cast<std::size_t>(i)] * std::sin(angle);
  }
  return std::hypot(real, imag);
}

// Lag nearest the expected period, then the normalized autocorrelation.
struct Period {
  double frequencyHz = 0.0;
  double autocorrelation = 0.0;
};

Period periodNear(const std::vector<double>& x, std::size_t a, double expectHz,
                  double rate) {
  const int center = std::max(8, static_cast<int>(std::llround(rate / expectHz)));
  const int lo = std::max(8, center - 12);
  const int hi = center + 12;
  double best = -2.0;
  int lag = center;
  for (int candidate = lo; candidate <= hi; ++candidate) {
    double energy0 = 0.0;
    double energy1 = 0.0;
    double cross = 0.0;
    for (int i = 0; i < 5000; ++i) {
      const double u = x[a + static_cast<std::size_t>(i)];
      const double v = x[a + static_cast<std::size_t>(i + candidate)];
      energy0 += u * u;
      energy1 += v * v;
      cross += u * v;
    }
    const double value = cross / std::sqrt(energy0 * energy1);
    if (value > best) {
      best = value;
      lag = candidate;
    }
  }
  Period period;
  period.frequencyHz = rate / static_cast<double>(lag);
  period.autocorrelation = best;
  return period;
}

// Transposed direct form II. a0 is 1.
struct Biquad {
  double b0 = 1.0;
  double b1 = 0.0;
  double b2 = 0.0;
  double a1 = 0.0;
  double a2 = 0.0;
  double z1 = 0.0;
  double z2 = 0.0;

  [[nodiscard]] double step(double x) {
    const double y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
  }
};

// ITU-R BS.1770-4 Annex 2. The shelf is +4 dB; the second stage is the
// RLB high-pass. Both are normalized to a0 = 1, so the RLB numerator is
// the published 1, −2, 1 divided by its a0.
Biquad kWeightShelf(double rate) {
  constexpr double kHz = 1681.974450955533;
  constexpr double kGainDb = 3.999843853973347;
  constexpr double kQ = 0.7071752369554196;
  const double k = std::tan(constants::pi * kHz / rate);
  const double vh = std::pow(10.0, kGainDb / 20.0);
  const double vb = std::pow(vh, 0.4996667741545416);
  const double a0 = 1.0 + k / kQ + k * k;
  Biquad filter;
  filter.b0 = (vh + vb * k / kQ + k * k) / a0;
  filter.b1 = 2.0 * (k * k - vh) / a0;
  filter.b2 = (vh - vb * k / kQ + k * k) / a0;
  filter.a1 = 2.0 * (k * k - 1.0) / a0;
  filter.a2 = (1.0 - k / kQ + k * k) / a0;
  return filter;
}

Biquad kWeightHighpass(double rate) {
  constexpr double kHz = 38.13547087602444;
  constexpr double kQ = 0.5003270373238773;
  const double k = std::tan(constants::pi * kHz / rate);
  const double a0 = 1.0 + k / kQ + k * k;
  Biquad filter;
  filter.b0 = 1.0 / a0;
  filter.b1 = -2.0 / a0;
  filter.b2 = 1.0 / a0;
  filter.a1 = 2.0 * (k * k - 1.0) / a0;
  filter.a2 = (1.0 - k / kQ + k * k) / a0;
  return filter;
}

// Ungated mean square of the K-weighted steady state. A continuous tone
// has no block below the absolute gate, so the gated and ungated values
// agree. The offset −0.691 LU is the mono/stereo constant in BS.1770.
double steadyLufs(const std::vector<double>& samples, std::size_t begin,
                  std::size_t end, double rate) {
  auto shelf = kWeightShelf(rate);
  auto highpass = kWeightHighpass(rate);
  double energy = 0.0;
  for (std::size_t i = 0; i < end; ++i) {
    const double weighted = highpass.step(shelf.step(samples[i]));
    if (i >= begin) energy += weighted * weighted;
  }
  const double meanSquare = energy / static_cast<double>(end - begin);
  REQUIRE(meanSquare > 0.0);
  return -0.691 + 10.0 * std::log10(meanSquare);
}

void checkKWeightTable() {
  const Biquad shelf = kWeightShelf(48000.0);
  CHECK(std::fabs(shelf.b0 - 1.53512485958697) < 1.0e-12);
  CHECK(std::fabs(shelf.b1 - -2.69169618940638) < 1.0e-12);
  CHECK(std::fabs(shelf.b2 - 1.19839281085285) < 1.0e-12);
  CHECK(std::fabs(shelf.a1 - -1.69065929318241) < 1.0e-12);
  CHECK(std::fabs(shelf.a2 - 0.73248077421585) < 1.0e-12);
  const Biquad highpass = kWeightHighpass(48000.0);
  CHECK(std::fabs(highpass.a1 - -1.99004745483398) < 1.0e-12);
  CHECK(std::fabs(highpass.a2 - 0.99007225036621) < 1.0e-12);
}

// Achieved integrated loudness of the file that was written.
double writeListening(const std::vector<double>& samples, const char* filename,
                    double rate) {
  const std::size_t begin = static_cast<std::size_t>(rate);
  const std::size_t end = static_cast<std::size_t>(2.0 * rate);
  REQUIRE(samples.size() >= end);
  const double measured = steadyLufs(samples, begin, end, rate);
  constexpr double kTargetLufs = -20.0;
  double gain = std::pow(10.0, (kTargetLufs - measured) / 20.0);
  double peak = 0.0;
  for (double sample : samples) peak = std::max(peak, std::fabs(sample));
  REQUIRE(peak > 0.0);
  // −1 dBFS. If the loudness gain would clip, the ceiling wins and the
  // file sits below −20 LUFS.
  const double ceiling = std::pow(10.0, -1.0 / 20.0);
  if (peak * gain > ceiling) gain = ceiling / peak;
  const double achieved = measured + 20.0 * std::log10(gain);
  std::cout << filename << " playback " << achieved << " LUFS  gain " << gain
            << "  from " << measured << " LUFS\n";
  // The target, or quieter when the −1 dBFS ceiling reduces the gain.
  CHECK(achieved <= kTargetLufs + 0.05);
  CHECK(peak * gain <= ceiling + 1.0e-12);
  std::vector<double> scaled(samples.size());
  for (std::size_t i = 0; i < samples.size(); ++i) scaled[i] = samples[i] * gain;
  const auto path = wavOutputPath(filename);
  auto opened = WavSink::open(path.string(), SampleRateHz{rate},
                              FrameCount{static_cast<std::uint64_t>(samples.size())});
  REQUIRE(opened.ok());
  auto sink = std::move(opened).value();
  REQUIRE(sink.consume(scaled.data(),
                       FrameCount{static_cast<std::uint64_t>(scaled.size())}));
  REQUIRE(sink.close().ok());
  REQUIRE(copyWavToArtifacts(path, filename));
  return achieved;
}

enum class Spectrum { Clarinet, Flute, Saxophone, Brass };

struct Tone {
  const char* name;
  double expectHz;
  Spectrum spectrum;
  std::vector<double> samples;
};

// Magnitude centroid of the harmonic series, sum(n A_n)/sum(A_n).
// The old dull trumpet, partials falling about 7 dB each from 119 Hz,
// sits near 2.2–2.5. A flat odd square sits far above that and is
// rejected separately.
void checkSpectrum(const Tone& tone, double f0, double rate) {
  const auto& x = tone.samples;
  const std::size_t a = 70000;
  constexpr int kPartials = 24;
  double amplitude[kPartials] = {};
  int used = 0;
  for (int h = 1; h <= kPartials; ++h) {
    if (static_cast<double>(h) * f0 > 12000.0) break;
    amplitude[used] = goertzel(x, a, 8192, static_cast<double>(h) * f0, rate);
    ++used;
  }
  REQUIRE(used >= 10);
  REQUIRE(amplitude[0] > 0.0);
  double db[10] = {};
  std::cout << tone.name << " harmonics";
  for (int h = 0; h < 10; ++h) {
    db[h] = 20.0 * std::log10(amplitude[h] / amplitude[0]);
    std::cout << " " << db[h];
  }
  double magnitude = 0.0;
  double moment = 0.0;
  int above = 0;
  int evens = 0;
  for (int h = 0; h < used; ++h) {
    magnitude += amplitude[h];
    moment += static_cast<double>(h + 1) * f0 * amplitude[h];
    const double decibel = 20.0 * std::log10(amplitude[h] / amplitude[0] + 1.0e-20);
    if (decibel > -40.0) ++above;
    if ((h + 1) % 2 == 0 && decibel > -40.0) ++evens;
  }
  const double centroidRatio = (moment / magnitude) / f0;
  std::cout << "  centroid/f0 " << centroidRatio << "  above -40 " << above
            << "\n";
  const bool flatOdds =
      db[2] > -3.0 && db[4] > -3.0 && db[6] > -3.0 && db[8] > -3.0;
  CHECK_FALSE(flatOdds);
  if (tone.spectrum == Spectrum::Clarinet) {
    CHECK(db[2] > db[1]);
    CHECK(db[4] < db[2] - 1.0);
    CHECK(db[6] < db[4] - 1.0);
    CHECK(db[8] < db[6] - 1.0);
  } else if (tone.spectrum == Spectrum::Flute) {
    for (int h = 1; h < 10; ++h) CHECK(db[h] < -3.0);
    CHECK(db[1] > -40.0);
  } else if (tone.spectrum == Spectrum::Saxophone) {
    CHECK(above >= 6);
    CHECK(evens >= 1);
    // Benade and Lutgen: above the break the envelope falls about
    // 18 dB/octave. Partials below 1 kHz against partials from 2.5 to
    // 5 kHz span more than an octave, so the means must differ by at
    // least that one-octave slope. The 8 kHz corner (harmonics still
    // near −20 dB at 4 kHz) does not.
    double lowSum = 0.0;
    double highSum = 0.0;
    int lowCount = 0;
    int highCount = 0;
    for (int h = 0; h < used; ++h) {
      const double hz = static_cast<double>(h + 1) * f0;
      const double decibel = 20.0 * std::log10(amplitude[h] / amplitude[0]);
      if (hz < 1000.0) {
        lowSum += decibel;
        ++lowCount;
      }
      if (hz > 2500.0 && hz < 5000.0) {
        highSum += decibel;
        ++highCount;
      }
    }
    REQUIRE(lowCount >= 3);
    REQUIRE(highCount >= 3);
    const double lowMean = lowSum / static_cast<double>(lowCount);
    const double highMean = highSum / static_cast<double>(highCount);
    std::cout << tone.name << " envelope below 1 kHz " << lowMean
              << " dB  2.5–5 kHz " << highMean << " dB\n";
    CHECK(highMean < lowMean - 18.0);
    // Halfway between partials. Turbulence 0 on this bore sits near
    // −125 dB (a line spectrum). The floor has to clear −80 dB, which
    // is above that deterministic floor, and stay under −45 dB so a
    // partial on Benade's slope is still the tone.
    std::vector<double> floors;
    for (int h = 4; h <= 12; ++h) {
      const double between = goertzel(x, a, 8192, (static_cast<double>(h) + 0.5) * f0, rate);
      floors.push_back(20.0 * std::log10(between / amplitude[0] + 1.0e-30));
    }
    std::sort(floors.begin(), floors.end());
    const double floorDb = floors[floors.size() / 2];
    std::cout << tone.name << " interharmonic floor " << floorDb << " dB\n";
    CHECK(floorDb > -80.0);
    CHECK(floorDb < -45.0);
  } else {
    CHECK(above >= 8);
    CHECK(evens >= 1);
    CHECK(centroidRatio > 2.5);
  }
}

struct BandPower {
  double aboveNyquistGuard = 0.0;
  double below5kHz = 0.0;
  double audibleRms = 0.0;
};
double attackMilliseconds(const std::vector<double>& x, double rate);
BandPower bandPower(const std::vector<double>& x, std::size_t origin, double rate);

void checkTone(const Tone& tone, double rate) {
  const auto& x = tone.samples;
  REQUIRE(x.size() >= 96000);
  const std::size_t a = 70000;
  const Period period = periodNear(x, a, tone.expectHz, rate);
  const double early = rmsOf(x, 72000, 84000);
  const double late = rmsOf(x, 84000, 96000);
  const double mean = meanOf(x, 72000, 96000);
  const double level = rmsOf(x, 72000, 96000);
  const double hnr =
      period.autocorrelation < 1.0
          ? 10.0 * std::log10(period.autocorrelation /
                              (1.0 - period.autocorrelation))
          : 100.0;
  std::cout << tone.name << " f0 " << period.frequencyHz << " Hz  HNR " << hnr
            << " dB  autocorr " << period.autocorrelation << "  rms " << early
            << " -> " << late << "\n";
  CHECK(std::fabs(period.frequencyHz - tone.expectHz) < 8.0);
  CHECK(period.autocorrelation > 0.9);
  CHECK(hnr > 9.5);
  CHECK(early > 1.0e-4);
  CHECK(std::fabs(late / early - 1.0) < 0.10);
  CHECK(std::fabs(mean) < 0.05 * level);
  double peak = 0.0;
  for (std::size_t i = 72000; i < 96000; ++i)
    peak = std::max(peak, std::fabs(x[i]));
  CHECK(peak / level < 40.0);
  // Ten 100 ms windows from 1.0 s to 2.0 s. A swell or a dip moves one
  // of them by a decibel relative to their mean. The mean itself is the
  // steady level, in dB re 1 Pa, at the shared 1 m listener.
  constexpr int kWindows = 10;
  const std::size_t win = static_cast<std::size_t>(0.1 * rate);
  const std::size_t steady = static_cast<std::size_t>(rate);
  double windowRms[kWindows] = {};
  double sum = 0.0;
  for (int i = 0; i < kWindows; ++i) {
    const std::size_t start = steady + static_cast<std::size_t>(i) * win;
    windowRms[i] = rmsOf(x, start, start + win);
    sum += windowRms[i];
  }
  const double meanRms = sum / static_cast<double>(kWindows);
  double worstDb = 0.0;
  for (int i = 0; i < kWindows; ++i) {
    const double db = 20.0 * std::log10(windowRms[i] / meanRms);
    worstDb = std::max(worstDb, std::fabs(db));
  }
  const double dbRePascal = 20.0 * std::log10(meanRms);
  const double spl = dbRePascal - 20.0 * std::log10(20.0e-6);
  const double attackMs = attackMilliseconds(x, rate);
  const BandPower band = bandPower(x, a, rate);
  const double audibleDb = 20.0 * std::log10(band.audibleRms + 1.0e-30);
  std::cout << tone.name << " level " << dbRePascal
            << " dB re 1 Pa at 1 m (" << spl << " dB SPL)  envelope ±"
            << worstDb << " dB  attack " << attackMs
            << " ms  audible-band " << audibleDb << " dB re 1 Pa\n";
  CHECK(worstDb < 1.0);
  // A Nyquist rail, or any energy above 0.4 fs, is not a musical tone.
  // Wind spectra sit below 5 kHz; a 12 kHz reed lock does not.
  CHECK(band.aboveNyquistGuard < 0.01);
  CHECK(band.below5kHz > 0.90);
  std::cout << tone.name << " band above 0.4 fs " << (100.0 * band.aboveNyquistGuard)
            << "%  below 5 kHz " << (100.0 * band.below5kHz) << "%\n";
  checkSpectrum(tone, period.frequencyHz, rate);
}

Clarinet workingClarinet(double length) {
  // Reed damping 0.12 N·s/m. 0.05 N·s/m on this bore left the third
  // harmonic a few decibels from the fundamental.
  ClarinetSpec spec{length, 2.0e-4, kAir, kTemp, 2.0e-5, 6000.0,
                    1.0e-6, 1.0e-4, 1.0e-3, 0.12};
  return unwrap(makeClarinet(spec));
}

ValveBlow sustained(double pressure, double cutoffHz, std::uint64_t attack) {
  ValveBlow blow;
  blow.pressurePa = pressure;
  blow.frames = kFrames;
  blow.reflectionMagnitude = 0.995;
  blow.initialDisplacementMeters = 2.0e-5;
  blow.attackFrames = attack;
  blow.listenerDistanceMeters = 1.0;
  blow.reflectionCutoffHz = cutoffHz;
  return blow;
}

// First time, in milliseconds, at which a 10 ms RMS stays within 1 dB of
// the 1.5–2.0 s level for 40 ms. The swell is the oscillator growing
// under the mouth-pressure ramp; this is when that swell has arrived.
double attackMilliseconds(const std::vector<double>& x, double rate) {
  const auto steadyStart = static_cast<std::size_t>(1.5 * rate);
  const auto steadyEnd = static_cast<std::size_t>(2.0 * rate);
  const double steady = rmsOf(x, steadyStart, steadyEnd);
  const auto hop = static_cast<std::size_t>(0.005 * rate);
  const auto box = static_cast<std::size_t>(0.010 * rate);
  constexpr int kHold = 8;
  for (std::size_t t = 0; t + box + static_cast<std::size_t>(kHold) * hop < x.size();
       t += hop) {
    bool held = true;
    for (int k = 0; k < kHold; ++k) {
      const auto a = t + static_cast<std::size_t>(k) * hop;
      const double window = rmsOf(x, a, a + box);
      if (std::fabs(20.0 * std::log10((window + 1.0e-30) / (steady + 1.0e-30))) > 1.0) {
        held = false;
        break;
      }
    }
    if (held) return 1000.0 * static_cast<double>(t) / rate;
  }
  return 1.0e9;
}

// Power fractions of a Hann-windowed steady FFT. high is above 0.4 fs,
// which is where a Nyquist rail lives. low is below 5 kHz.
BandPower bandPower(const std::vector<double>& x, std::size_t origin, double rate) {
  constexpr int kN = 16384;
  std::vector<std::complex<double>> buf(static_cast<std::size_t>(kN));
  for (int i = 0; i < kN; ++i) {
    const double window =
        0.5 * (1.0 - std::cos(constants::twoPi * static_cast<double>(i) / kN));
    buf[static_cast<std::size_t>(i)] = x[origin + static_cast<std::size_t>(i)] * window;
  }
  auto spec = fft(std::move(buf));
  REQUIRE(spec.ok());
  const auto& bins = spec.value();
  double total = 0.0;
  double high = 0.0;
  double low = 0.0;
  double audible = 0.0;
  for (int k = 0; k < kN; ++k) {
    const double power = std::norm(bins[static_cast<std::size_t>(k)]);
    total += power;
    const int folded = std::min(k, kN - k);
    const double frequency = static_cast<double>(folded) * rate / kN;
    if (frequency > 0.4 * rate) high += power;
    if (frequency < 5000.0) low += power;
    if (frequency >= 20.0 && frequency <= 16000.0) audible += power;
  }
  BandPower band;
  band.aboveNyquistGuard = total > 0.0 ? high / total : 1.0;
  band.below5kHz = total > 0.0 ? low / total : 0.0;
  // Parseval: sum |x_window|^2 = (1/N) sum |X|^2. The Hann window's
  // mean square is 0.375, so the unwindowed RMS follows.
  const double windowMeanSquare = 0.375;
  band.audibleRms =
      std::sqrt(audible / (static_cast<double>(kN) * windowMeanSquare * static_cast<double>(kN)));
  return band;
}

// The agent store is not on a Windows checkout. A missing directory is
// not a failure; the artifacts copy is the one the suite requires.
void archivePhraseWav(const std::filesystem::path& source, const char* filename) {
  const std::filesystem::path root{"/cursor/stores/self"};
  std::error_code error;
  if (!std::filesystem::is_directory(root, error) || error) return;
  const std::filesystem::path store = root / "winds-phrase";
  std::filesystem::create_directories(store, error);
  if (error) return;
  REQUIRE(copyWavFile(source, store / filename));
}

double writePhrase(const std::vector<double>& samples, const char* filename, double rate) {
  const std::size_t begin = static_cast<std::size_t>(0.5 * rate);
  const std::size_t end = samples.size();
  REQUIRE(end > begin + static_cast<std::size_t>(rate));
  const double measured = steadyLufs(samples, begin, end, rate);
  constexpr double kTargetLufs = -20.0;
  double gain = std::pow(10.0, (kTargetLufs - measured) / 20.0);
  double peak = 0.0;
  for (double sample : samples) peak = std::max(peak, std::fabs(sample));
  REQUIRE(peak > 0.0);
  const double ceiling = std::pow(10.0, -1.0 / 20.0);
  if (peak * gain > ceiling) gain = ceiling / peak;
  const double achieved = measured + 20.0 * std::log10(gain);
  std::cout << filename << " playback " << achieved << " LUFS  gain " << gain
            << "  from " << measured << " LUFS over 0.5 s to the end\n";
  CHECK(achieved <= kTargetLufs + 0.05);
  CHECK(peak * gain <= ceiling + 1.0e-12);
  std::vector<double> scaled(samples.size());
  for (std::size_t i = 0; i < samples.size(); ++i) scaled[i] = samples[i] * gain;
  const auto path = wavOutputPath(filename);
  auto opened = WavSink::open(path.string(), SampleRateHz{rate},
                              FrameCount{static_cast<std::uint64_t>(samples.size())});
  REQUIRE(opened.ok());
  auto sink = std::move(opened).value();
  REQUIRE(sink.consume(scaled.data(), FrameCount{static_cast<std::uint64_t>(scaled.size())}));
  REQUIRE(sink.close().ok());
  REQUIRE(copyWavToArtifacts(path, filename));
  archivePhraseWav(path, filename);
  return achieved;
}

// Parabolic autocorrelation around the expected period, on the second
// half of a note. A fixed lag is coarse once f0²/fs exceeds a few hertz.
Period phrasePeriod(const std::vector<double>& x, std::size_t begin, std::size_t end,
                    double expectHz, double rate) {
  const int n = 4096;
  const int center = std::max(8, static_cast<int>(std::llround(rate / expectHz)));
  // A tenth of the period is about 160 cents. An excerpt length can be
  // further off than that on the first tuning, and the lag then sits on
  // the edge of the window. A sixth still excludes the octave.
  const int span = std::max(8, center / 6);
  const int lo = std::max(6, center - span);
  const int hi = center + span;
  if (end < begin + static_cast<std::size_t>(n + hi + 4)) return {};
  const std::size_t a = end - static_cast<std::size_t>(n + hi + 4);
  if (a < begin) return {};
  double best = -2.0;
  int lag = center;
  auto at = [&](int candidate) {
    double e0 = 0.0, e1 = 0.0, cross = 0.0;
    for (int i = 0; i < n; ++i) {
      const double u = x[a + static_cast<std::size_t>(i)];
      const double v = x[a + static_cast<std::size_t>(i + candidate)];
      e0 += u * u;
      e1 += v * v;
      cross += u * v;
    }
    return cross / std::sqrt(e0 * e1 + 1.0e-30);
  };
  for (int candidate = lo; candidate <= hi; ++candidate) {
    const double ac = at(candidate);
    if (ac > best) {
      best = ac;
      lag = candidate;
    }
  }
  double refined = static_cast<double>(lag);
  if (lag > lo && lag < hi) {
    const double y0 = at(lag - 1);
    const double y1 = best;
    const double y2 = at(lag + 1);
    const double denom = y0 - 2.0 * y1 + y2;
    if (std::fabs(denom) > 1.0e-12) {
      const double delta = 0.5 * (y0 - y2) / denom;
      if (std::fabs(delta) < 1.2) refined += delta;
    }
  }
  return Period{rate / refined, best};
}

struct PhraseNote {
  const char* name;
  double startSec = 0.0;
  double targetHz = 0.0;
  double lengthM = 0.0;
  double lipHz = 0.0;
  double pressure = 0.0;
  double jetSamples = 0.0;
  bool tongue = false;
  bool vibrato = false;
  double tolCents = 40.0;
  bool require = true;
  double minRms = 0.01;
  bool rest = false;
  double gain = 1.0;
  const char* dynamic = "";
};

// 12 cents and 5.5 Hz are arguments. No stored wind-vibrato depth is
// used. The length modulation is the pitch control this bore already has.
constexpr double kVibratoHz = 5.5;
constexpr double kVibratoCents = 12.0;

template <typename Voice>
std::vector<double> renderPhrase(Voice& voice, const std::vector<PhraseNote>& notes,
                                 double totalSec, bool fluteJet) {
  const auto total = static_cast<std::uint64_t>(std::llround(totalSec * kFs));
  std::vector<double> out(static_cast<std::size_t>(total), 0.0);
  std::vector<char> armed(notes.size(), 0);
  constexpr std::uint64_t kHop = 64;
  // Bertsch and Hoole, fast single tonguing, about 40–64 ms. The closure
  // is that long; the release is the note. The first note keeps the
  // blow's own tongue, if it has one.
  constexpr std::uint64_t kTongue = 1920;
  const double depth = std::pow(2.0, kVibratoCents / 1200.0) - 1.0;
  for (std::uint64_t frame = 0; frame < total;) {
    const std::uint64_t count = std::min<std::uint64_t>(kHop, total - frame);
    int active = 0;
    for (int i = 0; i < static_cast<int>(notes.size()); ++i) {
      if (frame >= static_cast<std::uint64_t>(notes[static_cast<std::size_t>(i)].startSec * kFs))
        active = i;
    }
    // A tongued brass note retunes while the tongue is shut, then
    // releases. Setting the new lip on the open valve goes non-finite.
    int sounding = active;
    for (int i = 1; i < static_cast<int>(notes.size()); ++i) {
      const PhraseNote& upcoming = notes[static_cast<std::size_t>(i)];
      if (!upcoming.tongue || upcoming.rest) continue;
      const auto release = static_cast<std::uint64_t>(upcoming.startSec * kFs);
      if (release < kTongue) continue;
      if (frame >= release - kTongue && frame < release) {
        if (!armed[static_cast<std::size_t>(i)]) {
          voice.scheduleTongue(release);
          armed[static_cast<std::size_t>(i)] = 1;
        }
        if (upcoming.lipHz > 0.0) sounding = i;
      }
    }
    const PhraseNote& note = notes[static_cast<std::size_t>(sounding)];
    const double time = static_cast<double>(frame) / kFs;
    double env = note.rest ? 0.0 : note.gain;
    const bool dipping = fluteJet && note.tongue && sounding > 0 && !note.rest &&
                         time < note.startSec + 0.020;
    if (dipping) env *= 0.55;
    double length = note.lengthM;
    // A lip locked to one partial explodes if the bore glides through
    // another. Brass changes length at the note. Woodwind slurs glide.
    if (sounding > 0 && note.lipHz == 0.0 && !note.rest) {
      const double glide = note.tongue ? 0.010 : 0.035;
      double alpha = (time - note.startSec) / glide;
      if (alpha < 0.0) alpha = 0.0;
      if (alpha > 1.0) alpha = 1.0;
      alpha = alpha * alpha * (3.0 - 2.0 * alpha);
      const PhraseNote& prev = notes[static_cast<std::size_t>(sounding - 1)];
      length = prev.lengthM + (note.lengthM - prev.lengthM) * alpha;
    }
    if (note.vibrato && !note.rest && time > note.startSec + 0.20) {
      length *= 1.0 - depth * std::sin(constants::twoPi * kVibratoHz * time);
    }
    REQUIRE(voice.setSoundingLength(length));
    REQUIRE(voice.setBlowingPressure(note.rest ? 0.0 : note.pressure * env));
    if (note.lipHz > 0.0) REQUIRE(voice.setLipResonanceHz(note.lipHz));
    if constexpr (requires(Voice& probe) { probe.setJetDelaySamples(0.0); }) {
      if (!fluteJet || note.rest) {
        if (fluteJet && note.jetSamples > 0.0) REQUIRE(voice.setJetDelaySamples(note.jetSamples));
      } else {
      double jetNote = note.jetSamples;
      if (sounding > 0 && notes[static_cast<std::size_t>(sounding - 1)].jetSamples > 0.0) {
        const double glide = note.tongue ? 0.010 : 0.035;
        double alpha = (time - note.startSec) / glide;
        if (alpha < 0.0) alpha = 0.0;
        if (alpha > 1.0) alpha = 1.0;
        alpha = alpha * alpha * (3.0 - 2.0 * alpha);
        const double prev = notes[static_cast<std::size_t>(sounding - 1)].jetSamples;
        jetNote = prev + (note.jetSamples - prev) * alpha;
      }
      REQUIRE(voice.setJetDelaySamples(jetNote / std::sqrt(std::max(env, 1.0e-3))));
      }
    }
    auto stepped = voice.renderBlock(out.data() + frame, FrameCount{count});
    if (!stepped.ok()) {
      std::cout << "phrase render failed at " << time << " s on " << note.name << "\n";
      REQUIRE(stepped.ok());
    }
    frame += count;
  }
  for (double sample : out) REQUIRE(std::isfinite(sample));
  return out;
}

void reportPhrase(const char* instrument, const std::vector<PhraseNote>& notes,
                  const std::vector<double>& samples, double totalSec, double attackLead) {
  std::cout << instrument << " notes";
  int silent = 0;
  for (std::size_t i = 0; i < notes.size(); ++i) {
    const PhraseNote& note = notes[i];
    const double endSec = (i + 1 < notes.size()) ? notes[i + 1].startSec : totalSec;
    if (note.rest || !(note.targetHz > 0.0)) {
      std::cout << "\n  rest " << (endSec - note.startSec) << " s";
      continue;
    }
    // The autocorrelation window has to sit inside this note. A 0.23 s
    // triplet is printed and not graded: the next note would own the lag.
    double lead = (i == 0) ? attackLead : 0.045;
    const double tail = (i + 1 < notes.size() && notes[i + 1].tongue) ? 0.045 : 0.015;
    double windowBegin = note.startSec + lead;
    double windowEnd = endSec - tail;
    if (windowEnd - windowBegin < 0.10) {
      windowBegin = note.startSec + 0.030;
      windowEnd = endSec - 0.015;
    }
    const bool fits = windowEnd > windowBegin + 0.09 &&
                      static_cast<std::size_t>(windowEnd * kFs) <= samples.size() &&
                      static_cast<std::size_t>(windowBegin * kFs) < samples.size();
    Period period{};
    double rms = 0.0;
    if (fits) {
      period = phrasePeriod(samples, static_cast<std::size_t>(windowBegin * kFs),
                            static_cast<std::size_t>(windowEnd * kFs), note.targetHz, kFs);
      rms = rmsOf(samples, static_cast<std::size_t>(windowBegin * kFs),
                  static_cast<std::size_t>(windowEnd * kFs));
    }
    const bool spoke =
        fits && period.autocorrelation > 0.85 && rms > note.minRms && period.frequencyHz > 1.0;
    const double cents =
        spoke ? 1200.0 * std::log2(period.frequencyHz / note.targetHz) : 0.0;
    std::cout << "\n  " << note.name << " " << note.dynamic << " " << (endSec - note.startSec)
              << " s target " << note.targetHz << " played " << period.frequencyHz << " cents "
              << cents << " ac " << period.autocorrelation << " rms " << rms;
    if (!spoke) {
      std::cout << " DID NOT SPEAK";
      ++silent;
    }
    if (note.require && fits) {
      CHECK(spoke);
      CHECK(std::fabs(cents) < note.tolCents);
    }
  }
  std::cout << "\n" << instrument << " silent-or-unlocked " << silent << "\n";
  int last = -1;
  for (int i = static_cast<int>(notes.size()) - 1; i >= 0; --i) {
    if (notes[static_cast<std::size_t>(i)].rest || !(notes[static_cast<std::size_t>(i)].targetHz > 0.0))
      continue;
    const double endSec = (static_cast<std::size_t>(i) + 1 < notes.size())
                              ? notes[static_cast<std::size_t>(i) + 1].startSec
                              : totalSec;
    if (endSec - notes[static_cast<std::size_t>(i)].startSec >= 0.40) {
      last = i;
      break;
    }
  }
  if (last >= 0 && samples.size() > 20000) {
    const double endSec =
        (static_cast<std::size_t>(last) + 1 < notes.size())
            ? notes[static_cast<std::size_t>(last) + 1].startSec
            : totalSec;
    constexpr int kN = 16384;
    auto origin = static_cast<std::size_t>(endSec * kFs) - 800;
    if (origin > static_cast<std::size_t>(kN)) origin -= static_cast<std::size_t>(kN);
    else origin = 0;
    const auto noteBegin =
        static_cast<std::size_t>(notes[static_cast<std::size_t>(last)].startSec * kFs);
    if (origin < noteBegin) origin = noteBegin;
    if (origin + static_cast<std::size_t>(kN) <= samples.size()) {
      const BandPower band = bandPower(samples, origin, kFs);
      std::cout << instrument << " held-note band above 0.4 fs "
                << (100.0 * band.aboveNyquistGuard) << "%  below 5 kHz "
                << (100.0 * band.below5kHz) << "%\n";
      CHECK(band.aboveNyquistGuard < 0.01);
    }
  }
}

double conePartial(double length, double apex, int mode) {
  SaxophoneSpec spec{length, apex, 1.6e-4, kAir, kTemp, 1.8e-5, 7500.0,
                     2.0e-6, 1.4e-4, 7.0e-4, 0.09};
  return unwrap(saxophoneConeMode(unwrap(makeSaxophone(spec)), mode)).value();
}

}  // namespace

TEST_CASE("blown winds reach a periodic limit cycle", "[contact][wind]") {
  checkKWeightTable();
  const double speed =
      unwrap(speedOfSound(Air{TemperatureKelvin{kTemp}})).value();

  auto clarinet = workingClarinet(0.45);
  const double clarinetHz = speed / (4.0 * 0.45);
  auto clarinetVoice = unwrap(
      BoreValveVoice::fromClarinet(clarinet, sustained(4000.0, 600.0, kClarinetAttack), kRate));
  auto clarinetSamples = renderOf(clarinetVoice, kFrames);
  checkTone(Tone{"clarinet", clarinetHz, Spectrum::Clarinet, clarinetSamples}, kFs);
  CHECK(clarinetHz > 140.0);
  CHECK(clarinetHz < 500.0);
  const double clarinetLufs =
      writeListening(clarinetSamples, "mmath-clarinet.wav", kFs);

  auto quiet = unwrap(
      BoreValveVoice::fromClarinet(clarinet, sustained(250.0, 600.0, kClarinetAttack), kRate));
  auto quietSamples = renderOf(quiet, kFrames);
  CHECK(rmsOf(quietSamples, 72000, 96000) <
        0.05 * rmsOf(clarinetSamples, 72000, 96000));

  auto longer = workingClarinet(0.60);
  const double longerHz = speed / (4.0 * 0.60);
  auto longerVoice =
      unwrap(BoreValveVoice::fromClarinet(longer, sustained(4000.0, 600.0, kClarinetAttack), kRate));
  auto longerSamples = renderOf(longerVoice, kFrames);
  const Period low = periodNear(clarinetSamples, 70000, clarinetHz, kFs);
  const Period high = periodNear(longerSamples, 70000, longerHz, kFs);
  CHECK(std::fabs(low.frequencyHz / high.frequencyHz - 0.60 / 0.45) < 0.04);

  const SampleRateHz other{44100.0};
  auto otherVoice =
      unwrap(BoreValveVoice::fromClarinet(clarinet, sustained(4000.0, 600.0, kClarinetAttack), other));
  auto otherSamples = renderOf(otherVoice, 88200);
  const Period at44 = periodNear(otherSamples, 60000, clarinetHz, 44100.0);
  CHECK(std::fabs(at44.frequencyHz - low.frequencyHz) < 3.0);

  ValveBlow released = sustained(4000.0, 600.0, kClarinetAttack);
  released.frames = 70000;
  released.releaseFrames = 2400;
  auto releaseVoice =
      unwrap(BoreValveVoice::fromClarinet(clarinet, released, kRate));
  auto releaseSamples = renderOf(releaseVoice, kFrames);
  CHECK(rmsOf(releaseSamples, 88000, 96000) <
        0.7 * rmsOf(releaseSamples, 40000, 60000));

  // Alto-range truncated cone: L = 0.70 m, x0 = 0.20 m. The second
  // root stays within a few percent of the octave (still under 2.15),
  // and a light embouchure (0.09 N·s/m) lets the first mode beat instead
  // of sitting on a sine. Heavier damping on a smaller apex stayed dark;
  // a tongue slap on this reed jumps to the octave, so the note is a
  // breath attack under the 0.40 s Fuks ramp. 4200 Pa is below the 7500 Pa
  // lay. The reflection corner is 1500 Hz (see the file comment). Benade
  // and Lutgen's alto break, 837 Hz, shapes the radiated pressure. On
  // this reed a loop corner at 837 Hz itself jumps to the octave, so
  // that number is the external envelope and not the reflection pole.
  // reedTurbulence 0.12 is an argument.
  SaxophoneSpec saxSpec{0.70, 0.20, 1.6e-4, kAir, kTemp, 1.8e-5, 7500.0,
                        2.0e-6, 1.4e-4, 7.0e-4, 0.09};
  const auto saxophone = unwrap(makeSaxophone(saxSpec));
  CHECK(saxophone.fundamentalHz > 138.0);
  CHECK(saxophone.fundamentalHz < 830.0);
  const double second = unwrap(saxophoneConeMode(saxophone, 2)).value();
  CHECK(second / saxophone.fundamentalHz < 2.15);
  CHECK(second / saxophone.fundamentalHz > 1.85);
  ValveBlow saxBlow = sustained(4200.0, 1500.0, kSaxophoneAttack);
  saxBlow.initialDisplacementMeters = 1.0e-5;
  saxBlow.reedTurbulence = 0.12;
  saxBlow.spectrumBreakHz = 837.0;
  auto saxVoice =
      unwrap(ConeValveVoice::fromSaxophone(saxophone, saxBlow, kRate));
  auto saxSamples = renderOf(saxVoice, kFrames);
  checkTone(
      Tone{"saxophone", saxophone.fundamentalHz, Spectrum::Saxophone, saxSamples},
      kFs);
  const double saxLufs = writeListening(saxSamples, "mmath-saxophone.wav", kFs);
  archivePhraseWav(wavOutputPath("mmath-saxophone.wav"), "mmath-saxophone.wav");

  const double fluteLength = 0.60;
  const double pipe = speed / (2.0 * fluteLength);
  const double span = 7.0e-3;
  const double sigma = 0.45;
  const double thickness = 1.0e-3;
  const double jetSpeed = 2.0 * span * pipe / sigma;
  FluteSpec fluteSpec{fluteLength, 2.8e-4, kAir, kTemp, 1.2e-5, thickness, span,
                      sigma};
  const auto flute = unwrap(makeFlute(fluteSpec));
  const double overpressure =
      0.5 * flute.airDensityKgPerM3 * jetSpeed * jetSpeed;
  const double convection = unwrap(fluteConvectionTime(flute, overpressure));
  CHECK(std::fabs(convection - 0.5 / pipe) / (0.5 / pipe) < 0.02);
  const double fluteRadius = std::sqrt(flute.crossSectionM2 / constants::pi);
  // Aim 0.4 thicknesses and a 400 Hz corner keep the fundamental
  // strongest. A centered, brighter jet overshoots and the last decibel
  // of that ring takes a few hundred milliseconds.
  ValveBlow jet = sustained(overpressure, 400.0, kFluteAttack);
  jet.reflectionMagnitude = 0.985;
  jet.initialDisplacementMeters = 0.50 * thickness;
  jet.jetAimMeters = 0.40 * thickness;
  jet.jetNoiseMeters = 0.035 * thickness;
  // Levine and Schwinger, unflanged open end, about 0.61 radii.
  jet.endCorrectionMeters = 0.61 * fluteRadius;
  auto jetVoice = unwrap(BoreValveVoice::fromFlute(flute, jet, kRate));
  auto jetSamples = renderOf(jetVoice, kFrames);
  checkTone(Tone{"flute", jetVoice.fundamentalHz(), Spectrum::Flute, jetSamples},
            kFs);
  CHECK(std::fabs(jetVoice.fundamentalHz() - pipe) < 8.0);
  CHECK(pipe > 250.0);
  CHECK(pipe < 600.0);
  const double fluteLufs = writeListening(jetSamples, "mmath-flute.wav", kFs);

  // Length is an argument chosen so the cone's second root sits near
  // concert Bb3, 233.08 Hz at A4 = 440. The lip is 0.75 c/L, below that
  // root. The played note is the second register, not the pedal.
  const double trumpetLength = 1.45;
  const double lipMass = 3.0e-5;
  const double lipHz = 0.75 * speed / trumpetLength;
  const double lipOmega = constants::twoPi * lipHz;
  const double lipStiffness = lipOmega * lipOmega * lipMass;
  TrumpetSpec trumpetSpec{trumpetLength, 8.0e-5, kAir,        kTemp, 0.0,
                          8.0e-7,        1.5e-5, 1.2e-2,      lipMass,
                          lipStiffness,  1.2e-5, 0.003};
  const auto trumpet = unwrap(makeTrumpet(trumpetSpec));
  // Apex 0.26 m is an argument. A shorter apex leaves the lip growing for
  // about 0.4 s; this distance reaches a steady second-register tone on
  // the Luce and Clark timescale while the harmonic series stays bright.
  // Tongue release at 25 ms, while the 40 ms Bertsch ramp is still
  // rising. The lip then reaches Luce and Clark's ~50 ms window.
  ValveBlow lip = sustained(2800.0, 10000.0, kTrumpetAttack);
  lip.reflectionMagnitude = 0.999;
  lip.initialDisplacementMeters = 0.0;
  lip.tongueReleaseFrames = kTrumpetTongue;
  auto lipVoice =
      unwrap(ConeValveVoice::fromTrumpet(trumpet, 0.26, lip, kRate));
  CHECK(lipVoice.fundamentalHz() > 80.0);
  CHECK(lipVoice.fundamentalHz() < 160.0);
  auto lipSamples = renderOf(lipVoice, kFrames);
  const double trumpetNote = 230.0;
  checkTone(Tone{"trumpet", trumpetNote, Spectrum::Brass, lipSamples}, kFs);
  const Period trumpetPeriod = periodNear(lipSamples, 70000, trumpetNote, kFs);
  CHECK(trumpetPeriod.frequencyHz > 1.8 * lipVoice.fundamentalHz());
  CHECK(trumpetPeriod.frequencyHz < 2.4 * lipVoice.fundamentalHz());
  const double trumpetLufs =
      writeListening(lipSamples, "mmath-trumpet.wav", kFs);

  // L = 1.65 + 2*0.55 = 2.75 m places the same second-register recipe
  // near concert Bb2, 116.54 Hz. The stable note on this cone is a few
  // hertz above that pitch.
  const double slide = 0.55;
  const double tromboneClosed = 1.65;
  const double tromboneLength = tromboneClosed + 2.0 * slide;
  const double tromboneLip = 0.75 * speed / tromboneLength;
  const double tromboneOmega = constants::twoPi * tromboneLip;
  const double tromboneMass = 6.0e-5;
  TromboneSpec tromboneSpec{tromboneClosed,
                            slide,
                            1.0e-4,
                            kAir,
                            kTemp,
                            0.0,
                            1.0e-6,
                            2.5e-5,
                            1.5e-2,
                            tromboneMass,
                            tromboneOmega * tromboneOmega * tromboneMass,
                            2.0e-5,
                            0.004};
  const auto trombone = unwrap(makeTrombone(tromboneSpec));
  CHECK(std::fabs(trombone.bore.lengthMeters - tromboneLength) < 1.0e-12);
  // Apex 0.60 m is an argument, for the same reason as the trumpet: the
  // lip reaches the second register in a human attack instead of ~0.5 s.
  // Tongue release at 12 ms. A later release overshoots by several
  // decibels and the 1 dB hold waits that ring out.
  ValveBlow bone = sustained(1600.0, 5000.0, kTromboneAttack);
  bone.reflectionMagnitude = 0.999;
  bone.initialDisplacementMeters = 0.0;
  bone.tongueReleaseFrames = kTromboneTongue;
  auto boneVoice =
      unwrap(ConeValveVoice::fromTrombone(trombone, 0.60, bone, kRate));
  CHECK(boneVoice.fundamentalHz() > 40.0);
  CHECK(boneVoice.fundamentalHz() < 80.0);
  CHECK(boneVoice.fundamentalHz() < lipVoice.fundamentalHz());
  auto boneSamples = renderOf(boneVoice, kFrames);
  const double tromboneNote = 122.0;
  checkTone(Tone{"trombone", tromboneNote, Spectrum::Brass, boneSamples}, kFs);
  const Period trombonePeriod =
      periodNear(boneSamples, 70000, tromboneNote, kFs);
  CHECK(trombonePeriod.frequencyHz > 1.8 * boneVoice.fundamentalHz());
  CHECK(trombonePeriod.frequencyHz < 2.4 * boneVoice.fundamentalHz());
  const double tromboneLufs =
      writeListening(boneSamples, "mmath-trombone.wav", kFs);

  // Time to stay within 1 dB of the 1.5–2.0 s level. That hold is
  // stricter than a 10–90% rise, so a cited "significant change" can be
  // shorter than the number checked here. The bands are the measured
  // swells, wide enough for one sample of lag.
  const double clarinetMs = attackMilliseconds(clarinetSamples, kFs);
  const double saxMs = attackMilliseconds(saxSamples, kFs);
  const double fluteMs = attackMilliseconds(jetSamples, kFs);
  const double trumpetMs = attackMilliseconds(lipSamples, kFs);
  const double tromboneMs = attackMilliseconds(boneSamples, kFs);
  std::cout << "attack vs published: clarinet " << clarinetMs
            << " ms (Fuks & Sundberg pressure pattern 0.35–0.60 s)\n";
  std::cout << "attack vs published: saxophone " << saxMs
            << " ms (same 0.35–0.60 s pattern; Li et al. JASA 139, 2016, "
               "clarinet fundamental grows near 1000 dB/s once blowing "
               "pressure is above threshold)\n";
  std::cout << "attack vs published: flute " << fluteMs
            << " ms (Terrien et al. ISMA 2014, 10–30 ms typical, acoustic "
               "rise near 6 ms + 9.5/f0)\n";
  std::cout << "attack vs published: trumpet " << trumpetMs
            << " ms (Luce & Clark JASA 42, 1967, significant change "
               "50±20 ms; Bertsch & Hoole tongue 40–64 ms)\n";
  std::cout << "attack vs published: trombone " << tromboneMs
            << " ms (Luce & Clark: fundamental about seven cycles, higher "
               "partials toward 70 ms; 1 dB hold includes the tail)\n";
  CHECK(clarinetMs > 250.0);
  CHECK(clarinetMs < 600.0);
  CHECK(saxMs > 500.0);
  CHECK(saxMs < 950.0);
  CHECK(fluteMs > 20.0);
  CHECK(fluteMs < 80.0);
  CHECK(trumpetMs > 30.0);
  CHECK(trumpetMs < 90.0);
  CHECK(tromboneMs > 70.0);
  CHECK(tromboneMs < 180.0);

  const double playbackLufs[] = {clarinetLufs, saxLufs, fluteLufs, trumpetLufs,
                                 tromboneLufs};
  double quietest = playbackLufs[0];
  double loudest = playbackLufs[0];
  for (double lufs : playbackLufs) {
    quietest = std::min(quietest, lufs);
    loudest = std::max(loudest, lufs);
  }
  std::cout << "playback span " << (loudest - quietest) << " LU\n";
  CHECK(loudest - quietest <= 1.0);
}

// An excerpt is still the wind tone: the same valves, on a sounding
// length the delay line can already hold. It is not a tone-hole lattice,
// not a valve chart, and not a sampled waveform. The melodies are in
// wind_excerpts.h.
//
// Clarinet — air-column length. The 2 s demo keeps its 600 Hz corner.
// The excerpt uses 1500 Hz. This reed plays about 8 cents sharp of
// c/(4L) at that corner, so the length is the geometric one times
// 2^(8/1200). That offset is this voice, not a published correction.
//
// Saxophone — truncated-cone length, apex fixed, interpolated from the
// lengths that put this reed on concert pitch. Loop corner 1500 Hz,
// Benade and Lutgen envelope 837 Hz, turbulence 0.12. Vibrato is a
// length modulation, arguments 5.5 Hz and 12 cents.
//
// Flute — bore length, jet delay, and blowing pressure together. The
// 2 s corner is 400 Hz, and this excerpt stays in that corner: it does
// not reach the C7 that needed the 1200 Hz argument.
//
// Trumpet and trombone — sounding length plus lip resonance. Brass
// length jumps at the note. A tongued note takes the new length and lip
// while the tongue is shut. There is no slide glissando.
TEST_CASE("wind excerpts are audition passages", "[contact][wind][phrase]") {
  const double speed = unwrap(speedOfSound(Air{TemperatureKelvin{kTemp}})).value();

  auto push = [](std::vector<PhraseNote>& notes, const excerpts::Note& src, double start,
                 double length, double lip, double pressure, double jet, double tol,
                 double minRms) {
    PhraseNote note;
    note.name = src.name;
    note.startSec = start;
    note.targetHz = src.hertz;
    note.lengthM = length;
    note.lipHz = lip;
    note.pressure = src.hertz > 0.0 ? pressure : 0.0;
    note.jetSamples = jet;
    note.tongue = src.tongued;
    note.vibrato = src.vibrato;
    note.dynamic = src.dynamic;
    note.gain = 1.0;
    note.rest = !(src.hertz > 0.0);
    note.tolCents = tol;
    note.minRms = minRms;
    // Grade a note only when a window can sit inside it. Shorter notes
    // are printed, including a DID NOT SPEAK, and do not fail the suite.
    note.require = src.hertz > 0.0 && src.seconds >= 0.45;
    notes.push_back(note);
  };

  auto clarinetLen = [&](double frequency) {
    return speed / (4.0 * frequency) * std::pow(2.0, 8.0 / 1200.0);
  };
  {
    const double total = excerpts::excerptSeconds(excerpts::kClarinet);
    std::cout << "clarinet excerpt: Mozart, Clarinet Concerto K.622, Adagio "
                 "opening, concert pitch. Mechanism: sounding length. "
                 "Corner 1500 Hz.\n";
    double longest = 0.0;
    for (const auto& src : excerpts::kClarinet)
      if (src.hertz > 0.0) longest = std::max(longest, clarinetLen(src.hertz));
    auto clarinet = workingClarinet(longest * 1.04);
    const auto frames = static_cast<std::uint64_t>(std::llround(total * kFs));
    ValveBlow blow = sustained(4000.0, 1500.0, kClarinetAttack);
    blow.frames = frames;
    auto voice = unwrap(BoreValveVoice::fromClarinet(clarinet, blow, kRate));
    std::vector<PhraseNote> notes;
    double start = 0.0;
    double held = longest;
    for (const auto& src : excerpts::kClarinet) {
      const double length = src.hertz > 0.0 ? clarinetLen(src.hertz) : held;
      push(notes, src, start, length, 0.0, 4000.0, 0.0, 25.0, 0.01);
      if (src.hertz > 0.0) held = length;
      start += src.seconds;
    }
    auto samples = renderPhrase(voice, notes, total, false);
    reportPhrase("clarinet", notes, samples, total, 0.50);
    writePhrase(samples, "mmath-excerpt-clarinet.wav", kFs);
  }

  {
    const double anchorHz[] = {138.59, 174.61, 207.65, 277.18, 415.30, 554.37, 698.46, 830.61};
    const double anchorL[] = {1.1120, 0.8241, 0.6800, 0.4786, 0.2880, 0.1979, 0.1429, 0.1134};
    auto saxLength = [&](double frequency) {
      if (frequency <= anchorHz[0]) return anchorL[0] * anchorHz[0] / frequency;
      if (frequency >= anchorHz[7]) return anchorL[7] * anchorHz[7] / frequency;
      int i = 0;
      while (i < 6 && anchorHz[i + 1] < frequency) ++i;
      const double t =
          std::log(frequency / anchorHz[i]) / std::log(anchorHz[i + 1] / anchorHz[i]);
      double length =
          std::exp(std::log(anchorL[i]) * (1.0 - t) + std::log(anchorL[i + 1]) * t);
      // The anchor tubes were tuned in a scale phrase. Between them this
      // reed plays sharp, so each excerpt pitch is lengthened by the
      // played/target ratio from that render. G#3 is the Ab3 anchor and
      // was slightly flat.
      struct Scale { double hertz; double factor; };
      const Scale scale[] = {
          {207.65, 204.527 / 207.65}, {277.18, 293.507 / 277.18},
          {311.13, 332.68 / 311.13},  {329.63, 354.553 / 329.63},
          {369.99, 402.831 / 369.99}, {415.30, 1.15 * (406.161 / 415.30)},
          {466.16, 1.15},             {493.88, 1.15},
      };
      for (const Scale& row : scale)
        if (std::fabs(row.hertz - frequency) < 0.05) length *= row.factor;
      return length;
    };
    const double total = excerpts::excerptSeconds(excerpts::kSaxophone);
    std::cout << "saxophone excerpt: Mussorgsky/Ravel, Pictures at an Exhibition, "
                 "The Old Castle, alto solo, concert pitch. Mechanism: "
                 "truncated-cone length. Acciaccaturas omitted.\n";
    double longest = 0.0;
    for (const auto& src : excerpts::kSaxophone)
      if (src.hertz > 0.0) longest = std::max(longest, saxLength(src.hertz));
    SaxophoneSpec spec{longest * 1.08, 0.20, 1.6e-4, kAir, kTemp, 1.8e-5, 7500.0,
                       2.0e-6, 1.4e-4, 7.0e-4, 0.09};
    auto sax = unwrap(makeSaxophone(spec));
    const auto frames = static_cast<std::uint64_t>(std::llround(total * kFs));
    ValveBlow blow = sustained(4200.0, 1500.0, kSaxophoneAttack);
    blow.frames = frames;
    blow.initialDisplacementMeters = 1.0e-5;
    blow.reedTurbulence = 0.12;
    blow.spectrumBreakHz = 837.0;
    auto voice = unwrap(ConeValveVoice::fromSaxophone(sax, blow, kRate));
    std::vector<PhraseNote> notes;
    double start = 0.0;
    double held = longest;
    for (const auto& src : excerpts::kSaxophone) {
      const double length = src.hertz > 0.0 ? saxLength(src.hertz) : held;
      push(notes, src, start, length, 0.0, 4200.0, 0.0, 35.0, 0.01);
      if (src.hertz > 0.0) held = length;
      start += src.seconds;
    }
    auto samples = renderPhrase(voice, notes, total, false);
    reportPhrase("saxophone", notes, samples, total, 0.50);
    writePhrase(samples, "mmath-excerpt-saxophone.wav", kFs);
  }

  {
    const double span = 7.0e-3;
    const double sigma = 0.45;
    const double thickness = 1.0e-3;
    const double radius = std::sqrt(2.8e-4 / constants::pi);
    const double end = 0.61 * radius;
    auto bore = [&](double frequency) { return speed / (2.0 * frequency) - end; };
    const double total = excerpts::excerptSeconds(excerpts::kFlute);
    std::cout << "flute excerpt: Debussy, Prélude à l'après-midi d'un faune, "
                 "opening, bars 1–2, quarter = 44. Mechanism: bore length, "
                 "jet delay, and pressure. Corner 400 Hz, the 2 s demo.\n";
    double longest = 0.0;
    double lowest = 1.0e9;
    for (const auto& src : excerpts::kFlute) {
      if (!(src.hertz > 0.0)) continue;
      longest = std::max(longest, bore(src.hertz));
      lowest = std::min(lowest, src.hertz);
    }
    FluteSpec spec{longest * 1.04, 2.8e-4, kAir, kTemp, 1.2e-5, thickness, span, sigma};
    auto flute = unwrap(makeFlute(spec));
    auto pressureFor = [&](double frequency) {
      const double jetSpeed = 2.0 * span * frequency / sigma;
      return 0.5 * flute.airDensityKgPerM3 * jetSpeed * jetSpeed;
    };
    // The 20 ms tongue dip divides the jet delay by sqrt(0.55). The buffer
    // is sized from the construction pressure, so that pressure is half
    // the lowest note's, which is a longer delay than the dip.
    const auto frames = static_cast<std::uint64_t>(std::llround(total * kFs));
    ValveBlow jet = sustained(pressureFor(lowest) * 0.50, 400.0, kFluteAttack);
    jet.frames = frames;
    jet.reflectionMagnitude = 0.985;
    jet.initialDisplacementMeters = 0.50 * thickness;
    jet.jetAimMeters = 0.40 * thickness;
    jet.jetNoiseMeters = 0.035 * thickness;
    jet.endCorrectionMeters = end;
    auto voice = unwrap(BoreValveVoice::fromFlute(flute, jet, kRate));
    std::vector<PhraseNote> notes;
    double start = 0.0;
    double heldL = longest;
    double heldJ = 0.0;
    double heldP = pressureFor(lowest);
    for (const auto& src : excerpts::kFlute) {
      const double pressure = src.hertz > 0.0 ? pressureFor(src.hertz) : 0.0;
      const double length = src.hertz > 0.0 ? bore(src.hertz) : heldL;
      const double jetSamples =
          src.hertz > 0.0 ? kFs * unwrap(fluteConvectionTime(flute, pressure)) : heldJ;
      push(notes, src, start, length, 0.0, pressure > 0.0 ? pressure : heldP, jetSamples, 25.0,
           0.01);
      if (src.hertz > 0.0) {
        heldL = length;
        heldJ = jetSamples;
        heldP = pressure;
      }
      start += src.seconds;
    }
    auto samples = renderPhrase(voice, notes, total, true);
    reportPhrase("flute", notes, samples, total, 0.08);
    writePhrase(samples, "mmath-excerpt-flute.wav", kFs);
  }

  {
    struct Finger {
      double hertz;
      double length;
      int mode;
    };
    // Tuned so this lip and this length play the concert pitch, and the
    // Promenade order stays finite. F5 at 1.39 m into D5 at 1.38 m dies
    // on the open-valve slur; 1.410 into 1.386 completes. Mode 0 is the
    // open second-register recipe, lip = 0.75 c/L. Any other mode sets
    // the lip at 0.8 of that cone partial.
    const Finger finger[] = {
        {392.00, 0.804, 0}, {349.23, 0.912, 0}, {466.16, 1.023, 3},
        {523.25, 1.560, 5}, {698.46, 1.410, 6}, {587.33, 1.386, 5},
    };
    auto find = [&](double hertz) {
      for (const Finger& row : finger)
        if (std::fabs(row.hertz - hertz) < 0.02) return row;
      return Finger{};
    };
    const double total = excerpts::excerptSeconds(excerpts::kTrumpet);
    std::cout << "trumpet excerpt: Mussorgsky/Ravel, Pictures, Promenade "
                 "opening, quarter = 91. Mechanism: sounding length and lip "
                 "resonance. Not a piston chart.\n";
    const double mass = 3.0e-5;
    const double lip0 = 0.75 * speed / 1.45;
    const double omega = constants::twoPi * lip0;
    TrumpetSpec spec{1.70, 8.0e-5, kAir, kTemp, 0.0, 8.0e-7, 1.5e-5, 1.2e-2, mass,
                     omega * omega * mass, 1.2e-5, 0.003};
    auto trumpet = unwrap(makeTrumpet(spec));
    const auto frames = static_cast<std::uint64_t>(std::llround(total * kFs));
    ValveBlow lip = sustained(2800.0, 10000.0, kTrumpetAttack);
    lip.frames = frames;
    lip.reflectionMagnitude = 0.999;
    lip.initialDisplacementMeters = 0.0;
    lip.tongueReleaseFrames = kTrumpetTongue;
    auto voice = unwrap(ConeValveVoice::fromTrumpet(trumpet, 0.26, lip, kRate));
    std::vector<PhraseNote> notes;
    double start = 0.0;
    Finger held = finger[0];
    for (const auto& src : excerpts::kTrumpet) {
      const Finger row = src.hertz > 0.0 ? find(src.hertz) : held;
      const double lipHz =
          row.mode == 0 ? 0.75 * speed / row.length : 0.80 * conePartial(row.length, 0.26, row.mode);
      push(notes, src, start, row.length, lipHz, 2800.0, 0.0, 25.0, 0.01);
      if (src.hertz > 0.0) held = row;
      start += src.seconds;
    }
    auto samples = renderPhrase(voice, notes, total, false);
    reportPhrase("trumpet", notes, samples, total, 0.12);
    writePhrase(samples, "mmath-excerpt-trumpet.wav", kFs);
  }

  {
    struct Finger {
      double hertz;
      double length;
      int mode;
    };
    const Finger finger[] = {
        {466.16, 1.354, 4}, {440.00, 1.438, 4}, {392.00, 1.626, 4}, {349.23, 1.346, 3},
        {523.25, 1.200, 4}, {329.63, 1.442, 3}, {293.66, 1.626, 3}, {554.37, 1.750, 6},
    };
    auto find = [&](double hertz) {
      for (const Finger& row : finger)
        if (std::fabs(row.hertz - hertz) < 0.02) return row;
      return Finger{};
    };
    const double total = excerpts::excerptSeconds(excerpts::kTrombone);
    std::cout << "trombone excerpt: Ravel, Boléro, trombone solo, through the "
                 "high Db. Quarter = 72. Mechanism: slide length and lip "
                 "resonance. No glissando.\n";
    const double mass = 6.0e-5;
    const double lip0 = 0.75 * speed / 2.75;
    const double omega = constants::twoPi * lip0;
    auto horn = unwrap(makeTrombone(TromboneSpec{3.05, 0.0, 1.0e-4, kAir, kTemp, 0.0, 1.0e-6,
                                                 2.5e-5, 1.5e-2, mass, omega * omega * mass,
                                                 2.0e-5, 0.004}));
    const auto frames = static_cast<std::uint64_t>(std::llround(total * kFs));
    ValveBlow bone = sustained(1600.0, 5000.0, kTromboneAttack);
    bone.frames = frames;
    bone.reflectionMagnitude = 0.999;
    bone.initialDisplacementMeters = 0.0;
    bone.tongueReleaseFrames = kTromboneTongue;
    auto voice = unwrap(ConeValveVoice::fromTrombone(horn, 0.60, bone, kRate));
    std::vector<PhraseNote> notes;
    double start = 0.0;
    Finger held = finger[0];
    for (const auto& src : excerpts::kTrombone) {
      const Finger row = src.hertz > 0.0 ? find(src.hertz) : held;
      const double lipHz = 0.80 * conePartial(row.length, 0.60, row.mode);
      push(notes, src, start, row.length, lipHz, 1600.0, 0.0, 25.0, 0.005);
      if (src.hertz > 0.0) held = row;
      start += src.seconds;
    }
    auto samples = renderPhrase(voice, notes, total, false);
    reportPhrase("trombone", notes, samples, total, 0.15);
    writePhrase(samples, "mmath-excerpt-trombone.wav", kFs);
  }
}
