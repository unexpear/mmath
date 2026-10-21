#pragma once

// Player profiles for the non-wind band instruments: one saved struct of a
// player's habits per family, the counterpart of WindPlayerProfile
// (wind_player_profile.h) and the violin's BowingGestureParams
// (bowing_gesture.h). Families:
//   bowed   (cello, violin, viola,
//            double bass arco)          BowedPlayerProfile
//   plucked (guitar, double bass pizz,
//            harp, harpsichord,
//            electric guitar)           PluckedPlayerProfile
//   struck  (piano, snare, glockenspiel, celesta, tubular bells,
//            handbells)                 StruckPlayerProfile
//   drum kit (kick, snare, toms,
//            hi-hat, cymbals)           DrumKitPlayerProfile
//   shaken  (sleigh bells)              ShakenPlayerProfile
// Every family shares PlayerFeel: how a player's onsets and loudness
// scatter around the written part. A default-constructed profile changes
// nothing: performNotes returns the written notes unchanged and every
// per-instrument habit equals the instrument header's constant, so every
// score renders byte-identically unless a named profile is passed.
//
// Every value is cited from measurements of players or marked TUNED /
// VERIFY / UNSOURCED.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "bowed_note.h"
#include "cello_voice.h"
#include "double_bass_pizz.h"
#include "guitar_pluck.h"
#include "piano_voice.h"
#include "sleigh_bell.h"

namespace mmathen {

// One written note of a non-wind part (the winds use wind_presets.h's
// ScoreNote): onset and length in seconds, MIDI key and
// MIDI velocity (1-127; the band harnesses map it to the physical drive).
struct PartNote {
  double startSec = 0.0;
  double durSec = 0.0;
  int midi = 60;
  double velocity = 64.0;
};

// --- Shared habits ------------------------------------------------------------
struct PlayerFeel {
  // Overall touch: the written velocity is multiplied by this. 1 = as written.
  double velocityScale = 1.0;
  // Note-to-note loudness scatter: SD of the played velocity over its mean
  // (coefficient of variation). Measured as "CV of key velocity" for twelve
  // skilled pianists in an isochronous melody (W. Goebl & C. Palmer, PLoS
  // ONE 8, e50901, 2013, Fig. 3C); 0 = every note exactly as written.
  double velocityCv = 0.0;
  // Onset scatter: SD of each onset as a fraction of the interval before
  // it (a Weber-law timekeeper: B. H. Repp, Psychon. Bull. Rev. 4, 530,
  // 1997, IOI SD rises linearly with IOI, Weber fraction about 5 % in
  // expressive playing). 0 = on the grid.
  double timingCv = 0.0;
  // Ceiling on the onset SD, seconds (0 = none). In an ensemble a player
  // follows the shared beat, so long notes do not scatter in proportion:
  // R. A. Rasch, "Synchronization in performed ensemble music", Acustica
  // 43, 121-131 (1979), trios: rms asynchrony between parts 30-50 ms, i.e.
  // about 21-35 ms per player (divided by sqrt 2). The named profiles take
  // 20 ms, just under the low end (VERIFY).
  double timingSdMaxSec = 0.0;
  // Constant lead (-) or lag (+) of every onset, seconds.
  double timingOffsetSec = 0.0;
  // The scatter is a seeded generator, so a profile always plays the same
  // performance (renders stay reproducible).
  std::uint64_t seed = 1;

  [[nodiscard]] bool isDefault() const noexcept {
    return velocityScale == 1.0 && velocityCv == 0.0 && timingCv == 0.0 && timingOffsetSec == 0.0;
  }
};

namespace player_detail {
// xorshift64 + Box-Muller, the generator bowing_gesture.h uses.
class Rng {
 public:
  explicit Rng(std::uint64_t seed) : s_(seed ? seed : 1) {}
  double uniform() noexcept {
    s_ ^= s_ << 13; s_ ^= s_ >> 7; s_ ^= s_ << 17;
    return (static_cast<double>(s_ >> 11) + 0.5) * (1.0 / 9007199254740992.0);
  }
  double gauss() noexcept { return std::sqrt(-2.0 * std::log(uniform())) * std::cos(6.283185307179586 * uniform()); }
 private:
  std::uint64_t s_;
};
// The named profiles' ceiling on the onset SD (PlayerFeel::timingSdMaxSec).
inline constexpr double kEnsembleTimingSdMaxSec = 0.020;
// Notes whose onsets are within this of each other are one chord: one hand
// gesture, so they share a timing deviation (Goebl 2001: finger-key
// asynchrony within a chord is near zero once the velocity artifact is
// removed). 1 ms, UNSOURCED (a score-grouping tolerance, not a habit).
inline constexpr double kChordToleranceSec = 1.0e-3;
}  // namespace player_detail

// The written part as this player plays it: onsets moved (whole chords
// together) and velocities scaled and scattered. Order and lengths are
// kept. A default feel returns the input unchanged (bit-identical).
[[nodiscard]] inline std::vector<PartNote> performNotes(const std::vector<PartNote>& written, const PlayerFeel& feel) {
  std::vector<PartNote> out = written;
  if (feel.isDefault() || out.empty()) return out;
  player_detail::Rng rng(feel.seed);
  // Distinct onsets in time order.
  std::vector<double> onsets;
  for (const PartNote& n : written) onsets.push_back(n.startSec);
  std::sort(onsets.begin(), onsets.end());
  std::vector<double> groups;
  for (double t : onsets)
    if (groups.empty() || t - groups.back() > player_detail::kChordToleranceSec) groups.push_back(t);
  std::vector<double> shift(groups.size(), feel.timingOffsetSec);
  for (std::size_t g = 0; g < groups.size(); ++g) {
    double ioi = g > 0 ? groups[g] - groups[g - 1] : (groups.size() > 1 ? groups[1] - groups[0] : 0.0);
    double sd = feel.timingCv * ioi;
    if (feel.timingSdMaxSec > 0.0) sd = std::min(sd, feel.timingSdMaxSec);
    shift[g] += sd * rng.gauss();
  }
  for (PartNote& n : out) {
    const auto it = std::upper_bound(groups.begin(), groups.end(), n.startSec + player_detail::kChordToleranceSec);
    const std::size_t g = static_cast<std::size_t>(std::max<std::ptrdiff_t>(0, (it - groups.begin()) - 1));
    n.startSec = std::max(0.0, n.startSec + shift[g]);
    double v = n.velocity * feel.velocityScale;
    if (feel.velocityCv > 0.0) v *= 1.0 + feel.velocityCv * rng.gauss();
    n.velocity = std::clamp(v, 1.0, 127.0);
  }
  return out;
}

// --- Bowed ---------------------------------------------------------------------
// The cellist's controls (Askenfelt, JASA 86, 503, 1989; Schoonderwaldt &
// Demoucron, JASA 126, 2695, 2009): bow force, bow speed, bow-bridge
// distance (contact point) and the left-hand vibrato. The habits are
// CelloPlayerHabits (cello_voice.h); the default is the voice's own
// constants. The violin, viola and arco bass take BowedPlayerHabits
// (bowed_note.h, makeBowedNote) on their BowedVoicing; the default is the
// voicing's own values.
struct BowedPlayerProfile {
  const char* name = "default";
  PlayerFeel feel;
  CelloPlayerHabits cello;
  BowedPlayerHabits strings;
};

// A cellist from the string-vibrato literature. Vibrato: J. M. Geringer &
// M. L. Allen, "An analysis of vibrato among high school and university
// violin and cello students", JRME 52, 167-178 (2004), 40 players: mean
// rate about 5.5 Hz with no instrument difference; cellists' widths about
// 8 c narrower than violinists' (R. B. MacLeod, "String vibrato: research
// related to performance and perception", Update 29, 2010 review: student
// violin widths about 30 c peak to peak), so about 22 c peak to peak,
// +/-11 c (VERIFY the exact cello mean in Geringer & Allen Table 2). The
// voice's own 6.0 Hz +/-9 c was measured on one PD Sarabande recording.
// Bow force, speed and contact point stay at the voice's values (no
// cello-specific measurement to move them; VERIFY with Askenfelt-type
// cello data). Timing and loudness scatter: the pianists' figures
// (Goebl & Palmer 2008 / 2013, VERIFY for bowed strings).
[[nodiscard]] inline BowedPlayerProfile cellistPlayerProfile() {
  BowedPlayerProfile p;
  p.name = "cellist (Geringer & Allen 2004 vibrato)";
  p.cello.vibratoHz = 5.5;
  p.cello.vibratoCents = 11.0;
  p.feel.timingCv = 0.03;   // Goebl & Palmer 2008 lower bound (isochronous melody, CV 0.03-0.08), VERIFY for cello
  p.feel.timingSdMaxSec = player_detail::kEnsembleTimingSdMaxSec;
  p.feel.velocityCv = 0.05; // VERIFY (Goebl & Palmer 2013 Fig. 3C order of magnitude, pianists)
  p.feel.seed = 0xCE110;
  return p;
}

// Step 14 (kept current): the violinist, violist and arco-bassist
// profiles play on violinBowedVoicing / violaBowedVoicing /
// doubleBassBowedVoicing, which carry measured intonation
// (BowedVoicing::intonationCents) and a body (BowedBodySpec; the harness
// runs BowedBody on a part's summed bridge force). The profiles' habits
// leave bow force, speed and contact point at x1, where the intonation was
// measured, and their vibrato is centred on the corrected pitch: with each
// profile's vibrato the power-weighted centre of H2 over 0.8-2.3 s of an
// open-string note sits -0.5..+0.1 c (violin), -0.5..0.0 c (viola) and
// -3.6..+0.6 c (arco bass; the A string at level 1 is double slip, as in
// the step-14 gate) from the open string (test_band_voices.cpp). No
// profile value had to move.
// Violinists and violists: R. B. MacLeod, "Influences of dynamic level and
// pitch register on the vibrato rates and widths of violin and viola
// players", JRME 56 (2008), doi:10.1177/0022429408323070, 48 high-school and university players:
// mean rate 5.71 Hz violin, 5.49 Hz viola; mean width 51 c and 41 c peak to
// peak (measured highest to lowest pitch of a cycle), so +/-25.5 c and
// +/-20.5 c. The voice's 6.5 Hz +/-20 c violin default came from one
// recording (bowed_string.h, VERIFY there). Bow force, speed and contact
// point stay at the voicing's (the violin's bow-gesture habits are
// BowingGestureParams, bowing_gesture.h, Askenfelt 1988; leg25 uses them).
// Timing and loudness scatter: as the cellist (Goebl & Palmer, VERIFY for
// bowed strings).
[[nodiscard]] inline BowedPlayerProfile violinistPlayerProfile() {
  BowedPlayerProfile p;
  p.name = "violinist (MacLeod 2008 vibrato)";
  p.strings.vibratoHz = 5.71;
  p.strings.vibratoCents = 25.5;
  p.feel.timingCv = 0.03;   // VERIFY for violin (pianists' figure)
  p.feel.timingSdMaxSec = player_detail::kEnsembleTimingSdMaxSec;
  p.feel.velocityCv = 0.05; // VERIFY
  p.feel.seed = 0x7A1;
  return p;
}
[[nodiscard]] inline BowedPlayerProfile violistPlayerProfile() {
  BowedPlayerProfile p;
  p.name = "violist (MacLeod 2008 vibrato)";
  p.strings.vibratoHz = 5.49;
  p.strings.vibratoCents = 20.5;
  p.feel.timingCv = 0.03;   // VERIFY for viola
  p.feel.timingSdMaxSec = player_detail::kEnsembleTimingSdMaxSec;
  p.feel.velocityCv = 0.05; // VERIFY
  p.feel.seed = 0x7A2;
  return p;
}
// A double bassist playing arco. Vibrato: "An analysis of double bass
// vibrato", String Research Journal (2024), doi:10.1177/19484992241238989
// (FSU dissertation of the same title), 40 university bassists: mean rate
// 5.17 Hz, width 19 c (MacLeod's peak-to-peak method), so +/-9.5 c.
// Professionals on record are faster, 5-7 Hz, mostly 6.5-7 (M. D.
// Thibeault, American String Teacher 47, 1997); the students' mean is
// kept so all three bowed profiles come from like players. Timing: as the
// pizzicato bassist (Goebl & Palmer lower bound, VERIFY).
// Step 16: the bow attack is the voicing's (doubleBassBowedVoicing,
// kDoubleBassArcoAttackSeconds 0.027 s TUNED, fails the +-10 % robustness
// map 23/351, /workspace/s16/NOTES.md); this player brings no attack of
// their own, so no habit changes. With it the A string plays at pitch at
// full level and the full-level exclusion is gone from the pitch test.
[[nodiscard]] inline BowedPlayerProfile arcoBassistPlayerProfile() {
  BowedPlayerProfile p;
  p.name = "arco bassist (2024 double bass vibrato study)";
  p.strings.vibratoHz = 5.17;
  p.strings.vibratoCents = 9.5;
  p.feel.timingCv = 0.03;   // VERIFY for arco bass
  p.feel.timingSdMaxSec = player_detail::kEnsembleTimingSdMaxSec;
  p.feel.velocityCv = 0.05; // VERIFY
  p.feel.seed = 0xBA55A;
  return p;
}

// --- Plucked -------------------------------------------------------------------
struct PluckedPlayerProfile {
  const char* name = "default";
  PlayerFeel feel;
  // Pluck force: the finger's pull scales the string displacement. 1 = the
  // harness's velocity mapping.
  double pluckForceScale = 1.0;
  // Pluck point, metres from the bridge. <= 0: the instrument's default
  // (guitar kClassicalGuitarRoundPluckFromBridgeM, bass
  // kDoubleBassPizzFromBridgeM).
  double pluckFromBridgeM = 0.0;
  // Pluck point as a fraction of the string length (harp: from the
  // soundboard). <= 0: the harness default. Used where the habit is
  // measured as a fraction (harp); pluckFromBridgeM wins when both are set.
  double pluckFraction = 0.0;
  // Finger damping of a ringing string at a re-pluck or the note's end,
  // seconds of fade. <= 0: the harness default (guitar 10 ms, bass
  // kDoubleBassPizzDampSec).
  double fingerDampSec = 0.0;
};

// A classical guitarist. Pluck point: C. Traube & J. O. Smith, "Estimating
// the plucking point on a guitar string", DAFx 2000, and "Extracting the
// fingering and the plucking points on a guitar string from a recording",
// WASPAA 2001: normal playing 12-14 cm from the bridge, the
// 0.13 m default (kClassicalGuitarRoundPluckFromBridgeM), kept. Pluck
// force: no change. Timing and loudness scatter as the cellist (VERIFY
// for guitar).
// Step 18 (guitar scoreboard against the Iowa MIS guitar) checked this
// profile: the change was to the instrument (Woodhouse 2004 statistical
// body modes, guitar.h classicalGuitarBR2StatisticalBodyModes), not to a
// habit. Pluck point, force, finger damping and feel are unchanged
// (test_player_profile.cpp "the guitarist was checked at step 18").
[[nodiscard]] inline PluckedPlayerProfile guitaristPlayerProfile() {
  PluckedPlayerProfile p;
  p.name = "classical guitarist (Traube pluck point)";
  p.pluckFromBridgeM = kClassicalGuitarRoundPluckFromBridgeM;
  p.feel.timingCv = 0.03;   // VERIFY for guitar (pianists' figure)
  p.feel.timingSdMaxSec = player_detail::kEnsembleTimingSdMaxSec;
  p.feel.velocityCv = 0.05; // VERIFY
  p.feel.seed = 0x6017A2;
  return p;
}

// A jazz / orchestral bassist playing pizzicato. Rhythm-section quarter
// notes (walking bass with drums) had onset CV 0.074 in recorded jazz
// trios (Hofmann, Wesolowski & Goebl, "The tight-interlocked rhythm
// section", PLoS ONE 12, e0186520, 2017); that figure includes expressive
// tempo change, so the scatter here takes the isochronous-melody lower
// bound, 0.03 (Goebl & Palmer 2008), VERIFY. Pluck point: pizzicato over
// the end of the fingerboard, the 0.30 m default (kDoubleBassPizzFromBridgeM;
// teaching practice, VERIFY), kept.
[[nodiscard]] inline PluckedPlayerProfile bassistPlayerProfile() {
  PluckedPlayerProfile p;
  p.name = "bassist pizzicato (Hofmann 2017 rhythm section)";
  p.pluckFromBridgeM = kDoubleBassPizzFromBridgeM;
  p.feel.timingCv = 0.03;
  p.feel.timingSdMaxSec = player_detail::kEnsembleTimingSdMaxSec;
  p.feel.velocityCv = 0.05; // VERIFY
  p.feel.seed = 0xBA55;
  return p;
}

// A harpist. J. Chadefaux, J.-L. Le Carrou, B. Fabre, L. Daudet,
// "Experimentally based description of harp plucking", JASA 131, 844
// (2012), Fig. 7, nine harpists, 290 plucks: the string is plucked
// between about 1/3 and 2/5 of its length from the soundboard; 0.37, the
// middle of that band (rising slightly with octave, not modelled). The
// same paper finds each player highly reproducible (professionals more
// so), so the loudness scatter is the struck-family 0.05 (VERIFY).
[[nodiscard]] inline PluckedPlayerProfile harpistPlayerProfile() {
  PluckedPlayerProfile p;
  p.name = "harpist (Chadefaux 2012 pluck point)";
  p.pluckFraction = 0.37;
  p.feel.timingCv = 0.03;   // VERIFY for harp (pianists' figure)
  p.feel.timingSdMaxSec = player_detail::kEnsembleTimingSdMaxSec;
  p.feel.velocityCv = 0.05; // VERIFY
  p.feel.seed = 0x4A2;
  return p;
}
// A harpsichordist. The jack's plectrum sets the pluck point and, almost
// entirely, the loudness: key speed hardly changes the release (N. H.
// Fletcher & T. D. Rossing, The Physics of Musical Instruments, 2nd ed.,
// sec. 11.10, VERIFY the section number), so the player's habits are
// timing only: no loudness scatter and the pluck point stays the
// catalogue's. Timing scatter: Goebl & Palmer 2008 lower bound (VERIFY for
// harpsichord, whose players use timing for accent).
[[nodiscard]] inline PluckedPlayerProfile harpsichordistPlayerProfile() {
  PluckedPlayerProfile p;
  p.name = "harpsichordist (timing only)";
  p.feel.timingCv = 0.03;   // VERIFY
  p.feel.timingSdMaxSec = player_detail::kEnsembleTimingSdMaxSec;
  p.feel.seed = 0x4C0;
  return p;
}
// An electric guitarist with a pick. M. Zollner, Physik der Elektrogitarre
// (GITEC), ch. 1.5 (English ed. "Physics of the Electric Guitar", "Basics
// of the vibration of strings", sec. 1.5.1): "usually, the location of
// picking is about 6-10 cm from the bridge"; 0.08 m, the middle. Timing
// and loudness scatter as the classical guitarist (VERIFY).
[[nodiscard]] inline PluckedPlayerProfile electricGuitaristPlayerProfile() {
  PluckedPlayerProfile p;
  p.name = "electric guitarist (Zollner pick point)";
  p.pluckFromBridgeM = 0.08;
  p.feel.timingCv = 0.03;   // VERIFY
  p.feel.timingSdMaxSec = player_detail::kEnsembleTimingSdMaxSec;
  p.feel.velocityCv = 0.05; // VERIFY
  p.feel.seed = 0xE1E;
  return p;
}

// --- Struck --------------------------------------------------------------------
struct StruckPlayerProfile {
  const char* name = "default";
  PlayerFeel feel;
  // Piano only: the hammer reaches the string later for a softer key
  // stroke (pianoActionDelaySec), so a louder voice in a chord sounds
  // first: Goebl's "velocity artifact" melody lead. false = every hammer
  // on the written onset.
  bool pianoKeyAction = false;
  // Piano only: map the played velocity to hammer speed continuously
  // (interpolating pianoHammerSpeed's six steps) instead of rounding to a
  // step, so the loudness scatter is heard. false = the step mapping.
  bool continuousHammerSpeed = false;
};

// Finger-key contact to hammer-string contact on a grand action. A. Askenfelt
// & E. V. Jansson, "From touch to string vibrations. II", JASA 89, 2383
// (1991) and Askenfelt 1990, as summarised by W. Goebl, JASA 110, 563
// (2001), sec. I.B: key travel to key bottom 160 ms at 1 m/s final hammer
// velocity and 25 ms at 5 m/s; the hammer meets the string 12 ms before key
// bottom at 1 m/s and 3 ms after it at 5 m/s. So 148 ms at 1 m/s and 28 ms
// at 5 m/s; between (and beyond) them a power law through the two points
// (VERIFY: only the two end points are measured).
inline constexpr double kPianoActionSecAt1Mps = 0.160 - 0.012;
inline constexpr double kPianoActionSecAt5Mps = 0.025 + 0.003;
[[nodiscard]] inline double pianoActionDelaySec(double hammerSpeedMps) {
  const double b = std::log(kPianoActionSecAt1Mps / kPianoActionSecAt5Mps) / std::log(5.0);
  return kPianoActionSecAt1Mps * std::pow(std::max(hammerSpeedMps, 0.1), -b);
}
// pianoHammerSpeed's six steps read continuously: velocity 0-127 -> step
// 0-5, linear between the table's speeds (the table is Askenfelt &
// Jansson's range, piano_voice.h).
[[nodiscard]] inline double pianoHammerSpeedContinuous(double velocity) {
  const double s = std::clamp(velocity * 5.0 / 127.0, 0.0, 5.0);
  const int i = std::min(4, static_cast<int>(s));
  const double a = pianoHammerSpeed(i), b = pianoHammerSpeed(i + 1);
  return a + (b - a) * (s - i);
}
// The harness's hammer speed for a played note under this profile.
[[nodiscard]] inline double pianoSpeedFor(const StruckPlayerProfile& p, double velocity) {
  if (p.continuousHammerSpeed) return pianoHammerSpeedContinuous(velocity);
  return pianoHammerSpeed(static_cast<int>(std::lround(velocity * 5.0 / 127.0)));
}
// A piano part as played: the feel, then (pianoKeyAction) each hammer
// lands pianoActionDelaySec(speed) after the finger, less the part's mean
// delay: the player times the average stroke to the beat (UNSOURCED: an
// assumption that keeps the part aligned with the band), so a part at one
// dynamic stays on the grid and the louder voice of a chord leads.
[[nodiscard]] inline std::vector<PartNote> performPiano(const std::vector<PartNote>& written,
                                                         const StruckPlayerProfile& p) {
  std::vector<PartNote> out = performNotes(written, p.feel);
  if (!p.pianoKeyAction || out.empty()) return out;
  std::vector<double> d;
  double ref = 0.0;
  for (const PartNote& n : out) { d.push_back(pianoActionDelaySec(pianoSpeedFor(p, n.velocity))); ref += d.back(); }
  ref /= double(out.size());
  for (std::size_t i = 0; i < out.size(); ++i) out[i].startSec = std::max(0.0, out[i].startSec + d[i] - ref);
  return out;
}

// A concert pianist. Melody lead from the key action (Goebl 2001, 22
// skilled pianists: the hammer-level lead of about 30 ms vanishes at the
// finger-key level, so the fingers strike together and the action makes
// the lead). Timing: Goebl & Palmer, Exp. Brain Res. 186, 471 (2008),
// skilled pianists in an isochronous melody, IOI CV 0.03-0.08 with tempo
// (quoted by Hofmann et al. 2017); the lower bound, a careful accompanist.
// Loudness scatter 0.05 VERIFY (Goebl & Palmer 2013 Fig. 3C, read from the
// figure's order of magnitude, not a tabulated value).
[[nodiscard]] inline StruckPlayerProfile pianistPlayerProfile() {
  StruckPlayerProfile p;
  p.name = "pianist (Goebl 2001 key action, Goebl & Palmer timing)";
  p.pianoKeyAction = true;
  p.continuousHammerSpeed = true;
  p.feel.timingCv = 0.03;
  p.feel.timingSdMaxSec = player_detail::kEnsembleTimingSdMaxSec;
  p.feel.velocityCv = 0.05;  // VERIFY
  p.feel.seed = 0x9A40;
  return p;
}

// A snare drummer. S. Fujii et al., "Synchronization error of drum kit
// playing with a metronome at different tempi by professional drummers",
// Music Perception 28, 491 (2011), fifteen professionals: the left hand
// (snare) preceded the metronome by about 10 ms at 60 and 120 bpm. Timing
// scatter: 0.03 of the IOI (Goebl & Palmer 2008 lower bound; Hofmann et al.
// 2017 measured 0.068-0.087 for jazz rhythm sections including tempo
// drift; VERIFY for an isochronous snare part). Loudness scatter 0.05
// VERIFY (S. Dahl, Acta Acustica 90, 762, 2004: each player's own
// strategy is consistent; the accents' factor-2 striking velocity is
// already written into the part's velocities).
[[nodiscard]] inline StruckPlayerProfile snareDrummerPlayerProfile() {
  StruckPlayerProfile p;
  p.name = "snare drummer (Fujii 2011 anticipation)";
  p.feel.timingOffsetSec = -0.010;
  p.feel.timingCv = 0.03;
  p.feel.timingSdMaxSec = player_detail::kEnsembleTimingSdMaxSec;
  p.feel.velocityCv = 0.05;  // VERIFY
  p.feel.seed = 0x5DA2E;
  return p;
}

// Keyboard and mallet percussionists (glockenspiel, celesta, tubular
// bells): the struck-family scatter (Goebl & Palmer 2008 timing; loudness
// VERIFY); no instrument-specific measurement (VERIFY: S. Dahl,
// "Striking movements", Acoust. Sci. Tech. 32, 2011, reviews players'
// consistent own strategies but gives no per-instrument scatter).
[[nodiscard]] inline StruckPlayerProfile malletPlayerProfile(const char* name, std::uint64_t seed) {
  StruckPlayerProfile p;
  p.name = name;
  p.feel.timingCv = 0.03;
  p.feel.timingSdMaxSec = player_detail::kEnsembleTimingSdMaxSec;
  p.feel.velocityCv = 0.05;  // VERIFY
  p.feel.seed = seed;
  return p;
}
[[nodiscard]] inline StruckPlayerProfile glockenspielPlayerProfile() { return malletPlayerProfile("glockenspiel (struck feel)", 0x610C); }
[[nodiscard]] inline StruckPlayerProfile celestaPlayerProfile() { return malletPlayerProfile("celesta (struck feel)", 0xCE1E); }
[[nodiscard]] inline StruckPlayerProfile tubularBellsPlayerProfile() { return malletPlayerProfile("tubular bells (struck feel)", 0x7B); }

// A handbell ringer. Each ringer rings two bells and swings each from the
// shoulder; the clapper speed follows the swing (kHandbellRingerSwingMps
// in handbell.h). A choir's onsets scatter more than one keyboard
// player's because every bell is a different ringer: the jazz rhythm
// section's between-player figure, CV 0.074 (Hofmann et al. 2017), is
// used (VERIFY for handbell choirs). Swing speed: as written.
[[nodiscard]] inline StruckPlayerProfile handbellRingerPlayerProfile() {
  StruckPlayerProfile p;
  p.name = "handbell choir (Hofmann 2017 between-player scatter, VERIFY)";
  p.feel.timingCv = 0.074;
  p.feel.timingSdMaxSec = player_detail::kEnsembleTimingSdMaxSec;
  p.feel.velocityCv = 0.05;  // VERIFY
  p.feel.seed = 0x4B;
  return p;
}

// --- Drum kit ------------------------------------------------------------------
// Which limb plays a kit note: the right hand rides the hi-hat or cymbal,
// the left hand plays the snare, the right foot the bass drum (Fujii et
// al. 2011's pattern). Toms are played by the hands.
enum class DrumLimb { RightHand = 0, LeftHand = 1, RightFoot = 2 };
struct KitNote {
  PartNote note;
  DrumLimb limb = DrumLimb::RightHand;
};
struct DrumKitPlayerProfile {
  const char* name = "default";
  PlayerFeel feel;
  // Each limb's constant lead (-) or lag (+), seconds, added after the feel.
  std::array<double, 3> limbOffsetSec{0.0, 0.0, 0.0};
};
// A kit part as played. The limbs are timed together: S. Fujii et al.
// (Music Perception 28, 491, 2011) found the limbs' synchronization errors
// significantly correlated ("each limb synchronized in relation to the
// other limbs"), so the feel runs on the whole kit at once and notes
// struck together share one deviation; then each limb's own lead is
// added. Default: unchanged.
[[nodiscard]] inline std::vector<KitNote> performDrumKit(const std::vector<KitNote>& written,
                                                         const DrumKitPlayerProfile& p) {
  std::vector<KitNote> out = written;
  const bool noOffset = p.limbOffsetSec[0] == 0.0 && p.limbOffsetSec[1] == 0.0 && p.limbOffsetSec[2] == 0.0;
  if (out.empty() || (p.feel.isDefault() && noOffset)) return out;
  std::vector<PartNote> notes;
  for (const KitNote& k : written) notes.push_back(k.note);
  const std::vector<PartNote> played = performNotes(notes, p.feel);
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i].note = played[i];
    out[i].note.startSec = std::max(0.0, played[i].startSec + p.limbOffsetSec[static_cast<std::size_t>(out[i].limb)]);
  }
  return out;
}
// A kit drummer. Fujii et al. 2011, fifteen professionals with a
// metronome at 60 and 120 bpm: right hand (hi-hat) mean error about 2 ms
// (taken as 0; the sign is not given in the abstract, VERIFY), left hand
// (snare) and right foot (bass drum) about 10 ms ahead. At 200 bpm the
// pattern changes (right hand 10 ms late, the others about 1 ms): not
// modelled, VERIFY above 120 bpm. Toms take the left hand's lead (VERIFY:
// not measured). Timing scatter 0.03 and loudness 0.05 as the snare
// drummer (VERIFY).
[[nodiscard]] inline DrumKitPlayerProfile drumKitPlayerProfile() {
  DrumKitPlayerProfile p;
  p.name = "kit drummer (Fujii 2011 limb leads)";
  p.limbOffsetSec = {0.0, -0.010, -0.010};
  p.feel.timingCv = 0.03;
  p.feel.timingSdMaxSec = player_detail::kEnsembleTimingSdMaxSec;
  p.feel.velocityCv = 0.05;  // VERIFY
  p.feel.seed = 0xD2;
  return p;
}

// --- Shaken --------------------------------------------------------------------
struct ShakenPlayerProfile {
  const char* name = "default";
  // Strike timing and strike speed scatter (timingCv on the interval
  // between strikes, velocityCv on the hand speed); velocityScale scales
  // every strike speed.
  PlayerFeel feel;
  // Shake stroke amplitude multiplier. 1 = the timeline's.
  double shakeAmplitudeScale = 1.0;
};

// The sleigh-bell timeline as this player plays it: strikes moved and
// their hand speeds scattered; shakes scaled. Default: unchanged.
[[nodiscard]] inline std::vector<SleighBellEvent> performSleighBells(const std::vector<SleighBellEvent>& written,
                                                                    const ShakenPlayerProfile& p) {
  std::vector<SleighBellEvent> out = written;
  if (p.feel.isDefault() && p.shakeAmplitudeScale == 1.0) return out;
  std::vector<PartNote> strikes;
  for (const SleighBellEvent& e : written)
    if (e.kind == SleighBellEvent::Kind::Strike) strikes.push_back({e.timeSec, 0.0, 0, 64.0});
  const std::vector<PartNote> played = performNotes(strikes, p.feel);
  std::size_t k = 0;
  for (SleighBellEvent& e : out) {
    if (e.kind == SleighBellEvent::Kind::Strike) {
      e.timeSec = played[k].startSec;
      e.strength *= played[k].velocity / 64.0;
      ++k;
    } else {
      e.strength *= p.shakeAmplitudeScale;
      if (e.endStrength >= 0.0) e.endStrength *= p.shakeAmplitudeScale;
    }
  }
  return out;
}

// A sleigh-bell player. The shake rate is already the measured 3.36 Hz of
// the reference (kSleighBellShakeHz); the strikes take the struck-family
// timing scatter (Goebl & Palmer 2008, VERIFY for hand-held percussion)
// and a 0.05 hand-speed scatter (VERIFY).
[[nodiscard]] inline ShakenPlayerProfile sleighBellPlayerProfile() {
  ShakenPlayerProfile p;
  p.name = "sleigh bells (struck feel on the strikes)";
  p.feel.timingCv = 0.03;
  p.feel.timingSdMaxSec = player_detail::kEnsembleTimingSdMaxSec;
  p.feel.velocityCv = 0.05;  // VERIFY
  p.feel.seed = 0x51E1;
  return p;
}

}  // namespace mmathen
