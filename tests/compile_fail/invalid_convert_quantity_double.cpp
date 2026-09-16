// Compile probe: Quantity must NOT implicitly convert to double.
#include <mmathen/units/physical_units.h>

int main() {
  double d = mmathen::FrequencyHz{440.0};
  (void)d;
  return 0;
}
