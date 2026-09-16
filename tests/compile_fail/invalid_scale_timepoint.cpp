// Compile probe: scalar * TimePoint must NOT compile (TimePoint is not
// a scalable quantity).
#include <mmathen/units/physical_units.h>

int main() {
  mmathen::TimePointSeconds t{2.0};
  auto x = 2.0 * t;
  (void)x;
  return 0;
}
