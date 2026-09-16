// Compile probe: Frequency + AngularFrequency must NOT compile
// (conversion is only via the explicit toAngular function).
#include <mmathen/units/physical_units.h>

int main() {
  mmathen::FrequencyHz f{440.0};
  mmathen::AngularFrequencyRadPerSec w{1.0};
  auto x = f + w;
  (void)x;
  return 0;
}
