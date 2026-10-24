// Closed-cavity drum: mallet, membrane, air compliance, monopole.
//
// Density, tension interval, Q, felt, air state, and distance are
// arguments. Shell depth and radius are the catalog geometry. The
// compliance is V = π a² L and p = ρ c² ΔV / V. No port radius and no
// wire mass are stored.

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstring>
#include <vector>

#include <mmathen/analysis/fft.h>
#include <mmathen/instruments/cavity_drum.h>
#include <mmathen/instruments/drum.h>
#include <mmathen/instruments/render_status.h>
#include <mmathen/math/constants.h>
#include <mmathen/physics/impedance.h>
#include <mmathen/physics/medium.h>
#include <mmathen/physics/modes.h>
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

MembraneHead tunedHead(const MembraneShell& shell, double frequencyHz) {
  const double densityMin = 0.20;
  const double densityMax = 0.30;
  const double density = 0.5 * (densityMin + densityMax);
  const double alpha = unwrap(besselJZero(0, 1));
  const double wave =
      constants::twoPi * shell.headRadius.value() * frequencyHz / alpha;
  const double tension = density * wave * wave;
  return unwrap(tuneMembraneHead(shell, frequencyHz, densityMin, densityMax,
                                 tension * 0.5, tension * 1.5));
}

MalletStrike testMallet() {
  return MalletStrike{0.0, 0.0, 0.03, 1.5, 8.0e4, 1.0, 0.0, 40.0};
}

double loadedHz(const MembraneHead& head, double depthMeters, double airPressure,
                double temperature, bool bothHeads) {
  const double radius = head.radius.value();
  const double alpha = unwrap(besselJZero(0, 1));
  const double radial = std::cyl_bessel_j(1.0, alpha);
  const double coupling = constants::twoPi * radius * radius * radial / alpha;
  const double norm = constants::pi * radius * radius * radial * radial;
  const double mass = head.arealDensity.value() * norm;
  const double volume = constants::pi * radius * radius * depthMeters;
  const auto air = unwrap(makeAir(TemperatureKelvin{temperature}));
  const double speed = unwrap(speedOfSound(air)).value();
  const double rho =
      unwrap(airDensity(PressurePascals{airPressure}, TemperatureKelvin{temperature}))
          .value();
  const double kappa = rho * speed * speed / volume;
  const double omega = constants::twoPi * head.requestedFrequencyHz;
  const double sigma = omega / 80.0;
  const double factor = bothHeads ? 2.0 : 1.0;
  const double shifted =
      std::sqrt(omega * omega + factor * kappa * coupling * coupling / mass);
  return std::sqrt(shifted * shifted - sigma * sigma) / constants::twoPi;
}

double bareDamped(const MembraneHead& head, int nodal, int radialIndex) {
  const auto hertz = unwrap(circularMembraneMode(head.radius, head.tension,
                                                 head.arealDensity, nodal, radialIndex));
  const double omega = constants::twoPi * hertz.value();
  const double sigma = omega / 80.0;
  return std::sqrt(omega * omega - sigma * sigma) / constants::twoPi;
}

double energyAboveGuard(const std::vector<double>& samples, std::size_t origin) {
  constexpr int kN = 8192;
  REQUIRE(origin + static_cast<std::size_t>(kN) <= samples.size());
  std::vector<std::complex<double>> buf(static_cast<std::size_t>(kN));
  for (int i = 0; i < kN; ++i) {
    const double window =
        0.5 * (1.0 - std::cos(6.283185307179586 * static_cast<double>(i) / kN));
    buf[static_cast<std::size_t>(i)] =
        samples[origin + static_cast<std::size_t>(i)] * window;
  }
  auto spec = fft(std::move(buf));
  REQUIRE(spec.ok());
  const auto& bins = spec.value();
  double total = 0.0;
  double high = 0.0;
  for (int k = 0; k < kN; ++k) {
    const double power = std::norm(bins[static_cast<std::size_t>(k)]);
    total += power;
    const int folded = std::min(k, kN - k);
    if (static_cast<double>(folded) / static_cast<double>(kN) > 0.4) high += power;
  }
  return total > 0.0 ? high / total : 1.0;
}

double autocorrelationAt(const std::vector<double>& samples, std::size_t origin,
                         double frequencyHz) {
  const int lag = std::max(8, static_cast<int>(std::llround(kFs / frequencyHz)));
  REQUIRE(origin + static_cast<std::size_t>(lag + 4000) <= samples.size());
  double energy0 = 0.0;
  double energy1 = 0.0;
  double cross = 0.0;
  for (int i = 0; i < 4000; ++i) {
    const double u = samples[origin + static_cast<std::size_t>(i)];
    const double v = samples[origin + static_cast<std::size_t>(i + lag)];
    energy0 += u * u;
    energy1 += v * v;
    cross += u * v;
  }
  return cross / std::sqrt(energy0 * energy1);
}

std::vector<double> renderOf(CavityDrumVoice& voice, std::uint64_t frames) {
  std::vector<double> samples(static_cast<std::size_t>(frames));
  REQUIRE(voice.renderBlock(samples.data(), FrameCount{frames}).ok());
  return samples;
}

double pitchNear(const std::vector<double>& samples, std::size_t begin, double lowHz,
                 double highHz) {
  std::vector<double> window(samples.begin() + static_cast<std::ptrdiff_t>(begin),
                             samples.begin() +
                                 static_cast<std::ptrdiff_t>(begin + 8192));
  return unwrap(estimateSinusoid(window, kRate, Window::BlackmanHarris,
                                 FrequencyHz{lowHz}, FrequencyHz{highHz}))
      .frequency.value();
}

double amplitudeNear(const std::vector<double>& samples, std::size_t begin, double lowHz,
                     double highHz) {
  std::vector<double> window(samples.begin() + static_cast<std::ptrdiff_t>(begin),
                             samples.begin() +
                                 static_cast<std::ptrdiff_t>(begin + 4096));
  return unwrap(estimateSinusoid(window, kRate, Window::BlackmanHarris,
                                 FrequencyHz{lowHz}, FrequencyHz{highHz}))
      .amplitude;
}

void writeListening(const std::vector<double>& samples, const char* filename) {
  double peak = 0.0;
  for (double sample : samples) peak = std::max(peak, std::fabs(sample));
  REQUIRE(peak > 0.0);
  const double gain = 0.5 / peak;
  std::vector<double> scaled(samples.size());
  for (std::size_t i = 0; i < samples.size(); ++i) scaled[i] = samples[i] * gain;
  const auto path = wavOutputPath(filename);
  auto opened = WavSink::open(path.string(), kRate,
                              FrameCount{static_cast<std::uint64_t>(samples.size())});
  REQUIRE(opened.ok());
  auto sink = std::move(opened).value();
  REQUIRE(sink.consume(scaled.data(),
                       FrameCount{static_cast<std::uint64_t>(scaled.size())}));
  REQUIRE(sink.close().ok());
  REQUIRE(copyWavToArtifacts(path, filename));
}

}  // namespace

TEST_CASE("a closed cavity raises the membrane and radiates", "[cavity]") {
  const auto kit = unwrap(yamahaTourCustomKit());
  const MembraneHead bass = tunedHead(kit.bassDrum, 90.0);
  const MalletStrike strike = testMallet();
  auto voice = unwrap(CavityDrumVoice::create(bass, nullptr, kit.bassDrum, strike, 0, 1,
                                              kRate, 101325.0, 293.15, 1.0));
  const double expected =
      loadedHz(bass, kit.bassDrum.shellDepth.value(), 101325.0, 293.15, false);
  CHECK(std::fabs(voice.fundamentalHz() - expected) < 1.0e-6);
  CHECK(voice.fundamentalHz() > bareDamped(bass, 0, 1) + 20.0);
  const auto samples = renderOf(voice, 32768);
  for (double sample : samples) REQUIRE(std::isfinite(sample));
  double peak = 0.0;
  for (double sample : samples) peak = std::max(peak, std::fabs(sample));
  CHECK(peak > 1.0e-3);
  const double low = expected - 40.0;
  const double high = expected + 40.0;
  CHECK(std::fabs(pitchNear(samples, 8192, low, high) - expected) < 1.0);
  const double early = amplitudeNear(samples, 8192, low, high);
  const double late = amplitudeNear(samples, 12288, low, high);
  const double decay = voice.normalDecay(0);
  const double ratio = std::exp(-decay * (4096.0 / kFs));
  CHECK(std::fabs(late / early - ratio) / ratio < 0.05);

  MembraneShell deeper = kit.bassDrum;
  deeper.shellDepth = DistanceMeters{kit.bassDrum.shellDepth.value() * 2.0};
  auto deep = unwrap(CavityDrumVoice::create(bass, nullptr, deeper, strike, 0, 1, kRate,
                                             101325.0, 293.15, 1.0));
  CHECK(deep.fundamentalHz() < voice.fundamentalHz() - 5.0);
  CHECK(std::fabs(deep.fundamentalHz() -
                  loadedHz(bass, deeper.shellDepth.value(), 101325.0, 293.15, false)) <
        1.0e-6);

  auto withDiameter = unwrap(CavityDrumVoice::create(
      bass, nullptr, kit.bassDrum, strike, 1, 1, kRate, 101325.0, 293.15, 1.0));
  const double bare11 = bareDamped(bass, 1, 1);
  bool foundBare = false;
  for (std::size_t i = 0; i < withDiameter.normalCount(); ++i) {
    if (std::fabs(withDiameter.normalHz(i) - bare11) < 1.0e-6) foundBare = true;
  }
  CHECK(foundBare);

  const MembraneHead other = tunedHead(kit.rackTom10, 140.0);
  CHECK(CavityDrumVoice::create(other, nullptr, kit.bassDrum, strike, 0, 1, kRate,
                                101325.0, 293.15, 1.0)
            .error() == QuantityError::InvalidArgument);
  writeListening(samples, "mmath-bass-drum-cavity.wav");
}

TEST_CASE("two heads split a cavity mode and keep the bare partner", "[cavity]") {
  const auto kit = unwrap(yamahaTourCustomKit());
  const MembraneHead bass = tunedHead(kit.bassDrum, 90.0);
  const MalletStrike strike = testMallet();
  auto voice = unwrap(CavityDrumVoice::create(bass, &bass, kit.bassDrum, strike, 0, 1,
                                              kRate, 101325.0, 293.15, 1.0));
  REQUIRE(voice.normalCount() == 2);
  const double anti = bareDamped(bass, 0, 1);
  const double sym =
      loadedHz(bass, kit.bassDrum.shellDepth.value(), 101325.0, 293.15, true);
  CHECK(std::fabs(voice.normalHz(0) - anti) < 1.0e-4);
  CHECK(std::fabs(voice.normalHz(1) - sym) < 1.0e-4);
  const auto samples = renderOf(voice, 32768);
  for (double sample : samples) REQUIRE(std::isfinite(sample));
  const double heardAnti = pitchNear(samples, 8192, anti - 25.0, anti + 25.0);
  const double heardSym = pitchNear(samples, 8192, sym - 25.0, sym + 25.0);
  CHECK(std::fabs(heardAnti - anti) < 1.0);
  CHECK(std::fabs(heardSym - sym) < 1.0);
  const double early = amplitudeNear(samples, 8192, sym - 25.0, sym + 25.0);
  const double late = amplitudeNear(samples, 12288, sym - 25.0, sym + 25.0);
  const double ratio = std::exp(-voice.normalDecay(1) * (4096.0 / kFs));
  CHECK(std::fabs(late / early - ratio) / ratio < 0.05);

  voice.reset();
  std::vector<double> once(1024);
  std::vector<double> blocked(1024);
  REQUIRE(voice.renderBlock(once.data(), FrameCount{1024}).ok());
  voice.reset();
  for (std::size_t i = 0; i < blocked.size(); i += 128)
    REQUIRE(voice.renderBlock(blocked.data() + i, FrameCount{128}).ok());
  for (std::size_t i = 0; i < once.size(); ++i) CHECK(once[i] == blocked[i]);
}

TEST_CASE("toms radiate through two heads and a cavity", "[cavity]") {
  const auto kit = unwrap(yamahaTourCustomKit());
  const MalletStrike strike = testMallet();
  struct Row {
    const MembraneShell* shell;
    double pitch;
    const char* name;
    const char* path;
    bool renders;
  };
  const Row rows[] = {
      {&kit.rackTom10, 140.0, "rack tom 10 in",
       "mmath-rack-tom-10-cavity.wav", true},
      {&kit.rackTom12, 110.0, "rack tom 12 in",
       "mmath-rack-tom-12-cavity.wav", true},
      {&kit.floorTom, 80.0, "floor tom",
       "mmath-floor-tom-cavity.wav", true},
      {&kit.snare55, 180.0, "snare 5.5 in",
       "mmath-snare-55-cavity.wav", false},
      {&kit.snare65, 160.0, "snare 6.5 in",
       "mmath-snare-65-cavity.wav", false},
  };
  for (const Row& row : rows) {
    INFO(row.name);
    const MembraneHead head = tunedHead(*row.shell, row.pitch);
    auto voice = unwrap(CavityDrumVoice::create(head, &head, *row.shell, strike, 0, 1,
                                                kRate, 101325.0, 293.15, 1.0));
    const auto samples = renderOf(voice, 24576);
    for (double sample : samples) REQUIRE(std::isfinite(sample));
    double peak = 0.0;
    for (double sample : samples) peak = std::max(peak, std::fabs(sample));
    CHECK(peak > 1.0e-3);
    const double low = voice.normalHz(0);
    const double high = voice.normalHz(1);
    CHECK(std::fabs(pitchNear(samples, 8192, low - 20.0, low + 20.0) - low) < 1.5);
    if (row.renders) {
      CHECK(energyAboveGuard(samples, 8192) < 0.01);
      // Two heads ring at once, so one mode can sit near a null of the
      // autocorrelation. The stronger of the two is the heard pitch.
      const double lowAc = autocorrelationAt(samples, 8192, low);
      const double highAc = autocorrelationAt(samples, 8192, high);
      CHECK(std::max(lowAc, highAc) > 0.4);
    }
    CHECK(std::fabs(pitchNear(samples, 8192, high - 25.0, high + 25.0) - high) < 1.5);
    writeListening(samples, row.path);
    const InstrumentRenderStatus* status = nullptr;
    for (std::size_t i = 0; i < kInstrumentRenderStatusCount; ++i) {
      if (std::strcmp(kInstrumentRenderStatus[i].name, row.name) == 0)
        status = &kInstrumentRenderStatus[i];
    }
    REQUIRE(status != nullptr);
    CHECK(status->rendersAudio == row.renders);
  }
  CHECK(instrumentsThatRenderAudio() == 16);
  const InstrumentRenderStatus* bass = nullptr;
  for (std::size_t i = 0; i < kInstrumentRenderStatusCount; ++i) {
    if (std::strcmp(kInstrumentRenderStatus[i].name, "bass drum") == 0)
      bass = &kInstrumentRenderStatus[i];
  }
  REQUIRE(bass != nullptr);
  CHECK_FALSE(bass->rendersAudio);
  CHECK(std::strstr(bass->gap, "port") != nullptr);
}
