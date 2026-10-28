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

#include <cstdint>

#include "player_airway.h"

namespace mmathen {

// Cracked notes (step 24.2a). How long a player keeps a wrong embouchure
// setting before correcting it: the pitch-shift reflex of singers
// responds 104-223 ms after a heard pitch error (T. A. Burnett, M. B.
// Freedland, C. R. Larson & T. C. Hain, "Voice F0 responses to
// manipulations in pitch feedback", JASA 103, 3153-3161, 1998); 0.10 s,
// the low end, for a trained player (VERIFY for brass and reed players).
inline constexpr double kCrackHoldSec = 0.10;
// Time the correction takes once it starts (UNSOURCED).
inline constexpr double kCrackCorrectSec = 0.05;

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
  // When > 0, the build-up scales with the note's playing pressure:
  // attackBuildUpPa at this pressure (the measured note's), in proportion
  // elsewhere. 0 = the same build-up on every note.
  double attackBuildUpRefPa = 0.0;
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
  // Largest part of a note a breath may take; shorter notes are passed
  // over until the lungs are within half the margin (UNSOURCED; players
  // breathe at phrase joins on longer notes, Bouhuys 1964, VERIFY).
  double breathMaxNoteFraction = 0.5;
  // --- Intonation ----------------------------------------------------
  // Lip resonance over the instrument row's ratio (brass): a player who
  // "lips up" sits above the row. 1 = the row.
  double lipRatioScale = 1.0;
  // Constant pitch habit, cents (+ sharp), applied as a bore-length
  // change. 0 = in tune.
  double intonationCents = 0.0;
  // --- Cracked notes (step 24.2a) ------------------------------------
  // SD of the embouchure's setting error at a note's start, in cents of
  // the oscillator's own tuning (the lip resonance on brass, the jet on
  // the flute; on the reeds the same slip lowers the reed's resonance and
  // loosens the lip's damping of it, phrase_player.h). The phrase player draws one
  // error per note (seeded by crackSeed), holds it while the player hears
  // the note and then corrects it (crackHoldSec, crackCorrectSec). Whether
  // the note cracks is the instrument's physics: the bore's next
  // resonance captures the oscillation only when the error comes near it,
  // so notes with close partials crack more; soft attacks (pp, p, mp)
  // draw a larger error (kCrackSoftAttackGain). 0 = off.
  // No measured crack rate was found for professional players (step 24.2a
  // notes), so every named profile keeps 0 (UNSOURCED range in the
  // builder).
  double crackSdCents = 0.0;
  double crackHoldSec = kCrackHoldSec;
  double crackCorrectSec = kCrackCorrectSec;
  std::uint64_t crackSeed = 1;

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
  // Mean + 1 SD of Bianco's concatenated build-up (0.56 + 0.48 kPa): the
  // most proficient of the three players built the most (sec. 4.1).
  // JB sweep (jb14): 0.56 / 1.0 / 1.5 / 2.5 kPa, slowest leap attack
  // 107 / 89 / 83 / 71 ms, silent starts 1 / 0 / 0 / 0, steady worst
  // -36 / -35 / -34 / -36 c; 1.5 and 2.5 kPa lie outside one SD.
  p.attackBuildUpPa = 1040.0;
  // ... at the C5 mf playing pressure of this model (9.5 Pa/Hz x 523 Hz),
  // in proportion on other notes (UNSOURCED: Bianco report one level per
  // player, not per note). A fixed 1.04 kPa over-blew D4 (2.4 kPa) into a
  // 155 ms attack; scaled: slowest leap 83 ms, 0 silent, steady -35 c.
  p.attackBuildUpRefPa = 4970.0;
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
// Step 17: the pitch swing this breath vibrato makes depends on the bore
// resonance width (same paper, sec. V). With the flute's Kirchhoff wall
// fit on (wind_presets.h kFluteWallFitFundamental, bore Q 33 at C4 as
// Fletcher measured) the same 10 % swing gives +-5.6 c in Silent Night
// (Iowa MIS flute vib pp +-9.6 c; +-0.73 c before). The habits stay as
// cited: 5 Hz, 10 % pressure, vibratoCents 0; the breathiness is the
// voicing's (fluteVoicingFor), unchanged.
[[nodiscard]] inline WindPlayerProfile flutePlayerProfile() {
  WindPlayerProfile p;
  p.name = "flute (Fletcher 1975)";
  p.vibratoHz = 5.0;
  p.vibratoCents = 0.0;
  p.vibratoPressureFraction = 0.10;
  return p;
}

// A classical clarinettist. Blowing pressure: L. Fuks & J. Sundberg,
// "Blowing pressures in bassoon, clarinet, oboe and saxophone", Acustica
// 85, 267 (1999) (TMH-QPSR 37(3), 41, 1996), two professionals: 2-5.4
// kPa, 2-3 kPa soft and 4-4.5 kPa loud (Fletcher, Acoustics Australia
// 28, 53, 2000); the harness's 4 kPa sits inside, so the scale is 1.
// Tongue: the reed returns from the tongue in about 8 ms (Li, Almeida,
// Smith & Wolfe, JASA 140, 1089, 2016), the default. Release: by the
// breath (kReedBreathReleaseSec, Li et al. 2016). Vibrato: the
// classical (German / Viennese / most orchestral) clarinet plays
// straight; a note marked vibrato is played without it (VERIFY: a
// performance-practice statement, no measurement).
[[nodiscard]] inline WindPlayerProfile clarinetPlayerProfile() {
  WindPlayerProfile p;
  p.name = "clarinet (Fuks & Sundberg 1999; straight tone)";
  p.vibratoCents = 0.0;
  return p;
}

// A concert saxophonist. Vibrato: 873 notes by eight concert soloists,
// mean rate 5.96 Hz (SD 0.74) and pitch extent 37.3 c peak to peak (SD
// 16.7), i.e. +/-18.7 c ("An analysis of concert
// saxophone vibrato through the examination of recordings by eight
// prominent soloists", DMA thesis, U. Cincinnati, 2013, OhioLINK
// ucin1384428227; author VERIFY). Gilbert, Simon & Terroir, JASA
// 118, 2649 (2005): mostly jaw ("a la machoire") vibrato, the jaw
// moving the reed; here the bore-length swing stands in for it, and
// vibratoCents is the swing that sounds +/-18.7 c (TUNED, see
// kSaxVibratoSwingCents). Pressure: Fuks & Sundberg 1999, 1.5-7.8 kPa
// (about 2 kPa soft); the harness's 4.2 kPa is inside, scale 1.
// Swing TUNED on Silent Night (alto part, 9 vibrato notes, YIN 21 ms
// frames): a swing of 18.7 c sounded +/-14.1 c median (the reed and bore
// share the change), 24.8 c sounds +/-18.9 c (14.9-22.0).
inline constexpr double kSaxVibratoSwingCents = 24.8;
[[nodiscard]] inline WindPlayerProfile saxophonePlayerProfile() {
  WindPlayerProfile p;
  p.name = "alto sax (concert soloists, 5.96 Hz +/-18.7 c)";
  p.vibratoHz = 5.96;
  p.vibratoCents = kSaxVibratoSwingCents;
  return p;
}

// A trombonist. The brass attack is the trumpet's (Bianco et al. 2012
// measured trumpeters; their mean 0.56 kPa build-up is taken over, VERIFY for
// trombone, whose playing pressures are lower: 1.6 kPa here). Scaled by
// the pressure ratio would be about 0.24 kPa; the measured 0.56 kPa
// is used as an upper bound and swept. Vibrato: the trumpet's VSCO
// values (VERIFY: trombone players use slide or lip vibrato).
[[nodiscard]] inline WindPlayerProfile trombonePlayerProfile() {
  WindPlayerProfile p = trumpetPlayerProfile();
  p.name = "trombone (trumpet attack, VERIFY)";
  p.attackBuildUpPa = 560.0;  // Bianco mean (the trumpet profile takes mean + 1 SD)
  p.attackBuildUpRefPa = 0.0;
  return p;
}

}  // namespace mmathen
