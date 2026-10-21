#pragma once

// Wind and brass player profiles: one saved struct of a player's habits,
// the wind counterpart of the violin's BowingGestureParams
// (bowing_gesture.h). playPhrase (phrase_player.h) reads it for the
// tongue, the attack, the release, the vibrato, the slur glide and the
// breath plan; the airway (player_airway.h) takes its tongue speed and
// glottis from airwaySpec(). A default-constructed profile is the phrase
// player's earlier fixed constants, so every score plays as before
// unless a profile is passed.
//
// Every value is cited from measurements of players or marked TUNED /
// VERIFY / UNSOURCED. Embouchure (lip resonance ratio, reed) stays in
// the instrument rows (wind_presets.h); the profile scales it per player.

#include "player_airway.h"

namespace mmathen {

struct WindPlayerProfile {
  const char* name = "default";
  // --- Breath and dynamics -------------------------------------------
  // Blowing pressure multiplier on the instrument's per-note law
  // (trumpetMouthPressurePa etc.). Players differ: Bianco et al., JNMR
  // 41, 49-65 (2012), Fig. 4: one of three professionals needed visibly
  // more pressure for the same level. 1 = the law itself.
  double pressureScale = 1.0;
  // --- Attack (tongue) -----------------------------------------------
  // Tongue closure before a tongued note. 0 = the phrase player's fixed
  // 1920 frames (40 ms at 48 kHz; Bertsch & Hoole, fast single tonguing
  // about 40-64 ms).
  double tongueClosureSec = 0.0;
  // Tongue tip release (critically damped 10-90 %), PlayerAirwaySpec.
  // 8 ms: Li, Almeida, Smith & Wolfe, JASA 140, 1089 (2016), clarinet.
  double tongueReleaseSec = 0.008;
  // Pressure build-up behind the closed tongue before a tongued note in
  // a phrase: the mouth pressure rises above the playing level and drops
  // at the release (a plosive), and the lips start during the drop.
  // Bianco et al. 2012, sec. 4.1, concatenated C5 notes: build-up
  // amplitude 0.56 +/- 0.48 kPa over 94 +/- 60 ms (57 ms ff, 121 ms pp).
  // 0 = no build-up (the earlier player).
  double attackBuildUpPa = 0.0;
  // Time the build-up takes to fall back to the playing pressure after
  // the release (Bianco's Segment B; the paper gives its trend only,
  // shorter for louder notes, Fig. 8). VERIFY.
  double attackDropSec = 0.0;
  // --- Release -------------------------------------------------------
  double brassBreathReleaseSec = 0.12;  // kBrassBreathReleaseSec (TUNED to Lick_Trumpet)
  // --- Vibrato -------------------------------------------------------
  double vibratoHz = 5.5, vibratoCents = 12.0;             // woodwinds: arguments (kVibratoHz)
  double brassVibratoHz = 5.84, brassVibratoCents = 11.7;  // VSCO 2 CE trumpet susvib (kBrassVibrato*)
  double vibratoOnsetSec = 0.20;                           // vibrato starts after this (UNSOURCED)
  // Breath vibrato: fraction of the blowing pressure swung at vibratoHz
  // (0 = none; the vibrato is then the bore-length swing above).
  double vibratoPressureFraction = 0.0;
  // --- Slurs ---------------------------------------------------------
  double slurGlideSec = 0.035, tonguedGlideSec = 0.010;    // woodwind bore glides (UNSOURCED)
  // --- Breath placement ----------------------------------------------
  double breathMarginM3 = 1.0e-3, breathStealSec = 0.30;   // kBreathMarginM3 / kBreathStealSec (UNSOURCED)
  // --- Intonation ----------------------------------------------------
  // Lip resonance over the instrument row's ratio (brass): a player who
  // "lips up" sits above the row. 1 = the row.
  double lipRatioScale = 1.0;
  // Constant pitch habit, cents (+ sharp), applied as a bore-length
  // change. 0 = in tune.
  double intonationCents = 0.0;

  // The player's airway with this profile's tongue speed.
  [[nodiscard]] PlayerAirwaySpec airwaySpec(PlayerAirwaySpec base = {}) const {
    base.tongueReleaseSeconds = tongueReleaseSec;
    return base;
  }
};

// A professional trumpeter in a tongued phrase. Measured values:
// Bianco et al. 2012 (three professionals, C5, IRCAM): concatenated-note
// pressure build-up 0.56 kPa (mean), duration 94 ms (mean; the closure
// is kept at the phrase player's 40 ms, the fast-tonguing figure, since
// the JB notes are 0.3 s apart). Vibrato and release keep the VSCO /
// Lick_Trumpet values. The drop time is VERIFY (see attackDropSec).
[[nodiscard]] inline WindPlayerProfile trumpetPlayerProfile() {
  WindPlayerProfile p;
  p.name = "trumpet (Bianco 2012 professional)";
  p.attackBuildUpPa = 560.0;
  p.attackDropSec = 0.060;  // VERIFY: Bianco Fig. 8 trend only; JB sweep 15/30/60 ms, 60 kept the YIN gate (steady worst -37 c vs -55 c at 30 ms)
  return p;
}

// A flutist. N. H. Fletcher, "Acoustical correlates of flute performance
// technique", JASA 57, 233-237 (1975), four experienced players: vibrato
// "consistently about 5 Hz", made by the diaphragm as "a 10 % variation
// in blowing pressure", with frequency modulation of the partials under
// 1 % and mostly amplitude modulation of the upper partials (Table I).
// So the vibrato is breath, not a bore-length swing: vibratoCents 0 and
// the jet (pressure -> jet speed -> convection delay) makes whatever
// pitch and timbre swing follows. Fletcher, Acoustics Australia 28, 55
// (2000): 5-6 Hz. Tongue, release and breath plan stay at the defaults.
[[nodiscard]] inline WindPlayerProfile flutePlayerProfile() {
  WindPlayerProfile p;
  p.name = "flute (Fletcher 1975)";
  p.vibratoHz = 5.0;
  p.vibratoCents = 0.0;
  p.vibratoPressureFraction = 0.10;
  return p;
}

}  // namespace mmathen
