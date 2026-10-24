// Bessel power-law horn. The area exponent is an argument.
// ε = 2 is the cone. ε = 0 is the cylinder. No bell curve is stored.
// The 0.5 listening scale is playback only.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/instruments/bessel_horn.h>
#include <mmathen/instruments/cone_valve.h>
#include <mmathen/instruments/render_status.h>
#include <mmathen/instruments/trombone.h>
#include <mmathen/instruments/trumpet.h>
#include <mmathen/math/comparison.h>
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

constexpr double kFs = 48000.0;
const SampleRateHz kRate{kFs};
constexpr double kAir = 101325.0;
constexpr double kTemp = 293.15;

double closedJ(int half, double z) {
  const double s = std::sqrt(2.0 / (constants::pi * z));
  const double sine = std::sin(z);
  const double cosine = std::cos(z);
  if (half == 1) return s * sine;
  if (half == 3) return s * (sine / z - cosine);
  const double z2 = z * z;
  return s * ((3.0 / z2 - 1.0) * sine - (3.0 / z) * cosine);
}

double closedY(int half, double z) {
  const double s = std::sqrt(2.0 / (constants::pi * z));
  const double sine = std::sin(z);
  const double cosine = std::cos(z);
  if (half == 1) return -s * cosine;
  if (half == 3) return s * (-cosine / z - sine);
  const double z2 = z * z;
  return -s * ((3.0 / z2 - 1.0) * cosine + (3.0 / z) * sine);
}

// ε = 4 is order 3/2, closed order 5/2. Independent of std::cyl_bessel.
double flareDet(double apex, double length, double k) {
  const double za = k * apex;
  const double zb = k * (apex + length);
  return closedJ(5, za) * closedY(3, zb) - closedY(5, za) * closedJ(3, zb);
}

double flareRoot(double apex, double length) {
  const double step = constants::pi / (40.0 * length);
  double previous = flareDet(apex, length, step);
  double lo = step;
  double hi = 2.0 * step;
  for (double k = 2.0 * step; k < 40.0; k += step) {
    const double value = flareDet(apex, length, k);
    if (previous * value < 0.0) {
      lo = k - step;
      hi = k;
      break;
    }
    previous = value;
  }
  double flo = flareDet(apex, length, lo);
  for (int i = 0; i < 70; ++i) {
    const double mid = 0.5 * (lo + hi);
    if (flo * flareDet(apex, length, mid) <= 0.0)
      hi = mid;
    else {
      lo = mid;
      flo = flareDet(apex, length, lo);
    }
  }
  return 0.5 * (lo + hi);
}

double coneRootHz(double apex, double length, double speed) {
  double lo = 0.5 * constants::pi;
  double hi = constants::pi;
  const double ratio = apex / length;
  auto den = [&](double theta) { return std::sin(theta) + ratio * theta * std::cos(theta); };
  for (int step = 0; step < 70; ++step) {
    const double mid = 0.5 * (lo + hi);
    if (den(lo) * den(mid) <= 0.0)
      hi = mid;
    else
      lo = mid;
  }
  return (0.5 * (lo + hi)) * speed / (constants::twoPi * length);
}

Trumpet brass(double cupVolume, double length) {
  const double lipMass = 2.0e-5;
  const double speed = unwrap(speedOfSound(Air{TemperatureKelvin{kTemp}})).value();
  const double lipOmega = 2.0 * constants::pi * (speed / (4.0 * length));
  TrumpetSpec spec{length, 1.2e-4, kAir, kTemp, cupVolume, 8.0e-6, 4.0e-5, 8.0e-3,
                   lipMass, lipOmega * lipOmega * lipMass, 1.5e-5, 0.002};
  return unwrap(makeTrumpet(spec));
}

std::vector<double> renderOf(IVoice& voice, std::uint64_t frames) {
  std::vector<double> samples(static_cast<std::size_t>(frames));
  REQUIRE(voice.renderBlock(samples.data(), FrameCount{frames}).ok());
  for (double sample : samples) REQUIRE(std::isfinite(sample));
  return samples;
}

double pitchNear(const std::vector<double>& samples, std::size_t begin, double lowHz,
                 double highHz) {
  REQUIRE(begin + 8192 <= samples.size());
  std::vector<double> window(samples.begin() + static_cast<std::ptrdiff_t>(begin),
                             samples.begin() + static_cast<std::ptrdiff_t>(begin + 8192));
  return unwrap(estimateSinusoid(window, kRate, Window::BlackmanHarris, FrequencyHz{lowHz},
                                 FrequencyHz{highHz}))
      .frequency.value();
}

double amplitudeNear(const std::vector<double>& samples, std::size_t begin, double lowHz,
                     double highHz) {
  REQUIRE(begin + 8192 <= samples.size());
  std::vector<double> window(samples.begin() + static_cast<std::ptrdiff_t>(begin),
                             samples.begin() + static_cast<std::ptrdiff_t>(begin + 8192));
  return unwrap(estimateSinusoid(window, kRate, Window::BlackmanHarris, FrequencyHz{lowHz},
                                 FrequencyHz{highHz}))
      .amplitude;
}

double peakOf(const std::vector<double>& samples, std::size_t begin, std::size_t end) {
  double peak = 0.0;
  for (std::size_t i = begin; i < end; ++i) peak = std::max(peak, std::fabs(samples[i]));
  return peak;
}

const InstrumentRenderStatus* statusNamed(const char* name) {
  for (std::size_t i = 0; i < kInstrumentRenderStatusCount; ++i) {
    if (std::strcmp(kInstrumentRenderStatus[i].name, name) == 0)
      return &kInstrumentRenderStatus[i];
  }
  return nullptr;
}

void writeListening(const std::vector<double>& samples, const char* filename) {
  double peak = 0.0;
  for (double sample : samples) peak = std::max(peak, std::fabs(sample));
  REQUIRE(peak > 0.0);
  const double gain = 0.5 / peak;
  std::vector<double> scaled(samples.size());
  for (std::size_t i = 0; i < samples.size(); ++i) scaled[i] = samples[i] * gain;
  const auto path = wavOutputPath(filename);
  auto opened = unwrap(WavSink::open(path.string(), kRate,
                                     FrameCount{static_cast<std::uint64_t>(samples.size())}));
  REQUIRE(opened.consume(scaled.data(), FrameCount{static_cast<std::uint64_t>(scaled.size())}));
  REQUIRE(opened.close().ok());
  REQUIRE(copyWavToArtifacts(path, filename));
}
}  // namespace

TEST_CASE("a power-law horn recovers the cylinder and the cone", "[bore]") {
  const double speed = 343.0;
  const double apex = 0.2;
  const double length = 1.4;
  const double cylinder = unwrap(besselHornResonanceHz(0.0, apex, length, speed, 1));
  CHECK(nearlyEqual(cylinder, speed / (4.0 * length), 1e-8, 1e-10));
  const double third = unwrap(besselHornResonanceHz(0.0, apex, length, speed, 2));
  CHECK(nearlyEqual(third, 3.0 * speed / (4.0 * length), 1e-6, 1e-9));
  const double fifth = unwrap(besselHornResonanceHz(0.0, apex, length, speed, 3));
  CHECK(nearlyEqual(fifth, 5.0 * speed / (4.0 * length), 1e-6, 1e-9));

  const double cone = unwrap(besselHornResonanceHz(2.0, apex, length, speed, 1));
  CHECK(nearlyEqual(cone, coneRootHz(apex, length, speed), 1e-6, 1e-9));
  CHECK(cone > speed / (4.0 * length) + 20.0);
  CHECK(cone < speed / (2.0 * length));

  const double k = flareRoot(apex, length);
  const double flare = unwrap(besselHornResonanceHz(4.0, apex, length, speed, 1));
  CHECK(nearlyEqual(flare, k * speed / constants::twoPi, 1e-5, 1e-8));
  CHECK(std::fabs(flareDet(apex, length, k)) < 1e-8);
  CHECK(flare > cone + 20.0);

  CHECK(besselHornResonanceHz(-0.1, apex, length, speed, 1).error() ==
        QuantityError::InvalidArgument);
  CHECK(besselHornResonanceHz(2.0, 0.0, length, speed, 1).error() ==
        QuantityError::InvalidArgument);
  CHECK(besselHornResonanceHz(2.0, apex, length, speed, 0).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("exponent 2 matches the cone voice, including extra tubing", "[bore]") {
  const auto trumpet = brass(0.0, 0.45);
  const ValveBlow blow{2500.0, 1000, 1.0, 0.0};
  auto cone = unwrap(ConeValveVoice::fromTrumpet(trumpet, 0.2, blow, kRate));
  auto horn = unwrap(BesselHornVoice::fromTrumpet(trumpet, 0.2, 2.0, blow, kRate, 1));
  CHECK(nearlyEqual(horn.fundamentalHz(), cone.fundamentalHz(), 1e-4, 1e-8));
  CHECK(nearlyEqual(horn.ringingHz(), horn.fundamentalHz(), 1e-9, 1e-12));

  const double extra = 0.04 + 0.07;
  auto coneExtra = unwrap(ConeValveVoice::fromTrumpet(trumpet, 0.2, blow, kRate, extra));
  auto hornExtra =
      unwrap(BesselHornVoice::fromTrumpet(trumpet, 0.2, 2.0, blow, kRate, 1, extra));
  CHECK(nearlyEqual(hornExtra.fundamentalHz(), coneExtra.fundamentalHz(), 1e-4, 1e-8));
  CHECK(hornExtra.fundamentalHz() < horn.fundamentalHz() - 5.0);

  const auto trombone = unwrap(makeTrombone(
      TromboneSpec{0.45, 0.0, 1.2e-4, kAir, kTemp, 0.0, 8.0e-6, 4.0e-5, 8.0e-3, 2.0e-5,
                   trumpet.lipStiffnessNewtonsPerMeter, 1.5e-5, 0.002}));
  auto slide = unwrap(BesselHornVoice::fromTrombone(trombone, 0.2, 2.0, blow, kRate, 1, 0.12));
  CHECK(slide.fundamentalHz() < horn.fundamentalHz() - 2.0);
  CHECK(BesselHornVoice::fromTrumpet(trumpet, 0.2, 2.0, blow, kRate, 1, -0.01).error() ==
        QuantityError::InvalidArgument);
  CHECK(BesselHornVoice::fromTrumpet(trumpet, 0.2, -1.0, blow, kRate, 4).error() ==
        QuantityError::InvalidArgument);
  CHECK(BesselHornVoice::fromTrumpet(trumpet, 0.2, 2.0, blow, kRate, 0).error() ==
        QuantityError::InvalidArgument);
  CHECK(BesselHornVoice::fromTrumpet(trumpet, 0.2, 2.0, blow, kRate, 9).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("a cup on the Bessel horn lowers the ring", "[bore]") {
  const auto bare = brass(0.0, 0.45);
  const auto cupped = brass(8.0e-6, 0.45);
  const ValveBlow blow{2500.0, 8000, 1.0, 0.0, 0.03, 0.0};
  auto bareVoice = unwrap(BesselHornVoice::fromTrumpet(bare, 0.2, 4.0, blow, kRate, 6));
  auto cupVoice = unwrap(BesselHornVoice::fromTrumpet(cupped, 0.2, 4.0, blow, kRate, 6));
  CHECK(nearlyEqual(bareVoice.fundamentalHz(), cupVoice.fundamentalHz(), 1e-6, 1e-9));
  const auto bareSamples = renderOf(bareVoice, 48000);
  const auto cupSamples = renderOf(cupVoice, 48000);
  const double barePitch =
      pitchNear(bareSamples, 32000, bareVoice.ringingHz() - 40.0, bareVoice.ringingHz() + 40.0);
  const double cupPitch =
      pitchNear(cupSamples, 32000, 80.0, bareVoice.ringingHz() + 40.0);
  CHECK(cupPitch < barePitch - 2.0);
  CHECK(peakOf(cupSamples, 24000, 40000) > 10.0);
}

TEST_CASE("a non-conical exponent rings and radiates", "[bore]") {
  const auto trumpet = brass(0.0, 0.45);
  ValveBlow blow{2500.0, 8000, 1.0, 0.0, 0.03, 0.0};
  auto coneShaped = unwrap(BesselHornVoice::fromTrumpet(trumpet, 0.2, 2.0, blow, kRate, 6));
  auto flared = unwrap(BesselHornVoice::fromTrumpet(trumpet, 0.2, 4.0, blow, kRate, 6));
  CHECK(flared.fundamentalHz() > coneShaped.fundamentalHz() + 20.0);
  CHECK(flared.areaExponent() == 4.0);
  CHECK(flared.modeCount() == 6);
  CHECK(flared.reflectionDecay() > 0.0);

  ValveBlow wider = blow;
  wider.radiusMeters = 0.05;
  auto wide = unwrap(BesselHornVoice::fromTrumpet(trumpet, 0.2, 4.0, wider, kRate, 6));
  CHECK(wide.reflectionDecay() > flared.reflectionDecay());

  ValveBlow corrected = blow;
  corrected.endCorrectionMeters = 0.02;
  auto longer = unwrap(BesselHornVoice::fromTrumpet(trumpet, 0.2, 4.0, corrected, kRate, 6));
  CHECK(longer.ringingHz() < longer.fundamentalHz() - 2.0);

  // 8192-sample analysis windows. A 48000-frame buffer ends before the
  // window that starts at 40000, and that read faulted on MSVC (0xC0000005).
  const auto samples = renderOf(flared, 48000 + 8192);
  CHECK(peakOf(samples, 24000, 40000) > 20.0);
  const double heard = pitchNear(samples, 32000, flared.ringingHz() - 40.0, flared.ringingHz() + 40.0);
  CHECK(std::fabs(heard - flared.ringingHz()) < 6.0);
  const double early = amplitudeNear(samples, 24000, heard - 30.0, heard + 30.0);
  const double middle = amplitudeNear(samples, 32000, heard - 30.0, heard + 30.0);
  const double late = amplitudeNear(samples, 40000, heard - 30.0, heard + 30.0);
  const double ratio1 = middle / early;
  const double ratio2 = late / middle;
  CHECK(ratio1 < 0.9);
  CHECK(std::fabs(ratio1 - ratio2) / ratio1 < 0.15);
  const auto wideSamples = renderOf(wide, 48000);
  const double wideRatio = amplitudeNear(wideSamples, 32000, wide.ringingHz() - 40.0,
                                         wide.ringingHz() + 40.0) /
                           amplitudeNear(wideSamples, 24000, wide.ringingHz() - 40.0,
                                         wide.ringingHz() + 40.0);
  CHECK(wideRatio < ratio1);

  const auto correctedSamples = renderOf(longer, 48000);
  const double correctedHeard =
      pitchNear(correctedSamples, 32000, 80.0, longer.fundamentalHz() + 40.0);
  CHECK(correctedHeard < flared.fundamentalHz() - 2.0);

  flared.reset();
  BesselHornVoice again = flared;
  std::vector<double> one(1000);
  std::vector<double> two(1000);
  REQUIRE(flared.renderBlock(one.data(), FrameCount{1000}).ok());
  REQUIRE(again.renderBlock(two.data(), FrameCount{500}).ok());
  REQUIRE(again.renderBlock(two.data() + 500, FrameCount{500}).ok());
  for (std::size_t i = 0; i < one.size(); ++i) CHECK(one[i] == two[i]);

  writeListening(samples, "mmath-trumpet-flare.wav");

  const auto trombone = unwrap(makeTrombone(
      TromboneSpec{0.45, 0.15, 1.2e-4, kAir, kTemp, 0.0, 8.0e-6, 4.0e-5, 8.0e-3, 2.0e-5,
                   trumpet.lipStiffnessNewtonsPerMeter, 1.5e-5, 0.002}));
  auto slide = unwrap(BesselHornVoice::fromTrombone(trombone, 0.2, 4.0, blow, kRate, 6));
  CHECK(slide.fundamentalHz() < flared.fundamentalHz());
  const auto slideSamples = renderOf(slide, 48000);
  CHECK(peakOf(slideSamples, 24000, 40000) > 10.0);
  writeListening(slideSamples, "mmath-trombone-flare.wav");

  CHECK(std::strstr(statusNamed("trumpet")->gap, "flare") != nullptr);
  CHECK_FALSE(statusNamed("trumpet")->fullInstrumentComplete);
  CHECK_FALSE(statusNamed("trombone")->fullInstrumentComplete);
  CHECK(instrumentsThatRenderAudio() == 16);
  CHECK(instrumentsFullyComplete() == 0);
}
