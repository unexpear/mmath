#pragma once

// Ensemble: one conductor clock, parts aligned to the leader, level balance.
//
// Every part renders against the same score clock (the conductor's beat
// grid). A wind's sound starts some milliseconds after its notated onset
// (tongue release, then the standing wave builds), and that latency differs
// by instrument. Players in a section follow the leader's sound, not the
// grid (docs/ensemble-play.md). The ensemble therefore measures each part's
// median onset latency on its tongued notes and shifts the part so that
// median lands on the leader's. Ported from the off-line mix (ensmix.py).
//
// Onset: the RMS envelope over 96 samples (centred); from 40 ms before the
// notated start, the envelope minimum within 100 ms, then the first sample
// at half the rise from that minimum to the maximum before start + 200 ms.
// The 96-sample window, 40/100/200 ms and 50% point are UNSOURCED analysis
// choices. Part gains are the caller's; the Saints balance (clarinet -5 dB,
// trombone -3 dB re trumpet) is UNSOURCED, set by ear. The mix is scaled to
// a 0.89 peak (about -1 dBFS).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace mmathen {

class Ensemble {
 public:
  struct Part {
    std::string name;
    std::vector<double> samples;
    std::vector<double> tonguedStartsSec;
    double gain = 1.0;
    double medianLatencyMs = 0.0;
    double shiftMs = 0.0;
  };

  explicit Ensemble(double sampleRate, double beatSeconds = 0.0)
      : rate_(sampleRate), beat_(beatSeconds) {}

  double sampleRate() const { return rate_; }
  // Conductor clock: score time of beat n.
  double beatTimeSec(double beat) const { return beat * beat_; }

  // The first part added is the leader unless setLeader names another.
  std::size_t addPart(std::string name, std::vector<double> samples,
                      std::vector<double> tonguedStartsSec, double gainDb = 0.0) {
    Part part;
    part.name = std::move(name);
    part.samples = std::move(samples);
    part.tonguedStartsSec = std::move(tonguedStartsSec);
    part.gain = std::pow(10.0, gainDb / 20.0);
    part.medianLatencyMs = median(onsetLatenciesMs(part.samples, part.tonguedStartsSec, rate_));
    parts_.push_back(std::move(part));
    return parts_.size() - 1;
  }
  void setLeader(std::size_t index) { leader_ = index; }
  const std::vector<Part>& parts() const { return parts_; }

  // Parts shifted onto the leader's median onset, balanced, summed, scaled
  // to a 0.89 peak. Length is the shortest shifted part.
  std::vector<double> mix() {
    std::vector<double> out;
    if (parts_.empty()) return out;
    const double lead = parts_[leader_].medianLatencyMs;
    bool first = true;
    for (Part& part : parts_) {
      const long shift = std::lround((part.medianLatencyMs - lead) / 1000.0 * rate_);
      part.shiftMs = static_cast<double>(shift) / rate_ * 1000.0;
      const long n = static_cast<long>(part.samples.size());
      std::vector<double> y(part.samples.size(), 0.0);
      for (long i = 0; i < n; ++i) {
        const long j = i + shift;
        if (j >= 0 && j < n) y[static_cast<std::size_t>(i)] = part.samples[static_cast<std::size_t>(j)];
      }
      if (first) {
        out.resize(y.size());
        for (std::size_t i = 0; i < y.size(); ++i) out[i] = y[i] * part.gain;
        first = false;
      } else {
        const std::size_t m = std::min(out.size(), y.size());
        out.resize(m);
        for (std::size_t i = 0; i < m; ++i) out[i] += y[i] * part.gain;
      }
    }
    double peak = 0.0;
    for (double s : out) peak = std::max(peak, std::fabs(s));
    if (peak > 0.0)
      for (double& s : out) s = s / peak * 0.89;
    return out;
  }

  static std::vector<double> onsetLatenciesMs(const std::vector<double>& x,
                                              const std::vector<double>& startsSec, double fs) {
    const long n = static_cast<long>(x.size());
    std::vector<double> prefix(x.size() + 1, 0.0);
    for (long i = 0; i < n; ++i) prefix[i + 1] = prefix[i] + x[i] * x[i];
    auto env = [&](long i) {  // centred 96-sample RMS, zero padded
      const long lo = std::max(0L, i - 48), hi = std::min(n - 1, i + 47);
      if (hi < lo) return 0.0;
      return std::sqrt(std::max(0.0, (prefix[hi + 1] - prefix[lo]) / 96.0));
    };
    std::vector<double> out;
    for (double s : startsSec) {
      const long a = static_cast<long>((s - 0.04) * fs);
      const long b = std::min(n, static_cast<long>((s + 0.2) * fs));
      if (a < 0 || b <= a) continue;
      const long search = std::min(b - a, static_cast<long>(0.1 * fs));
      long m = 0;
      for (long k = 1; k < search; ++k)
        if (env(a + k) < env(a + m)) m = k;
      const double lo = env(a + m);
      double hi = lo;
      for (long k = m; k < b - a; ++k) hi = std::max(hi, env(a + k));
      long j = m;
      for (long k = m; k < b - a; ++k)
        if (env(a + k) > lo + 0.5 * (hi - lo)) { j = k; break; }
      out.push_back((static_cast<double>(a + j) / fs - s) * 1000.0);
    }
    return out;
  }

  static double median(std::vector<double> v) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const std::size_t h = v.size() / 2;
    return v.size() % 2 ? v[h] : 0.5 * (v[h - 1] + v[h]);
  }

 private:
  double rate_;
  double beat_;
  std::size_t leader_ = 0;
  std::vector<Part> parts_;
};

}  // namespace mmathen
