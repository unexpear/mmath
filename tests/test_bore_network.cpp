// Tone-hole lattice, register key, and brass extra tubing.
//
// Hole radii, chimney heights, positions, and valve lengths are this
// test's arguments. They are not a fingering chart or a piston chart.
// The 0.5 listening scale is playback only.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/instruments/bore_network.h>
#include <mmathen/instruments/bore_valve.h>
#include <mmathen/instruments/clarinet.h>
#include <mmathen/instruments/cone_valve.h>
#include <mmathen/instruments/flute.h>
#include <mmathen/instruments/render_status.h>
#include <mmathen/instruments/saxophone.h>
#include <mmathen/instruments/trombone.h>
#include <mmathen/instruments/trumpet.h>
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

std::vector<double> renderOf(IVoice& voice, std::uint64_t frames) {
  std::vector<double> samples(static_cast<std::size_t>(frames));
  REQUIRE(voice.renderBlock(samples.data(), FrameCount{frames}).ok());
  for (double sample : samples) REQUIRE(std::isfinite(sample));
  return samples;
}

double pitchNear(const std::vector<double>& samples, std::size_t begin, double lowHz,
                 double highHz) {
  std::vector<double> window(samples.begin() + static_cast<std::ptrdiff_t>(begin),
                             samples.begin() + static_cast<std::ptrdiff_t>(begin + 8192));
  return unwrap(estimateSinusoid(window, kRate, Window::BlackmanHarris, FrequencyHz{lowHz},
                                 FrequencyHz{highHz}))
      .frequency.value();
}

double peakOf(const std::vector<double>& samples, std::size_t begin, std::size_t end) {
  double peak = 0.0;
  for (std::size_t i = begin; i < end; ++i) peak = std::max(peak, std::fabs(samples[i]));
  return peak;
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

Clarinet testClarinet() {
  return unwrap(makeClarinet(ClarinetSpec{0.45, 2.0e-4, kAir, kTemp, 2.0e-5, 6000.0, 1.0e-6,
                                          1.0e-4, 1.0e-3, 0.01}));
}

const InstrumentRenderStatus* statusNamed(const char* name) {
  for (std::size_t i = 0; i < kInstrumentRenderStatusCount; ++i) {
    if (std::strcmp(kInstrumentRenderStatus[i].name, name) == 0)
      return &kInstrumentRenderStatus[i];
  }
  return nullptr;
}
}  // namespace

TEST_CASE("published tone-hole lengths keep their signs", "[bore]") {
  const auto keefe = unwrap(
      toneHoleBranch(ToneHoleModel::Keefe1990, 0.0075, 0.004, 0.006, 0.0, 0.0));
  CHECK(std::fabs(keefe.closedHeightMeters - 0.006279713185185185) < 1.0e-12);
  CHECK(std::fabs(keefe.openHeightMeters - 0.011219802074074074) < 1.0e-12);
  CHECK(std::fabs(keefe.seriesMeters - (-9.981497693763526e-05)) < 1.0e-15);
  const auto keefeOpen =
      unwrap(toneHoleSeries(ToneHoleModel::Keefe1990, true, 0.0075, 0.004, 0.006));
  const auto keefeClosed =
      unwrap(toneHoleSeries(ToneHoleModel::Keefe1990, false, 0.0075, 0.004, 0.006));
  CHECK(keefeOpen < keefeClosed);
  CHECK(keefeOpen < 0.0);
  const auto keefeHz = unwrap(
      toneHoleBranch(ToneHoleModel::Keefe1990, 0.0075, 0.004, 0.006, 400.0, 343.0));
  CHECK(std::fabs(keefeHz.openHeightMeters - 0.011233485610040176) < 1.0e-12);

  const auto lefebvre = unwrap(
      toneHoleBranch(ToneHoleModel::Lefebvre2012, 0.0075, 0.004, 0.006, 0.0, 0.0));
  CHECK(std::fabs(lefebvre.closedHeightMeters - 0.006275040711111111) < 1.0e-12);
  CHECK(std::fabs(lefebvre.openHeightMeters - 0.010871871188806583) < 1.0e-12);
  CHECK(lefebvre.seriesMeters < 0.0);
  const auto lefebvreHz = unwrap(
      toneHoleBranch(ToneHoleModel::Lefebvre2012, 0.0075, 0.004, 0.006, 400.0, 343.0));
  CHECK(std::fabs(lefebvreHz.openHeightMeters - (0.006275040711111111 + 0.002168123027954542 +
                                                 0.61 * 0.004)) < 1.0e-12);
  const auto lefebvreOpen =
      unwrap(toneHoleSeries(ToneHoleModel::Lefebvre2012, true, 0.0075, 0.004, 0.006));
  CHECK(std::fabs(lefebvreOpen - (-0.0003299969861900555)) < 1.0e-15);

  const auto dalmont = unwrap(
      toneHoleBranch(ToneHoleModel::Dalmont2002, 0.0075, 0.004, 0.006, 0.0, 0.0));
  CHECK(std::fabs(dalmont.seriesMeters - (-0.00036662477771459)) < 1.0e-15);
  const auto dalmontOpen =
      unwrap(toneHoleSeries(ToneHoleModel::Dalmont2002, true, 0.0075, 0.004, 0.006));
  CHECK(std::fabs(dalmontOpen - (-0.0003185777777777778)) < 1.0e-15);

  CHECK(toneHoleBranch(ToneHoleModel::Keefe1990, 0.0, 0.004, 0.006, 0.0, 0.0).error() ==
        QuantityError::InvalidArgument);
  CHECK(toneHoleBranch(ToneHoleModel::Keefe1990, 0.0075, -0.001, 0.006, 0.0, 0.0).error() ==
        QuantityError::InvalidArgument);

  const double slides[] = {0.05, 0.08, 0.12};
  const bool engaged[] = {true, false, true};
  CHECK(std::fabs(unwrap(sumEngagedLengths(slides, engaged)) - 0.17) < 1.0e-12);
  CHECK(unwrap(sumEngagedLengths({}, {})) == 0.0);
  const bool mismatch[] = {true};
  CHECK(sumEngagedLengths(slides, std::span<const bool>(mismatch, 1)).error() ==
        QuantityError::InvalidArgument);
  const double negative[] = {-0.01};
  const bool one[] = {true};
  CHECK(sumEngagedLengths(negative, one).error() == QuantityError::InvalidArgument);
}

TEST_CASE("a closed hole with no compliance passes the wave", "[bore]") {
  auto plain = unwrap(ToneHoleJunction::create(false, 1.5, 1.5, 0.0, 0.0));
  double left = 0.0;
  double right = 0.0;
  plain.step(0.3, -0.1, 1.0 / kFs, left, right);
  CHECK(std::fabs(left + 0.1) < 1.0e-12);
  CHECK(std::fabs(right - 0.3) < 1.0e-12);
  auto vent = unwrap(ToneHoleJunction::create(true, 1.5, 1.5, 0.0, 1.0e-12));
  vent.step(0.3, -0.1, 1.0 / kFs, left, right);
  CHECK(std::fabs(left + 0.3) < 1.0e-6);
  CHECK(std::fabs(right - 0.1) < 1.0e-6);
  CHECK(ToneHoleJunction::create(true, 1.5, 1.5, 0.0, 0.0).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("a register hole does not cut the clarinet the way a tone hole does", "[bore]") {
  const auto clarinet = testClarinet();
  const ValveBlow blow{4000.0, 12000, 0.97, 2.0e-5};
  auto cylinder = unwrap(BoreValveVoice::fromClarinet(clarinet, blow, kRate));
  auto closed = unwrap(BoreLatticeVoice::fromClarinet(clarinet, blow, {}, kRate));
  CHECK(std::fabs(closed.fundamentalHz() - cylinder.fundamentalHz()) < 1.0);
  const LatticeHole covered{0.15, 0.005, 0.008, false, ToneHoleModel::Lefebvre2012};
  const LatticeHole tone{0.15, 0.005, 0.008, true, ToneHoleModel::Lefebvre2012};
  const LatticeHole registerKey{0.15, 0.0012, 0.008, true, ToneHoleModel::Lefebvre2012};
  auto coveredVoice =
      unwrap(BoreLatticeVoice::fromClarinet(clarinet, blow, std::span(&covered, 1), kRate));
  auto toneVoice =
      unwrap(BoreLatticeVoice::fromClarinet(clarinet, blow, std::span(&tone, 1), kRate));
  auto registerVoice = unwrap(
      BoreLatticeVoice::fromClarinet(clarinet, blow, std::span(&registerKey, 1), kRate));
  const auto coveredSamples = renderOf(coveredVoice, 32768);
  const auto toneSamples = renderOf(toneVoice, 32768);
  const auto registerSamples = renderOf(registerVoice, 32768);
  CHECK(peakOf(coveredSamples, 14000, 24000) > 50.0);
  CHECK(peakOf(toneSamples, 14000, 24000) > 20.0);
  const double coveredPitch = pitchNear(coveredSamples, 16000, 140.0, 260.0);
  const double registerPitch = pitchNear(registerSamples, 16000, 140.0, 320.0);
  const double tonePitch = pitchNear(toneSamples, 16000, 250.0, 900.0);
  CHECK(std::fabs(coveredPitch - coveredVoice.fundamentalHz()) < 3.0);
  // The twelfth is three times the fundamental. A small open hole is not
  // that jump; a large open hole shortens the bore and sits above it.
  CHECK(registerPitch < coveredPitch * 2.0);
  CHECK(tonePitch > coveredPitch + 40.0);
  CHECK(tonePitch > registerPitch + 30.0);
  writeListening(registerSamples, "mmath-clarinet-register.wav");
  writeListening(toneSamples, "mmath-clarinet-tone-hole.wav");

  const LatticeHole backwards[] = {tone, covered};
  CHECK(BoreLatticeVoice::fromClarinet(clarinet, blow, backwards, kRate).error() ==
        QuantityError::InvalidArgument);
  CHECK_FALSE(statusNamed("clarinet")->fullInstrumentComplete);
  CHECK(std::strstr(statusNamed("clarinet")->gap, "bore") != nullptr);
  CHECK(instrumentsFullyComplete() == 0);
}

TEST_CASE("an open flute hole and a sax hole raise the bore", "[bore]") {
  const double speed = unwrap(speedOfSound(Air{TemperatureKelvin{kTemp}})).value();
  const double length = 0.55;
  const double pipe = speed / (2.0 * length);
  const double density =
      unwrap(airDensity(PressurePascals{kAir}, TemperatureKelvin{kTemp})).value();
  const double span = 8.0e-3;
  const double sigma = 0.4;
  const double jetSpeed = span / (sigma * (0.5 / pipe));
  const double overpressure = 0.5 * density * jetSpeed * jetSpeed;
  const auto flute =
      unwrap(makeFlute(FluteSpec{length, 2.8e-4, kAir, kTemp, 1.2e-5, 1.2e-3, span, sigma}));
  const ValveBlow jet{overpressure, 12000, 0.99, 0.25 * 1.2e-3};
  auto plain = unwrap(BoreLatticeVoice::fromFlute(flute, jet, {}, kRate));
  CHECK(std::fabs(plain.fundamentalHz() - pipe) / pipe < 0.02);
  const LatticeHole openHole{0.22, 0.006, 0.005, true, ToneHoleModel::Keefe1990};
  auto opened =
      unwrap(BoreLatticeVoice::fromFlute(flute, jet, std::span(&openHole, 1), kRate));
  const auto plainSamples = renderOf(plain, 32768);
  const auto openSamples = renderOf(opened, 32768);
  const double plainPitch = pitchNear(plainSamples, 16000, pipe - 40.0, pipe + 40.0);
  const double openPitch = pitchNear(openSamples, 16000, pipe, pipe + 400.0);
  CHECK(std::fabs(plainPitch - plain.fundamentalHz()) < 2.0);
  CHECK(openPitch > plainPitch + 20.0);
  writeListening(openSamples, "mmath-flute-tone-hole.wav");

  const auto saxophone = unwrap(makeSaxophone(
      SaxophoneSpec{0.45, 0.12, 2.0e-4, kAir, kTemp, 2.0e-5, 6000.0, 1.0e-6, 1.0e-4, 1.0e-3,
                    0.01}));
  const ValveBlow reed{4000.0, 12000, 0.97, 2.0e-5};
  const LatticeHole shut{0.18, 0.004, 0.006, false, ToneHoleModel::Dalmont2002};
  const LatticeHole vent{0.18, 0.004, 0.006, true, ToneHoleModel::Dalmont2002};
  auto shutVoice =
      unwrap(BoreLatticeVoice::fromSaxophone(saxophone, reed, std::span(&shut, 1), kRate, 8));
  auto ventVoice =
      unwrap(BoreLatticeVoice::fromSaxophone(saxophone, reed, std::span(&vent, 1), kRate, 8));
  const auto shutSamples = renderOf(shutVoice, 32768);
  const auto ventSamples = renderOf(ventVoice, 32768);
  const double shutPitch = pitchNear(shutSamples, 16000, 120.0, 320.0);
  const double ventPitch = pitchNear(ventSamples, 16000, 180.0, 900.0);
  CHECK(ventPitch > shutPitch + 20.0);
  CHECK(peakOf(ventSamples, 14000, 24000) > 20.0);
  writeListening(ventSamples, "mmath-saxophone-tone-hole.wav");
  CHECK_FALSE(statusNamed("flute")->fullInstrumentComplete);
  CHECK_FALSE(statusNamed("saxophone")->fullInstrumentComplete);
}

TEST_CASE("valve slides and an F attachment lengthen the cone", "[bore]") {
  const double lipMass = 2.0e-5;
  const double speed = unwrap(speedOfSound(Air{TemperatureKelvin{kTemp}})).value();
  const double lipOmega = 2.0 * constants::pi * (speed / (4.0 * 0.45));
  const auto trumpet = unwrap(makeTrumpet(TrumpetSpec{0.45, 1.2e-4, kAir, kTemp, 0.0, 8.0e-6,
                                                     4.0e-5, 8.0e-3, lipMass,
                                                     lipOmega * lipOmega * lipMass, 1.5e-5, 0.002}));
  const ValveBlow lip{2500.0, 8000, 1.0, 0.0};
  auto open = unwrap(ConeValveVoice::fromTrumpet(trumpet, 0.2, lip, kRate));
  auto same = unwrap(ConeValveVoice::fromTrumpet(trumpet, 0.2, lip, kRate, 0.0));
  CHECK(same.fundamentalHz() == open.fundamentalHz());
  CHECK(same.delaySamples() == open.delaySamples());
  const auto openSamples = renderOf(open, 512);
  const auto sameSamples = renderOf(same, 512);
  REQUIRE(openSamples.size() == sameSamples.size());
  for (std::size_t i = 0; i < openSamples.size(); ++i)
    CHECK(openSamples[i] == sameSamples[i]);

  const double slides[] = {0.04, 0.03, 0.07};
  const bool engaged[] = {true, false, true};
  const double extra = unwrap(sumEngagedLengths(slides, engaged));
  auto valved = unwrap(ConeValveVoice::fromTrumpet(trumpet, 0.2, lip, kRate, extra));
  CHECK(valved.fundamentalHz() < open.fundamentalHz() - 5.0);
  CHECK(valved.delaySamples() > open.delaySamples());
  const auto valvedSamples = renderOf(valved, 24576);
  const auto ring = renderOf(open, 24576);
  const double openPitch = pitchNear(ring, 12000, 180.0, 360.0);
  const double valvedPitch = pitchNear(valvedSamples, 12000, 120.0, 360.0);
  CHECK(valvedPitch < openPitch - 5.0);
  // The sample is radiated bell pressure, a few pascals, not the mouth
  // pressure. The pitch checks above are the musical claim.
  CHECK(peakOf(valvedSamples, 10000, 18000) > 1.0e-3);
  writeListening(valvedSamples, "mmath-trumpet-valves.wav");
  CHECK(ConeValveVoice::fromTrumpet(trumpet, 0.2, lip, kRate, -0.01).error() ==
        QuantityError::InvalidArgument);

  const auto trombone = unwrap(makeTrombone(TromboneSpec{0.45, 0.15, 1.2e-4, kAir, kTemp, 0.0,
                                                        8.0e-6, 4.0e-5, 8.0e-3, lipMass,
                                                        lipOmega * lipOmega * lipMass, 1.5e-5,
                                                        0.002}));
  auto slide = unwrap(ConeValveVoice::fromTrombone(trombone, 0.2, lip, kRate));
  auto attached = unwrap(ConeValveVoice::fromTrombone(trombone, 0.2, lip, kRate, 0.12));
  CHECK(attached.fundamentalHz() < slide.fundamentalHz() - 2.0);
  const auto attachedSamples = renderOf(attached, 24576);
  const auto slideSamples = renderOf(slide, 24576);
  const double slidePitch = pitchNear(slideSamples, 12000, 80.0, 260.0);
  const double attachedPitch = pitchNear(attachedSamples, 12000, 60.0, 260.0);
  CHECK(attachedPitch < slidePitch - 2.0);
  writeListening(attachedSamples, "mmath-trombone-f-attachment.wav");
  CHECK_FALSE(statusNamed("trumpet")->fullInstrumentComplete);
  CHECK(std::strstr(statusNamed("trumpet")->gap, "flare") != nullptr);
  CHECK_FALSE(statusNamed("trombone")->fullInstrumentComplete);
}

TEST_CASE("a long plain-bore note keeps the bore pitch", "[bore]") {
  // Fractional far-end delay (Thiran allpass): a 3 s flute note must sit
  // at c/(2L) and not drift between an early and a late window.
  const double speed = unwrap(speedOfSound(Air{TemperatureKelvin{kTemp}})).value();
  const double length = 0.5537;  // not a whole number of samples
  const double pipe = speed / (2.0 * length);
  const double density =
      unwrap(airDensity(PressurePascals{kAir}, TemperatureKelvin{kTemp})).value();
  const double span = 8.0e-3;
  const double sigma = 0.4;
  const double jetSpeed = span / (sigma * (0.5 / pipe));
  const double overpressure = 0.5 * density * jetSpeed * jetSpeed;
  const auto flute =
      unwrap(makeFlute(FluteSpec{length, 2.8e-4, kAir, kTemp, 1.2e-5, 1.2e-3, span, sigma}));
  const ValveBlow jet{overpressure, 150000, 0.99, 0.25 * 1.2e-3};
  auto voice = unwrap(BoreLatticeVoice::fromFlute(flute, jet, {}, kRate));
  CHECK(std::fabs(voice.fundamentalHz() - pipe) / pipe < 0.002);
  const auto samples = renderOf(voice, 144000);
  const double early = pitchNear(samples, 16000, pipe - 40.0, pipe + 40.0);
  const double late = pitchNear(samples, 130000, pipe - 40.0, pipe + 40.0);
  CHECK(std::fabs(late - early) < 0.5);
  CHECK(std::fabs(late - pipe) / pipe < 0.005);
}
