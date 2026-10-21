// Struck bar (struck_bar.h) and glockenspiel (glockenspiel.h).

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <tuple>
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

TEST_CASE("glockenspiel: fixed off-centre strike reaches mode 2 on treble bars", "[bar][glock]") {
  const double lo = glockenspielBar(784.0).lengthM, hi = glockenspielBar(4186.0).lengthM;
  CHECK(glockenspielStrikeFraction(lo) > glockenspielStrikeFraction(hi));
  CHECK(glockenspielStrikeFraction(lo) < 0.5);
  CHECK(glockenspielStrikeFraction(hi) >= 0.25);
  CHECK(std::fabs((0.5 - glockenspielStrikeFraction(lo)) * lo - kGlockStrikeOffCentreM) < 1e-12);
}

TEST_CASE("bar radiation: a baffle lifts the fundamental of a long bar", "[bar][glock]") {
  const BarSpec b = glockenspielBar(784.0);
  auto modes = barModes(b, glockenspielLosses(), 20000.0);
  REQUIRE(modes.ok());
  BarListener free;
  free.polarRad = constants::pi / 6.0;
  free.azimuthRad = constants::pi / 6.0;
  BarListener cased = free;
  cased.baffle = 1.0;
  const BarMode* m1 = nullptr;
  const BarMode* m2 = nullptr;
  for (const BarMode& m : modes.value()) {
    if (m.torsion) continue;
    if (!m1) m1 = &m;
    else if (!m2) { m2 = &m; break; }
  }
  REQUIRE(m1);
  REQUIRE(m2);
  const double freeRatio = barListenerGain(*m1, b, free) / barListenerGain(*m2, b, free);
  const double casedRatio = barListenerGain(*m1, b, cased) / barListenerGain(*m2, b, cased);
  CHECK(casedRatio > 1.5 * freeRatio);
  free.baffle = 0.0;
  CHECK(barListenerGain(*m1, b, free) > 0.0);
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

TEST_CASE("handbell: size-scaled clapper keeps a small bell's (3,0) audible", "[handbell]") {
  CHECK(handbellClapperMassKg(174.61) == kHandbellClapperMassKg);
  CHECK(handbellClapperMassKg(1568.0) < 0.1 * kHandbellClapperMassKg);
  const double fs = 48000.0, f1 = 1568.0;
  auto v = HandbellVoice::create(f1, 1.0, SampleRateHz{fs});
  REQUIRE(v.ok());
  auto voice = std::move(v).value();
  std::vector<double> y(48000);
  REQUIRE(voice.renderBlock(y.data(), FrameCount{y.size()}).ok());
  CHECK(voice.contactSeconds() < 1e-3);
  auto amp = [&](double f) {
    std::complex<double> s{};
    for (int i = 2400; i < 26400; ++i) s += y[i] * std::polar(1.0, -2.0 * constants::pi * f * i / fs);
    return std::abs(s);
  };
  const double r = 20.0 * std::log10(amp(3.0 * f1) / amp(f1));
  CHECK(r > -16.0);  // radwoc G6 (English-tuned, CC0): -7 dB
  CHECK(r < 0.0);
}

TEST_CASE("handbell: breathing second harmonic is quadratic and decays at twice the rate", "[handbell]") {
  const double fs = 48000.0, f1 = 174.61, axisAngle = 20.0 * constants::pi / 180.0;
  auto ratioAt = [&](double speed, int from) {
    auto v = HandbellVoice::create(f1, speed, SampleRateHz{fs}, 1.0, 0.0, 4, 0.0, axisAngle);
    REQUIRE(v.ok());
    auto voice = std::move(v).value();
    std::vector<double> y(from + 24000);
    REQUIRE(voice.renderBlock(y.data(), FrameCount{y.size()}).ok());
    auto amp = [&](double f) {
      std::complex<double> s{};
      for (int i = from; i < from + 24000; ++i) s += y[i] * std::polar(1.0, -2.0 * constants::pi * f * i / fs);
      return std::abs(s);
    };
    return amp(2.0 * f1) / amp(f1);
  };
  const double soft = ratioAt(0.5, 4800), loud = ratioAt(1.0, 4800);
  // 2f pressure ~ q^2, f1 ~ q: the ratio doubles with the amplitude
  // (the felt contact is not linear, so allow 1.6-3x; measured 2.5).
  CHECK(loud / soft > 1.6);
  CHECK(loud / soft < 3.0);
  // The ratio falls as the ring decays: 2 sigma against sigma.
  const double early = ratioAt(1.0, 4800), late = ratioAt(1.0, 96000);
  const double sigma = kHandbellSigmaAtF3;
  CHECK(std::fabs(std::log(early / late) - sigma * (96000 - 4800) / fs) < 0.15);
  CHECK_FALSE(HandbellVoice::create(f1, 1.0, SampleRateHz{fs}, 1.0, 0.0, 4, 0.0,
                                     std::numeric_limits<double>::quiet_NaN()).ok());
}

TEST_CASE("handbell: a swung clapper strikes by momentum, its spring returns it", "[handbell]") {
  const double fs = 48000.0, f1 = 174.61;
  auto ring = [&](double swing, double stop, double springHz, double zeta) {
    auto v = HandbellVoice::create(f1, 1.0, SampleRateHz{fs});
    REQUIRE(v.ok());
    auto voice = std::move(v).value();
    REQUIRE(voice.setSwing(swing, stop, kHandbellClapperGapM, springHz, zeta));
    std::vector<double> y(48000);
    REQUIRE(voice.renderBlock(y.data(), FrameCount{y.size()}).ok());
    double peak = 0.0;
    for (double s : y) { REQUIRE(std::isfinite(s)); peak = std::max(peak, std::fabs(s)); }
    return std::make_tuple(voice.strikeCount(), voice.strikeSpeedMps(), peak);
  };
  // Default spring: one stroke; the strike speed is what the stop leaves
  // of the swing, below it and growing with it.
  auto [n1, v1, p1] = ring(1.0, kHandbellSwingStopSeconds, kHandbellClapperSpringHz, kHandbellClapperSpringDamping);
  auto [n2, v2, p2] = ring(1.5, kHandbellSwingStopSeconds, kHandbellClapperSpringHz, kHandbellClapperSpringDamping);
  CHECK(n1 == 1);
  CHECK(n2 == 1);
  CHECK(v1 > 0.2);
  CHECK(v1 < 1.0);
  CHECK(v2 > v1);
  CHECK(p2 > p1);
  // The ringer's stroke (kHandbellRingerSwingMps): one strike on F3, the
  // stop leaving 1.30 m/s at the rim (the swing sweep in handbell.h).
  auto [nr, vr, pr] = ring(kHandbellRingerSwingMps, kHandbellSwingStopSeconds, kHandbellClapperSpringHz, kHandbellClapperSpringDamping);
  CHECK(nr == 1);
  CHECK(vr > 1.2);
  CHECK(vr < 1.4);
  // A swing too gentle for the spring never reaches the bronze.
  auto [n0, v0, p0] = ring(0.5, kHandbellSwingStopSeconds, kHandbellClapperSpringHz, kHandbellClapperSpringDamping);
  CHECK(n0 == 0);
  CHECK(p0 == 0.0);
  // A soft, lightly damped spring lets the head come back: more strokes.
  auto [n3, v3, p3] = ring(1.0, kHandbellSwingStopSeconds, 3.0, 0.05);
  CHECK(n3 > 1);
  auto bad = HandbellVoice::create(f1, 1.0, SampleRateHz{fs});
  REQUIRE(bad.ok());
  auto b = std::move(bad).value();
  CHECK_FALSE(b.setSwing(0.0));
  CHECK_FALSE(b.setSwing(1.0, 0.0));
}
