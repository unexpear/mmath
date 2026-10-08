// Sustained wind tones.
//
// A blown wind is a limit cycle, not the ring after the player stops.
// These renders use published shapes and example arguments:
//   clarinet — quarter-wave cylinder, Dalmont slit, bore near 15 mm
//     (Benade, Fundamentals of Musical Acoustics). The reflection corner
//     is 600 Hz, under Benade's open-hole cutoff near 1.5 kHz, so the
//     odd partials roll off and the evens are not absent;
//   saxophone — truncated cone, tan(kL) = -k x0, small x0/L so the
//     series is near n c/(2L) (Dalmont; saxophone.h). Mouth pressure is
//     below the lay closure and the damping is light enough that the
//     reed still buzzes;
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
  // lay. The 8 kHz corner is the reflection low-pass.
  SaxophoneSpec saxSpec{0.70, 0.20, 1.6e-4, kAir, kTemp, 1.8e-5, 7500.0,
                        2.0e-6, 1.4e-4, 7.0e-4, 0.09};
  const auto saxophone = unwrap(makeSaxophone(saxSpec));
  CHECK(saxophone.fundamentalHz > 138.0);
  CHECK(saxophone.fundamentalHz < 830.0);
  const double second = unwrap(saxophoneConeMode(saxophone, 2)).value();
  CHECK(second / saxophone.fundamentalHz < 2.15);
  CHECK(second / saxophone.fundamentalHz > 1.85);
  ValveBlow saxBlow = sustained(4200.0, 8000.0, kSaxophoneAttack);
  saxBlow.initialDisplacementMeters = 1.0e-5;
  auto saxVoice =
      unwrap(ConeValveVoice::fromSaxophone(saxophone, saxBlow, kRate));
  auto saxSamples = renderOf(saxVoice, kFrames);
  checkTone(
      Tone{"saxophone", saxophone.fundamentalHz, Spectrum::Saxophone, saxSamples},
      kFs);
  const double saxLufs = writeListening(saxSamples, "mmath-saxophone.wav", kFs);

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
