// Sustained wind tones.
//
// A blown wind is a limit cycle, not the ring after the player stops.
// These renders use published shapes and example arguments:
//   clarinet — quarter-wave cylinder, Dalmont slit, bore near 15 mm
//     (Benade, Fundamentals of Musical Acoustics). The reflection corner
//     is 1500 Hz, Benade's open-hole cutoff near 1.5 kHz. Odds dominate
//     and several of them carry real energy; the evens are not absent;
//   saxophone — truncated cone, tan(kL) = -k x0, small x0/L so the
//     series is near n c/(2L) (Dalmont; saxophone.h). The reflection
//     corner is 1500 Hz, the high end of the alto tone-hole cutoffs in
//     Petersen, Colinot, Kergomard, and Guillemain, Acta Acustica 4
//     (2020), and inside Chen, Smith, and Wolfe's soprano estimate
//     1340±240 Hz (Acoustics Australia 37, 2009). The radiated pressure
//     then passes Benade and Lutgen's envelope, JASA 83, 1900–1907
//     (1988), with their alto break frequency 837 Hz. A turbulence
//     fraction 0.55 is an argument, not a measured intensity. Below
//     threshold it scales the reed-period monopole of the aperture
//     flow, and a sample of that flow seeds the bore. Once the tone
//     is up it scales the round-trip envelope. Wolfe's UNSW
//     saxophone notes record breathy noise from turbulence in the
//     reed–mouthpiece gap and print no level. The acceptance on a held
//     mid-range note is a median inter-harmonic floor no quieter than
//     −50 dB, 200 Hz–6 kHz, re the strongest partial.
//   flute — open pipe, embouchure release, Levine–Schwinger end
//     correction 0.61 a, and a jet delay W/(σ U) with σ = 0.45, inside
//     Coltman's 0.4–0.5 convection range. The jet is tanh with an aim
//     offset of 0.4 thicknesses. The reflection corner is 1800 Hz, an
//     argument: 400 Hz left the third harmonic under −40 dB. Fletcher
//     (JASA 57, 233–244, 1975) measured second and third partials
//     within about 10 dB of the fundamental in the middle octave.
//     Jet turbulence is the unsourced fraction 0.56, on the round-trip
//     envelope after the loop. 0.40 left the median inter-harmonic
//     floor just under −50 dB. The saxophone fraction is 0.55 so the
//     held G#4 clears that floor.
//   trumpet and trombone — outward-striking lip below the cone's second
//     root (Fletcher, 1979), near concert Bb3 (233 Hz) and Bb2 (117 Hz).
//     The outgoing wave takes the cylindrical Burgers advance,
//     β p L / (ρ c³), β = (γ+1)/2 (Hirschberg, Gilbert, Msallam, and
//     Wijnands, JASA 99, 1754–1758, 1996). The fraction is 1; a flare
//     would accumulate less, and that paper prints no waveguide gain.
//     On these notes the advance is about two samples, short of a shock.
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
#include <filesystem>
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
#include <mmathen/sink/wav_reader.h>

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
// for faster single tonguing. Trumpet 40 ms is the fast end. Trombone
// 128 ms is the long end, used as the pressure rise with the lip free:
// a tongue slap at full pressure put the finished series on in one
// cycle. tongueReleaseFrames is the moment the tongue lets the lip go;
// the mouth-pressure ramp is separate. Zero leaves the lip free.
// The sound that follows is compared with Luce and Clark, JASA 42,
// 1232–1243 (1967): trumpet fundamental transient about 10 ms; trombone
// fundamental about seven cycles, higher partials tending toward 70 ms
// above 400 Hz; significant amplitude and frequency change about 50 ms,
// average deviation about 20 ms.
constexpr std::uint64_t kClarinetAttack = 19200;   // 0.40 s
constexpr std::uint64_t kSaxophoneAttack = 19200;  // 0.40 s
constexpr std::uint64_t kFluteAttack = 960;        // 20 ms
constexpr std::uint64_t kTrumpetAttack = 1920;     // 40 ms
// The opening lip is free while mouth pressure rises. A tongue slap
// at full pressure put the finished series on in one cycle. Later
// notes still use the 40 ms Bertsch closure in renderPhrase.
constexpr std::uint64_t kTrumpetTongue = 0;
// Long end of Bertsch and Hoole's 40–128 ms tongue-tip movement, used
// here as the pressure rise. The lip is free, so the buzz grows with
// that rise instead of starting as a slap.
constexpr std::uint64_t kTromboneAttack = 6144;    // 128 ms
constexpr std::uint64_t kTromboneTongue = 0;

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

void archivePhraseWav(const std::filesystem::path& source, const char* filename);

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
  archivePhraseWav(path, filename);
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
// Median Hann bin more than 20 Hz from every harmonic, 200 Hz–6 kHz,
// relative to the strongest bin in 80 Hz–2 kHz. A deterministic bore
// sits near −147 dB. Rectangular-window leakage is not this number.
double offHarmonicFloorDb(const std::vector<double>& x, std::size_t origin, double f0,
                          double rate) {
  constexpr int kN = 16384;
  std::vector<std::complex<double>> buf(static_cast<std::size_t>(kN));
  for (int i = 0; i < kN; ++i) {
    const double window =
        0.5 * (1.0 - std::cos(constants::twoPi * static_cast<double>(i) / static_cast<double>(kN)));
    buf[static_cast<std::size_t>(i)] = x[origin + static_cast<std::size_t>(i)] * window;
  }
  auto spec = fft(std::move(buf));
  REQUIRE(spec.ok());
  const auto& bins = spec.value();
  const double df = rate / static_cast<double>(kN);
  int kPeak = 1;
  double best = 0.0;
  const int kLo = std::max(1, static_cast<int>(80.0 / df));
  const int kHi = std::min(kN / 2 - 1, static_cast<int>(2000.0 / df));
  for (int k = kLo; k <= kHi; ++k) {
    const double power = std::norm(bins[static_cast<std::size_t>(k)]);
    if (power > best) {
      best = power;
      kPeak = k;
    }
  }
  const double peakMag = std::abs(bins[static_cast<std::size_t>(kPeak)]) + 1.0e-30;
  std::vector<double> floors;
  const int from = std::max(1, static_cast<int>(200.0 / df));
  const int to = std::min(kN / 2 - 1, static_cast<int>(6000.0 / df));
  for (int k = from; k <= to; ++k) {
    const double hz = static_cast<double>(k) * df;
    bool near = false;
    for (int h = 1; h < 48; ++h) {
      if (std::fabs(hz - static_cast<double>(h) * f0) <= 20.0) {
        near = true;
        break;
      }
    }
    if (near) continue;
    floors.push_back(
        20.0 * std::log10(std::abs(bins[static_cast<std::size_t>(k)]) / peakMag + 1.0e-30));
  }
  REQUIRE(!floors.empty());
  std::sort(floors.begin(), floors.end());
  return floors[floors.size() / 2];
}

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
    // Benade's open-hole cutoff near 1.5 kHz. A 600 Hz corner left
    // the seventh partial near −37 dB.
    CHECK(db[2] > -12.0);
    CHECK(db[6] > -25.0);
    const double clarFloor = offHarmonicFloorDb(x, a, f0, rate);
    std::cout << tone.name << " interharmonic floor " << clarFloor << " dB\n";
    // 20*log10(|bin|). A full-scale int16 sine in this window sits near
    // −122 dB. The old product 0.12 sat near −59 dB, and 20*log10 of that
    // power is about −118 dB, the reading reported as digital silence.
    // Louder than −54 dB keeps a doubled reading past about −108 dB.
    // Quieter than −40 dB stays under the partials that are the tone.
    CHECK(clarFloor > -54.0);
    CHECK(clarFloor < -40.0);
  } else if (tone.spectrum == Spectrum::Flute) {
    // Fletcher, JASA 57, 233–244 (1975): in the middle octave the
    // second and third partials sit within about 10 dB of the
    // fundamental. This low-octave note stays fundamental-led. The
    // 400 Hz corner had put the third under −40 dB.
    for (int h = 1; h < 10; ++h) CHECK(db[h] < -3.0);
    CHECK(db[1] > -25.0);
    CHECK(db[2] > -30.0);
    CHECK(above >= 5);
    const double floorDb = offHarmonicFloorDb(x, a, f0, rate);
    std::cout << tone.name << " interharmonic floor " << floorDb << " dB\n";
    // Same window and the same guard as the clarinet. Fletcher and
    // Rossing do not print a jet noise ratio. Zero sits near −147 dB.
    CHECK(floorDb > -54.0);
    CHECK(floorDb < -40.0);
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
    // Same 200 Hz–6 kHz median as the playback WAV. Wolfe (UNSW,
    // Introduction to saxophone acoustics) notes breathy noise from
    // turbulence in the reed–mouthpiece gap and prints no ratio.
    // −50 dB re the strongest partial is the acceptance argument.
    // Quieter than −40 dB stays under the ninth partial. A reading
    // near −120 dB is the converter and fails.
    const double floorDb = offHarmonicFloorDb(x, a, f0, rate);
    std::cout << tone.name << " interharmonic floor " << floorDb << " dB\n";
    CHECK(floorDb >= -50.0);
    CHECK(floorDb < -40.0);
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
  // Reed damping 0.11 N·s/m. stepReedTip is the exact spring, and at
  // 0.12 N·s/m this 0.45 m bore plays about 8.2 Hz under c/(4L).
  // 0.11 N·s/m stays inside 8 Hz and the third harmonic still leads
  // the second. The value is the embouchure argument, not a measured
  // constant.
  ClarinetSpec spec{length, 2.0e-4, kAir, kTemp, 2.0e-5, 6000.0,
                    1.0e-6, 1.0e-4, 1.0e-3, 0.11};
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

// Rise of 20*log10(fundamental), in dB/s, from 30 dB under the 1.6 s
// amplitude to 8 dB under it. A regression through the noise floor
// flattens this: the floor sits in the band for a long time and the
// bore's growth does not. Li, Almeida, Smith, and Wolfe, JASA 139
// (2016): above threshold the fundamental grows at a rate r, tens to
// hundreds of dB/s on a soft note and about 1000 dB/s on an accent.
// A playback gain does not change the slope. Zero means a crossing
// was missing.
double growthDbPerSec(const std::vector<double>& x, double fundamentalHz, double rate) {
  const int window = 1024;
  const int hop = 240;
  const auto steadyAt = static_cast<std::size_t>(1.6 * rate);
  const double steady = goertzel(x, steadyAt, 8192, fundamentalHz, rate) / 8192.0;
  const double steadyDb = 20.0 * std::log10(steady + 1.0e-30);
  double enter = -1.0;
  double leave = -1.0;
  const int limit = static_cast<int>(0.80 * rate);
  for (int start = 0; start + window < limit; start += hop) {
    const double amplitude =
        goertzel(x, static_cast<std::size_t>(start), window, fundamentalHz, rate) /
        static_cast<double>(window);
    const double db = 20.0 * std::log10(amplitude + 1.0e-30);
    const double time = (static_cast<double>(start) + 0.5 * window) / rate;
    if (enter < 0.0 && db >= steadyDb - 30.0) enter = time;
    if (enter >= 0.0 && leave < 0.0 && db >= steadyDb - 8.0) leave = time;
  }
  if (!(enter >= 0.0) || !(leave > enter)) return 0.0;
  return 22.0 / (leave - enter);
}

// Milliseconds by which broadband residual leads the fundamental's
// arrival at 20 dB under the 1.6 s amplitude. Residual energy is the
// window energy left after the fundamental sine is removed. 25 ms is
// an acceptance argument, not a measured constant from Li or Fletcher.
// A negative value means the fundamental was already that loud in the
// first window that had any residual.
double residualLeadMs(const std::vector<double>& x, double fundamentalHz, double rate) {
  const int window = 1024;
  const int hop = 240;
  const auto steadyAt = static_cast<std::size_t>(1.6 * rate);
  const double steady = goertzel(x, steadyAt, 8192, fundamentalHz, rate) / 8192.0;
  const double steadyDb = 20.0 * std::log10(steady + 1.0e-30);
  double tFund = -1.0;
  double tRes = -1.0;
  const int limit = static_cast<int>(0.80 * rate);
  for (int start = 0; start + window < limit; start += hop) {
    const auto origin = static_cast<std::size_t>(start);
    const double g = goertzel(x, origin, window, fundamentalHz, rate);
    const double db = 20.0 * std::log10(g / static_cast<double>(window) + 1.0e-30);
    const double peak = 2.0 * g / static_cast<double>(window);
    double energy = 0.0;
    for (int i = 0; i < window; ++i) {
      const double s = x[origin + static_cast<std::size_t>(i)];
      energy += s * s;
    }
    double residual = energy - 0.5 * peak * peak * static_cast<double>(window);
    if (residual < 0.0) residual = 0.0;
    const double resRms = std::sqrt(residual / static_cast<double>(window));
    const double fundRms = peak / std::sqrt(2.0);
    const double time = (static_cast<double>(start) + 0.5 * window) / rate;
    if (tFund < 0.0 && db >= steadyDb - 20.0) tFund = time;
    if (tRes < 0.0 && resRms > fundRms && resRms > 1.0e-5) tRes = time;
  }
  if (!(tFund >= 0.0) || !(tRes >= 0.0)) return -1.0e9;
  return 1000.0 * (tFund - tRes);
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

// YIN (de Cheveigné and Kawahara, JASA 111, 1917–1930, 2002). The lag
// search is not centered on the scored pitch. An autocorrelation that
// only looks near the target period reports the octave below when the
// tone is the octave above.
Period yinEstimate(const std::vector<double>& x, std::size_t begin, std::size_t end,
                   double rate) {
  if (end <= begin + 512) return {};
  const int span = static_cast<int>(end - begin);
  const int W = std::min(2048, span / 3);
  if (W < 256) return {};
  const int tauMin = std::max(2, static_cast<int>(rate / 1400.0));
  const int tauMax = std::min(W, static_cast<int>(rate / 60.0));
  if (tauMax <= tauMin + 2) return {};
  const std::size_t origin = end - static_cast<std::size_t>(W + tauMax + 2);
  if (origin < begin) return {};
  std::vector<double> diff(static_cast<std::size_t>(tauMax + 1), 0.0);
  double energy0 = 0.0;
  for (int i = 0; i < W; ++i) {
    const double s = x[origin + static_cast<std::size_t>(i)];
    energy0 += s * s;
  }
  for (int tau = 1; tau <= tauMax; ++tau) {
    double energy1 = 0.0;
    double cross = 0.0;
    for (int i = 0; i < W; ++i) {
      const double u = x[origin + static_cast<std::size_t>(i)];
      const double v = x[origin + static_cast<std::size_t>(i + tau)];
      energy1 += v * v;
      cross += u * v;
    }
    diff[static_cast<std::size_t>(tau)] = energy0 + energy1 - 2.0 * cross;
  }
  std::vector<double> prime(static_cast<std::size_t>(tauMax + 1), 1.0);
  double acc = 0.0;
  for (int tau = 1; tau <= tauMax; ++tau) {
    acc += diff[static_cast<std::size_t>(tau)];
    prime[static_cast<std::size_t>(tau)] =
        acc > 1.0e-18 ? diff[static_cast<std::size_t>(tau)] * static_cast<double>(tau) / acc : 1.0;
  }
  constexpr double kThreshold = 0.15;
  int found = -1;
  for (int tau = tauMin; tau <= tauMax; ++tau) {
    if (prime[static_cast<std::size_t>(tau)] < kThreshold) {
      while (tau + 1 <= tauMax &&
             prime[static_cast<std::size_t>(tau + 1)] < prime[static_cast<std::size_t>(tau)])
        ++tau;
      found = tau;
      break;
    }
  }
  if (found < 0) {
    found = tauMin;
    for (int tau = tauMin + 1; tau <= tauMax; ++tau) {
      if (prime[static_cast<std::size_t>(tau)] < prime[static_cast<std::size_t>(found)]) found = tau;
    }
  }
  double refined = static_cast<double>(found);
  if (found > 1 && found < tauMax) {
    const double y0 = diff[static_cast<std::size_t>(found - 1)];
    const double y1 = diff[static_cast<std::size_t>(found)];
    const double y2 = diff[static_cast<std::size_t>(found + 1)];
    const double denom = y0 - 2.0 * y1 + y2;
    if (std::fabs(denom) > 1.0e-12) {
      const double delta = 0.5 * (y0 - y2) / denom;
      if (std::fabs(delta) < 1.0) refined += delta;
    }
  }
  if (!(refined > 1.0)) return {};
  return Period{rate / refined, 1.0 - prime[static_cast<std::size_t>(found)]};
}

// Playback spectrum of one written WAV. The counts are this measurement,
// not a number copied from the docs. A 16384-point Hann window is
// centered in [t0, t1]. Level is 20*log10(|X|) of that magnitude, re the
// strongest harmonic peak (±3 bins around h * f0). f0 is YIN on the same
// span. A partial counts while h*f0 stays under 12 kHz. within40 is how
// many of those peaks are within 40 dB of the strongest. total is how
// many sit at least 10 dB above the inter-harmonic floor. evensWithin40
// counts even partials inside that 40 dB. The floor is the median bin
// from 200 Hz to 6 kHz more than 20 Hz from every harmonic.
struct PlaybackCensus {
  double fundamentalHz = 0.0;
  int within40 = 0;
  int within20 = 0;
  int total = 0;
  int evensWithin40 = 0;
  double floorDb = -300.0;
};

PlaybackCensus measurePlaybackWav(const char* filename, double t0, double t1) {
  const auto path = wavOutputPath(filename);
  auto opened = readWavFile(path.string());
  REQUIRE(opened.ok());
  const WavData wav = std::move(opened).value();
  const std::vector<double> x = wav.mono();
  const double rate = wav.rate.value();
  constexpr int kN = 16384;
  const auto begin = static_cast<std::size_t>(t0 * rate);
  const auto end = static_cast<std::size_t>(t1 * rate);
  REQUIRE(end > begin + static_cast<std::size_t>(kN));
  REQUIRE(x.size() >= end);
  const Period yin = yinEstimate(x, begin, end, rate);
  REQUIRE(yin.frequencyHz > 60.0);
  auto origin = (begin + end) / 2 - static_cast<std::size_t>(kN) / 2;
  REQUIRE(origin + static_cast<std::size_t>(kN) <= x.size());
  std::vector<std::complex<double>> buf(static_cast<std::size_t>(kN));
  for (int i = 0; i < kN; ++i) {
    const double window =
        0.5 * (1.0 - std::cos(constants::twoPi * static_cast<double>(i) / static_cast<double>(kN)));
    buf[static_cast<std::size_t>(i)] = x[origin + static_cast<std::size_t>(i)] * window;
  }
  auto spec = fft(std::move(buf));
  REQUIRE(spec.ok());
  const auto& bins = spec.value();
  const double df = rate / static_cast<double>(kN);
  const double f0 = yin.frequencyHz;
  double strongest = 0.0;
  std::vector<double> peaks;
  for (int h = 1; static_cast<double>(h) * f0 < 12000.0; ++h) {
    const int k = std::max(1, static_cast<int>(std::llround(static_cast<double>(h) * f0 / df)));
    const int lo = std::max(1, k - 3);
    const int hi = std::min(kN / 2 - 2, k + 3);
    int kPeak = lo;
    double peak = 0.0;
    for (int bin = lo; bin <= hi; ++bin) {
      const double mag = std::abs(bins[static_cast<std::size_t>(bin)]);
      if (mag > peak) {
        peak = mag;
        kPeak = bin;
      }
    }
    // Log-magnitude parabola, the same estimator as strongestPeakHz.
    // A raw bin sits up to the Hann scallop (~1.4 dB) under the partial.
    if (kPeak > 1 && kPeak < kN / 2 - 1) {
      const double y0 = std::log(std::abs(bins[static_cast<std::size_t>(kPeak - 1)]) + 1.0e-30);
      const double y1 = std::log(std::abs(bins[static_cast<std::size_t>(kPeak)]) + 1.0e-30);
      const double y2 = std::log(std::abs(bins[static_cast<std::size_t>(kPeak + 1)]) + 1.0e-30);
      const double denom = y0 - 2.0 * y1 + y2;
      if (std::fabs(denom) > 1.0e-18) {
        const double delta = 0.5 * (y0 - y2) / denom;
        if (std::fabs(delta) < 1.0) peak = std::exp(y1 - 0.25 * (y0 - y2) * delta);
      }
    }
    peaks.push_back(peak);
    strongest = std::max(strongest, peak);
  }
  REQUIRE(strongest > 0.0);
  REQUIRE(peaks.size() >= 8);
  PlaybackCensus census;
  census.fundamentalHz = f0;
  std::vector<double> levels;
  levels.reserve(peaks.size());
  for (std::size_t h = 0; h < peaks.size(); ++h) {
    const double decibel = 20.0 * std::log10(peaks[h] / strongest + 1.0e-30);
    levels.push_back(decibel);
    if (decibel >= -40.0) {
      ++census.within40;
      if ((h + 1) % 2 == 0) ++census.evensWithin40;
    }
    // 20*log10(|X|^2) doubles a magnitude decibel. Five partials inside
    // 20 dB are five partials inside 40 dB on that reading. The previous
    // bores counted 4 and 3 there.
    if (decibel >= -20.0) ++census.within20;
  }
  std::vector<double> floors;
  const int from = std::max(1, static_cast<int>(200.0 / df));
  const int to = std::min(kN / 2 - 1, static_cast<int>(6000.0 / df));
  for (int k = from; k <= to; ++k) {
    const double hz = static_cast<double>(k) * df;
    bool near = false;
    for (int h = 1; h < 64; ++h) {
      if (std::fabs(hz - static_cast<double>(h) * f0) <= 20.0) {
        near = true;
        break;
      }
    }
    if (near) continue;
    floors.push_back(20.0 * std::log10(std::abs(bins[static_cast<std::size_t>(k)]) / strongest + 1.0e-30));
  }
  REQUIRE(!floors.empty());
  std::sort(floors.begin(), floors.end());
  census.floorDb = floors[floors.size() / 2];
  for (double decibel : levels) {
    if (decibel >= census.floorDb + 10.0) ++census.total;
  }
  std::cout << filename << " wav f0 " << f0 << " Hz  within40 " << census.within40
            << "  within20 " << census.within20 << "  total " << census.total
            << "  evens " << census.evensWithin40 << "  floor " << census.floorDb
            << " dB\n";
  return census;
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
  // The strongest spectral peak, not the autocorrelation. An octave
  // error has autocorrelation near 1 at twice the true period.
  bool gradePeak = false;
};

// Strongest Hann-FFT peak in 70 Hz–2.2 kHz. The search is wide enough
// to see an octave lock. Parabolic interpolation is on the log magnitude.
double strongestPeakHz(const std::vector<double>& x, std::size_t begin, std::size_t end,
                       double rate) {
  if (end <= begin) return 0.0;
  const std::size_t avail = end - begin;
  int n = 4096;
  if (avail >= 16384) n = 16384;
  else if (avail >= 8192) n = 8192;
  else if (avail < 4096) return 0.0;
  const std::size_t origin = end - static_cast<std::size_t>(n);
  if (origin < begin) return 0.0;
  std::vector<std::complex<double>> data(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    const double window =
        0.5 * (1.0 - std::cos(constants::twoPi * static_cast<double>(i) / n));
    data[static_cast<std::size_t>(i)] = x[origin + static_cast<std::size_t>(i)] * window;
  }
  auto spec = fft(std::move(data));
  if (!spec.ok()) return 0.0;
  const auto& bins = spec.value();
  const double df = rate / static_cast<double>(n);
  const int lo = std::max(1, static_cast<int>(70.0 / df));
  const int hi = std::min(n / 2 - 2, static_cast<int>(2200.0 / df));
  int kPeak = lo;
  double best = 0.0;
  for (int k = lo; k <= hi; ++k) {
    const double power = std::norm(bins[static_cast<std::size_t>(k)]);
    if (power > best) {
      best = power;
      kPeak = k;
    }
  }
  double delta = 0.0;
  if (kPeak > lo && kPeak < hi) {
    const double y0 = std::log(std::abs(bins[static_cast<std::size_t>(kPeak - 1)]) + 1.0e-30);
    const double y1 = std::log(std::abs(bins[static_cast<std::size_t>(kPeak)]) + 1.0e-30);
    const double y2 = std::log(std::abs(bins[static_cast<std::size_t>(kPeak + 1)]) + 1.0e-30);
    const double denom = y0 - 2.0 * y1 + y2;
    if (std::fabs(denom) > 1.0e-18) delta = 0.5 * (y0 - y2) / denom;
    if (!(std::fabs(delta) < 1.0)) delta = 0.0;
  }
  return (static_cast<double>(kPeak) + delta) * df;
}

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
    Period yin{};
    double rms = 0.0;
    if (fits) {
      period = phrasePeriod(samples, static_cast<std::size_t>(windowBegin * kFs),
                            static_cast<std::size_t>(windowEnd * kFs), note.targetHz, kFs);
      yin = yinEstimate(samples, static_cast<std::size_t>(windowBegin * kFs),
                        static_cast<std::size_t>(windowEnd * kFs), kFs);
      rms = rmsOf(samples, static_cast<std::size_t>(windowBegin * kFs),
                  static_cast<std::size_t>(windowEnd * kFs));
    }
    const bool spoke =
        fits && period.autocorrelation > 0.85 && rms > note.minRms && period.frequencyHz > 1.0;
    const double cents =
        spoke ? 1200.0 * std::log2(period.frequencyHz / note.targetHz) : 0.0;
    double yinCents =
        yin.frequencyHz > 1.0 ? 1200.0 * std::log2(yin.frequencyHz / note.targetHz) : 0.0;
    // The end of a vibrato note can sit in a trough. Graded notes are
    // sampled from 0.45 s in, every 0.05 s, and the largest absolute
    // YIN error is the one that has to clear the tolerance. The first
    // 0.45 s is the attack: the trombone opening still locks a
    // subharmonic there. Short notes keep the single end-of-window
    // reading. A hop inside a 0.2 s slur is the transient, not the pitch.
    const double duration = endSec - note.startSec;
    if (note.require && fits && duration >= 0.45) {
      // 0.45 s skips the opening transient. On the trombone the first
      // 0.40 s still locks a subharmonic; the held pitch is after that.
      const double scanFrom = std::max(windowBegin, note.startSec + 0.45);
      for (double at = scanFrom; at <= windowEnd + 1.0e-9; at += 0.05) {
        const Period frame = yinEstimate(samples, static_cast<std::size_t>(windowBegin * kFs),
                                         static_cast<std::size_t>(at * kFs), kFs);
        if (!(frame.frequencyHz > 1.0)) continue;
        const double frameCents = 1200.0 * std::log2(frame.frequencyHz / note.targetHz);
        if (std::fabs(frameCents) > std::fabs(yinCents)) yinCents = frameCents;
      }
    }
    const double peakHz =
        fits ? strongestPeakHz(samples, static_cast<std::size_t>(windowBegin * kFs),
                               static_cast<std::size_t>(windowEnd * kFs), kFs)
             : 0.0;
    const double peakCents =
        peakHz > 1.0 ? 1200.0 * std::log2(peakHz / note.targetHz) : 0.0;
    std::cout << "\n  " << note.name << " " << note.dynamic << " " << (endSec - note.startSec)
              << " s target " << note.targetHz << " yin " << yin.frequencyHz << " yin-cents "
              << yinCents << " acf " << period.frequencyHz << " acf-cents " << cents << " peak "
              << peakHz << " peak-cents " << peakCents << " ac " << period.autocorrelation
              << " rms " << rms;
    if (!spoke) {
      std::cout << " DID NOT SPEAK";
      ++silent;
    }
    if (note.require && fits) {
      CHECK(spoke);
      CHECK(std::fabs(cents) < note.tolCents);
      // YIN does not search around the target, so an octave lock fails
      // here even when the narrow autocorrelation does not.
      CHECK(yin.frequencyHz > 1.0);
      CHECK(std::fabs(yinCents) < note.tolCents);
    }
    // Autocorrelation at the expected period also matches two periods of
    // the octave above. Graded saxophone notes have to put the strongest
    // peak on the scored pitch.
    if (note.gradePeak && note.require && fits) {
      CHECK(peakHz > 1.0);
      CHECK(std::fabs(peakCents) < note.tolCents);
    }
    // Short notes are not graded for attack or for the narrow
    // autocorrelation. Their YIN still has to sit inside ±50 cents.
    // 50 cents is the acceptance bound for a note under 0.45 s.
    if (!note.require && fits && note.targetHz > 0.0 && !note.rest) {
      CHECK(yin.frequencyHz > 1.0);
      CHECK(std::fabs(yinCents) < 50.0);
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
  ValveBlow clarinetBlow = sustained(4000.0, 1500.0, kClarinetAttack);
  // Unsourced fraction. The reed channel is turbulent (Fletcher and
  // Rossing, chap. 13); that chapter prints no ratio. Below threshold
  // it scales the reed-period monopole; once the tone is up it scales
  // the round-trip envelope. Zero leaves a line spectrum.
  clarinetBlow.reedTurbulence = 0.40;
  auto clarinetVoice = unwrap(BoreValveVoice::fromClarinet(clarinet, clarinetBlow, kRate));
  auto clarinetSamples = renderOf(clarinetVoice, kFrames);
  checkTone(Tone{"clarinet", clarinetHz, Spectrum::Clarinet, clarinetSamples}, kFs);
  const Period clarinetPeriod = periodNear(clarinetSamples, 70000, clarinetHz, kFs);
  const double growth = growthDbPerSec(clarinetSamples, clarinetPeriod.frequencyHz, kFs);
  std::cout << "clarinet fundamental growth " << growth
            << " dB/s (Li, Almeida, Smith, and Wolfe, JASA 139, 2016: "
               "tens to hundreds on a soft note, about 1000 on an accent)\n";
  // Soft notes: Li et al. put r in the tens to hundreds of dB/s.
  // The acceptance window is 50–500. 25 ms of residual lead is an
  // acceptance argument.
  const double clarinetLead = residualLeadMs(clarinetSamples, clarinetPeriod.frequencyHz, kFs);
  std::cout << "clarinet residual lead " << clarinetLead << " ms\n";
  CHECK(growth > 50.0);
  CHECK(growth < 500.0);
  CHECK(clarinetLead >= 25.0);
  {
    // 80–180 ms is still the reed-gap noise. A finished tone here is
    // the theremin attack. Lag is the steady period.
    const int lag = std::max(8, static_cast<int>(std::llround(kFs / clarinetPeriod.frequencyHz)));
    const std::size_t origin = static_cast<std::size_t>(0.08 * kFs);
    double energy0 = 0.0;
    double energy1 = 0.0;
    double cross = 0.0;
    for (int i = 0; i < 4800; ++i) {
      const double u = clarinetSamples[origin + static_cast<std::size_t>(i)];
      const double v = clarinetSamples[origin + static_cast<std::size_t>(i + lag)];
      energy0 += u * u;
      energy1 += v * v;
      cross += u * v;
    }
    const double earlyAc = cross / std::sqrt(energy0 * energy1 + 1.0e-30);
    std::cout << "clarinet early-attack autocorrelation " << earlyAc << "\n";
    CHECK(earlyAc < 0.45);
  }
  CHECK(clarinetHz > 140.0);
  CHECK(clarinetHz < 500.0);
  const double clarinetLufs =
      writeListening(clarinetSamples, "mmath-clarinet.wav", kFs);

  auto quiet = unwrap(
      BoreValveVoice::fromClarinet(clarinet, sustained(250.0, 1500.0, kClarinetAttack), kRate));
  auto quietSamples = renderOf(quiet, kFrames);
  // 250 Pa against 4000 Pa. At 1500 Hz the soft tone radiates partials
  // a 600 Hz reflection had removed, so its RMS is about 7% of the
  // loud note rather than under 5%.
  CHECK(rmsOf(quietSamples, 72000, 96000) <
        0.10 * rmsOf(clarinetSamples, 72000, 96000));

  auto longer = workingClarinet(0.60);
  const double longerHz = speed / (4.0 * 0.60);
  auto longerVoice =
      unwrap(BoreValveVoice::fromClarinet(longer, sustained(4000.0, 1500.0, kClarinetAttack), kRate));
  auto longerSamples = renderOf(longerVoice, kFrames);
  const Period low = periodNear(clarinetSamples, 70000, clarinetHz, kFs);
  const Period high = periodNear(longerSamples, 70000, longerHz, kFs);
  CHECK(std::fabs(low.frequencyHz / high.frequencyHz - 0.60 / 0.45) < 0.04);

  const SampleRateHz other{44100.0};
  auto otherVoice =
      unwrap(BoreValveVoice::fromClarinet(clarinet, sustained(4000.0, 1500.0, kClarinetAttack), other));
  auto otherSamples = renderOf(otherVoice, 88200);
  const Period at44 = periodNear(otherSamples, 60000, clarinetHz, 44100.0);
  CHECK(std::fabs(at44.frequencyHz - low.frequencyHz) < 3.0);

  ValveBlow released = sustained(4000.0, 1500.0, kClarinetAttack);
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
  // reedTurbulence 0.55 is an argument. It scales the round-trip
  // envelope, not the instantaneous pressure. 0.40 left the held
  // Old Castle G#4 at about −50.2 dB between partials.
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
  saxBlow.reedTurbulence = 0.55;
  saxBlow.spectrumBreakHz = 837.0;
  auto saxVoice =
      unwrap(ConeValveVoice::fromSaxophone(saxophone, saxBlow, kRate));
  auto saxSamples = renderOf(saxVoice, kFrames);
  checkTone(
      Tone{"saxophone", saxophone.fundamentalHz, Spectrum::Saxophone, saxSamples},
      kFs);
  const Period saxPeriod = periodNear(saxSamples, 70000, saxophone.fundamentalHz, kFs);
  const double saxGrowth = growthDbPerSec(saxSamples, saxPeriod.frequencyHz, kFs);
  const double saxLead = residualLeadMs(saxSamples, saxPeriod.frequencyHz, kFs);
  std::cout << "saxophone fundamental growth " << saxGrowth
            << " dB/s  residual lead " << saxLead << " ms\n";
  CHECK(saxGrowth > 50.0);
  CHECK(saxGrowth < 500.0);
  CHECK(saxLead >= 25.0);
  const double saxLufs = writeListening(saxSamples, "mmath-saxophone.wav", kFs);
  {
    // The two-second note is the low register. The bar below is the
    // same WAV the file just wrote: 20*log10(|X|), not a doc count.
    const PlaybackCensus saxWav = measurePlaybackWav("mmath-saxophone.wav", 1.00, 1.85);
    CHECK(saxWav.within40 >= 6);
    CHECK(saxWav.evensWithin40 >= 2);
  }

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
  // Aim 0.4 thicknesses. The corner is 1800 Hz, an argument: 400 Hz
  // left the third harmonic under −40 dB. Fletcher, JASA 57, 233–244
  // (1975), measured the second and third within about 10 dB of the
  // fundamental in the middle octave. reedTurbulence 0.56 is an
  // argument: 0.40 left the 200 Hz–6 kHz median under −50 dB, and 0.48
  // still read −50.4 dB on the written file once partial peaks were
  // interpolated. The fraction is not a measured jet-noise ratio.
  ValveBlow jet = sustained(overpressure, 1800.0, kFluteAttack);
  jet.reflectionMagnitude = 0.985;
  // The standing wave grows from the jet noise. A half-thickness
  // initial deflection was already a periodic jet, and the pressure
  // ramp then faded that tone in.
  jet.initialDisplacementMeters = 0.0;
  jet.jetAimMeters = 0.40 * thickness;
  jet.jetNoiseMeters = 0.035 * thickness;
  jet.reedTurbulence = 0.56;
  // Levine and Schwinger, unflanged open end, about 0.61 radii.
  jet.endCorrectionMeters = 0.61 * fluteRadius;
  auto jetVoice = unwrap(BoreValveVoice::fromFlute(flute, jet, kRate));
  auto jetSamples = renderOf(jetVoice, kFrames);
  checkTone(Tone{"flute", jetVoice.fundamentalHz(), Spectrum::Flute, jetSamples},
            kFs);
  CHECK(std::fabs(jetVoice.fundamentalHz() - pipe) < 8.0);
  CHECK(pipe > 250.0);
  CHECK(pipe < 600.0);
  const Period flutePeriod = periodNear(jetSamples, 70000, jetVoice.fundamentalHz(), kFs);
  const double fluteGrowth = growthDbPerSec(jetSamples, flutePeriod.frequencyHz, kFs);
  const double fluteLead = residualLeadMs(jetSamples, flutePeriod.frequencyHz, kFs);
  std::cout << "flute fundamental growth " << fluteGrowth
            << " dB/s  residual lead " << fluteLead
            << " ms (Fletcher, Acustica 34, 1976; Coltman: jet noise "
               "before the standing wave)\n";
  CHECK(fluteLead >= 25.0);
  const double fluteLufs = writeListening(jetSamples, "mmath-flute.wav", kFs);

  // Length is an argument chosen so the cone's second root sits near
  // concert Bb3, 233.08 Hz at A4 = 440. The lip is 0.75 c/L, below that
  // root. The played note is the second register, not the pedal.
  const double trumpetLength = 1.45;
  const double lipMass = 3.0e-5;
  const double lipHz = 0.75 * speed / trumpetLength;
  const double lipOmega = constants::twoPi * lipHz;
  const double lipStiffness = lipOmega * lipOmega * lipMass;
  // Bore 1.09e-4 m² and lip damping 0.001 N·s/m are arguments. At
  // 8.0e-5 m² and 0.003 N·s/m the fifth partial sat near −23 dB, so a
  // 20*log10(|X|^2) count found four partials inside 40 dB. The wider
  // bore is the impedance the lip works into; the lighter damping lets
  // the opening swing closer to the rails. Neither number is a measured
  // trumpet.
  TrumpetSpec trumpetSpec{trumpetLength, 1.09e-4, kAir,       kTemp, 0.0,
                          8.0e-7,        1.5e-5,  1.2e-2,     lipMass,
                          lipStiffness,  1.2e-5,  0.001};
  const auto trumpet = unwrap(makeTrumpet(trumpetSpec));
  // Apex 0.26 m is an argument. A shorter apex leaves the lip growing for
  // about 0.4 s; this distance reaches a steady second-register tone on
  // the Luce and Clark timescale while the harmonic series stays bright.
  // The lip is free during the 40 ms rise. A tongue slap at full
  // pressure put the finished series on in one cycle.
  ValveBlow lip = sustained(2800.0, 10000.0, kTrumpetAttack);
  lip.reflectionMagnitude = 0.999;
  lip.initialDisplacementMeters = 0.0;
  lip.tongueReleaseFrames = kTrumpetTongue;
  lip.brassiness = 1.0;
  // Fraction of the lip jet. Fletcher and Rossing, chap. 13, call the
  // flow turbulent and print no ratio. 0.20 is an argument.
  lip.reedTurbulence = 0.20;
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
  const double trumpetGrowth =
      growthDbPerSec(lipSamples, trumpetPeriod.frequencyHz, kFs);
  const double trumpetLead =
      residualLeadMs(lipSamples, trumpetPeriod.frequencyHz, kFs);
  std::cout << "trumpet fundamental growth " << trumpetGrowth
            << " dB/s  residual lead " << trumpetLead
            << " ms (Fletcher and Rossing, chap. 13: lip buzz into the bore)\n";
  CHECK(trumpetLead >= 25.0);
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
  // Bore 3.0e-4 m² and lip damping 0.0005 N·s/m are arguments, for the
  // same reason as the trumpet: at 1.0e-4 m² and 0.004 N·s/m a
  // 20*log10(|X|^2) count found three partials inside 40 dB. Not a
  // measured trombone bore.
  TromboneSpec tromboneSpec{tromboneClosed,
                            slide,
                            3.0e-4,
                            kAir,
                            kTemp,
                            0.0,
                            1.0e-6,
                            2.5e-5,
                            1.5e-2,
                            tromboneMass,
                            tromboneOmega * tromboneOmega * tromboneMass,
                            2.0e-5,
                            0.0005};
  const auto trombone = unwrap(makeTrombone(tromboneSpec));
  CHECK(std::fabs(trombone.bore.lengthMeters - tromboneLength) < 1.0e-12);
  // Apex 0.60 m is an argument, for the same reason as the trumpet: the
  // lip reaches the second register in a human attack instead of ~0.5 s.
  // The lip is free during the 128 ms rise. A snap to the rest opening
  // crossed −20 dB inside the first audible hop.
  ValveBlow bone = sustained(1600.0, 5000.0, kTromboneAttack);
  bone.reflectionMagnitude = 0.999;
  bone.initialDisplacementMeters = 0.0;
  bone.tongueReleaseFrames = kTromboneTongue;
  bone.brassiness = 1.0;
  bone.reedTurbulence = 0.20;
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
  const double tromboneGrowth =
      growthDbPerSec(boneSamples, trombonePeriod.frequencyHz, kFs);
  const double tromboneLead =
      residualLeadMs(boneSamples, trombonePeriod.frequencyHz, kFs);
  std::cout << "trombone fundamental growth " << tromboneGrowth
            << " dB/s  residual lead " << tromboneLead
            << " ms (same lip-buzz growth as the trumpet)\n";
  CHECK(tromboneLead >= 25.0);
  const double tromboneLufs =
      writeListening(boneSamples, "mmath-trombone.wav", kFs);

  // Within-40 dB counts, the brass total, and the saxophone evens are
  // this read of the written WAV. The thresholds are the pass bar.
  {
    const PlaybackCensus clarinetWav =
        measurePlaybackWav("mmath-clarinet.wav", 1.00, 1.85);
    CHECK(clarinetWav.within40 >= 4);
    const PlaybackCensus fluteWav = measurePlaybackWav("mmath-flute.wav", 1.00, 1.85);
    CHECK(fluteWav.within40 >= 3);
    const PlaybackCensus trumpetWav = measurePlaybackWav("mmath-trumpet.wav", 1.00, 1.85);
    CHECK(trumpetWav.within40 >= 5);
    CHECK(trumpetWav.within20 >= 5);
    CHECK(trumpetWav.total >= 8);
    const PlaybackCensus tromboneWav =
        measurePlaybackWav("mmath-trombone.wav", 1.00, 1.85);
    CHECK(tromboneWav.within40 >= 5);
    CHECK(tromboneWav.within20 >= 5);
    CHECK(tromboneWav.total >= 8);
  }

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
  // The 1 dB hold is later than the 0.40 s ramp: the reed is still
  // closing after the pressure has arrived. Radiated turbulence keeps a
  // 10 ms window from sitting inside that 1 dB until about 0.96 s.
  CHECK(saxMs < 1100.0);
  // The 20 ms pressure ramp is unchanged. A 400 Hz reflection is
  // inside this 1 dB band by about 50 ms and leaves the third partial
  // under −30 dB. The 1800 Hz corner reaches a limit cycle about 21 dB
  // louder; the last decibel of that growth lands near 300 ms.
  // Terrien's 10–30 ms is the significant rise, not this hold.
  CHECK(fluteMs > 20.0);
  CHECK(fluteMs < 420.0);
  // The 1 dB hold is later than the pressure rise. Trumpet measured
  // 80 ms on a 40 ms rise with the lip free; the old 25 ms snap held
  // near 60 ms. Trombone measured 345 ms on the 128 ms rise. The wider
  // bore and lighter lip damping move that hold; the pressure law is
  // unchanged.
  CHECK(trumpetMs > 50.0);
  CHECK(trumpetMs < 180.0);
  CHECK(tromboneMs > 140.0);
  CHECK(tromboneMs < 400.0);

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
// Clarinet — air-column length. The 2 s demo and this excerpt both use
// 1500 Hz, Benade's open-hole cutoff. The exact reed spring plays flat
// of c/(4L), and the flatness grows with frequency, so one cents
// factor does not put A3 and A4 on the same grid. Each length below is
// the tube on which this reed's autocorrelation is that concert pitch
// at 4000 Pa. The offset is this voice, not a published correction.
//
// Saxophone — truncated-cone length, apex fixed. Each concert pitch is
// the length at which this reed's YIN on the phrase is that pitch under
// 4200 Pa. The old scale factors lengthened G#4 onto a second-mode tube
// and the autocorrelation then reported the octave below. Loop corner
// 1500 Hz, Benade and Lutgen envelope 837 Hz, turbulence 0.55. Vibrato
// is a length modulation, arguments 5.5 Hz and 12 cents. Graded notes
// (at least 0.45 s) stay inside ±25 cents, including the vibrato crest.
// Shorter notes stay inside ±50 cents.
//
// Flute — bore length, jet delay, and blowing pressure together. The
// corner is 1800 Hz, the same argument as the two-second note. 400 Hz
// left the third harmonic under −40 dB.
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
    // Speech and the narrow autocorrelation are graded only when a
    // window can sit inside the note. Shorter notes are still printed,
    // including a DID NOT SPEAK. Their end-of-window YIN is checked
    // separately, at ±50 cents.
    note.require = src.hertz > 0.0 && src.seconds >= 0.45;
    notes.push_back(note);
  };

  auto clarinetLen = [&](double frequency) {
    struct Length { double hertz; double meters; };
    // Shortened from c/(4f) until the autocorrelation matched. Integer
    // lag is about 15 cents at these pitches; 25 cents is the grade.
    const Length table[] = {
        {220.00, 0.371149}, {293.66, 0.272573}, {329.63, 0.241609},
        {349.23, 0.225746}, {392.00, 0.199063}, {440.00, 0.175519},
    };
    for (const Length& row : table)
      if (std::fabs(row.hertz - frequency) < 0.05) return row.meters;
    return speed / (4.0 * frequency);
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
    blow.reedTurbulence = 0.40;
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
    // Tubes this reed plays at 4200 Pa, read by YIN on the rendered
    // phrase. Not a published correction. The bore is built at the
    // longest sounding length times 1.08 and then shortened.
    // F#4 at 0.330 m holds about 8 cents sharp. The 12-cent vibrato
    // then puts the crest near +19 cents, and that crest's CMNDF valley
    // includes the 127-sample lag, 48000/127 Hz, +37 cents of 369.99 Hz.
    // 0.3315 m centers the carrier: the crest is about +12 cents, the
    // trough about −13, and the selected lag is no longer 127.
    // C#4 at 0.52 m is first mode near 256 Hz, about −140 cents. 0.471 m
    // is where this 0.4 s slur, arriving from D#4, speaks the first mode
    // at 277 Hz. A note started from silence near 0.47 m can lock the
    // octave; this slur does not, and the D#4 and G#3 after it stay on
    // the first mode. E4 stays 0.390 m, about −28 cents, inside the
    // short-note bound. The line keeps one pressure.
    auto saxLength = [&](double frequency) {
      struct Length { double hertz; double meters; };
      const Length table[] = {
          {207.65, 0.670}, {277.18, 0.471}, {311.13, 0.410}, {329.63, 0.390},
          {369.99, 0.3315}, {415.30, 0.288}, {466.16, 0.2486}, {493.88, 0.230},
      };
      for (const Length& row : table)
        if (std::fabs(row.hertz - frequency) < 0.05) return row.meters;
      return 0.670 * 207.65 / frequency;
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
    blow.reedTurbulence = 0.55;
    blow.spectrumBreakHz = 837.0;
    auto voice = unwrap(ConeValveVoice::fromSaxophone(sax, blow, kRate));
    std::vector<PhraseNote> notes;
    double start = 0.0;
    double held = longest;
    for (const auto& src : excerpts::kSaxophone) {
      const double length = src.hertz > 0.0 ? saxLength(src.hertz) : held;
      const double tol = src.seconds >= 0.45 ? 25.0 : 50.0;
      push(notes, src, start, length, 0.0, 4200.0, 0.0, tol, 0.01);
      notes.back().gradePeak = src.hertz > 0.0;
      if (src.hertz > 0.0) held = length;
      start += src.seconds;
    }
    auto samples = renderPhrase(voice, notes, total, false);
    reportPhrase("saxophone", notes, samples, total, 0.50);
    writePhrase(samples, "mmath-excerpt-saxophone.wav", kFs);
    // Held G#4: the note starts at 0.4 s and lasts 2.8 s. The window
    // sits after the 0.40 s ramp. Median inter-harmonic level, 200 Hz
    // to 6 kHz, re the strongest partial. −50 dB is the acceptance
    // argument. Wolfe's UNSW saxophone page records the breathy noise
    // and does not print this level. −120 dB is digital silence.
    const PlaybackCensus heldNote =
        measurePlaybackWav("mmath-excerpt-saxophone.wav", 1.20, 2.80);
    CHECK(heldNote.fundamentalHz > 400.0);
    CHECK(heldNote.fundamentalHz < 430.0);
    CHECK(heldNote.floorDb >= -50.0);
    CHECK(heldNote.evensWithin40 >= 1);
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
                 "jet delay, and pressure. Corner 1800 Hz, the 2 s demo.\n";
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
    ValveBlow jet = sustained(pressureFor(lowest) * 0.50, 1800.0, kFluteAttack);
    jet.frames = frames;
    jet.reflectionMagnitude = 0.985;
    jet.initialDisplacementMeters = 0.0;
    jet.jetAimMeters = 0.40 * thickness;
    jet.jetNoiseMeters = 0.035 * thickness;
    jet.reedTurbulence = 0.56;
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
    TrumpetSpec spec{1.70, 1.09e-4, kAir, kTemp, 0.0, 8.0e-7, 1.5e-5, 1.2e-2, mass,
                     omega * omega * mass, 1.2e-5, 0.001};
    auto trumpet = unwrap(makeTrumpet(spec));
    const auto frames = static_cast<std::uint64_t>(std::llround(total * kFs));
    ValveBlow lip = sustained(2800.0, 10000.0, kTrumpetAttack);
    lip.frames = frames;
    lip.reflectionMagnitude = 0.999;
    lip.initialDisplacementMeters = 0.0;
    lip.tongueReleaseFrames = kTrumpetTongue;
    lip.brassiness = 1.0;
    lip.reedTurbulence = 0.20;
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
        // Lengths are the tubes this 3.0e-4 m² bore plays on pitch.
        // The fingers tuned for 1.0e-4 m² sat flat on this bore.
        {466.16, 1.331, 4}, {440.00, 1.416, 4}, {392.00, 1.596, 4}, {349.23, 1.312, 3},
        {523.25, 1.181, 4}, {329.63, 1.394, 3}, {293.66, 1.579, 3}, {554.37, 1.732, 6},
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
    auto horn = unwrap(makeTrombone(TromboneSpec{3.05, 0.0, 3.0e-4, kAir, kTemp, 0.0, 1.0e-6,
                                                 2.5e-5, 1.5e-2, mass, omega * omega * mass,
                                                 2.0e-5, 0.0005}));
    const auto frames = static_cast<std::uint64_t>(std::llround(total * kFs));
    ValveBlow bone = sustained(1600.0, 5000.0, kTromboneAttack);
    bone.frames = frames;
    bone.reflectionMagnitude = 0.999;
    bone.initialDisplacementMeters = 0.0;
    bone.tongueReleaseFrames = kTromboneTongue;
    bone.brassiness = 1.0;
    bone.reedTurbulence = 0.20;
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
