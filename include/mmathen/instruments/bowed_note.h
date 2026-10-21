#pragma once

// One bowed note for any instrument of the violin family (violin, viola,
// cello, double bass played arco): makeCelloNote with the cello's numbers
// lifted into a BowedVoicing, so the same note maker serves every bowed
// string and its player habits (player_profile.h, BowedPlayerProfile).
// celloBowedVoicing() is exactly cello_voice.h's constants: makeBowedNote
// with it and default habits renders sample for sample what makeCelloNote
// renders (tests/test_player_profile.cpp checks this).
//
// Output is the string's force on a rigid bridge, as makeCelloNote. Only
// the cello has a body model (CelloBody); the violin, viola and bass
// notes here are string force alone.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "bowed_string.h"
#include "cello_voice.h"
#include "instrument.h"

namespace mmathen {

// The instrument side of a bowed note: what the voice does when the
// player brings nothing of their own.
struct BowedVoicing {
  const char* name = "cello";
  double t60LowSeconds = kCelloT60LowSeconds;    // string loss at f0
  double t60HighSeconds = kCelloT60HighSeconds;  // and at 6 f0
  double betaForte = kCelloBetaForte;            // bow-bridge distance / string length at +12 dB
  double betaPiano = kCelloBetaPiano;            // and at -12 dB
  double bowSpeedMps = kCelloBowSpeed;           // at level 1
  double bowForceFraction = kCelloBowForceFraction;  // of Schelleng's maximum
  double attackSeconds = kCelloAttackSeconds;
  double releaseSeconds = kCelloReleaseSeconds;
  double vibratoHz = kCelloVibratoRateHz;
  double vibratoCents = kCelloVibratoCents;      // +/- cents
  double vibratoOnsetSeconds = kCelloVibratoOnsetSeconds;
  double vibratoRampSeconds = kCelloVibratoRampSeconds;
  // Per open string (low to high), the stop is lowered by this many cents
  // to cancel the bowed loop's sharp pitch (cello: measured on the model).
  std::array<double, 4> intonationCents = kCelloIntonationCents;
};

// The cello: cello_voice.h's constants, unchanged.
[[nodiscard]] inline BowedVoicing celloBowedVoicing() { return BowedVoicing{}; }

// The violin: bowed_string.h's violin constants (loss kViolinT60*,
// kViolinBetaForte / kViolinBetaPiano, kViolinBowForceFraction, vibrato
// kViolinVibrato*). Bow speed 0.2 m/s is the stock stroke the violin tests
// and leg25 bow at (inside Askenfelt 1989's 0.05-1 m/s; VERIFY as a
// typical speed). Attack and release: the cello's (UNSOURCED for violin).
// Intonation 0: not measured on this voice (VERIFY: the cello's loop read
// 4-10 c sharp, so the violin may too).
[[nodiscard]] inline BowedVoicing violinBowedVoicing() {
  BowedVoicing v;
  v.name = "violin";
  v.t60LowSeconds = kViolinT60LowSeconds;
  v.t60HighSeconds = kViolinT60HighSeconds;
  v.betaForte = kViolinBetaForte;
  v.betaPiano = kViolinBetaPiano;
  v.bowSpeedMps = 0.2;  // VERIFY (stock violin stroke)
  v.bowForceFraction = kViolinBowForceFraction;
  v.vibratoHz = kViolinVibratoRateHz;
  v.vibratoCents = kViolinVibratoCents;
  v.vibratoOnsetSeconds = kViolinVibratoOnsetSeconds;
  v.vibratoRampSeconds = kViolinVibratoRampSeconds;
  v.intonationCents = {0.0, 0.0, 0.0, 0.0};  // UNSOURCED (not measured on this voice)
  return v;
}

// The viola: no viola-specific voice measurement exists here, so the
// violin's voicing (VERIFY every value for the viola's longer, heavier
// strings). The player's own vibrato is in violistPlayerProfile.
[[nodiscard]] inline BowedVoicing violaBowedVoicing() {
  BowedVoicing v = violinBowedVoicing();
  v.name = "viola";
  return v;
}

// The double bass played arco: the cello's voicing (VERIFY for the bass:
// loss, contact points, force fraction and attack were fitted on cello
// notes). Bow speed: the cello's 0.15 m/s (VERIFY). Intonation 0
// (UNSOURCED, not measured on this voice).
[[nodiscard]] inline BowedVoicing doubleBassBowedVoicing() {
  BowedVoicing v;
  v.name = "double bass (arco)";
  v.intonationCents = {0.0, 0.0, 0.0, 0.0};  // UNSOURCED
  return v;
}

// A bowed player's habits on any bowed voicing. Scales multiply the
// voicing's own value; a negative vibrato field keeps the voicing's. The
// default changes nothing.
struct BowedPlayerHabits {
  double bowForceScale = 1.0;
  double bowSpeedScale = 1.0;
  double contactPointScale = 1.0;
  double vibratoHz = -1.0;
  double vibratoCents = -1.0;
  double vibratoOnsetSeconds = -1.0;
};

// One bowed note on `instrument` at `hz`, `seconds` of bow, level 1 = mf
// (+-12 dB span maps the bow-bridge distance between the voicing's
// betaPiano and betaForte, as makeCelloNote).
[[nodiscard]] inline Result<OversampledBowedStringVoice, QuantityError> makeBowedNote(
    const StringInstrument& instrument, double hz, double seconds, double level, SampleRateHz rate,
    const BowedVoicing& voicing, bool vibrato = true, const BowedPlayerHabits& habits = BowedPlayerHabits{}) {
  using R = Result<OversampledBowedStringVoice, QuantityError>;
  const int k = celloCourseFor(instrument, hz);
  StringCourse c = instrument.courses[static_cast<std::size_t>(k)];
  const double tune = std::pow(2.0, -voicing.intonationCents[static_cast<std::size_t>(k)] / 1200.0);
  c.tuning.frequency = FrequencyHz{c.tuning.frequency.value() * tune};
  c.tension = TensionNewtons{c.tension.value() * tune * tune};
  const double lv = std::clamp(level, 0.05, 2.0);
  const double ldb = 20.0 * std::log10(lv);
  const double x = std::clamp(0.5 - ldb / 24.0, 0.0, 1.0);  // +-12 dB span (UNSOURCED, as makeCelloNote)
  const double beta = (voicing.betaForte + x * (voicing.betaPiano - voicing.betaForte)) * habits.contactPointScale;
  const double f = c.tuning.frequency.value();
  auto ideal = courseString(c);
  if (!ideal.ok()) return R::failure(ideal.error());
  auto loss = lossFromT60(ideal.value().speed, FrequencyHz{f}, DurationSeconds{voicing.t60LowSeconds},
                          FrequencyHz{6.0 * f}, DurationSeconds{voicing.t60HighSeconds});
  if (!loss.ok()) return R::failure(loss.error());
  BowStroke stroke = violinBowStroke(c, beta, voicing.bowSpeedMps * lv * habits.bowSpeedScale);
  stroke.normalForceNewtons *= voicing.bowForceFraction / kViolinBowForceFraction;
  stroke.normalForceNewtons *= habits.bowForceScale;
  auto v = OversampledBowedStringVoice::create(
      c, stroke, loss.value(), 0.0, 4, 0, rate,
      static_cast<std::uint64_t>(std::max(0.0, seconds) * rate.value()), 4);
  if (!v.ok()) return R::failure(v.error());
  OversampledBowedStringVoice voice = std::move(v).value();
  const double vHz = habits.vibratoHz >= 0.0 ? habits.vibratoHz : voicing.vibratoHz;
  const double vCents = habits.vibratoCents >= 0.0 ? habits.vibratoCents : voicing.vibratoCents;
  const double vOnset = habits.vibratoOnsetSeconds >= 0.0 ? habits.vibratoOnsetSeconds : voicing.vibratoOnsetSeconds;
  BowExpression e{vibrato ? vHz : 0.0, vibrato ? vCents : 0.0, vOnset, voicing.vibratoRampSeconds,
                  voicing.attackSeconds, 0.2, voicing.releaseSeconds};
  if (!voice.setExpression(e)) return R::failure(QuantityError::InvalidArgument);
  voice.setBridgeForceOutput(true);
  if (!voice.setStoppedFrequency(std::max(hz * tune, c.tuning.frequency.value()))) return R::failure(QuantityError::DomainError);
  return R::success(std::move(voice));
}

}  // namespace mmathen
