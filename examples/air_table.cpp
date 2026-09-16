// Air table demo: the same 440 Hz tone's wavelength, wavenumber, and
// 10-meter propagation delay in cold, room, and warm air. No new audible
// capability in M6 (kinematics only, solvers deferred) — the evidence is
// the printed table plus the suite's analytic anchors.

#include <cstdio>

#include <mmathen/physics/medium.h>
#include <mmathen/physics/waves.h>
#include <mmathen/units/physical_units.h>

namespace {
int row(double celsius) {
  using namespace mmathen;
  auto air = makeAir(TemperatureKelvin{celsius + 273.15});
  if (!air.ok()) {
    std::printf("%.0f C: FAILED\n", celsius);
    return 1;
  }
  auto c = speedOfSound(std::move(air).value());
  auto lam = wavelengthFromFrequency(FrequencyHz{440.0}, c.value());
  auto k = wavenumber(lam.value());
  auto delay = travelTime(DistanceMeters{10.0}, c.value());
  if (!c.ok() || !lam.ok() || !k.ok() || !delay.ok()) {
    std::printf("%.0f C: FAILED\n", celsius);
    return 1;
  }
  std::printf("%.0f C: c=%.4f m/s  lambda440=%.6f m  k=%.6f rad/m  "
              "delay10m=%.6f s\n",
              celsius, c.value().value(), lam.value().value(),
              k.value().value(), delay.value().value());
  return 0;
}
}  // namespace

int main() {
  using namespace mmathen;
  std::printf("dry air, %s\n", mediumFormula());
  int rc = 0;
  rc |= row(0.0);
  rc |= row(20.0);
  rc |= row(30.0);
  return rc;
}
