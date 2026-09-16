// Compile probe: TimePoint + TimePoint must NOT compile.
#include <mmathen/units/physical_units.h>

int main() {
  mmathen::TimePointSeconds a{1.0};
  mmathen::TimePointSeconds b{2.0};
  auto x = a + b;
  (void)x;
  return 0;
}
