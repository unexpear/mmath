// Compile probe: Frequency + Duration must NOT compile (no such overload).
#include <mmathen/units/physical_units.h>

int main() {
  mmathen::FrequencyHz f{440.0};
  mmathen::DurationSeconds d{0.5};
  auto x = f + d;
  (void)x;
  return 0;
}
