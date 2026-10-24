// Von Karman crash cymbal (cymbal_vk.h, step 19): energy, determinism, the
// soft-strike linear limit and the kit profile's stroke.
// Short renders on a reduced plate (modes to 3 kHz, 12 in-plane modes) keep
// the suite fast; the full crash plate is used for the loudest-strike check.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include <mmathen/instruments/cymbal_vk.h>
#include <mmathen/instruments/player_profile.h>
#include <mmathen/instruments/snare_drum.h>

using namespace mmathen;

namespace {
constexpr double kFs = 48000.0;

std::shared_ptr<const VonKarmanPlateModel> smallPlate(double lossScale) {
  VonKarmanPlateSpec sp;
  sp.maxModeHz = 3000.0;
  sp.inPlaneModes = 12;
  sp.lossScale = lossScale;
  auto m = buildVonKarmanPlate(sp);
  REQUIRE(m.ok());
  return m.value();
}

MalletStrike stick(double speed) {
  return MalletStrike{0.85, 0.0, kSnareStickMassKg, speed, kSnareStickStiffness, kSnareStickExponent, 0.0, 600.0};
}

VonKarmanCymbalVoice voiceOn(std::shared_ptr<const VonKarmanPlateModel> m, double speed, bool nonlinear,
                             int oversample = 1) {
  auto v = VonKarmanCymbalVoice::create(std::move(m), stick(speed), SampleRateHz{kFs}, kVonKarmanObserveRadialFraction,
                                        kVonKarmanObserveAngle, nonlinear, VonKarmanOutput::Acceleration, oversample);
  REQUIRE(v.ok());
  auto vv = std::move(v).value();
  return vv;
}

std::vector<double> render(VonKarmanCymbalVoice& v, std::size_t frames) {
  std::vector<double> x(frames, 0.0);
  REQUIRE(v.renderBlock(x.data(), FrameCount{frames}).ok());
  return x;
}

double rms(const std::vector<double>& x) {
  double s = 0.0;
  for (double v : x) s += v * v;
  return std::sqrt(s / static_cast<double>(x.size()));
}
double relDiff(const std::vector<double>& a, const std::vector<double>& b) {
  double d = 0.0, n = 0.0;
  for (std::size_t i = 0; i < a.size(); ++i) { d += (a[i] - b[i]) * (a[i] - b[i]); n += b[i] * b[i]; }
  return std::sqrt(d / n);
}
}  // namespace

TEST_CASE("von Karman plate: free-edge eigenvalues and coupling count", "[cymbal][vk]") {
  // D&T 2015 Table 1 (nu 0.38): free-edge eigenvalues (lambda^2 on the unit
  // disk) 5.093 (k 2), 9.175 (k 0), 11.90 (k 3).
  VonKarmanPlateSpec sp;
  sp.poisson = 0.38;
  sp.maxModeHz = 1500.0;
  sp.inPlaneModes = 4;
  auto m = buildVonKarmanPlate(sp);
  REQUIRE(m.ok());
  const auto& modes = m.value()->modes;
  REQUIRE(modes.size() > 3);
  const double ref[3] = {5.093, 9.175, 11.90};
  const int refK[3] = {2, 0, 3};
  for (int i = 0; i < 3; ++i) {
    CHECK(modes[static_cast<std::size_t>(i)].k == refK[i]);
    CHECK(std::fabs(modes[static_cast<std::size_t>(i)].lambda * modes[static_cast<std::size_t>(i)].lambda - ref[i]) < 5e-3);
  }
  CHECK(!m.value()->pairW.empty());
  VonKarmanPlateSpec bad;
  bad.thicknessM = 0.0;
  CHECK_FALSE(buildVonKarmanPlate(bad).ok());
}

TEST_CASE("von Karman cymbal: energy holds without losses, falls with them, loudest strike stays bounded",
          "[cymbal][vk]") {
  {
    // Lossless plate, loudest stroke: once the stick has gone the SAV energy
    // (kinetic + bending + psi^2 / 2) is constant to rounding, while V stays
    // a real share of it (the coupling is being exercised).
    auto v = voiceOn(smallPlate(0.0), 11.0, true);
    std::vector<double> x(48);
    double peakRatio = 0.0, e0 = 0.0, maxDev = 0.0;
    for (int b = 0; b < 200; ++b) {  // 0.2 s
      REQUIRE(v.renderBlock(x.data(), FrameCount{x.size()}).ok());
      peakRatio = std::max(peakRatio, v.nonlinearPotential() / v.linearEnergy());
      if (b == 40) e0 = v.numericalEnergy();  // 40 ms: contact (a few ms) long over
      if (b > 40) maxDev = std::max(maxDev, std::fabs(v.numericalEnergy() - e0) / e0);
    }
    CHECK(e0 > 0.0);
    CHECK(maxDev < 1e-9);
    CHECK(peakRatio > 1e-3);
  }
  {
    // With the crash losses the energy only falls after the strike.
    auto v = voiceOn(smallPlate(kCrashCymbalLossScale), 11.0, true);
    std::vector<double> x(48);
    double prev = 0.0;
    bool monotone = true;
    for (int b = 0; b < 200; ++b) {
      REQUIRE(v.renderBlock(x.data(), FrameCount{x.size()}).ok());
      const double e = v.numericalEnergy();
      if (b > 40 && e > prev * (1.0 + 1e-12)) monotone = false;
      prev = e;
    }
    CHECK(monotone);
  }
  {
    // Full crash plate (modes to 10 kHz, 40 in-plane), default oversampling,
    // velocity 127 stroke (11 m/s): 50 ms of the cascade stays finite.
    auto m = buildVonKarmanPlate(VonKarmanPlateSpec{});
    REQUIRE(m.ok());
    auto v = VonKarmanCymbalVoice::create(m.value(), stick(11.0), SampleRateHz{kFs});
    REQUIRE(v.ok());
    auto vv = std::move(v).value();
    const auto x = render(vv, 2400);
    double peak = 0.0;
    bool finite = true;
    for (double s : x) { finite = finite && std::isfinite(s); peak = std::max(peak, std::fabs(s)); }
    CHECK(finite);
    CHECK(peak > 0.0);
    CHECK(peak < 1e6);  // m/s^2; the converged runs peak near 5e4
    CHECK(std::isfinite(vv.numericalEnergy()));
  }
}

TEST_CASE("von Karman cymbal: deterministic, and reset replays the strike", "[cymbal][vk]") {
  const auto m = smallPlate(kCrashCymbalLossScale);
  auto a = voiceOn(m, 8.25, true, 2);
  auto b = voiceOn(m, 8.25, true, 2);
  const auto xa = render(a, 4800);
  const auto xb = render(b, 4800);
  CHECK(xa == xb);
  a.reset();
  const auto xr = render(a, 4800);
  CHECK(xr == xa);
}

TEST_CASE("von Karman cymbal: soft strikes stay at the linear plate", "[cymbal][vk]") {
  const auto m = smallPlate(kCrashCymbalLossScale);
  // Velocity 5 and 10 on the kit map (11 (v/127)^2 m/s): 0.017 and 0.068 m/s.
  const double soft = kitCrashStickSpeed(drumKitPlayerProfile(), 5.0);
  auto nl = voiceOn(m, soft, true);
  auto ln = voiceOn(m, soft, false);
  const auto xn = render(nl, 4800);
  const auto xl = render(ln, 4800);
  CHECK(relDiff(xn, xl) < 0.02);  // 100 ms waveform
  const double soft10 = kitCrashStickSpeed(drumKitPlayerProfile(), 10.0);
  auto nl10 = voiceOn(m, soft10, true);
  auto ln10 = voiceOn(m, soft10, false);
  const double db = 20.0 * std::log10(rms(render(nl10, 4800)) / rms(render(ln10, 4800)));
  CHECK(std::fabs(db) < 0.5);
  // The loudest stroke is far from linear: the limit is not vacuous.
  auto nlL = voiceOn(m, 11.0, true);
  auto lnL = voiceOn(m, 11.0, false);
  CHECK(relDiff(render(nlL, 4800), render(lnL, 4800)) > 0.3);
}

TEST_CASE("kit drummer: crash stroke speed (Dahl et al. 2011)", "[cymbal][vk][player]") {
  const DrumKitPlayerProfile p = drumKitPlayerProfile();
  CHECK(p.crashStickSpeedAt127 == 11.0);
  CHECK(kitCrashStickSpeed(p, 127.0) == 11.0);
  CHECK(std::fabs(kitCrashStickSpeed(p, 30.0) - 11.0 * (30.0 / 127.0) * (30.0 / 127.0)) < 1e-12);
  CHECK(kitCrashStickSpeed(p, 200.0) == 11.0);
  CHECK(kitCrashStickSpeed(DrumKitPlayerProfile{}, 100.0) == 0.0);
}
