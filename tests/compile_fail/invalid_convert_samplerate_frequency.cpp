// Compile probe: SampleRate must NOT implicitly convert to Frequency.
#include <mmathen/units/physical_units.h>

void take_frequency(mmathen::FrequencyHz) {}

int main() {
  take_frequency(mmathen::SampleRateHz{48000.0});
  return 0;
}
