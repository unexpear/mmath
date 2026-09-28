#pragma once

// The desktop tuner's existing D35 settings, shared with the audio-only
// comparison runner. No reference notes or target strings enter this detector.
#include <algorithm>
#include <cmath>
#include <vector>
#include <mmathen/analysis/pyin.h>
#include "analysis_profiles.h"

namespace app {
class StreamingTuner {
  std::vector<std::vector<mmathen::PitchCandidate>> candidates_;
  std::vector<double> times_;
  double clock_ = 0;
  PitchRange range_ = PitchRange::Guitar;
public:
  double levelDb = -120;
  void reset() { candidates_.clear(); times_.clear(); clock_ = 0; levelDb = -120; }
  mmathen::PitchEstimate process(const std::vector<double>& frame, double rate, PitchRange range = PitchRange::Guitar) {
    using namespace mmathen;
    if (range != range_) { reset(); range_ = range; }
    clock_ += 1.0 / 15.0;
    double ss = 0;
    for (double x : frame) ss += x * x;
    levelDb = ss > 0 ? 10 * std::log10(ss / static_cast<double>(frame.size())) : -120;
    PyinOptions po;
    po.fMinHz = tunerMinimum(range);
    po.fMaxHz = tunerMaximum(range, rate);
    po.maxJumpBins = 60;
    auto cand = pyinCandidates(frame, SampleRateHz{rate}, po);
    if (!cand.ok()) return {};
    candidates_.push_back(std::move(cand).value());
    times_.push_back(clock_);
    if (candidates_.size() > 24) {
      candidates_.erase(candidates_.begin());
      times_.erase(times_.begin());
    }
    auto decoded = pyinDecode(candidates_, times_, SampleRateHz{rate}, po);
    return decoded.ok() ? decoded.value().back().estimate : PitchEstimate{};
  }
};
}
