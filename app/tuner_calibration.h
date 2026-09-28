#pragma once
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace app::tuner {
// Correction is known/observed, not the inverse. It applies to microphone
// measurements only, never changes A4 or alters playback sample rate.
class Calibration {
  std::vector<double> readings_;
  int attempts_ = 0;
public:
  double referenceHz = 440, factor = 1;
  bool collecting = false, applied = false;
  std::string message = "No device correction";
  void reset() { readings_.clear(); attempts_ = 0; collecting = false; applied = false; factor = 1; message = "No device correction"; }
  bool start(double hz) {
    if (!std::isfinite(hz) || hz < 100 || hz > 1000) {
      message = "Enter a reference frequency between 100 and 1000 Hz"; return false;
    }
    referenceHz = hz; readings_.clear(); attempts_ = 0; collecting = true;
    message = "Listening to the external reference..."; return true;
  }
  void cancel() { collecting = false; readings_.clear(); message = "Calibration cancelled"; }
  void observe(double hz, bool voiced, double db) {
    if (!collecting) return;
    ++attempts_;
    const bool valid = voiced && std::isfinite(hz) && hz > 0 && std::isfinite(db) && db > -45 &&
                       std::abs(1200 * std::log2(hz / referenceHz)) <= 20;
    if (!valid) readings_.clear();
    else {
      readings_.push_back(hz);
      if (readings_.size() > 45) readings_.erase(readings_.begin());
      if (readings_.size() == 45) {
        auto sorted = readings_; std::sort(sorted.begin(),sorted.end());
        const double spread = 1200 * std::log2(sorted[40] / sorted[4]);
        const double correction = referenceHz / sorted[22];
        if (spread <= .2 && std::abs(correction-1) <= .001) {
          factor = correction; applied = true; collecting = false;
          message = "Correction set from your reference; absolute accuracy unverified";
          return;
        }
      }
    }
    if (attempts_ >= 150) {
      collecting = false; readings_.clear();
      message = "Reference was unstable, too quiet, or too far from the entered frequency";
    }
  }
  double correct(double hz, bool microphone) const { return microphone && applied ? hz * factor : hz; }
  double ppm() const { return (factor-1) * 1e6; }
  double progress() const { return static_cast<double>(readings_.size()) / 45; }
};
}
