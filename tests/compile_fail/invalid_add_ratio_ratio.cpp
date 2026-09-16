// Compile probe: FrequencyRatio + FrequencyRatio must NOT compile
// (ratios combine by multiplication, which is a Phase-1 concern).
#include <mmathen/units/music_units.h>

int main() {
  mmathen::FrequencyRatio a{1.5};
  mmathen::FrequencyRatio b{2.0};
  auto x = a + b;
  (void)x;
  return 0;
}
