// Struck bar (struck_bar.h) and glockenspiel (glockenspiel.h).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

#include <mmathen/instruments/glockenspiel.h>

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
