#pragma once
// Score-driven expressive timing for a monophonic phrase player.
//
// Rules (all optional; zero/one switches a rule off):
// - tempoScale: global performed/score duration ratio (the performer's
//   tempo choice; a per-recording value, not a constant).
// - Phrase arch: within each phrase of phraseScoreSeconds, note durations
//   are lengthened at the phrase ends and shortened in the middle, the
//   "phrase arch" rule of the KTH performance rule system (Friberg,
//   Bresin and Sundberg, "Overview of the KTH rule system for musical
//   performance", Adv. Cogn. Psychol. 2, 2006) and the parabolic timing
//   profiles measured by Repp ("Diversity and commonality in music
//   performance", JASA 92, 1992). Shape: m(x) = 1 + depth((2x-1)^2 - 1/3),
//   mean 1 over the phrase. depth VERIFY (Repp's profiles show
//   phrase-final lengthening of roughly 10-30 %).
// - Final ritardando over the last finalRitardScoreSeconds: tempo
//   v(x) = [1 + (w^q - 1) x]^(1/q), q = 2, w = finalTempoRatio
//   (Friberg and Sundberg, "Does music performance allude to locomotion?",
//   JASA 105, 1999).
// - Articulation ratio: detached notes sound for detachedRatio of their
//   inter-onset interval (Bresin and Battel, "Articulation strategies in
//   expressive piano performance", J. New Music Res. 29, 2000, report
//   staccato around 40-60 % and detache/legato near 70-100 %; VERIFY for
//   violin).
#include <algorithm>
#include <cmath>
#include <vector>

namespace mmathen {

struct ScoreNote {
  double onsetSeconds = 0.0;  // score time
  double ioiSeconds = 0.0;    // score inter-onset interval
  int midi = 0;
  int velocity = 0;
};

struct PerformedNote {
  double onsetSeconds = 0.0;
  double ioiSeconds = 0.0;
  double soundSeconds = 0.0;
  int midi = 0;
  int velocity = 0;
};

struct TimingRules {
  double tempoScale = 1.0;
  double phraseScoreSeconds = 0.0;
  double phraseArchDepth = 0.0;
  double finalRitardScoreSeconds = 0.0;
  double finalTempoRatio = 1.0;
  double detachedRatio = 1.0;
};

[[nodiscard]] inline std::vector<PerformedNote> applyExpressiveTiming(
    const std::vector<ScoreNote>& notes, const TimingRules& r) {
  std::vector<PerformedNote> out;
  out.reserve(notes.size());
  double end = 0.0;
  for (const ScoreNote& n : notes) end = std::max(end, n.onsetSeconds + n.ioiSeconds);
  double t = notes.empty() ? 0.0 : notes.front().onsetSeconds * r.tempoScale;
  for (const ScoreNote& n : notes) {
    const double mid = n.onsetSeconds + 0.5 * n.ioiSeconds;
    double m = r.tempoScale;
    if (r.phraseScoreSeconds > 0.0 && r.phraseArchDepth != 0.0) {
      const double x = std::fmod(mid, r.phraseScoreSeconds) / r.phraseScoreSeconds;
      m *= 1.0 + r.phraseArchDepth * ((2.0 * x - 1.0) * (2.0 * x - 1.0) - 1.0 / 3.0);
    }
    if (r.finalRitardScoreSeconds > 0.0 && r.finalTempoRatio > 0.0 &&
        mid > end - r.finalRitardScoreSeconds) {
      const double x = (mid - (end - r.finalRitardScoreSeconds)) / r.finalRitardScoreSeconds;
      const double v = std::sqrt(1.0 + (r.finalTempoRatio * r.finalTempoRatio - 1.0) * x);
      m /= std::max(v, 1e-3);
    }
    PerformedNote p;
    p.onsetSeconds = t;
    p.ioiSeconds = n.ioiSeconds * m;
    p.soundSeconds = p.ioiSeconds * std::clamp(r.detachedRatio, 0.0, 1.0);
    p.midi = n.midi;
    p.velocity = n.velocity;
    out.push_back(p);
    t += p.ioiSeconds;
  }
  return out;
}

}  // namespace mmathen
