#pragma once

// The phrase player: plays a list of notes on a wind voice through the
// player's airway (player_airway.h), with tongue closures before tongued
// notes, slurred bore glides, vibrato, breath rests and the brass ring wall
// once the breath stops. Moved verbatim from tests/test_wind_tone.cpp so
// every demo and test plays with the same player. Grading fields on
// PhraseNote (tolCents, require, minRms, gradePeak) are read by the suite,
// not by the player. ensureLeadIn gives every score the phrase lead-in.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <string_view>
#include <vector>

#include "../math/constants.h"
#include "bore_valve.h"
#include "cone_valve.h"
#include "player_airway.h"
#include "wind_presets.h"
#include "wind_player_profile.h"

namespace mmathen {

struct PhraseNote {
  const char* name;
  double startSec = 0.0;
  double targetHz = 0.0;
  double lengthM = 0.0;
  double lipHz = 0.0;
  double pressure = 0.0;
  double jetSamples = 0.0;
  bool tongue = false;
  bool vibrato = false;
  double tolCents = 40.0;
  bool require = true;
  double minRms = 0.01;
  bool rest = false;
  double gain = 1.0;
  const char* dynamic = "";
  // Cone resonance this fingering plays, the input-impedance peak.
  // Zero on a reed. The free ring's wall is matched here.
  double peakHz = 0.0;
  // The strongest spectral peak, not the autocorrelation. An octave
  // error has autocorrelation near 1 at twice the true period.
  bool gradePeak = false;
  // Breath release (reed woodwinds with the airway): a note before a rest
  // or the phrase end is stopped by lowering the mouth pressure over this
  // time instead of by the tongue. 0 = tongue stop. See kReedBreathReleaseSec.
  double releaseSec = 0.0;
  // Step 24.2a: an embouchure setting error at this note's start, cents,
  // added to the player's random one (WindPlayerProfile::crackSdCents).
  // Tests use it to force a crack; 0 = none.
  double onsetErrorCents = 0.0;
  // Forced scoop depth in cents (step 24.2b; tests). Above 0 the note
  // scoops this deep whatever the player's scoop chance; 0 = the player's.
  double scoopCents = 0.0;
  // Forced fall-off depth in cents (step 24.2c; tests). Above 0 the note
  // falls this deep whatever the player's chance, unless it is slurred
  // into the next note; 0 = the player's.
  double fallCents = 0.0;
};

// Cracked notes (step 24.2a). A cracked or split note is the lip (or
// reed, or jet) oscillator captured for a moment by the wrong air-column
// resonance: the player's embouchure is set for a neighbouring partial,
// the bore's next resonance wins until the player hears it and corrects.
// While the first reflection has not yet returned from the bell the lip
// gets no help from the bore, so the setting must be right on its own;
// this is worst for high notes, where the partials are close (J. Wolfe,
// "Musical instruments", Oxford Handbook, 2022: "hornists especially fear
// 'cracked' notes"; M. Campbell, J. Gilbert & A. Myers, The Science of
// Brass Instruments, Springer 2021, regime selection by the lip
// resonance; L. Velut, C. Vergez, J. Gilbert & M. Djahanbani, Acta
// Acustica u. A. 103, 132-148, 2017, coexisting regimes of a lip model).
// Here the error moves only the player's own controls; the regime that
// sounds is the voice's physics:
//   brass   the lip resonance, by the error in cents;
//   flute   the jet delay, shorter by |error| in cents (a jet that is
//           too fast overblows to the octave, N. H. Fletcher & T. D.
//           Rossing, The Physics of Musical Instruments, 2nd ed., ch. 16,
//           VERIFY section);
//   reeds   the reed as the lip holds it: |error| lowers the reed's own
//           resonance by the same cents and halves its damping per
//           kReedSlipCentsPerHalving (an unsettled lower lip damps the
//           reed less and lets more of it vibrate; little damping, or a
//           reed resonance near an upper bore mode, lets the clarinet and
//           saxophone sound a higher mode: Wilson & Beavers, JASA 56,
//           653, 1974; Silva et al., JASA 124, 3284, 2008).
// The error holds for crackHoldSec after the note starts (from the
// tongue closure before it); over crackCorrectSec the player then pulls
// past the setting by kCrackOvershoot of the error and back over another
// crackCorrectSec. Without that pull a captured regime can hold to the
// end of the note: the regimes coexist (Velut et al. 2017), so undoing
// the error alone does not always bring the note back.
// Soft attacks: the error SD grows by kCrackSoftAttackGain x softness
// (pp 1, p 2/3, mp 1/3, louder 0). Wolfe: a soft start builds slowly, so
// the bore's help comes later; the size is UNSOURCED.
// TUNED (step 24.2a, runs/pb24/crack sweeps, YIN pitch track, 6 seeds of
// the audition tune at crack SD 300 c): with no pull 2 of 66 trombone
// and 2 of 66 saxophone cracks held to the note's end; a pull of 1x
// left none on trumpet and saxophone, longest crack 0.21 s and 0.14 s.
// The reed's pull is capped at one damping halving: a larger one (x2^10
// at 1000 c) choked the clarinet for 50 ms after the squeak.
// A reed resonance moved alone (damping kept) gave no squeak at 1000 c;
// damping alone gave a clarinet pitch pull of +100 c, not a new mode.
inline constexpr double kReedSlipCentsPerHalving = 100.0;  // TUNED (see above)
inline constexpr double kReedResonanceCentsPerCent = 1.0;   // TUNED: the reed's resonance moves by the error's cents
inline constexpr double kCrackOvershoot = 1.0;              // TUNED (see above)
inline constexpr double kReedOvershootCapCents = 100.0;     // TUNED (see above)
inline constexpr double kCrackSoftAttackGain = 1.0;         // UNSOURCED

namespace phrase_detail {
inline double softness(const char* dynamic) {
  if (dynamic == nullptr) return 0.0;
  const std::string_view d(dynamic);
  if (d == "pp" || d == "ppp") return 1.0;
  if (d == "p") return 2.0 / 3.0;
  if (d == "mp") return 1.0 / 3.0;
  return 0.0;
}
// xorshift64 + Box-Muller (the generator of player_profile.h).
struct CrackRng {
  std::uint64_t s;
  double uniform() noexcept {
    s ^= s << 13; s ^= s >> 7; s ^= s << 17;
    return (static_cast<double>(s >> 11) + 0.5) * (1.0 / 9007199254740992.0);
  }
  double gauss() noexcept { return std::sqrt(-2.0 * std::log(uniform())) * std::cos(constants::twoPi * uniform()); }
};
}  // namespace phrase_detail

// Player fatigue (step 24.2d). The embouchure and breath muscles follow
// the three-compartment muscle fatigue model (T. Xia & L. A. Frey Law, "A
// theoretical approach for modeling peripheral muscle fatigue and
// recovery", J. Biomech. 41, 3046-3052, 2008): muscle is resting, active
// or fatigued; the active share follows the target load TL, active muscle
// fatigues at rate F and fatigued muscle recovers at rate R, R x r in a
// rest (J. M. Looft, N. Herkert & L. Frey-Law, J. Biomech. 77, 16-25,
// 2018). No lip or breathing values were published; the hand/grip values
// are used (F = 0.00980/s, R = 0.00064/s, Frey-Law et al., J. Biomech.
// 45, 1803-1808, 2012; r = 30, Looft et al. 2018): VERIFY for the lips.
// In a 20 s rest that gives back about a third of the fatigue (tau 52 s).
// The load: surface EMG of trumpeters rises with pitch and loudness
// ("Relationships between abdominal and around-lip muscle activities and
// acoustic features when playing the trumpet", ISMA 2017, 11 players: the
// lip muscle with pitch, the abdominal muscle with intensity and
// duration; also JASA 140, 3418, 2016, doi 10.1121/1.4970806), so TL = kFatigueLoadScale x loudness x
// register (note pitch / the instrument's middle, fatigueRefHz); its
// time integral is the note duration. kFatigueLoadScale and the
// dynamic factors are UNSOURCED.
inline constexpr double kFatigueRatePerSec = 0.00980;     // F, grip (VERIFY for lips)
inline constexpr double kRecoveryRatePerSec = 0.00064;    // R, grip (VERIFY for lips)
inline constexpr double kRestRecoveryMultiplier = 30.0;   // r, grip (Looft et al. 2018)
inline constexpr double kFatigueLoadScale = 0.45;         // UNSOURCED
// How the fatigued share u (0..1) moves the controls. All TUNED by the
// step 24.2d sweeps (runs/pb24/fat: the tune a fourth up, 28 passes,
// about 2.5 minutes, then a 20 s rest; tiredness 1 and 2): at tiredness 1
// every wind drifts 18-34 cents flat, steadily, and a 20 s rest brings
// back 14-50% of it. Larger values broke the clarinet, saxophone and
// trumpet into squeaks or silence at tiredness 1; a 40-cent conical reed
// went sharp (the less-damped reed won), hence its own values.
inline constexpr double kFatigueBreathSag = 0.5;        // TUNED blowing pressure x (1 - this u)
inline constexpr double kFatigueNoiseGain = 3.0;        // TUNED reed / jet turbulence and edge noise x (1 + this u)
inline constexpr double kBrassFatigueNoiseGain = 4.0;   // TUNED lip turbulence x (1 + this u)
inline constexpr double kBrassFatigueLipCents = 150.0;  // TUNED lip resonance sags this u x register, cents
inline constexpr double kBrassFatigueDampGain = 0.4;    // TUNED lip damping x (1 + this u)
inline constexpr double kFatigueCrackSdCents = 150.0;   // TUNED extra onset error SD x u x register (24.2a)
inline constexpr double kReedFatigueCents = 40.0;       // TUNED cylindrical reed: equivalent length, cents x u
inline constexpr double kReedFatigueDampDrop = 0.4;     // TUNED cylindrical reed: lip damping x (1 - this u)
inline constexpr double kSaxFatigueCents = 120.0;       // TUNED conical reed: equivalent length, cents x u
inline constexpr double kSaxFatigueDampDrop = 0.0;      // TUNED conical reed: lip damping x (1 - this u)
inline constexpr double kFluteFatigueCoverCents = 30.0; // TUNED lip cover drift, cents x u
inline constexpr double kFluteFatigueAimMeters = 3.0e-4;// TUNED jet aim drift x u, m
inline constexpr double kFatigueVibratoJitter = 0.5;    // UNSOURCED (not swept) vibrato rate / depth SD x u, per note
inline constexpr double kFatigueAttackGain = 1.5;       // UNSOURCED (not swept) tongue release time x (1 + this u)

namespace phrase_detail {
inline double dynamicLoudness(const char* d) {
  const std::string_view s = d ? d : "";
  if (s == "pp") return 0.40;
  if (s == "p") return 0.55;
  if (s == "mp") return 0.70;
  if (s == "f") return 1.00;
  if (s == "ff") return 1.15;
  return 0.85;  // mf or unmarked
}
inline double registerHeight(const PhraseNote& n, const WindPlayerProfile& player) {
  const double ref = player.fatigueRefHz > 0.0 ? player.fatigueRefHz : 440.0;
  return std::clamp(n.targetHz / ref, 0.25, 4.0);
}
}  // namespace phrase_detail

// The fatigued share of the embouchure muscles, one value per hop of
// hopFrames from the phrase's start (empty when fatigueRate is 0).
[[nodiscard]] inline std::vector<double> phraseFatigue(const std::vector<PhraseNote>& notes,
                                                       const WindPlayerProfile& player, std::uint64_t totalFrames,
                                                       double rateHz, std::uint64_t hopFrames) {
  std::vector<double> u;
  if (!(player.fatigueRate > 0.0) || notes.empty()) return u;
  const std::uint64_t hops = (totalFrames + hopFrames - 1) / hopFrames;
  u.resize(static_cast<std::size_t>(hops), 0.0);
  const double dt = static_cast<double>(hopFrames) / rateHz;
  const double fRate = kFatigueRatePerSec * player.fatigueRate, rRate = kRecoveryRatePerSec * player.fatigueRecovery;
  double mf = 0.0;
  std::size_t active = 0;
  for (std::uint64_t h = 0; h < hops; ++h) {
    const double t = static_cast<double>(h * hopFrames) / rateHz;
    while (active + 1 < notes.size() && notes[active + 1].startSec <= t) ++active;
    const PhraseNote& n = notes[active];
    const bool playing = !n.rest && t >= n.startSec && n.targetHz > 0.0;
    const double tl = playing ? std::min(0.95, kFatigueLoadScale * n.gain * phrase_detail::dynamicLoudness(n.dynamic) *
                                                     phrase_detail::registerHeight(n, player))
                              : 0.0;
    u[static_cast<std::size_t>(h)] = mf;
    const double ma = std::min(tl, 1.0 - mf);
    mf += (fRate * ma - rRate * (tl > 0.0 ? 1.0 : kRestRecoveryMultiplier) * mf) * dt;
    mf = std::clamp(mf, 0.0, 1.0);
  }
  return u;
}

// Each note's onset error in cents (0 for rests): the note's own
// onsetErrorCents plus the player's random error.
[[nodiscard]] inline std::vector<double> phraseOnsetErrors(const std::vector<PhraseNote>& notes,
                                                           const WindPlayerProfile& player,
                                                           const std::vector<double>* onsetFatigue = nullptr) {
  std::vector<double> e(notes.size(), 0.0);
  phrase_detail::CrackRng rng{player.crackSeed ? player.crackSeed : 1};
  for (std::size_t i = 0; i < notes.size(); ++i) {
    const double draw = player.crackSdCents > 0.0 ? rng.gauss() : 0.0;  // one draw per note, rests too
    if (notes[i].rest) continue;
    e[i] = notes[i].onsetErrorCents +
           player.crackSdCents * (1.0 + kCrackSoftAttackGain * phrase_detail::softness(notes[i].dynamic)) * draw;
  }
  // Step 24.2d: a tired embouchure sets the lips less accurately (brass:
  // the upper range gets unstable), its own draws so the cracks above
  // are unchanged.
  if (onsetFatigue != nullptr && onsetFatigue->size() == notes.size()) {
    phrase_detail::CrackRng tired{(player.fatigueSeed ? player.fatigueSeed : 1) ^ 0x9E3779B97F4A7C15ull};
    for (std::size_t i = 0; i < notes.size(); ++i) {
      const double draw = tired.gauss();
      if (notes[i].rest || !(notes[i].lipHz > 0.0)) continue;
      e[i] += kFatigueCrackSdCents * (*onsetFatigue)[i] * phrase_detail::registerHeight(notes[i], player) * draw;
    }
  }
  return e;
}

// Scoops (step 24.2b). A scoop starts a note below its pitch and slides
// up into it. The slide is made by the player's embouchure acting on the
// instrument; the sounding pitch is the voice's physics (no pitch shifting
// of the output). Per family:
//   brass   the player lips the note down and the bore follows: the lip
//           resonance and the bore's sounding length both start
//           scoopCents low and rise to the note (kBrassScoopLipGain,
//           kBrassScoopLengthGain). Moving the lip alone barely moves the
//           pitch in this model: the bore holds it near its resonance
//           (24.2b probe, lip held 300 c low for the whole note: trumpet
//           F4 -11 c, C5 -63 c; a 300 c lip offset at 0.15 s cracked
//           C5). Real players lip a resonance by about half a semitone (D.
//           Norman, M. Campbell, J. Chick, A. Myers & J. Gilbert, "Pitch
//           bending on early brass instruments", ISMA 2010) and use the
//           trombone slide or a half-pressed valve for more; the length
//           change stands in for that (a half valve is really a branched
//           tube: VERIFY, approximation). Wolfe (UNSW, "Brass instrument
//           (lip reed) acoustics") notes players lip down further than up.
//   reeds   the jaw drops: a less pinched reed is more compliant, and the
//           reed's compliance acts as extra air-column length (the reed
//           "equivalent volume / length", A. H. Benade, Fundamentals of
//           Musical Acoustics, 1976, ch. 21; J.-P. Dalmont, B. Gazengel,
//           J. Gilbert & J. Kergomard, "Some aspects of tuning and clean
//           intonation in reed instruments", Applied Acoustics 46, 1995).
//           The model's reed has no pumping flow, so moving its resonance
//           or damping did not bend the pitch (24.2b probe, about 0 c);
//           the jaw's effect enters as that lumped length,
//           x 2^(kReedScoopLengthGain * cents / 1200). A saxophone scoop
//           "starts a half step flat, by loosening the jaw" (Timbral
//           Explorations for Wind & Percussion Instruments, doi
//           10.71889/5fylantbak.29862011).
//   flute   the lip covers more of the embouchure hole (rolling in): a
//           smaller hole is a longer end correction, so the air column is
//           longer (BoreValveVoice::setEmbouchureCoverMeters). Turning
//           the flute bends it "approximately a half step" (same source).
//   bowed strings: not done in 24.2b (the vibrato delay ring would have to
//           be resized and the slide threaded through three note paths).
// Depth and time: sung scoops measured 50-150 cents, in the first 220 ms
// of the tone, approaching the pitch exponentially (P. Larrouy-Maestri &
// P. Q. Pfordresher, "Pitch perception in music: do scoops matter?",
// J. Exp. Psych.: Human Perception and Performance 44, 1523-1541, 2018).
// Jazz: 708 of 85,821 annotated tones in the Weimar Jazz Database start
// with a slide (0.8%), Sidney Bechet almost 15% (J. Abesser, E. Cano, K.
// Frieler, M. Pfleiderer & W.-G. Zaddach, ISMIR 2015). No depth or time
// was reported for wind slides, and named players are not jazz players,
// so every named profile keeps scoops off.
// The offset is the full depth from the tongue closure to the release and
// falls to 0 over scoopSec on a smoothstep (no step at either end).
// Gains: TUNED so the YIN pitch at the release lands near the requested
// depth (24.2b sweep, 100 c over 0.15 s, deepest YIN pitch in the first
// 40 ms after the release, cents from the note; low note / high note):
//   brass length 1 + lip 1: trumpet F4 -85, C5 -111; trombone F4 -144.
//     (lip 0: about the same; lip 1 keeps the lip on the bore's
//     resonance so the slide does not crack.)
//   reeds length 1: clarinet E4 -70, C5 -43; sax E4 -88, C5 -51.
//         length 1.5: clarinet -113, -67; sax -129, -77.
//         length 2: clarinet -157, -108; sax -168, -112.  -> 1.5
//   flute cover 1: C5 -87, G5 -82; 1.15: -100, -94.  -> 1.15
inline constexpr double kBrassScoopLipGain = 1.0;      // TUNED, see above
inline constexpr double kBrassScoopLengthGain = 1.0;   // TUNED, see above
inline constexpr double kReedScoopLengthGain = 1.5;    // TUNED, see above
inline constexpr double kFluteScoopCoverGain = 1.15;   // TUNED, see above

// Each note's scoop depth in cents: the note's forced scoopCents, else
// the player's scoopCents on a tongued note with probability scoopChance
// (slurred notes are joined at pitch and do not scoop). One draw per note
// when the player scoops, rests too, seeded by scoopSeed.
[[nodiscard]] inline std::vector<double> phraseScoopDepths(const std::vector<PhraseNote>& notes,
                                                           const WindPlayerProfile& player) {
  std::vector<double> d(notes.size(), 0.0);
  const bool drawing = player.scoopCents > 0.0 && player.scoopSec >= 0.0;
  phrase_detail::CrackRng rng{player.scoopSeed ? player.scoopSeed : 1};
  for (std::size_t i = 0; i < notes.size(); ++i) {
    const double u = drawing ? rng.uniform() : 1.0;
    if (notes[i].rest) continue;
    if (notes[i].scoopCents > 0.0) d[i] = notes[i].scoopCents;
    else if (drawing && notes[i].tongue && u < player.scoopChance) d[i] = player.scoopCents;
  }
  return d;
}

// Fall-offs (step 24.2c): the end of a note drops below its pitch while
// the breath fades. The scoop mechanisms (above) run in reverse:
//   brass   the lips relax and the effective tube lengthens (the slide,
//           or on the trumpet a half-pressed valve: VERIFY, the length
//           change approximates the branched tube), kBrassFall...
//   reeds   the jaw loosens (the reed's equivalent length grows) and the
//           breath drops.
//   flute   the lip rolls in over the hole and the breath drops; the jet
//           delay follows the falling breath (the jet slows).
// The breath falls by k...FallBreathDrop of its level over the fall, so the
// note fades as it falls. Jazz: 146 of 87,643 annotated tones in the
// Weimar Jazz Database fall off, "a drop of the f0 contour in the end of
// the tone after a stationary part" (Abesser, Frieler, Cano, Pfleiderer
// & Zaddach, ISMIR 2015, Table 3); no depth or duration was published.
// What the model does (24.2c probes, fall harness, 0.8 s notes, fall over
// the last 0.3 s; pitch at the fall's end, cents from the note):
//   brass: lips and tube together glide down on the same resonance, a
//     smooth smear (trumpet F4/C5: 100 c -> -99/-115, 300 c -> -277/-303,
//     500 c -> -454/-489; trombone C4/F4 300 c -> -286/-301). The model
//     does not rip down the partials: with the tube capped at 200 c and
//     the lips alone going further, the pitch stalled near -200 to -290 c
//     on the same partial instead of stepping to the next one down.
//   reeds (length 1.5): breath drop 0.2 / 0.3 / 0.4 -> clarinet F4
//     -412 / -402 / -381, sax E4 -367 / -362 / -404 at 300 c; at 0.4 the
//     clarinet C5 dropped to its low register (-1601 c) and the level fell
//     11-17 dB. Length 1.2 at drop 0.3: clarinet -316/-341, sax -278/-212.
//   flute (cover 1.15): breath drop 0.3 -> -340/-336, 0.5 -> -347/-344 at
//     300 c; at 0.5 the jet needed a longer delay than the build's
//     (BoreValveVoice::reserveJetDelaySamples).
// Gains and breath drops: TUNED from those sweeps.
inline constexpr double kBrassFallLipGain = 1.0;     // TUNED
inline constexpr double kBrassFallLengthGain = 1.0;  // TUNED
inline constexpr double kReedFallLengthGain = 1.3;   // TUNED (between 1.2 and 1.5)
inline constexpr double kFluteFallCoverGain = 1.0;   // TUNED (1.15 overshot by 15%)
inline constexpr double kBrassFallBreathDrop = 0.5;  // TUNED: still speaking, -5 to -10 dB
inline constexpr double kReedFallBreathDrop = 0.3;   // TUNED: 0.4 drops the clarinet's register
inline constexpr double kFluteFallBreathDrop = 0.5;  // TUNED
// A fall never takes more than this share of the note: the start stays
// on pitch. UNSOURCED.
inline constexpr double kFallMaxNoteFraction = 0.7;

namespace phrase_detail {
// A phrase-ending note (fall-offs): the next note is a rest or the end, or
// the note is held at least 1.5x as long as each sounding neighbour (a
// held note closing a group of shorter ones). A heuristic, UNSOURCED;
// the KTH punctuation rule finds such boundaries in more detail (Friberg,
// Bresin, Fryden & Sundberg, J. New Music Research 27, 271-292, 1998).
inline bool phraseEnd(const std::vector<PhraseNote>& notes, std::size_t i) {
  const std::size_t n = notes.size();
  if (i + 1 >= n || notes[i + 1].rest) return true;
  const double here = notes[i + 1].startSec - notes[i].startSec;
  const double next = i + 2 < n ? notes[i + 2].startSec - notes[i + 1].startSec : here;
  const bool prevOk = i == 0 || notes[i - 1].rest || here >= 1.5 * (notes[i].startSec - notes[i - 1].startSec);
  return prevOk && here >= 1.5 * next;
}
}  // namespace phrase_detail

// Each note's fall-off depth in cents: never on a rest or a note slurred
// into the next; else the note's forced fallCents, else the player's
// fallCents with probability fallChance on an eligible note (phrase ends
// only with fallPhraseEndsOnly). One draw per note when the player falls.
[[nodiscard]] inline std::vector<double> phraseFallDepths(const std::vector<PhraseNote>& notes,
                                                          const WindPlayerProfile& player) {
  std::vector<double> d(notes.size(), 0.0);
  const bool drawing = player.fallCents > 0.0 && player.fallSec >= 0.0;
  phrase_detail::CrackRng rng{player.fallSeed ? player.fallSeed : 1};
  for (std::size_t i = 0; i < notes.size(); ++i) {
    const double u = drawing ? rng.uniform() : 1.0;
    if (notes[i].rest) continue;
    if (i + 1 < notes.size() && !notes[i + 1].rest && !notes[i + 1].tongue) continue;  // slurred on
    if (notes[i].fallCents > 0.0) d[i] = notes[i].fallCents;
    else if (drawing && u < player.fallChance && (!player.fallPhraseEndsOnly || phrase_detail::phraseEnd(notes, i)))
      d[i] = player.fallCents;
  }
  return d;
}

// Li, Almeida, Smith and Wolfe (JASA 139, 825-838, 2016): clarinettists end
// staccato notes with the tongue (exponential decay ~400 dB/s, the bore's
// bandwidth); "in all other cases, notes are stopped by decreasing" the
// mouth pressure, with a decay time about twice the tongue-stopped one
// (Li et al., ISMRA 2016, Fig. 4: ~0.3 s for a written C4). The pressure
// fall is linear over kReedBreathReleaseSec, TUNED to that decay time:
// winds pro3 sweep, last note of the clarinet / alto sax excerpts, time
// from the note end to -30 dB: tongue stop 216 / 183 ms; breath release
// 0.06 s -> 66 / 63, 0.12 -> 121 / 118, 0.25 -> 251 / 243, 0.40 -> 401 / 388.
// Target: Li's written C4 tongue stop, -260 dB/s (30 dB in 115 ms), times
// two = 230 ms. (The model's own tongue-stop tail is slower than Li's.)
constexpr double kReedBreathReleaseSec = 0.25;

// The score with the phrase lead-in (kPhraseLeadInSeconds, wind_presets.h)
// in front, unless it already opens on a rest at least that long. The
// lead-in rest takes the first note's dynamic.
template <typename Song>
std::vector<ScoreNote> ensureLeadIn(const Song& song) {
  auto first = std::begin(song);
  if (first == std::end(song)) return {};
  if (!(first->hertz > 0.0) && first->seconds >= kPhraseLeadInSeconds)
    return std::vector<ScoreNote>(std::begin(song), std::end(song));
  return withLeadIn(song, first->dynamic);
}

// 12 cents and 5.5 Hz are arguments. No stored wind-vibrato depth is
// used. The length modulation is the pitch control this bore already has.
constexpr double kVibratoHz = 5.5;
constexpr double kVibratoCents = 12.0;
// Brass (a lip note): measured on the VSCO 2 Community Edition trumpet
// sustain-vibrato set (Sam Gossner, CC0, github.com/sgossner/VSCO-2-CE,
// Brass/Trumpet/susvib): 14 notes with f0 250-800 Hz, rate 5.84 Hz
// median (4.8-6.5), extent +/-9.1 cents median (3.0-23.5), AM +/-0.9 dB
// (tools/demo/winds_pro1 vibm.py, peak of the 3.5-9 Hz band of the
// detrended H1 track).
// The bore length swing that gives +/-9.1 c: at 9.1 the locked lip and
// bore sounded +/-7.1 c (E4 7.7, G4 6.5 in Jingle Bells), so the swing
// is 9.1 x 9.1 / 7.1 = 11.7 c, TUNED to the measured extent.
constexpr double kBrassVibratoHz = 5.84;
constexpr double kBrassVibratoCents = 11.7;
// A breath taken from a brass note ends it by the breath, not the
// tongue: the pressure falls over this time, then the lungs refill for
// the rest of the stolen time. TUNED: the Lick_Trumpet reference notes
// fall about 10 dB over their last 50-60 ms; a phrase-end release is
// slower. UNSOURCED for phrase ends.
constexpr double kBrassBreathReleaseSec = 0.12;

// Breath plan for scores without enough rests. Wind players breathe at
// phrase joins and take the time from the end of the note before the join
// (Bouhuys, J. Appl. Physiol. 19, 967-975, 1964: playing breaths are quick
// inspirations between phrases, VERIFY the figures). When the lungs come within
// kBreathMarginM3 of residual volume, the next note join takes a breath: the last
// kBreathStealSec of the sounding note is silent and the lungs refill.
// Both values are UNSOURCED arguments.
constexpr double kBreathMarginM3 = 1.0e-3;
constexpr double kBreathStealSec = 0.30;

template <typename Voice>
std::vector<double> playPhrase(Voice& voice, const std::vector<PhraseNote>& notes,
                               double totalSec, bool fluteJet, double ringTailSec = 0.0,
                               PlayerAirway* airway = nullptr, double sampleRateHz = 48000.0,
                               const WindPlayerProfile& player = WindPlayerProfile{}) {
  const double rateHz = sampleRateHz;
  const auto scoreFrames = static_cast<std::uint64_t>(std::llround(totalSec * rateHz));
  const auto total =
      scoreFrames + static_cast<std::uint64_t>(std::llround(std::max(0.0, ringTailSec) * rateHz));
  std::vector<double> out(static_cast<std::size_t>(total), 0.0);
  std::vector<char> armed(notes.size(), 0);
  constexpr std::uint64_t kHop = 64;
  // Bertsch and Hoole, fast single tonguing, about 40Ã¢â‚¬â€œ64 ms. The closure
  // is that long; the release is the note. The first note keeps the
  // blow's own tongue, if it has one.
  const std::uint64_t kTongue = player.tongueClosureSec > 0.0
                                    ? static_cast<std::uint64_t>(std::llround(player.tongueClosureSec * rateHz))
                                    : 1920;
  const double depth = std::pow(2.0, player.vibratoCents / 1200.0) - 1.0;
  const double brassDepth = std::pow(2.0, player.brassVibratoCents / 1200.0) - 1.0;
  const double tuneScale = std::pow(2.0, -player.intonationCents / 1200.0);
  double stealStartSec = -1.0;
  // With the player's airway attached, the airway's breath feeds the
  // valve and its tongue articulates: the tip is on the reed (or behind
  // the teeth) for the kTongue closure before a tongued note and leaves
  // at the note (player_airway.h, critically damped, Li et al. 2016).
  // A slurred note keeps the tongue off: the join is the bore change
  // under a steady breath. A woodwind note before a rest is ended by
  // the tongue for the same closure. The voice's own tongue schedule
  // and the flute's 20 ms pressure dip are not used then.
  if (airway != nullptr) {
    voice.attachAirway(airway);
    airway->placeTongue(1.0);
  }
  // Step 24.2d: the fatigued share per hop (empty when the player does
  // not tire); with it the onset errors grow, the vibrato wanders per
  // note, and the controls below sag.
  const std::vector<double> fatigue = phraseFatigue(notes, player, total, rateHz, kHop);
  const bool tiring = !fatigue.empty();
  std::vector<double> onsetFatigue, vibRate, vibDepth;
  double vibPhase = 0.0, baseAim = 0.0, baseRelease = 0.0;
  if (tiring) {
    phrase_detail::CrackRng wobble{(player.fatigueSeed ? player.fatigueSeed : 1) ^ 0xD1B54A32D192ED03ull};
    for (const PhraseNote& n : notes) {
      const auto h = std::min<std::size_t>(fatigue.size() - 1, static_cast<std::size_t>(std::max(0.0, n.startSec) * rateHz) / kHop);
      const double u = fatigue[h];
      onsetFatigue.push_back(u);
      vibRate.push_back(std::max(0.3, 1.0 + kFatigueVibratoJitter * u * wobble.gauss()));
      vibDepth.push_back(std::max(0.0, 1.0 + kFatigueVibratoJitter * u * wobble.gauss()));
    }
    if constexpr (requires(Voice& probe) { probe.jetAimMeters(); }) baseAim = voice.jetAimMeters();
    if (airway != nullptr) baseRelease = airway->tongueReleaseSeconds();
  }
  // Step 24.2a: onset errors; none at all leaves every control as before.
  const std::vector<double> onsetError = phraseOnsetErrors(notes, player, tiring ? &onsetFatigue : nullptr);
  const bool anyCrack = std::any_of(onsetError.begin(), onsetError.end(), [](double x) { return x != 0.0; });
  const std::vector<double> scoopDepth = phraseScoopDepths(notes, player);
  const bool anyScoop = std::any_of(scoopDepth.begin(), scoopDepth.end(), [](double x) { return x > 0.0; });
  const double scoopSec = player.scoopSec > 0.0 ? player.scoopSec : kScoopDefaultSec;
  // Step 24.2c: fall-offs. A falling note falls over its last fallSec,
  // ending at the next note's tongue closure (or the score's end).
  const std::vector<double> fallDepth = phraseFallDepths(notes, player);
  const bool anyFall = std::any_of(fallDepth.begin(), fallDepth.end(), [](double x) { return x > 0.0; });
  std::vector<double> fallStart(notes.size(), 0.0), fallLen(notes.size(), 0.0);
  if (anyFall) {
    const double want = player.fallSec > 0.0 ? player.fallSec : kFallDefaultSec;
    const double closureSec = static_cast<double>(kTongue) / rateHz;
    for (std::size_t i = 0; i < notes.size(); ++i) {
      if (!(fallDepth[i] > 0.0)) continue;
      const double end = (i + 1 < notes.size() ? notes[i + 1].startSec : static_cast<double>(scoreFrames) / rateHz) - closureSec;
      fallLen[i] = std::max(1.0e-3, std::min(want, kFallMaxNoteFraction * (end - notes[i].startSec)));
      fallStart[i] = end - fallLen[i];
    }
  }
  bool breathing = false;
  for (std::uint64_t frame = 0; frame < total;) {
    const std::uint64_t count = std::min<std::uint64_t>(kHop, total - frame);
    int active = 0;
    for (int i = 0; i < static_cast<int>(notes.size()); ++i) {
      if (frame >= static_cast<std::uint64_t>(notes[static_cast<std::size_t>(i)].startSec * rateHz))
        active = i;
    }
    // A tongued brass note retunes while the tongue is shut, then
    // releases. Setting the new lip on the open valve goes non-finite.
    // Woodwinds do not take this branch (no lip resonance). A tongue
    // stop at the end of a note is on hold: the breath is what ends it.
    int sounding = active;
    bool tongueShut = false;
    for (int i = 1; i < static_cast<int>(notes.size()); ++i) {
      const PhraseNote& upcoming = notes[static_cast<std::size_t>(i)];
      if (!upcoming.tongue || upcoming.rest) continue;
      const auto release = static_cast<std::uint64_t>(upcoming.startSec * rateHz);
      if (release < kTongue) continue;
      if (frame >= release - kTongue && frame < release) {
        tongueShut = true;
        if (!armed[static_cast<std::size_t>(i)]) {
          if (airway == nullptr) voice.scheduleTongue(release);
          armed[static_cast<std::size_t>(i)] = 1;
        }
        if (upcoming.lipHz > 0.0) sounding = i;
      }
    }
    const bool afterScore = frame >= scoreFrames;
    if (airway != nullptr) {
      const auto next = static_cast<std::size_t>(active) + 1;
      const PhraseNote& here = notes[static_cast<std::size_t>(active)];
      if (here.lipHz == 0.0 && !here.rest) {
        const std::uint64_t noteEnd =
            next < notes.size() ? static_cast<std::uint64_t>(notes[next].startSec * rateHz) : scoreFrames;
        const bool restNext = next >= notes.size() || notes[next].rest;
        if (restNext && noteEnd >= kTongue && frame >= noteEnd - kTongue && !(here.releaseSec > 0.0 && !fluteJet))
          tongueShut = true;
      }
      airway->setTongueTarget(tongueShut ? 0.0 : 1.0);
    }
    const double time = static_cast<double>(frame) / rateHz;
    bool stolenBreath = false;
    if (airway != nullptr && !afterScore && !notes[static_cast<std::size_t>(sounding)].rest) {
      const auto next = static_cast<std::size_t>(active) + 1;
      if (next < notes.size() && !notes[next].rest) {
        const auto join = static_cast<std::uint64_t>(notes[next].startSec * rateHz);
        // A player breathes on a note that can spare the time: at most
        // breathMaxNoteFraction of the note is taken, and a short note
        // is passed over for a later, longer one unless the lungs are
        // within half the margin of residual (jb13: a 0.3 s steal ate
        // whole 0.3 s Jingle Bells notes).
        const double noteSec = notes[next].startSec - notes[static_cast<std::size_t>(active)].startSec;
        const double spare = player.breathMaxNoteFraction * noteSec;
        const bool roomy = spare >= player.breathStealSec;
        const auto steal = static_cast<std::uint64_t>(std::min(player.breathStealSec, spare) * rateHz);
        if (breathing && frame < join) stolenBreath = true;
        else if (join > steal && steal > 0 && frame >= join - steal && frame < join &&
                 airway->needsBreath(player.breathMarginM3) &&
                 (roomy || airway->needsBreath(0.5 * player.breathMarginM3))) {
          breathing = true; stolenBreath = true;
          stealStartSec = static_cast<double>(frame) / rateHz;
        }
        if (frame + count >= join) breathing = false;
      }
    }
    // A brass note's stolen breath starts with the breath release.
    double brassRelease = -1.0;
    if (stolenBreath && notes[static_cast<std::size_t>(sounding)].lipHz > 0.0) {
      const double since = time - stealStartSec;
      if (stealStartSec >= 0.0 && since < player.brassBreathReleaseSec) {
        brassRelease = 1.0 - since / player.brassBreathReleaseSec;
        stolenBreath = false;
      }
    }
    // The note after a breath is tongued, as after a written rest.
    if (stolenBreath) airway->setTongueTarget(0.0);
    // Breath release: after a reed note that is ended by the breath, the
    // pressure falls over its releaseSec while the fingering stays.
    int releasing = -1;
    double releaseLeft = 0.0;
    if (airway != nullptr && !fluteJet && !stolenBreath) {
      const PhraseNote& here = notes[static_cast<std::size_t>(active)];
      const int prevIdx = here.rest ? active - 1 : (afterScore ? active : -1);
      if (prevIdx >= 0) {
        const PhraseNote& prev = notes[static_cast<std::size_t>(prevIdx)];
        const double end = here.rest && prevIdx < active ? here.startSec : static_cast<double>(scoreFrames) / rateHz;
        const double since = time - end;
        if (!prev.rest && prev.lipHz == 0.0 && prev.releaseSec > 0.0 && since >= 0.0 && since < prev.releaseSec) {
          releasing = prevIdx;
          releaseLeft = 1.0 - since / prev.releaseSec;
        }
      }
    }
    if (releasing >= 0) {
      sounding = releasing;
      if (!tongueShut) airway->setTongueTarget(1.0);  // a closure for the next note still wins
    }
    const PhraseNote& note = notes[static_cast<std::size_t>(sounding)];
    const bool breathOff = releasing < 0 && (note.rest || afterScore || stolenBreath);
    // Once the breath is off, the lip seals and the wall is Kirchhoff
    // at this fingering's impedance peak. The next blow restores the
    // mild playing pole. The radiated sample is not scaled.
    // A reed on the cone, stopped by the tongue, rings on the same
    // Kirchhoff wall at its fingering: the playing loss is the
    // regeneration-tuned pole, and on it a 40 ms stop dipped the alto
    // sax 11 dB where the Lick_Alto_Sax reference dips 21 dB (18 dB on
    // this wall, winds pro6). The breath release keeps the playing wall.
    if (note.lipHz == 0.0 && !note.rest && !fluteJet && airway != nullptr) {
      if constexpr (requires(Voice& probe) { probe.engageRingWall(0.0); }) {
        const double ringHz = note.peakHz > 0.0 ? note.peakHz : note.targetHz;
        if (tongueShut && releasing < 0 && ringHz > 0.0) {
          if (!(voice.engageRingWall(ringHz))) return {};
        } else {
          if (!(voice.restorePlayingWall())) return {};
        }
      }
    }
    if (note.lipHz > 0.0) {
      if constexpr (requires(Voice& probe) { probe.engageRingWall(0.0); }) {
        // A tongue stop seals the lip as a breath stop does (the airflow
        // is cut at the teeth), so the bore rings on the same Kirchhoff
        // wall, the published bandwidth (winds pro2: on the playing pole
        // a 40 ms stop dipped a trumpet only 4 dB; the CC reference dips
        // 15-21 dB).
        if ((breathOff || tongueShut) && note.peakHz > 0.0) {
          if (!(voice.engageRingWall(note.peakHz))) return {};
        } else {
          if (!(voice.restorePlayingWall())) return {};
        }
      }
    }
    // The onset error of the sounding note, weighted 1 from its tongue
    // closure until crackHoldSec after it starts, then fading out.
    double crackCents = 0.0, crackSize = 0.0;  // signed error; |error| signed by the weight
    if (anyCrack && !note.rest && !breathOff) {
      const double since = time - note.startSec;
      const double closureSec = static_cast<double>(kTongue) / rateHz;
      double w = 0.0;
      const double hold = player.crackHoldSec, fix = player.crackCorrectSec;
      if (since >= -closureSec && since < hold) w = 1.0;
      else if (since >= hold && since < hold + fix) {
        const double a = (since - hold) / fix;
        w = 1.0 - (1.0 + kCrackOvershoot) * a * a * (3.0 - 2.0 * a);
      } else if (kCrackOvershoot > 0.0 && since >= hold + fix && since < hold + 2.0 * fix) {
        const double a = (since - hold - fix) / fix;
        w = -kCrackOvershoot * (1.0 - a * a * (3.0 - 2.0 * a));
      }
      crackCents = w * onsetError[static_cast<std::size_t>(sounding)];
      crackSize = w * std::fabs(onsetError[static_cast<std::size_t>(sounding)]);
    }
    // The scoop of the sounding note: its full depth from the tongue
    // closure to the release, then down to 0 over scoopSec.
    double scoopNow = 0.0;
    if (anyScoop && !note.rest && !breathOff) {
      const double since = time - note.startSec;
      const double closureSec = static_cast<double>(kTongue) / rateHz;
      const double depthCents = scoopDepth[static_cast<std::size_t>(sounding)];
      if (depthCents > 0.0 && since >= -closureSec && since < scoopSec) {
        const double a = since <= 0.0 ? 0.0 : since / scoopSec;
        scoopNow = depthCents * (1.0 - a * a * (3.0 - 2.0 * a));
      }
    }
    // The fall-off of the sounding note: 0 until its fall starts, then
    // down to its full depth on a smoothstep; it stays there through a
    // breath release. The breath falls with it.
    double fallNow = 0.0, fallBreath = 1.0;
    if (anyFall && !note.rest) {
      const double depthCents = fallDepth[static_cast<std::size_t>(sounding)];
      const double since = time - fallStart[static_cast<std::size_t>(sounding)];
      if (depthCents > 0.0 && since >= 0.0) {
        const double a = std::min(1.0, since / fallLen[static_cast<std::size_t>(sounding)]);
        const double w = a * a * (3.0 - 2.0 * a);
        fallNow = depthCents * w;
        fallBreath = 1.0 - (note.lipHz > 0.0 ? kBrassFallBreathDrop : fluteJet ? kFluteFallBreathDrop : kReedFallBreathDrop) * w;
      }
    }
    const double tired = tiring ? fatigue[std::min<std::size_t>(fatigue.size() - 1, static_cast<std::size_t>(frame / kHop))] : 0.0;
    if ((anyCrack || tiring) && note.lipHz == 0.0 && !fluteJet) {
      if constexpr (requires(Voice& probe) { probe.setReedDynamics(1.0, 1.0); }) {
        const double reedSize = std::max(crackSize, -kReedOvershootCapCents);
        // A tired embouchure presses the reed less: less lip damping on it.
        constexpr bool conical = requires(Voice& probe) { probe.setLipDampingScale(1.0); };
        const double tiredDamp = tiring ? 1.0 - (conical ? kSaxFatigueDampDrop : kReedFatigueDampDrop) * tired : 1.0;
        if (!(voice.setReedDynamics(std::pow(2.0, -kReedResonanceCentsPerCent * reedSize / 1200.0),
                                    std::pow(2.0, -reedSize / kReedSlipCentsPerHalving) * tiredDamp))) return {};
      }
    }
    if (tiring) {
      if constexpr (requires(Voice& probe) { probe.setTurbulenceScale(1.0); }) {
        if (!(voice.setTurbulenceScale(1.0 + (note.lipHz > 0.0 ? kBrassFatigueNoiseGain : kFatigueNoiseGain) * tired))) return {};
      }
      if (airway != nullptr && baseRelease > 0.0)
        if (!airway->setTongueReleaseSeconds(baseRelease * (1.0 + kFatigueAttackGain * tired))) return {};
    }
    double env = breathOff ? 0.0 : note.gain * (releasing >= 0 ? releaseLeft : 1.0);
    if (brassRelease >= 0.0) env *= brassRelease;
    if (fallNow > 0.0) env *= fallBreath;
    if (tiring) env *= 1.0 - kFatigueBreathSag * tired;  // breath support weakens
    const bool dipping = airway == nullptr && fluteJet && note.tongue && sounding > 0 && !note.rest &&
                         time < note.startSec + 0.020;
    if (dipping) env *= 0.55;
    double length = note.lengthM;
    // A lip locked to one partial explodes if the bore glides through
    // another. Brass changes length at the note. Woodwind slurs glide.
    if (sounding > 0 && note.lipHz == 0.0 && !note.rest) {
      const double glide = note.tongue ? player.tonguedGlideSec : player.slurGlideSec;
      double alpha = (time - note.startSec) / glide;
      if (alpha < 0.0) alpha = 0.0;
      if (alpha > 1.0) alpha = 1.0;
      alpha = alpha * alpha * (3.0 - 2.0 * alpha);
      const PhraseNote& prev = notes[static_cast<std::size_t>(sounding - 1)];
      length = prev.lengthM + (note.lengthM - prev.lengthM) * alpha;
    }
    double vibrato = 1.0;
    if (tiring) {  // a tired player's vibrato wanders in rate and width from note to note
      const double hz = (note.lipHz > 0.0 ? player.brassVibratoHz : player.vibratoHz) * vibRate[static_cast<std::size_t>(sounding)];
      vibPhase = std::fmod(vibPhase + constants::twoPi * hz * static_cast<double>(count) / rateHz, constants::twoPi);
    }
    if (note.vibrato && !note.rest && time > note.startSec + player.vibratoOnsetSec) {
      vibrato = tiring ? 1.0 - (note.lipHz > 0.0 ? brassDepth : depth) * vibDepth[static_cast<std::size_t>(sounding)] * std::sin(vibPhase)
                : note.lipHz > 0.0
                    ? 1.0 - brassDepth * std::sin(constants::twoPi * player.brassVibratoHz * time)
                    : 1.0 - depth * std::sin(constants::twoPi * player.vibratoHz * time);
      length *= vibrato;
      // Breath vibrato (WindPlayerProfile::vibratoPressureFraction): the
      // diaphragm swings the blowing pressure; the pitch and the upper
      // partials follow through the jet or the reed (Fletcher 1975).
      // The flute's jet delay below reads env, so the jet speed follows.
      if (player.vibratoPressureFraction > 0.0)
        env *= 1.0 + player.vibratoPressureFraction * std::sin(constants::twoPi * player.vibratoHz * time);
    }
    if (player.intonationCents != 0.0) length *= tuneScale;
    if (anyScoop && !fluteJet && scoopNow > 0.0)
      length *= std::pow(2.0, (note.lipHz > 0.0 ? kBrassScoopLengthGain : kReedScoopLengthGain) * scoopNow / 1200.0);
    if (anyFall && !fluteJet && fallNow > 0.0)
      length *= std::pow(2.0, (note.lipHz > 0.0 ? kBrassFallLengthGain : kReedFallLengthGain) * fallNow / 1200.0);
    // A tired reed embouchure: the reed is less pinched, its equivalent
    // length grows and the pitch sags (the scoop mechanism, 24.2b).
    if (tiring && note.lipHz == 0.0 && !fluteJet) {
      constexpr bool conical = requires(Voice& probe) { probe.setLipDampingScale(1.0); };
      length *= std::pow(2.0, (conical ? kSaxFatigueCents : kReedFatigueCents) * tired / 1200.0);
    }
    if constexpr (requires(Voice& probe) { probe.setEmbouchureCoverMeters(0.0); }) {
      if ((anyScoop || anyFall || tiring) && fluteJet)
        if (!(voice.setEmbouchureCoverMeters(
                length * (std::pow(2.0, (kFluteScoopCoverGain * scoopNow + kFluteFallCoverGain * fallNow +
                                         kFluteFatigueCoverCents * tired) / 1200.0) - 1.0))))
          return {};
    }
    if constexpr (requires(Voice& probe) { probe.setJetAimMeters(0.0); }) {
      if (tiring && fluteJet)  // the tired lip lets the jet's aim drift
        if (!(voice.setJetAimMeters(baseAim + kFluteFatigueAimMeters * tired))) return {};
    }
    if (!(voice.setSoundingLength(length))) return {};
    // Attack build-up (WindPlayerProfile::attackBuildUpPa, Bianco et al.
    // 2012): behind the closed tongue before a tongued note that follows
    // a sounding note, the pressure rises by the build-up over the
    // closure; after the release it falls back over attackDropSec.
    double buildUp = 0.0;
    if (player.attackBuildUpPa > 0.0 && airway != nullptr && !note.rest && note.tongue && sounding > 0 &&
        !notes[static_cast<std::size_t>(sounding - 1)].rest) {
      const double release = note.startSec, closure = static_cast<double>(kTongue) / rateHz;
      const double amount = player.attackBuildUpRefPa > 0.0
                                ? player.attackBuildUpPa * note.pressure * player.pressureScale / player.attackBuildUpRefPa
                                : player.attackBuildUpPa;
      if (tongueShut && time < release) {
        buildUp = amount * std::clamp((time - (release - closure)) / closure, 0.0, 1.0);
      } else if (time >= release && player.attackDropSec > 0.0 && time < release + player.attackDropSec) {
        buildUp = amount * (1.0 - (time - release) / player.attackDropSec);
      }
    }
    if (!(voice.setBlowingPressure(breathOff ? 0.0 : note.pressure * player.pressureScale * env + buildUp))) return {};
    // Brass vibrato is made at the lips and jaw (Fletcher & Rossing, The
    // Physics of Musical Instruments, 2nd ed., sec. 14.7, VERIFY section):
    // the lip resonance moves with the pitch. Moving only the bore pulled
    // the bore partial away from a fixed lip and the 1.2 s vibrato notes
    // of Jingle Bells (E4, G4) lost the lock and died (-45 dB end vs mid).
    if (note.lipHz > 0.0) {
      double crackRatio = crackCents != 0.0 ? std::pow(2.0, crackCents / 1200.0) : 1.0;
      if (scoopNow > 0.0) crackRatio *= std::pow(2.0, -kBrassScoopLipGain * scoopNow / 1200.0);
      if (fallNow > 0.0) crackRatio *= std::pow(2.0, -kBrassFallLipGain * fallNow / 1200.0);
      // A tired lip holds less tension: its resonance sags, most on high
      // notes, and it damps more.
      if (tiring) {
        crackRatio *= std::pow(2.0, -kBrassFatigueLipCents * tired * phrase_detail::registerHeight(note, player) / 1200.0);
        if constexpr (requires(Voice& probe) { probe.setLipDampingScale(1.0); }) {
          if (!(voice.setLipDampingScale(1.0 + kBrassFatigueDampGain * tired))) return {};
        }
      }
      if (!(voice.setLipResonanceHz(note.lipHz * player.lipRatioScale / vibrato * crackRatio))) return {};
    }
    if constexpr (requires(Voice& probe) { probe.setJetDelaySamples(0.0); }) {
      if (!fluteJet || note.rest || breathOff) {  // no breath, no jet to time
        if (fluteJet && note.jetSamples > 0.0) if (!(voice.setJetDelaySamples(note.jetSamples))) return {};
      } else {
      double jetNote = note.jetSamples;
      if (sounding > 0 && notes[static_cast<std::size_t>(sounding - 1)].jetSamples > 0.0) {
        const double glide = note.tongue ? player.tonguedGlideSec : player.slurGlideSec;
        double alpha = (time - note.startSec) / glide;
        if (alpha < 0.0) alpha = 0.0;
        if (alpha > 1.0) alpha = 1.0;
        alpha = alpha * alpha * (3.0 - 2.0 * alpha);
        const double prev = notes[static_cast<std::size_t>(sounding - 1)].jetSamples;
        jetNote = prev + (note.jetSamples - prev) * alpha;
      }
      if (crackSize > 0.0) jetNote *= std::pow(2.0, -crackSize / 1200.0);
      if (!(voice.setJetDelaySamples(jetNote / std::sqrt(std::max(env, 1.0e-3))))) return {};
      }
    }
    auto stepped = voice.renderBlock(out.data() + frame, FrameCount{count});
    if (!stepped.ok()) return {};
    frame += count;
  }
  for (double sample : out)
    if (!std::isfinite(sample)) return {};
  return out;
}

}  // namespace mmathen
