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

// Each note's onset error in cents (0 for rests): the note's own
// onsetErrorCents plus the player's random error.
[[nodiscard]] inline std::vector<double> phraseOnsetErrors(const std::vector<PhraseNote>& notes,
                                                           const WindPlayerProfile& player) {
  std::vector<double> e(notes.size(), 0.0);
  phrase_detail::CrackRng rng{player.crackSeed ? player.crackSeed : 1};
  for (std::size_t i = 0; i < notes.size(); ++i) {
    const double draw = player.crackSdCents > 0.0 ? rng.gauss() : 0.0;  // one draw per note, rests too
    if (notes[i].rest) continue;
    e[i] = notes[i].onsetErrorCents +
           player.crackSdCents * (1.0 + kCrackSoftAttackGain * phrase_detail::softness(notes[i].dynamic)) * draw;
  }
  return e;
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
  // Step 24.2a: onset errors; none at all leaves every control as before.
  const std::vector<double> onsetError = phraseOnsetErrors(notes, player);
  const bool anyCrack = std::any_of(onsetError.begin(), onsetError.end(), [](double x) { return x != 0.0; });
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
    if (anyCrack && note.lipHz == 0.0 && !fluteJet) {
      if constexpr (requires(Voice& probe) { probe.setReedDynamics(1.0, 1.0); }) {
        const double reedSize = std::max(crackSize, -kReedOvershootCapCents);
        if (!(voice.setReedDynamics(std::pow(2.0, -kReedResonanceCentsPerCent * reedSize / 1200.0),
                                    std::pow(2.0, -reedSize / kReedSlipCentsPerHalving)))) return {};
      }
    }
    double env = breathOff ? 0.0 : note.gain * (releasing >= 0 ? releaseLeft : 1.0);
    if (brassRelease >= 0.0) env *= brassRelease;
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
    if (note.vibrato && !note.rest && time > note.startSec + player.vibratoOnsetSec) {
      vibrato = note.lipHz > 0.0
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
      const double crackRatio = crackCents != 0.0 ? std::pow(2.0, crackCents / 1200.0) : 1.0;
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
