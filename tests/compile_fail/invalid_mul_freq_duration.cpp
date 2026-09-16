// Compile probe: Frequency * Duration must NOT compile.
// There is deliberately no Frequency x Duration -> Phase overload: forming a
// phase requires the explicit 2*pi conversion via toAngular().
#include <mmathen/units/conversions.h>
#include <mmathen/units/physical_units.h>

int main() {
  mmathen::FrequencyHz f{440.0};
  mmathen::DurationSeconds d{0.5};
  auto x = f * d;
  (void)x;
  return 0;
}
