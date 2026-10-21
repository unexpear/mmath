#pragma once

// One bowed note for any instrument of the violin family (violin, viola,
// cello, double bass played arco): makeCelloNote with the cello's numbers
// lifted into a BowedVoicing, so the same note maker serves every bowed
// string and its player habits (player_profile.h, BowedPlayerProfile).
// celloBowedVoicing() is exactly cello_voice.h's constants: makeBowedNote
// with it and default habits renders sample for sample what makeCelloNote
// renders (tests/test_player_profile.cpp checks this).
//
// Output is the string's force on a rigid bridge, as makeCelloNote. The
// voicing's body (BowedBodySpec, step 14) turns that force into sound:
// run the summed force of a part through BowedBody. The cello's body is
// CelloBody (cello_voice.h); celloBowedVoicing() has no BowedBodySpec.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

#include "bowed_string.h"
#include "cello_voice.h"
#include "instrument.h"
#include "violin_body.h"

namespace mmathen {

// A bowed instrument's body as a filter chain on the bridge force (step
// 14), the chain violin_body.h already holds for the violin: bridge hill
// (BridgeHillFilter), four signature modes (SignatureModeFilter) and the
// radiation fall below A0 (RadiationHighpass). present = false: no body,
// the output stays string force on a rigid bridge (the cello's body is
// CelloBody in cello_voice.h).
struct BowedBodySpec {
  bool present = false;
  double bridgeHz = 0.0, bridgeQ = 0.0, bridgeGain = 1.0;
  double modeHz[4] = {0, 0, 0, 0}, modeQ[4] = {1, 1, 1, 1}, modeGain[4] = {0, 0, 0, 0};
  double radiationCornerHz = 0.0;
};

// The violin: violin_body.h's numbers (Jansson 1997/1999 bridge hill at
// 2.5 kHz, Woodhouse 2005 Q and height; Bissinger 2008 A0 / CBR / B1- / B1+;
// Bremaud 2012 wood loss; the TUNED gain and Q scales and 200 Hz corner).
[[nodiscard]] inline BowedBodySpec violinBodySpec() {
  BowedBodySpec b;
  b.present = true;
  b.bridgeHz = kViolinBridgeRockingHz;
  b.bridgeQ = kViolinBridgeHillQ;
  b.bridgeGain = kViolinBridgeHillGain;
  for (int m = 0; m < 4; ++m) {
    b.modeHz[m] = kViolinSignatureHz[m];
    b.modeQ[m] = std::max(8.0, violinSignatureQ(m) * kViolinSignatureQScale);
    b.modeGain[m] = kViolinSignatureGainScale * kViolinSignatureRelGain[m];
  }
  b.radiationCornerHz = kViolinRadiationCornerHz;
  return b;
}

// The viola. Modes: A0 224 Hz (Powell, "Acoustic analysis of the viola",
// UIUC Physics 406 REU report 2012, one measured viola, VERIFY); B1- 350 Hz
// and B1+ 440 Hz (H. Johannsson, "Acoustics", hansjohannsson.com: "in the
// viola the main body resonances are around F 350 Hz and Ab-A 440", VERIFY:
// a maker's typical values). CBR: the violin's 407 Hz scaled by the B1-
// ratio 350 / 475 (UNSOURCED). Q and gains: the violin's (same wood, same
// mode kinds; VERIFY). Bridge hill: the violin's 2.5 kHz scaled by the body
// length ratio 356 / 410 mm (UNSOURCED), Q from Woodhouse's formula at that
// frequency with the violin bridge's inertia (VERIFY), the violin's height.
// Radiation corner: the violin's TUNED 200 Hz at the same ratio to A0
// (200 / 275 x 224 = 163 Hz, UNSOURCED scaling).
inline constexpr double kViolaSignatureHz[4] = {224.0, 407.0 * 350.0 / 475.0, 350.0, 440.0};
[[nodiscard]] inline BowedBodySpec violaBodySpec() {
  BowedBodySpec b = violinBodySpec();
  for (int m = 0; m < 4; ++m) b.modeHz[m] = kViolaSignatureHz[m];
  b.bridgeHz = kViolinBridgeRockingHz * 0.356 / 0.410;
  b.bridgeQ = violinBridgeHillQ(b.bridgeHz);
  b.radiationCornerHz = kViolinRadiationCornerHz / kViolinSignatureHz[0] * kViolaSignatureHz[0];
  return b;
}

// The double bass. Askenfelt, "Eigenmodes and tone quality of the double
// bass", KTH STL-QPSR 23(4), 1982: Helmholtz A0 near 65 Hz, the first top
// plate mode T1 near 110 Hz; the ISMA 2004 laser-vibrometer survey of a
// round-back bass ("Documentation of double bass plate modes using the
// scanning laser vibrometer") finds A0 65 Hz, T1 115 Hz, C3 155 Hz. T1 and
// C3 are the B1- / B1+ pair of the violin under older names (Woodhouse's
// glossary), so they take the violin's B1 Q and gains; no CBR (gain 0).
// Bridge: Askenfelt 1982 finds three bridge eigenmodes, the lowest giving
// a wide hump above 400 Hz: hill at 400 Hz (VERIFY), the violin's Q and
// height (VERIFY: Askenfelt gives no damping). Radiation corner: the
// violin's ratio to A0 (200 / 275 x 65 = 47 Hz, just above open E1, as
// the violin's sits just above open G; UNSOURCED scaling).
inline constexpr double kDoubleBassSignatureHz[4] = {65.0, 110.0, 155.0, 155.0};
[[nodiscard]] inline BowedBodySpec doubleBassBodySpec() {
  BowedBodySpec b = violinBodySpec();
  b.modeHz[0] = kDoubleBassSignatureHz[0];
  b.modeHz[1] = kDoubleBassSignatureHz[3];  // unused slot (gain 0)
  b.modeGain[1] = 0.0;
  b.modeHz[2] = kDoubleBassSignatureHz[1];
  b.modeHz[3] = kDoubleBassSignatureHz[2];
  b.bridgeHz = 400.0;  // VERIFY (Askenfelt 1982: humps above 400 Hz)
  b.radiationCornerHz = kViolinRadiationCornerHz / kViolinSignatureHz[0] * kDoubleBassSignatureHz[0];
  return b;
}

// The body chain at a sample rate: out = Radiation(Signature(Bridge(force))).
class BowedBody {
 public:
  BowedBody(const BowedBodySpec& s, double sampleRate)
      : bridge_(sampleRate, s.bridgeHz, s.bridgeQ, s.bridgeGain),
        modes_(sampleRate, s.modeHz, s.modeQ, s.modeGain),
        radiation_(sampleRate, s.radiationCornerHz) {}
  double process(double x) noexcept { return radiation_.process(modes_.process(bridge_.process(x))); }
  void processInPlace(std::vector<double>& v) noexcept { for (double& x : v) x = process(x); }
 private:
  BridgeHillFilter bridge_;
  SignatureModeFilter modes_;
  RadiationHighpass radiation_;
};

// The instrument side of a bowed note: what the voice does when the
// player brings nothing of their own.
struct BowedVoicing {
  const char* name = "cello";
  double t60LowSeconds = kCelloT60LowSeconds;    // string loss at f0
  double t60HighSeconds = kCelloT60HighSeconds;  // and at t60HighHz
  // Frequency of t60HighSeconds; 0 = 6 f0 of the course (the violin's and
  // the bass's loss as fitted). The cello: kCelloT60HighHz (cello_voice.h).
  double t60HighHz = kCelloT60HighHz;
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
  // The body after the bridge force (none for the cello: CelloBody).
  BowedBodySpec body{};
};

// The cello: cello_voice.h's constants, unchanged.
[[nodiscard]] inline BowedVoicing celloBowedVoicing() { return BowedVoicing{}; }

// The violin: bowed_string.h's violin constants (loss kViolinT60*,
// kViolinBetaForte / kViolinBetaPiano, kViolinBowForceFraction, vibrato
// kViolinVibrato*). Bow speed 0.2 m/s is the stock stroke the violin tests
// and leg25 bow at (inside Askenfelt 1989's 0.05-1 m/s; VERIFY as a
// typical speed). Attack and release: the cello's (UNSOURCED for violin).
// Intonation: measured on this voice (step 14, below).
[[nodiscard]] inline BowedVoicing violinBowedVoicing() {
  BowedVoicing v;
  v.name = "violin";
  v.t60LowSeconds = kViolinT60LowSeconds;
  v.t60HighSeconds = kViolinT60HighSeconds;
  v.t60HighHz = 0.0;  // 6 f0, as kViolinT60* were fitted
  v.betaForte = kViolinBetaForte;
  v.betaPiano = kViolinBetaPiano;
  v.bowSpeedMps = 0.2;  // VERIFY (stock violin stroke)
  v.bowForceFraction = kViolinBowForceFraction;
  v.vibratoHz = kViolinVibratoRateHz;
  v.vibratoCents = kViolinVibratoCents;
  v.vibratoOnsetSeconds = kViolinVibratoOnsetSeconds;
  v.vibratoRampSeconds = kViolinVibratoRampSeconds;
  // Step 14: measured on this voice as the cello's (no vibrato, levels
  // 0.25 / 0.5 / 1, open and +2 / +4 semitone stops, parabolic FFT peak over
  // H1-H4, median per string): the loop sounded 3.5 / 2.6 / 2.1 / 1.7 c sharp.
  v.intonationCents = {3.5, 2.6, 2.1, 1.7};  // TUNED (measured on the model)
  v.body = violinBodySpec();
  return v;
}

// The viola: no viola-specific voice measurement exists here, so the
// violin's voicing (VERIFY every value for the viola's longer, heavier
// strings). The player's own vibrato is in violistPlayerProfile.
[[nodiscard]] inline BowedVoicing violaBowedVoicing() {
  BowedVoicing v = violinBowedVoicing();
  v.name = "viola";
  // Step 14: measured on the viola as on the violin: 4.1 / 3.5 / 2.6 / 2.1 c sharp.
  v.intonationCents = {4.1, 3.5, 2.6, 2.1};  // TUNED (measured on the model)
  v.body = violaBodySpec();
  return v;
}

// The double bass played arco: the cello's voicing (VERIFY for the bass:
// loss, contact points, force fraction and attack were fitted on cello
// notes). Bow speed: the cello's 0.15 m/s (VERIFY). Intonation measured
// on this voice (step 14, below).
[[nodiscard]] inline BowedVoicing doubleBassBowedVoicing() {
  BowedVoicing v;
  v.name = "double bass (arco)";
  v.t60HighHz = 0.0;  // 6 f0: the bass arco loss as before step 10 (VERIFY)
  // Step 14: measured as the violin's (E1 A1 D2 G2): 12.8 / 11.5 / 9.4 / 8.0 c sharp.
  v.intonationCents = {12.8, 11.5, 9.4, 8.0};  // TUNED (measured on the model)
  v.body = doubleBassBodySpec();
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
                          FrequencyHz{voicing.t60HighHz > 0.0 ? voicing.t60HighHz : 6.0 * f},
                          DurationSeconds{voicing.t60HighSeconds});
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
