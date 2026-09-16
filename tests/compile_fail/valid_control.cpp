// Compile probe (valid control): listed time arithmetic must compile.
// Uses the same headers and C++20 configuration as the invalid probes.
#include <mmathen/units/physical_units.h>

int main() {
  mmathen::TimePointSeconds t{2.0};
  mmathen::DurationSeconds d{0.5};
  mmathen::TimePointSeconds t2 = t + d;
  mmathen::DurationSeconds span = t2 - t;
  (void)span;
  return 0;
}
