// Compile probe: FrameCount + FrameCount must NOT compile.
// No generic index arithmetic: use tryIncrement/tryDecrement.
#include <mmathen/units/physical_units.h>

int main() {
  mmathen::FrameCount a{40};
  mmathen::FrameCount b{2};
  auto x = a + b;
  (void)x;
  return 0;
}
