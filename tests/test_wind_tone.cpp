// Sustained wind tones.
//
// A blown wind is a limit cycle, not the ring after the player stops.
// These renders use published shapes and example arguments:
//   clarinet — quarter-wave cylinder, Dalmont slit, bore near 15 mm
//     (Benade, Fundamentals of Musical Acoustics);
//   saxophone — truncated cone, tan(kL) = -k x0, small x0/L so the
//     series is near n c/(2L) (Dalmont; saxophone.h);
//   flute — open pipe and a jet delay W/(σ U) with σ = 0.45, inside
//     Coltman's 0.4–0.5 convection range, first hydrodynamic regime;
//   trumpet and trombone — outward-striking lip below a cone resonance
//     (Fletcher, 1979). Lip damping, mouth pressure, and apex distance
//     are arguments. No stored alto, tenor, or trumpet length.
//
// The sample is radiated bell pressure. Listening files are peak-scaled
// to 0.5; the assertions read the unscaled samples.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

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
constexpr std::uint64_t kAttack = 1440;  // 30 ms, inside 20–50 ms

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

void writeListening(const std::vector<double>& samples, const char* filename,
                    double rate) {
  double peak = 0.0;
  for (double sample : samples) peak = std::max(peak, std::fabs(sample));
  REQUIRE(peak > 0.0);
  const double gain = 0.5 / peak;
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
}

struct Tone {
  const char* name;
  double expectHz;
  bool oddDominant;
  std::vector<double> samples;
};

void checkTone(const Tone& tone, double rate) {
  const auto& x = tone.samples;
  REQUIRE(x.size() >= 96000);
  const std::size_t a = 70000;
  const Period period = periodNear(x, a, tone.expectHz, rate);
  const double harmonic2 =
      goertzel(x, a, 8192, 2.0 * period.frequencyHz, rate);
  const double harmonic3 =
      goertzel(x, a, 8192, 3.0 * period.frequencyHz, rate);
  const double fundamental = goertzel(x, a, 8192, period.frequencyHz, rate);
  REQUIRE(fundamental > 0.0);
  const double h2 = 20.0 * std::log10(harmonic2 / fundamental);
  const double h3 = 20.0 * std::log10(harmonic3 / fundamental);
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
            << " dB  autocorr " << period.autocorrelation << "  h2 " << h2
            << " dB  h3 " << h3 << " dB  rms " << early << " -> " << late
            << "\n";
  CHECK(std::fabs(period.frequencyHz - tone.expectHz) < 8.0);
  CHECK(period.autocorrelation > 0.9);
  CHECK(hnr > 9.5);
  CHECK(early > 1.0e-4);
  CHECK(std::fabs(late / early - 1.0) < 0.10);
  CHECK(std::fabs(mean) < 0.05 * level);
  if (tone.oddDominant) {
    CHECK(h2 < -20.0);
    CHECK(harmonic3 > harmonic2);
  } else {
    CHECK(h2 > -30.0);
  }
  double peak = 0.0;
  for (std::size_t i = 72000; i < 96000; ++i)
    peak = std::max(peak, std::fabs(x[i]));
  CHECK(peak / level < 40.0);
}

Clarinet workingClarinet(double length) {
  ClarinetSpec spec{length, 2.0e-4, kAir, kTemp, 2.0e-5, 6000.0,
                    1.0e-6, 1.0e-4, 1.0e-3, 0.01};
  return unwrap(makeClarinet(spec));
}

ValveBlow sustained(double pressure) {
  ValveBlow blow;
  blow.pressurePa = pressure;
  blow.frames = kFrames;
  blow.reflectionMagnitude = 0.995;
  blow.initialDisplacementMeters = 2.0e-5;
  blow.attackFrames = kAttack;
  blow.listenerDistanceMeters = 1.0;
  return blow;
}

}  // namespace

TEST_CASE("blown winds reach a periodic limit cycle", "[contact][wind]") {
  const double speed =
      unwrap(speedOfSound(Air{TemperatureKelvin{kTemp}})).value();

  auto clarinet = workingClarinet(0.45);
  const double clarinetHz = speed / (4.0 * 0.45);
  auto clarinetVoice =
      unwrap(BoreValveVoice::fromClarinet(clarinet, sustained(4000.0), kRate));
  auto clarinetSamples = renderOf(clarinetVoice, kFrames);
  checkTone(Tone{"clarinet", clarinetHz, true, clarinetSamples}, kFs);
  CHECK(clarinetHz > 140.0);
  CHECK(clarinetHz < 500.0);
  writeListening(clarinetSamples, "mmath-clarinet.wav", kFs);

  auto quiet = unwrap(
      BoreValveVoice::fromClarinet(clarinet, sustained(250.0), kRate));
  auto quietSamples = renderOf(quiet, kFrames);
  CHECK(rmsOf(quietSamples, 72000, 96000) <
        0.05 * rmsOf(clarinetSamples, 72000, 96000));

  auto longer = workingClarinet(0.60);
  const double longerHz = speed / (4.0 * 0.60);
  auto longerVoice =
      unwrap(BoreValveVoice::fromClarinet(longer, sustained(4000.0), kRate));
  auto longerSamples = renderOf(longerVoice, kFrames);
  const Period low = periodNear(clarinetSamples, 70000, clarinetHz, kFs);
  const Period high = periodNear(longerSamples, 70000, longerHz, kFs);
  CHECK(std::fabs(low.frequencyHz / high.frequencyHz - 0.60 / 0.45) < 0.04);

  const SampleRateHz other{44100.0};
  auto otherVoice =
      unwrap(BoreValveVoice::fromClarinet(clarinet, sustained(4000.0), other));
  auto otherSamples = renderOf(otherVoice, 88200);
  const Period at44 = periodNear(otherSamples, 60000, clarinetHz, 44100.0);
  CHECK(std::fabs(at44.frequencyHz - low.frequencyHz) < 3.0);

  ValveBlow released = sustained(4000.0);
  released.frames = 70000;
  released.releaseFrames = 2400;
  auto releaseVoice =
      unwrap(BoreValveVoice::fromClarinet(clarinet, released, kRate));
  auto releaseSamples = renderOf(releaseVoice, kFrames);
  CHECK(rmsOf(releaseSamples, 88000, 96000) <
        0.7 * rmsOf(releaseSamples, 40000, 60000));

  // Alto-range truncated cone: L = 0.70 m, x0 = 0.05 m, first root near
  // c/(2L) which sits in 138–830 Hz. Embouchure damping is high so the
  // reed does not select its own resonance.
  SaxophoneSpec saxSpec{0.70, 0.05, 1.8e-4, kAir, kTemp, 1.2e-5, 8000.0,
                        2.0e-6, 1.4e-4, 8.0e-4, 0.8};
  const auto saxophone = unwrap(makeSaxophone(saxSpec));
  CHECK(saxophone.fundamentalHz > 138.0);
  CHECK(saxophone.fundamentalHz < 830.0);
  const double second = unwrap(saxophoneConeMode(saxophone, 2)).value();
  CHECK(second / saxophone.fundamentalHz < 2.15);
  CHECK(second / saxophone.fundamentalHz > 1.85);
  ValveBlow saxBlow = sustained(2000.0);
  saxBlow.initialDisplacementMeters = 1.0e-5;
  auto saxVoice =
      unwrap(ConeValveVoice::fromSaxophone(saxophone, saxBlow, kRate));
  auto saxSamples = renderOf(saxVoice, kFrames);
  checkTone(Tone{"saxophone", saxophone.fundamentalHz, false, saxSamples}, kFs);
  writeListening(saxSamples, "mmath-saxophone.wav", kFs);

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
  ValveBlow jet = sustained(overpressure);
  jet.reflectionMagnitude = 0.98;
  jet.initialDisplacementMeters = 0.2 * thickness;
  jet.jetAimMeters = 0.85 * thickness;
  auto jetVoice = unwrap(BoreValveVoice::fromFlute(flute, jet, kRate));
  auto jetSamples = renderOf(jetVoice, kFrames);
  checkTone(Tone{"flute", pipe, false, jetSamples}, kFs);
  CHECK(pipe > 250.0);
  CHECK(pipe < 600.0);
  writeListening(jetSamples, "mmath-flute.wav", kFs);

  const double trumpetLength = 1.40;
  const double lipMass = 2.0e-5;
  const double lipHz = 0.75 * speed / (2.0 * trumpetLength);
  const double lipOmega = constants::twoPi * lipHz;
  const double lipStiffness = lipOmega * lipOmega * lipMass;
  TrumpetSpec trumpetSpec{trumpetLength, 1.2e-4, kAir,         kTemp, 0.0,
                          6.0e-6,        4.0e-5, 1.0e-2,       lipMass,
                          lipStiffness,  1.2e-5, 0.001};
  const auto trumpet = unwrap(makeTrumpet(trumpetSpec));
  ValveBlow lip = sustained(1500.0);
  lip.reflectionMagnitude = 0.999;
  lip.initialDisplacementMeters = 0.0;
  auto lipVoice =
      unwrap(ConeValveVoice::fromTrumpet(trumpet, 0.08, lip, kRate));
  CHECK(lipVoice.fundamentalHz() > 80.0);
  CHECK(lipVoice.fundamentalHz() < 250.0);
  auto lipSamples = renderOf(lipVoice, kFrames);
  checkTone(Tone{"trumpet", lipVoice.fundamentalHz(), false, lipSamples}, kFs);
  writeListening(lipSamples, "mmath-trumpet.wav", kFs);

  const double slide = 0.25;
  const double tromboneLength = 1.60 + 2.0 * slide;
  const double tromboneLip = 0.75 * speed / (2.0 * tromboneLength);
  const double tromboneOmega = constants::twoPi * tromboneLip;
  TromboneSpec tromboneSpec{1.60,
                            slide,
                            1.2e-4,
                            kAir,
                            kTemp,
                            0.0,
                            6.0e-6,
                            4.0e-5,
                            1.0e-2,
                            lipMass,
                            tromboneOmega * tromboneOmega * lipMass,
                            1.2e-5,
                            0.001};
  const auto trombone = unwrap(makeTrombone(tromboneSpec));
  CHECK(std::fabs(trombone.bore.lengthMeters - tromboneLength) < 1.0e-12);
  ValveBlow bone = sustained(600.0);
  bone.reflectionMagnitude = 0.999;
  bone.initialDisplacementMeters = 0.0;
  auto boneVoice =
      unwrap(ConeValveVoice::fromTrombone(trombone, 0.08, bone, kRate));
  CHECK(boneVoice.fundamentalHz() > 50.0);
  CHECK(boneVoice.fundamentalHz() < 120.0);
  CHECK(boneVoice.fundamentalHz() < lipVoice.fundamentalHz());
  auto boneSamples = renderOf(boneVoice, kFrames);
  checkTone(Tone{"trombone", boneVoice.fundamentalHz(), false, boneSamples}, kFs);
  writeListening(boneSamples, "mmath-trombone.wav", kFs);
}
