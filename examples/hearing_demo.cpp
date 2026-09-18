// Hearing demo: what the ear does to four octaves of A at equal loudness.
// For 60 phon, the SPL the engine demands rises steeply below 200 Hz (the
// "bass needs power" row), dips near 4 kHz (peak sensitivity), and the
// masking row shows a 1 kHz tone at 70 dB burying a 2 kHz probe far above
// the quiet floor. Evidence is the printed table plus the suite's anchors.

#include <cstdio>

#include <mmathen/hearing/loudness.h>
#include <mmathen/hearing/masking.h>
#include <mmathen/hearing/scales.h>
#include <mmathen/units/physical_units.h>

namespace {
int row(double f) {
  using namespace mmathen;
  auto spl = splFromPhon(FrequencyHz{f}, Phons{60.0});
  auto bark = barkRate(FrequencyHz{f});
  auto erb = erbBandwidth(FrequencyHz{f});
  auto w = aWeightingDb(FrequencyHz{f});
  auto mask = maskingThresholdDb(FrequencyHz{1000.0}, Decibels{70.0},
                                 FrequencyHz{f});
  if (!spl.ok() || !bark.ok() || !erb.ok() || !w.ok() || !mask.ok()) {
    std::printf("%7.0f Hz: FAILED\n", f);
    return 1;
  }
  std::printf("%7.0f Hz: 60phon=%6.2f dB  bark=%5.2f  erb=%6.1f Hz  "
              "A=%6.2f dB  masked-by-1kHz@70=%6.2f dB\n",
              f, spl.value().value(), bark.value(), erb.value(),
              w.value(), mask.value().value());
  return 0;
}
}  // namespace

int main() {
  int rc = 0;
  for (double f : {55.0, 110.0, 220.0, 440.0, 880.0, 1760.0, 3520.0}) {
    rc |= row(f);
  }
  return rc;
}
