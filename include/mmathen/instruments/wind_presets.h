#pragma once

// Wind presets that define the instruments of the wind excerpts and songs:
// the Bach 37 / Bach 42 bells, the phrase trumpet and trombone bores and
// lips, each instrument's fingering (sounding-length) table, the breath and
// turbulence defaults, and the phrase lead-in. Moved verbatim from
// tests/test_wind_tone.cpp so that a demo or test only supplies notes.
// Values are unchanged; their sources are the comments carried with them.
// Anything without a source is marked UNSOURCED.

#include <cmath>
#include <cstddef>
#include <vector>

#include "../math/constants.h"
#include "bore_valve.h"
#include "trombone.h"
#include "trumpet.h"

namespace mmathen {

// One score note: concert pitch (0 = rest), seconds, articulation, dynamic.
struct ScoreNote {
  const char* name;
  double hertz;
  double seconds;
  bool tongued;
  bool vibrato;
  const char* dynamic;
};

// Phrase-player default: a 0.56 s rest before the first note so the breath
// and the player airway (player_airway.h) settle before the tone starts.
// 0.56 s is two beats of the 0.28 s Saints grid. UNSOURCED: chosen by ear
// against slow first notes, not a measured player lead-in.
inline constexpr double kPhraseLeadInSeconds = 0.560;

template <typename Song>
std::vector<ScoreNote> withLeadIn(const Song& song, const char* dynamic,
                                  double seconds = kPhraseLeadInSeconds) {
  std::vector<ScoreNote> out;
  out.push_back(ScoreNote{"rest", 0.00, seconds, false, false, dynamic});
  for (const ScoreNote& note : song) out.push_back(note);
  return out;
}

// Breath and turbulence defaults (ValveBlow::reedTurbulence,
// ValveBlow::lockedBreathFraction).
// Clarinet: locked breath 0.013, University of Iowa MIS Bb clarinet ff
// (measurement reference only). Turbulence 0.25 TUNED (was 0.40
// UNSOURCED): with clarinetBoreLosses the transcribed excerpt's
// inter-harmonic floor read -63.6 / -67.0 / -70.4 / -75.5 dB at
// 0.40 / 0.25 / 0.15 / 0.05 against -68.0 on the recording. The suite's
// held tone read floor -68.8 / -68.2 / -66.7 / -66.4 / -65.1 dB at
// 0.22 / 0.25 / 0.26 / 0.28 / 0.32 (bound -66.8) and soft growth
// 629 / 489 / 489 / 489 / 489 dB/s (bound 500); 0.25 meets both.
inline constexpr double kClarinetReedTurbulence = 0.25;
inline constexpr double kClarinetLockedBreathFraction = 0.013;
// Alto saxophone: locked breath 0.0176, Iowa MIS alto sax NoVib ff.
// Turbulence: see below (Wolfe, UNSW, prints no level).
// Clarinet bore losses: reflection corner 4500 Hz TUNED, with the wall
// fitted to Kirchhoff at the sounding fundamental
// (ValveBlow::wallFitFundamental). Li, Almeida, Smith and Wolfe (JASA 139,
// 825, 2016) measured tongue-stopped clarinet decays of about 400 dB/s
// (C4 340, G4 400, C5 380, G5 520), which they matched to the bandwidths of
// the bore resonances, i.e. bore loss. Winds pro4, slope of the last 30 ms
// before each tongued onset on the transcribed clarinet excerpt, median of
// 10 closures (G4 / C5 / G5):
//   lone wall pole, corner 1500: -262 / -600..-782 / -2389..-2896, median -1245
//   lone wall pole, corner 3000: -176 / -245..-262 / -742..-955, median -401
//   fitted wall, corner 1500: -377 / -801..-1037 / -2551..-3112, median -1455
//   fitted wall, corner 3000: -288 / -402..-490 / -904..-1141, median -587
//   fitted wall, corner 4500: -257 / -338..-392 / -560..-736, median -435
// A lone corner fast enough for G5 is too slow below C5 (the pole's loss
// grows as f^2, Kirchhoff's as sqrt(f)); the fitted wall carries the low
// notes. The same 4500 Hz run read centroid 914 Hz (real 974, 1500 Hz 673),
// 1-3 kHz -7.9 dB (-5.4), 3-8 kHz -15.1 (-15.1), H2/H3/H4 -11.1/-13.3/-13.8
// (-11.1/-8.7/-14.8).
inline constexpr double kClarinetReflectionCutoffHz = 4500.0;
inline ValveBlow clarinetBoreLosses(ValveBlow blow) {
  blow.reflectionCutoffHz = kClarinetReflectionCutoffHz;
  blow.wallFitFundamental = true;
  return blow;
}
inline constexpr double kSaxophoneReedTurbulence = 0.30;
// Alto saxophone radiation (ValveBlow::spectrumBreakHz,
// ValveBlow::radiationHz), TUNED. Benade and Lutgen's envelope (JASA 83,
// 1900, 1988; alto fb 837 Hz) is the external spectrum of the whole
// instrument. Filtering this cone's radiated pressure with it counts the
// bore and radiation roll-off twice: the transcribed excerpt read 3-8 kHz
// -41.7 dB against -14.1 recorded. Winds pro5 sweep, centroid (Hz) /
// 1-3 kHz / 3-8 kHz / 8-16 kHz (dB) / H2 / H3 / H4 / floor, recording
// 960 / -5.6 / -14.1 / -32.4 / -2.5 / -10.4 / -11.8 / -63.8:
//   break 837 (old):       537 / -16.8 / -41.7 / -41.0 / -6.0 / -19.3 / -25.0 / -72.5
//   break 1800:            842 /  -5.3 / -31.1 / -40.1 / -2.5 /  -6.6 /  -5.9 / -68.5
//   break off, corner 1500 (the reflection corner): 650 / -8.9 / -22.8 / -37.9
//   break off, corner 3000 / 4000 / 5000 / 6000 / 8000 Hz:
//     3-8 kHz -19.1 / -17.9 / -17.2 / -16.7 / -16.1,
//     8-16 kHz -34.3 / -32.5 / -31.1 / -30.0 / -28.5
// 5000 Hz, break off. Turbulence 0.30 TUNED (was 0.55 UNSOURCED): excerpt
// floor -60.8 at 0.55, -63.6 at 0.35. The suite's held tone needs the
// noise to seed the reed: 0.15 / 0.20 never reach -30 dB in 0.8 s, 0.25
// grows at 51.8 dB/s (bound 50), 0.30 at 60.3 with floor -69.0 dB.
inline constexpr double kSaxophoneSpectrumBreakHz = 0.0;
inline constexpr double kSaxophoneRadiationHz = 5000.0;
inline constexpr double kSaxophoneLockedBreathFraction = 0.0176;
// Flute: locked breath 0.063 = 0.56 less 19 dB was set against the Iowa MIS
// flute A4 ff (-66.8 dB median inter-harmonic bin); on the pro phrase it
// read -62.0 against the CC excerpt's -64.6 (winds pro6: 0.050 / 0.047 /
// 0.040 / 0.030 read -64.0 / -64.5 / -65.9 / -68.4). 0.047 TUNED to the
// excerpt, between the two references. Jet turbulence 0.56 UNSOURCED.
inline constexpr double kFluteJetTurbulence = 0.56;
inline constexpr double kFluteLockedBreathFraction = 0.047;
// Flute jet aim (ValveBlow::jetAimMeters, as a fraction of the lip
// thickness) and reflection corner, both TUNED. Fletcher and Douglas
// (JASA 68, 767, 1980): a jet centred on the edge suppresses the even
// harmonics and an offset raises them, so the offset sets H2. Winds pro4
// sweep on the transcribed flute excerpt, H2/H3/H4 re H1 (dB), 1-3 kHz and
// 3-8 kHz band (dB), against the recording -6.0/-12.3/-20.0, -9.6, -30.3:
//   aim 0.40, 1800 Hz (old):  -13.6/-24.0/-23.3, -13.5, -29.6
//   aim 0.80, 1800 Hz:         -8.3/-19.3/-31.3,  -9.2, -30.9
//   aim 0.40, 4500 Hz:        -14.6/-13.2/-19.6, -11.9, -24.3
//   aim 0.80, 3000 Hz:         -7.9/-21.7/-24.1,  -8.7, -27.7
//   aim 1.00, 3000 Hz:         -7.0/-16.0/-24.6,  -7.3, -27.2
//   aim 1.20, 3000 Hz:         -7.5/-15.1/-23.4,  -6.6, -29.3
//   aim 1.00, 2500 / 3500 Hz:  -7.5/-16.3/-24.6 / -6.5/-17.2/-23.3
// Aim 1.00 with 3000 Hz has the smallest summed error; pitch median 2.5 c
// on every run. The Kirchhoff wall fit (wallFitFundamental) on the flute
// moved H3 by +-5 dB between neighbouring corners and is left off.
inline constexpr double kFluteJetAimFraction = 1.00;
inline constexpr double kFluteReflectionCutoffHz = 3000.0;
// Soft flute voicing by score dynamic. Fletcher and Douglas (JASA 68,
// 1980): the jet offset sets the even harmonics; Fletcher (JASA 57, 233,
// 1975): soft playing lowers the blowing pressure (about a factor 2 from
// pp to ff at one pitch, VERIFY) while the player shortens the jet so the
// convection time stays tuned (the harness keeps the jet delay of the
// nominal pressure). Aim TUNED to VSCO 2 CE Flute susNV soft (v1) A4-A5:
// H2 re H1 median -23 dB, H3 -26 dB; aim 1.0 read -6/-16, 0.3 read
// -17/-19, 0.1 read -27/-17, 0.15 -24/-15. Reflection corner: the jet law
// here is homogeneous in blowing pressure, so the weaker jet-edge
// saturation of soft playing (harmonic content rising with pressure,
// Fletcher and Rossing ch. 16) does not appear by itself; a lower corner
// stands in for it (TUNED stand-in, not the mechanism): aim 0.15 with
// 3000 / 1500 / 1000 Hz read H3 -15 / -21 / -21 dB, pitch +0.2 c. Locked breath raised above the v1 median
// inter-harmonic level (-37 dB; 0.047 read -38, 0.12 read -30) on
// request for a breathy soft tone: TUNED, not measured.
struct FluteVoicing {
  double pressureScale;
  double aimFraction;
  double lockedBreathFraction;
  double reflectionCutoffHz;
};
[[nodiscard]] inline FluteVoicing fluteVoicingFor(const char* dynamic) {
  const auto is = [&](const char* d) {
    if (dynamic == nullptr) return false;
    const char* a = dynamic; const char* b = d;
    while (*a && *a == *b) { ++a; ++b; }
    return *a == 0 && *b == 0;
  };
  if (is("pp")) return {0.55, 0.10, 0.14, 1500.0};  // pressure VERIFY, rest TUNED
  if (is("p")) return {0.70, 0.15, 0.10, 1500.0};   // pressure VERIFY, rest TUNED
  if (is("mp")) return {0.85, 0.60, kFluteLockedBreathFraction, 2250.0};  // UNSOURCED midpoints
  return {1.0, kFluteJetAimFraction, kFluteLockedBreathFraction, kFluteReflectionCutoffHz};
}
// Lip jet fraction. Fletcher and Rossing, chap. 13, call the flow
// turbulent and print no ratio. 0.20 UNSOURCED.
inline constexpr double kTrumpetReedTurbulence = 0.20;
inline constexpr double kTromboneReedTurbulence = 0.20;

// Phrase trumpet: 1.70 m sounding cone (retuned per note), 1.09e-4 m^2
// throat, lip mass 3.0e-5 kg tuned to 0.75 c / 1.45 m (the second-register
// recipe at 1.45 m). Bore, damping and lip numbers are arguments, not a
// measured trumpet (UNSOURCED); see the suite comments.
inline constexpr double kPhraseTrumpetLengthMeters = 1.70;
inline constexpr double kTrumpetLipTuningLengthMeters = 1.45;
inline constexpr double kTrumpetLipMassKg = 3.0e-5;
inline TrumpetSpec phraseTrumpetSpec(double speed, double airPa, double tempK,
                                     double lengthMeters = kPhraseTrumpetLengthMeters) {
  const double mass = kTrumpetLipMassKg;
  const double lip0 = 0.75 * speed / kTrumpetLipTuningLengthMeters;
  const double omega = constants::twoPi * lip0;
  return TrumpetSpec{lengthMeters, 1.09e-4, airPa, tempK, 0.0, 8.0e-7, 1.5e-5, 1.2e-2, mass,
                     omega * omega * mass, 1.2e-5, 0.001};
}

// Single-note "blown winds" trumpet: the phrase spec on a 1.45 m cone, the
// length whose second root sits near concert Bb3 (an argument, UNSOURCED).
inline constexpr double kBlownTrumpetLengthMeters = 1.45;
inline TrumpetSpec blownTrumpetSpec(double speed, double airPa, double tempK) {
  const double mass = kTrumpetLipMassKg;
  const double lip0 = 0.75 * speed / kBlownTrumpetLengthMeters;
  const double omega = constants::twoPi * lip0;
  return TrumpetSpec{kBlownTrumpetLengthMeters, 1.09e-4, airPa, tempK, 0.0, 8.0e-7, 1.5e-5, 1.2e-2,
                     mass, omega * omega * mass, 1.2e-5, 0.001};
}

// Flute bore: 2.8e-4 m^2 cross-section (argument, UNSOURCED). Open-end
// correction 0.61 radii (Levine and Schwinger, unflanged). Sounding length
// is the open-open half wave c/(2f) less that correction.
inline constexpr double kFluteBoreAreaM2 = 2.8e-4;
inline double fluteEndCorrectionMeters(double areaM2 = kFluteBoreAreaM2) {
  const double radius = std::sqrt(areaM2 / constants::pi);
  return 0.61 * radius;
}
inline double fluteSoundingLengthMeters(double speed, double frequency, double endCorrectionMeters) {
  return speed / (2.0 * frequency) - endCorrectionMeters;
}

// Phrase trombone: 3.05 m, 3.0e-4 m^2 cone, lip mass 6.0e-5 kg tuned to
// 0.75 c / 2.75 m. Arguments, not a measured trombone (UNSOURCED).
inline constexpr double kTromboneLipTuningLengthMeters = 2.75;
inline constexpr double kTromboneLipMassKg = 6.0e-5;
// lengthMeters sizes the tube buffer, as the trumpet's: the longest
// sounding length a phrase asks for. Winds pro1: E3-B2 (Rimsky-Korsakov,
// the trombone reference) need 3.3-4.6 m on the mode-3 rows and the fixed
// 3.05 m tube refused them, so that phrase rendered nothing.
inline constexpr double kPhraseTromboneLengthMeters = 3.05;
inline TromboneSpec phraseTromboneSpec(double speed, double airPa, double tempK,
                                       double lengthMeters = kPhraseTromboneLengthMeters) {
  const double mass = kTromboneLipMassKg;
  const double lip0 = 0.75 * speed / kTromboneLipTuningLengthMeters;
  const double omega = constants::twoPi * lip0;
  return TromboneSpec{lengthMeters, 0.0, 3.0e-4, airPa, tempK, 0.0, 1.0e-6,
                      2.5e-5, 1.5e-2, mass, omega * omega * mass,
                      2.0e-5, 0.0005};
}
// Single-note "blown winds" trombone: 1.65 m closed + 2 x 0.55 m slide =
// 2.75 m, the second-register recipe near concert Bb2 (arguments,
// UNSOURCED). Lip at 0.75 c / (closed + 2 slide).
inline constexpr double kBlownTromboneClosedMeters = 1.65;
inline constexpr double kBlownTromboneSlideMeters = 0.55;
inline TromboneSpec blownTromboneSpec(double speed, double airPa, double tempK) {
  const double length = kBlownTromboneClosedMeters + 2.0 * kBlownTromboneSlideMeters;
  const double mass = kTromboneLipMassKg;
  const double lip = 0.75 * speed / length;
  const double omega = constants::twoPi * lip;
  return TromboneSpec{kBlownTromboneClosedMeters, kBlownTromboneSlideMeters, 3.0e-4, airPa, tempK,
                      0.0, 1.0e-6, 2.5e-5, 1.5e-2, mass, omega * omega * mass, 2.0e-5, 0.0005};
}

// Bach Stradivarius 42 tenor trombone. Conn-Selmer prints an 8-1/2 in
// one-piece hand-hammered bell and a .547 in large bore:
// https://connselmer.com/instruments/trombones/bach-stradivarius-tenor-trombone-in-bb-42
// The international inch is 0.0254 m. The radius is half that diameter.
// The .547 in bore is not substituted for the excerpt throat.
inline constexpr double kBach42BellRadiusMeters = 0.5 * 8.5 * 0.0254;

// Bach 180S37. The maker's page names a #37 bell and a .459 in bore and
// does not print a diameter:
// https://bachbrass.com/trumpets/bach-stradivarius-trumpet-in-bb-180s37
// The Vincent Bach catalog prints 4 13/16 in (122.24 mm) for Bb trumpet
// bells and says every Bb trumpet bell has that diameter; the bell number
// is the mandrel, not the diameter. 4.8 in is that figure rounded and is
// not the value used here. The .459 in bore is not substituted for the
// excerpt throat.
inline constexpr double kBach37BellRadiusMeters = 0.5 * (4.0 + 13.0 / 16.0) * 0.0254;

// Bach 42 large bore, the same Conn-Selmer page. Kirchhoff's ring uses
// this radius. The cone area stays 3.0e-4 mÂ².
inline constexpr double kBach42BoreRadiusMeters = 0.5 * 0.547 * 0.0254;

// Scott, Measurement and Synthesis of the Trombone (SFU M.Sc., 2012),
// Table 2.2. a(x) = b (x + x0)^{âˆ’Î³}, x from the mouth. The table prints
// a 0.108 m mouth; the Bach 42 mouth is the Levineâ€“Schwinger termination.
inline void applyScottBell(ValveBlow& blow) {
  blow.radiusMeters = kBach42BellRadiusMeters;
  blow.bellLengthMeters = 0.502;
  blow.bellFlare = 0.7;
  blow.bellCoefficient = 0.0063;
  blow.bellMouthOffsetMeters = 0.0174;
}

// Thibault and Chabassier, JASA 150 (2021): a natural trumpet, cylinder
// radius 6 mm, then a Bessel horn of exponent 0.7 from 0.716 m to
// 1.335 m. Their mouth is 60 mm. b and x0 are the pair that meets the
// 6 mm throat and the Bach #37 mouth. They are not solved onto a Q.
// Trumpet radiation: the bell radiates the wave it transmits
// (ValveBlow::bellTransmission), and the radiation corner is 3000 Hz,
// TUNED. Winds pro5 on the transcribed trumpet excerpt, centroid (Hz) /
// 1-3 kHz / 3-8 kHz / 8-16 kHz (dB) / H2 / H3 re H1, recording
// 1436 / -1.4 / -15.8 / -46.1 / +5.3 / +6.0:
//   throat flow, 10 kHz corner (old): 494 / -17.1 / -29.6 / -31.6 / -12.6 / -20.8
//   transmitted, 10 kHz:  1723 / -5.5 / -9.1 / -17.5 / -1.4 / -7.4
//   transmitted, 2000 / 3000 / 4000 / 6000 Hz: centroid 1005 / 1160 / 1282 /
//     1467, 3-8 kHz -14.0 / -12.1 / -11.0 / -9.9, 8-16 kHz -27.5 / -24.7 /
//     -22.8 / -20.2, H2 -1.9 / -1.6 / -1.5 / -1.4
// Pitch median 2.5 c on all transmitted runs (7.5 c on the old). Still
// open: H2-H3 sit 7-14 dB under the recording and 8-16 kHz 21 dB over;
// the one-pole corner falls 6 dB/oct where Luce and Clark (JASA 42, 1967)
// measured 15-25 dB/oct above about 1 kHz.
inline constexpr double kTrumpetRadiationHz = 3000.0;
// Burgers steepening (ConeValveVoice::setBoreBurgers): L_eff as a
// fraction of the sounding length, the Myers brassiness B (Myers, Pyle,
// Gilbert, Campbell, Chick and Logie, JASA 131, 678-688, 2012: Bb
// trumpets 0.58-0.76). With the trip's wall loss spread over the march,
// the top of that range scored best of the in-range values on the Lick
// Trumpet excerpt (winds_pro1/tscore.py: 0.58-0.60 40.5, 0.76 36.8; the
// lossless line needed 0.45, outside the range). Value: Myers et al.
inline constexpr double kTrumpetBurgersFraction = 0.76;
// Lip quality factor. The fixed 0.001 N s/m of the phrase spec is Q ~ 85
// at A4: the lip rang at its own resonance for 50-110 ms before the bore
// took it, so every note started 70-320 c flat (winds_pro1 h/tp ptrack.py,
// YIN at 1 c: Lick excerpt attacks G4 -314, C5 -165, F4 an octave low;
// Jingle Bells 85 of 98 attacks off by > 50 c). Measured lips: human
// Q about 1.2-1.8, artificial 8-10 (Newton, Campbell and Gilbert, JASA
// 123, EL14-EL20, 2008). Q 1.5 and 3 lost the regime (notes an octave
// or more off); 4 is the lowest that held every note. TUNED to that,
// with the 0.80 lip ratio: Jingle Bells attacks off by > 50 c 85 -> 8
// of 98, settle (within 25 c for 30 ms) median 65 -> 23 ms.
inline constexpr double kTrumpetLipQualityFactor = 4.0;
inline void applyBach37Bell(ValveBlow& blow) {
  constexpr double kLength = 1.335 - 0.716;
  constexpr double kGamma = 0.7;
  constexpr double kThroat = 0.006;
  const double mouth = kBach37BellRadiusMeters;
  const double ratio = std::pow(mouth / kThroat, 1.0 / kGamma);
  const double offset = kLength / (ratio - 1.0);
  blow.radiusMeters = mouth;
  blow.bellLengthMeters = kLength;
  blow.bellFlare = kGamma;
  blow.bellCoefficient = mouth * std::pow(offset, kGamma);
  blow.bellMouthOffsetMeters = offset;
  blow.bellTransmission = true;
  // The trumpet default steepens with the Burgers line (it scored best of
  // the old four-sample advance, that advance with the bell return aligned,
  // and Burgers, on the Lick Trumpet excerpt; winds_pro1/tscore.py).
  blow.burgersFraction = kTrumpetBurgersFraction;
  blow.lipQualityFactor = kTrumpetLipQualityFactor;
  blow.radiationHz = kTrumpetRadiationHz;
}
inline double clarinetSoundingLengthMeters(double frequency) {
    struct Length { double hertz; double meters; };
    // Shortened from c/(4f) until the autocorrelation matched. Integer
    // lag is about 15 cents at these pitches; 25 cents is the grade.
    const Length table[] = {
        // Winds pro4 (clarinetBoreLosses): F#4 read +21 c and C5 -29 on a
        // 3000 Hz lone-pole pass; those rows are held length x 2^(c/1200).
        // F4 moved +62 / -59 / -31 c over 0.219 / 0.2269 / 0.2229 m across
        // passes and is back at 0.219.
        // B4 (Silent Night alto, interpolated) read +32 c: interpolated
        // length x 2^(32/1200).
        {220.00, 0.371149}, {293.66, 0.270847}, {329.63, 0.23842},
        {349.23, 0.219000}, {369.99, 0.2160}, {392.00, 0.201376}, {440.00, 0.177866},
        // Winds pro1 rows (TUNED, same method): the held A4 product read
        // flat 32-212 cents above A4 and F#4 +28 c on the transcribed
        // Serolillo phrase. Each row is the held length x 2^(cents/1200),
        // one pass.
        {493.88, 0.15767}, {523.25, 0.14437}, {587.33, 0.12491}, {622.25, 0.11790}, {659.26, 0.11122},
        {698.46, 0.10436}, {783.99, 0.09144}, {932.33, 0.07118},  // A#5: second pass, -72 c
    };
    for (const Length& row : table)
      if (std::fabs(row.hertz - frequency) < 0.05) return row.meters;
    // Unlisted pitch: the length-frequency product interpolated in log f
    // between the measured rows, held at the end rows outside them.
    const std::size_t n = sizeof(table) / sizeof(table[0]);
    if (frequency <= table[0].hertz) return table[0].meters * table[0].hertz / frequency;
    for (std::size_t i = 1; i < n; ++i) {
      if (frequency <= table[i].hertz) {
        const double t = std::log(frequency / table[i - 1].hertz) /
                         std::log(table[i].hertz / table[i - 1].hertz);
        const double product = (1.0 - t) * table[i - 1].meters * table[i - 1].hertz +
                               t * table[i].meters * table[i].hertz;
        return product / frequency;
      }
    }
    return table[n - 1].meters * table[n - 1].hertz / frequency;
  }

// Saxophone cone lengths tuned by rendering the Old Castle excerpt at
// 4200 Pa and reading the autocorrelation pitch; not published lengths.
// E4: 0.390 m read -28.9 cents, 0.380 m +8.0; 0.3822 m reads about 0.
inline double saxophoneSoundingLengthMeters(double frequency) {
      struct Length { double hertz; double meters; };
      const Length table[] = {
          {207.65, 0.670}, {277.18, 0.471}, {311.13, 0.410}, {329.63, 0.3822},
          {369.99, 0.3315}, {415.30, 0.288}, {466.16, 0.2486}, {493.88, 0.230},
          // C5, winds pro1 (TUNED, same method): the held B4 product read -42 c.
          {523.25, 0.21187},
      };
      for (const Length& row : table)
        if (std::fabs(row.hertz - frequency) < 0.05) return row.meters;
      // Unlisted pitch: the length-frequency product interpolated in log f
      // between rows, held at the end rows (as the clarinet). Winds pro1:
      // the old fallback held the G#3 product (139 m/s) at every pitch,
      // 115-123 m/s in the rows above E4, and read 160-290 cents flat on
      // G4, A4, C5, F4.
      const std::size_t n = sizeof(table) / sizeof(table[0]);
      if (frequency <= table[0].hertz) return table[0].meters * table[0].hertz / frequency;
      for (std::size_t i = 1; i < n; ++i) {
        if (frequency <= table[i].hertz) {
          const double t = std::log(frequency / table[i - 1].hertz) /
                           std::log(table[i].hertz / table[i - 1].hertz);
          return ((1.0 - t) * table[i - 1].meters * table[i - 1].hertz +
                  t * table[i].meters * table[i].hertz) / frequency;
        }
      }
      return table[n - 1].meters * table[n - 1].hertz / frequency;
    }

struct BrassFinger {
  double hertz;
  double length;
  int mode;
  // Lip natural frequency over the cone partial for this row; zero takes
  // the mode's ratio (trumpetLipPartialRatio).
  double lipRatio = 0.0;
};

  inline constexpr BrassFinger kTromboneFingers[] = {
      // Lengths are the tubes the previous 3.0e-4 mÂ² bore played on pitch.
      // The throat is now the Bach 42 .547 in bore. The fingers tuned
      // for 1.0e-4 mÂ² sat flat on the wider bore.
      {466.16, 1.331, 4}, {440.00, 1.416, 4}, {392.00, 1.596, 4}, {349.23, 1.312, 3},
      {523.25, 1.181, 4}, {329.63, 1.394, 3}, {293.66, 1.579, 3}, {554.37, 1.732, 6},
      // D5 for the song passages: the Db5 row scaled, then lengthened by the
      // +33 cents that scaling measured.
      {587.33, 1.6435, 6},
  };
  inline BrassFinger tromboneFinger(double hertz) {
      for (const BrassFinger& row : kTromboneFingers)
        if (std::fabs(row.hertz - hertz) < 0.02) return row;
      // Unlisted pitch: the nearest tuned row, same mode, with the cone
      // (or, for mode 0, the tube) scaled by the frequency ratio.
      const BrassFinger* nearest = &kTromboneFingers[0];
      for (const BrassFinger& row : kTromboneFingers)
        if (std::fabs(std::log(row.hertz / hertz)) <
            std::fabs(std::log(nearest->hertz / hertz)))
          nearest = &row;
      const double apex = nearest->mode == 0 ? 0.0 : 0.60;
      return BrassFinger{hertz, (nearest->length + apex) * nearest->hertz / hertz - apex,
                    nearest->mode, nearest->lipRatio};
    }

  // Tuned so this lip and this length play the concert pitch, and the
  // Promenade order stays finite. F5 at 1.39 m into D5 at 1.38 m dies
  // on the open-valve slur; 1.410 into 1.386 completes. Mode 0 is the
  // open second-register recipe, lip = 0.75 c/L. Any other mode sets
  // the lip at trumpetLipPartialRatio(mode) of that cone partial.
  inline constexpr BrassFinger kTrumpetFingers[] = {
      // G4 was the mode-0 recipe (0.804 m, lip 0.75 c/L) and played 8-12 dB
      // under the cone-mode notes in Jingle Bells (winds pro6). It is now
      // mode 3 like A4 and Bb4: the Bb4 cone + apex scaled by 466.16/392,
      // then x 2^(-53/1200) for the -53 c it read (one pass). F4 on the
      // same recipe locked to a lower partial on the pro phrase and keeps
      // the mode-0 row; E4 and F#4 are mode 3 rows scaled from F4 (mode 3
      // trial 1.4010 m) and G4 and corrected by the +2 c / -6 c they read.
      // Mode 3 rows retuned for the 0.70 lip ratio (christmas-jb3): each
      // (L + apex) x 2^(cents/1200) for the cents read at 0.70 in Jingle
      // Bells, one pass: G4 0, Bb4 -18 (its B4 read -18), A4 -15, F#4 -3,
      // E4 -25, D4 -4 (D4 trial 1.665 m).
      // All rows retuned for the Q 4 lip (kTrumpetLipQualityFactor) and its
      // ratios, one pass, (L + apex) x 2^(cents/1200) (mode 0: L x ...) for
      // the steady cents read by YIN (winds_pro1 h/tp ptrack.py on the Lick
      // excerpt, jbtrack.py on Jingle Bells): G4 -7.7, F4 -33.0, Bb4 -10.8,
      // C5 +3.5, D5 +16.0, A4 +3.2, E5 -9.2 then +11.6 at 0.85, C#5 +17.3,
      // E4 -11.3, F#4 -8.2, D4 +6 at 0.78, F5 -21.8 at 0.92.
      {392.00, 1.2132, 3}, {349.23, 0.8948, 0}, {466.16, 1.0018, 3},
      {523.25, 1.2132, 4}, {698.46, 1.3891, 6, 0.92}, {587.33, 1.0720, 4},
      // A4 for When the Saints: the Bb4 row scaled sat 27.8 cents flat;
      // (L + apex) shortened by that measured ratio.
      {440.00, 1.0686, 3},
      // Jingle Bells (tools/demo/christmas): D4 scaled from the F4 row read
      // +68 c, E5 scaled from F5 -39 c, C#5 scaled from D5 -27 c; each row
      // is that scaled length (cone + apex for a cone mode) times
      // 2^(cents/1200), one pass.
      // D4 was the mode-0 row (1.1281 m, F4 scaled) and E4 mode 3 at
      // 1.5018 m with the 0.80 lip: both played biphonic (D4 295 + 409 Hz,
      // E4 332 + 423 Hz, harmonic-energy fraction 1-10 %), the 'fully
      // wrong' trumpet of christmas-jb2. A real Bb trumpet plays both as
      // its 3rd partial (D4 1+2 valves, E4 valve 2), so they are mode 3
      // rows here; a grid of 1.55-1.83 m x lip 0.6-1.1 locked single-pitch
      // only at lip 0.6-0.7 (tools/demo/winds_pro1, d4/e4 grid).
      // C5 (mode 4, lip 0.70) interpolated from the grid (1.20 m +13 c,
      // 1.24 m -38 c); C#5 and D5 are its cone + apex scaled by the
      // frequency ratio; D5 then read +13 c and is lengthened by it. E5 (mode 5, lip 0.75) from 1.22 m -11 c, 1.26 m
      // -64 c.
      {293.66, 1.6661, 3, 0.78}, {659.26, 1.2137, 5}, {554.37, 1.1416, 4},
      // B4 (Jingle Bells) was the Bb4 row scaled; at the Q 4 / 0.80 lip it
      // read +5.3 c and is that scaled length lengthened by it.
      {493.88, 0.9421, 3},
      {329.63, 1.4653, 3}, {369.99, 1.2924, 3},
  };
  // Lip natural frequency over the bore partial it drives. An
  // outward-striking lip plays just above its own resonance, so the lip
  // sits below the impedance peak (Yoshikawa, JASA 97, 1929 (1995);
  // Cullen, Gilbert & Campbell, Acta Acustica 86, 704 (2000)). The values
  // are TUNED: at 0.80 the mode 3 notes (D4-B4) came out near-sinusoidal
  // (H2 -3 to -11 dB, H3 -6 to -10 dB re H1) and D4/E4 split into two
  // pitches; at 0.70 they lock single-pitch with H2 +3 to +9 dB, H3 -3 to
  // +12 dB, the side of a played trumpet (H2 +8, H3 +7 on the CC
  // reference). The cone mode number is the played partial of a Bb
  // trumpet: C5-D5 are its 4th partial (concert C5 = 1st valve), E5 the
  // 5th (2nd valve), F5 the 6th (open). As 5th-mode rows C5-D5 read
  // H2 ~0, H3 -6 to -9 dB; as 4th-mode rows at 0.70 they lock with
  // H2 +2, H3 ~0 (grid 1.20-1.32 m x 0.60-0.80). Mode 5 (E5) locks at
  // 0.75-0.80 and splits at 0.70; mode 6 (F5) split below 0.78.
  // Those were found with the old fixed lip damping (Q ~ 85). With the lip
  // Q at kTrumpetLipQualityFactor the ratios move up: at 0.70 the low-Q
  // lip lost C5-E5 (-450 c and more), at 0.75 D5 split (Jingle Bells grid
  // Q 3-8 x 0.70-0.80, winds_pro1 h/tp jbtrack.py); 0.80 holds modes 3-4.
  // The ratio has to clear (n-1)/n, the next partial down, which a broad
  // low-Q lip also drives: E5 (mode 5, 0.80 = 4/5) sat 25 dB down with the
  // mode 4 partial in its attack, so modes 5-6 take 0.85, and the F5 row
  // 0.92 (0.85: -27 c, attack -132 c; 0.88 / 0.92: attack -76 / -40 c).
  // D4 at 0.75 and 0.80 drifted to F4 on its 1.2 s vibrato notes (+305 c),
  // its row takes 0.78, which held. TUNED. Cost: the 0.80 notes are rounder
  // (Lick excerpt centroid 847 -> 741 Hz, H3 +4.9 -> +2.9 dB; recording
  // 1436 Hz, +6.0 dB).
  inline constexpr double trumpetLipPartialRatio(int mode) {
    return mode <= 0 ? 0.75 : (mode <= 4 ? 0.80 : 0.85);
  }
  inline constexpr double trumpetLipRatio(const BrassFinger& row) {
    return row.lipRatio > 0.0 ? row.lipRatio : trumpetLipPartialRatio(row.mode);
  }
  inline BrassFinger trumpetFinger(double hertz) {
      for (const BrassFinger& row : kTrumpetFingers)
        if (std::fabs(row.hertz - hertz) < 0.02) return row;
      // Unlisted pitch: the nearest tuned row, same mode, with the cone
      // (or, for mode 0, the tube) scaled by the frequency ratio.
      const BrassFinger* nearest = &kTrumpetFingers[0];
      for (const BrassFinger& row : kTrumpetFingers)
        if (std::fabs(std::log(row.hertz / hertz)) <
            std::fabs(std::log(nearest->hertz / hertz)))
          nearest = &row;
      const double apex = nearest->mode == 0 ? 0.0 : 0.26;
      return BrassFinger{hertz, (nearest->length + apex) * nearest->hertz / hertz - apex,
                    nearest->mode, nearest->lipRatio};
    }

// Exact fingering row for a listed pitch (within 0.02 Hz); a zero row when
// the pitch is not listed. The ring grade uses only listed rows.
inline BrassFinger brassFingerExact(const BrassFinger* rows, int count, double hertz) {
  for (int i = 0; i < count; ++i)
    if (std::fabs(rows[i].hertz - hertz) < 0.02) return rows[i];
  return BrassFinger{};
}

}  // namespace mmathen

