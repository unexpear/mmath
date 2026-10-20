// Struck bar (struck_bar.h) and glockenspiel (glockenspiel.h).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

#include <mmathen/instruments/glockenspiel.h>
#include <mmathen/instruments/celesta.h>
#include <mmathen/instruments/tubular_bells.h>
#include <mmathen/instruments/handbell.h>

using namespace mmathen;

TEST_CASE("free-free bar ratios reach Euler-Bernoulli when thin and drop when thick", "[bar]") {
  BarSpec b;
  b.material = {200e9, 7850.0, 0.29};
  b.section.widthM = 0.025;
  b.section.thicknessM = 1e-4;
  b.lengthM = 0.5;
  // Rayleigh: (7.8532 / 4.7300)^2 = 2.7565, (10.9956 / 4.7300)^2 = 5.4039.
  CHECK(std::fabs(barFlexuralHz(b, 2) / barFlexuralHz(b, 1) - 2.7565) < 1e-3);
  CHECK(std::fabs(barFlexuralHz(b, 3) / barFlexuralHz(b, 1) - 5.4039) < 2e-3);
  // Euler-Bernoulli f1 = (4.73^2 / 2 pi L^2) r sqrt(E/rho).
  const double eb = 4.730040745 * 4.730040745 / (2.0 * constants::pi * 0.25) * (1e-4 / std::sqrt(12.0)) * std::sqrt(200e9 / 7850.0);
  CHECK(std::fabs(barFlexuralHz(b, 1) / eb - 1.0) < 1e-6);
  b.section.thicknessM = 0.02;
  CHECK(barFlexuralHz(b, 2) / barFlexuralHz(b, 1) < 2.74);
  CHECK(barFlexuralHz(b, 3) / barFlexuralHz(b, 1) < 5.35);
}

TEST_CASE("Hertz sphere stiffness follows Johnson's E*", "[bar]") {
  const double k = hertzSphereOnFlatStiffness(0.01, 1e9, 0.0, 1e300, 0.0);
  CHECK(std::fabs(k - 4.0 / 3.0 * 1e9 * 0.1) < 1e-3 * k);
}

TEST_CASE("glockenspiel bar length and modes", "[bar][glock]") {
  const double hz = 1046.5;
  const BarSpec b = glockenspielBar(hz);
  CHECK(std::fabs(barFlexuralHz(b, 1) - hz) < 1e-6 * hz);
  CHECK(b.lengthM > 0.15);
  CHECK(b.lengthM < 0.20);
  auto modes = barModes(b, glockenspielLosses(), 20000.0);
  REQUIRE(modes.ok());
  // Radiation damping rises steeply with the flexural mode number.
  std::vector<double> rad;
  for (const auto& m : modes.value())
    if (!m.torsion) rad.push_back(m.radiationSigma);
  REQUIRE(rad.size() >= 3);
  CHECK(rad[1] > 10.0 * rad[0]);
  CHECK(rad[2] > rad[1]);
  // The supports sit at the fundamental's nodes: mode 1 decays at the internal Q.
  const auto& m1 = modes.value()[0];
  CHECK(std::fabs(m1.sigma - (constants::twoPi * hz / (2.0 * kGlockInternalQ) + m1.radiationSigma)) < 0.02 * m1.sigma);
}

TEST_CASE("a glockenspiel note sounds its pitch, rings, and the mallet leaves", "[bar][glock]") {
  const double fs = 48000.0, hz = 1568.0;
  auto v = glockenspielNote(hz, 1.5, SampleRateHz{fs});
  REQUIRE(v.ok());
  auto voice = std::move(v).value();
  std::vector<double> y(96000);
  REQUIRE(voice.renderBlock(y.data(), FrameCount{y.size()}).ok());
  for (double s : y) REQUIRE(std::isfinite(s));
  CHECK(voice.contactSeconds() > 1e-4);
  CHECK(voice.contactSeconds() < 2e-3);
  // Strongest frequency 1-2.2 kHz over 0.2-0.6 s is the fundamental.
  double best = 0, bestF = 0;
  for (double f = 1000.0; f < 2200.0; f += 1.0) {
    std::complex<double> s{};
    for (int i = 9600; i < 28800; ++i) s += y[i] * std::polar(1.0, -2.0 * constants::pi * f * i / fs);
    if (std::abs(s) > best) { best = std::abs(s); bestF = f; }
  }
  CHECK(std::fabs(bestF - hz) < 3.0);
  double early = 0, late = 0;
  for (int i = 0; i < 4800; ++i) early = std::max(early, std::fabs(y[i]));
  for (int i = 91200; i < 96000; ++i) late = std::max(late, std::fabs(y[i]));
  CHECK(late < early);
  CHECK(late > 0.01 * early);  // a glockenspiel rings for seconds
}

TEST_CASE("celesta: resonator lifts the fundamental and the note rings at pitch", "[bar][celesta]") {
  const double fs = 48000.0, hz = 523.25;
  auto with = celestaNote(hz, 1.0, SampleRateHz{fs});
  auto without = celestaNote(hz, 1.0, SampleRateHz{fs}, 0.0);
  REQUIRE(with.ok());
  REQUIRE(without.ok());
  auto a = std::move(with).value();
  auto b = std::move(without).value();
  std::vector<double> ya(48000), yb(48000);
  REQUIRE(a.renderBlock(ya.data(), FrameCount{ya.size()}).ok());
  REQUIRE(b.renderBlock(yb.data(), FrameCount{yb.size()}).ok());
  auto amp = [&](const std::vector<double>& y, double f) {
    std::complex<double> s{};
    for (int i = 4800; i < 24000; ++i) s += y[i] * std::polar(1.0, -2.0 * constants::pi * f * i / fs);
    return std::abs(s);
  };
  CHECK(amp(ya, hz) > 10.0 * amp(yb, hz));
  BarResonator bad;
  CHECK_FALSE(b.attachResonator(bad));
}

TEST_CASE("tubular bell: mode 4 sits an octave above the strike note", "[bar][chimes]") {
  const double note = 261.63;
  const BarSpec b = chimeTube(note);
  CHECK(std::fabs(barFlexuralHz(b, 4) - 2.0 * note) < 1e-6 * note);
  CHECK(b.lengthM > 1.2);
  CHECK(b.lengthM < 2.0);
  // Modes 4, 5, 6 near 2 : 3 : 4 of the strike note (Rossing), within 4 %.
  CHECK(std::fabs(barFlexuralHz(b, 5) / note - 3.0) < 0.12);
  CHECK(std::fabs(barFlexuralHz(b, 6) / note - 4.0) < 0.16);
  auto v = tubularBellNote(note, 2.0, SampleRateHz{48000.0});
  REQUIRE(v.ok());
  auto voice = std::move(v).value();
  std::vector<double> y(24000);
  REQUIRE(voice.renderBlock(y.data(), FrameCount{y.size()}).ok());
  for (double s : y) REQUIRE(std::isfinite(s));
  CHECK(voice.contactSeconds() > 5e-4);
}

TEST_CASE("handbell: English (3,0) at three times (2,0), one stroke, decays", "[handbell]") {
  CHECK(kHandbellModes[0].ratio == 1.0);
  CHECK(kHandbellModes[1].ratio == 3.0);
  const double fs = 48000.0, f1 = 174.61;
  auto v = HandbellVoice::create(f1, 1.0, SampleRateHz{fs});
  REQUIRE(v.ok());
  auto voice = std::move(v).value();
  std::vector<double> y(96000);
  REQUIRE(voice.renderBlock(y.data(), FrameCount{y.size()}).ok());
  for (double s : y) REQUIRE(std::isfinite(s));
  CHECK(voice.contactSeconds() > 5e-4);
  CHECK(voice.contactSeconds() < 1e-2);
  auto amp = [&](int from, double f) {
    std::complex<double> s{};
    for (int i = from; i < from + 24000; ++i) s += y[i] * std::polar(1.0, -2.0 * constants::pi * f * i / fs);
    return std::abs(s);
  };
  CHECK(amp(2400, f1) > amp(2400, 2.5 * f1) * 10.0);
  CHECK(amp(2400, 3.0 * f1) > amp(2400, 2.5 * f1) * 3.0);
  CHECK(amp(72000, f1) < amp(2400, f1));
  CHECK_FALSE(HandbellVoice::create(f1, 0.0, SampleRateHz{fs}).ok());
}
