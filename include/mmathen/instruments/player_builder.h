#pragma once

// Player builder (step 23): a reflective description of every player
// profile's adjustable habits, so a user can build a custom player for any
// instrument that has a profile (player_profile.h, wind_player_profile.h).
//
//   playerInstruments()       every instrument with a player profile
//   builtInPlayer(i)          the instrument's named, cited profile
//   playerFields(i)           its adjustable habits: key, plain-English
//                             label, one-line tooltip, group, safe range,
//                             default (the built-in profile's own value),
//                             unit and the default's source tag
//   makePlayer(i, values)     a profile from slider values, each clamped
//                             to its field's safe range
//   loadPlayersFile /         named custom players in a plain text file
//   editPlayersFile           (defaultPlayersPath(): the user's app-data
//                             folder, %APPDATA%\mmathEn\players.txt);
//                             format 2 keeps what it can't read and reports
//                             problems by line (step 24.1)
//
// Defaults are read from the built-in profile at run time, so they always
// match it; makePlayer(i, defaultValues(i)) plays exactly as builtInPlayer(i)
// (tests/test_player_builder.cpp). Where a profile stores "0 = the
// harness's value" the field shows that value (for example the guitar's
// 10 ms finger damping) and writes it back explicitly, which the renderers
// treat the same.
//
// Safe ranges are not physics data: they keep each habit inside the span
// where the model stays well-behaved and the result still sounds like a
// player (UNSOURCED, chosen around each cited default).

#include <algorithm>
#include <charconv>
#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#include "bowed_note.h"
#include "phrase_player.h"
#include "player_profile.h"
#include "sleigh_bell.h"
#include "wind_player_profile.h"

namespace mmathen {

enum class PlayerInstrument {
  Violin, Viola, Cello, DoubleBassArco,
  ClassicalGuitar, DoubleBassPizz, Harp, Harpsichord, ElectricGuitar,
  Piano, Glockenspiel, Celesta, TubularBells, Handbells,
  SnareDrum, DrumKit, SleighBells,
  Flute, Clarinet, AltoSaxophone, Trumpet, Trombone,
};
inline constexpr int kPlayerInstrumentCount = 22;

enum class PlayerFamily { Bowed, Cello, Plucked, Struck, Kit, Shaken, Wind };

struct PlayerInstrumentInfo {
  PlayerInstrument id;
  const char* key;       // stable id used in the saved file
  const char* label;     // plain name shown to the user
  const char* category;  // list heading
  PlayerFamily family;
};

[[nodiscard]] inline const std::vector<PlayerInstrumentInfo>& playerInstruments() {
  using I = PlayerInstrument;
  using F = PlayerFamily;
  static const std::vector<PlayerInstrumentInfo> list{
      {I::Violin, "violin", "Violin", "Bowed strings", F::Bowed},
      {I::Viola, "viola", "Viola", "Bowed strings", F::Bowed},
      {I::Cello, "cello", "Cello", "Bowed strings", F::Cello},
      {I::DoubleBassArco, "bass_arco", "Double bass (bowed)", "Bowed strings", F::Bowed},
      {I::ClassicalGuitar, "guitar", "Classical guitar", "Plucked strings", F::Plucked},
      {I::DoubleBassPizz, "bass_pizz", "Double bass (plucked)", "Plucked strings", F::Plucked},
      {I::Harp, "harp", "Harp", "Plucked strings", F::Plucked},
      {I::Harpsichord, "harpsichord", "Harpsichord", "Plucked strings", F::Plucked},
      {I::ElectricGuitar, "electric_guitar", "Electric guitar", "Plucked strings", F::Plucked},
      {I::Piano, "piano", "Piano", "Keyboard & bells", F::Struck},
      {I::Glockenspiel, "glockenspiel", "Glockenspiel", "Keyboard & bells", F::Struck},
      {I::Celesta, "celesta", "Celesta", "Keyboard & bells", F::Struck},
      {I::TubularBells, "tubular_bells", "Tubular bells", "Keyboard & bells", F::Struck},
      {I::Handbells, "handbells", "Handbells", "Keyboard & bells", F::Struck},
      {I::SnareDrum, "snare", "Snare drum", "Drums & shakers", F::Struck},
      {I::DrumKit, "drum_kit", "Drum kit", "Drums & shakers", F::Kit},
      {I::SleighBells, "sleigh_bells", "Sleigh bells", "Drums & shakers", F::Shaken},
      {I::Flute, "flute", "Flute", "Woodwinds", F::Wind},
      {I::Clarinet, "clarinet", "Clarinet", "Woodwinds", F::Wind},
      {I::AltoSaxophone, "alto_sax", "Alto saxophone", "Woodwinds", F::Wind},
      {I::Trumpet, "trumpet", "Trumpet", "Brass", F::Wind},
      {I::Trombone, "trombone", "Trombone", "Brass", F::Wind},
  };
  return list;
}
[[nodiscard]] inline const PlayerInstrumentInfo& playerInstrumentInfo(PlayerInstrument i) {
  return playerInstruments()[static_cast<std::size_t>(i)];
}
[[nodiscard]] inline bool playerInstrumentFromKey(const std::string& key, PlayerInstrument& out) {
  for (const auto& info : playerInstruments())
    if (key == info.key) { out = info.id; return true; }
  return false;
}
[[nodiscard]] inline bool isBrass(PlayerInstrument i) { return i == PlayerInstrument::Trumpet || i == PlayerInstrument::Trombone; }

// Phrasing habits from the expressive-timing rules (music/expressive_timing.h,
// TimingRules: Friberg et al. 2006 KTH phrase arch, Repp 1992 variation),
// applied before the profile's scatter. All zero / one = off, so a built-in
// player is unchanged.
struct PlayerExpression {
  double tempoArch = 0.0;          // TimingRules::phraseArchDepth (UNSOURCED depth)
  double swellDb = 0.0;            // TimingRules::levelArchDb (VERIFY: a few dB)
  double phraseLevelSdDb = 0.0;    // TimingRules::phraseLevelSdDb (UNSOURCED)
  double intonationSdCents = 0.0;  // TimingRules::intonationSdCents (UNSOURCED), bowed strings
  double noteLength = 1.0;         // TimingRules::detachedRatio: 1 = full length
  double endingSlowdown = 1.0;     // TimingRules::finalTempoRatio over the last 1.5 s
  [[nodiscard]] bool isDefault() const noexcept {
    return tempoArch == 0.0 && swellDb == 0.0 && phraseLevelSdDb == 0.0 && intonationSdCents == 0.0 &&
           noteLength == 1.0 && endingSlowdown == 1.0;
  }
};
// The phrase-arch length the builder's phrases use: two bars of the 0.3 s
// beat (harness choice, UNSOURCED).
inline constexpr double kPlayerPhraseArchSeconds = 2.4;

// Player habits that live in the voices and harnesses rather than in the
// profile structs (step 23 scope addition). Negative = the voicing's or
// harness's own value.
struct PlayerExtras {
  // Bowed (BowedVoicing / BowExpression, bowed_note.h).
  double bowAttackSec = -1.0;      // voicing.attackSeconds
  double bowReleaseSec = -1.0;     // voicing.releaseSeconds
  double vibratoRampSec = -1.0;    // voicing.vibratoRampSeconds
  double bowNoise = 0.0;           // BowedStringVoice::setForceJitter (TUNED in leg25, default off)
  // Winds (ValveBlow / PhraseNote, phrase_player.h).
  double breathNoiseScale = 1.0;   // x the instrument's reed / jet turbulence
  double reedReleaseSec = -1.0;    // PhraseNote::releaseSec, kReedBreathReleaseSec
  double lipMassScale = 1.0;       // brass lip mass and stiffness together (wp1 WP_MASS)
  // Drums: stick speed at velocity 127 in the band harness's square law.
  double stickSpeedAt127 = 6.0;    // band1 / band3 (UNSOURCED)
  // Sleigh bells (ShakeGesture).
  double shakeHz = kSleighBellShakeHz;
  bool smoothStrokes = true;       // ShakeGesture::minimumJerkStrokes
  // Handbells: swing the bell (HandbellVoice::setSwing), m/s; 0 = fixed clapper.
  double handbellSwingMps = 0.0;
  // Glockenspiel: strike point x glockenspielStrikeFraction.
  double strikeSpotScale = 1.0;
};

// One player of one instrument: the profile struct of its family is used,
// the others stay default.
struct PlayerRig {
  PlayerInstrument instrument = PlayerInstrument::Violin;
  BowedPlayerProfile bowed;
  PluckedPlayerProfile plucked;
  StruckPlayerProfile struck;
  DrumKitPlayerProfile kit;
  ShakenPlayerProfile shaken;
  WindPlayerProfile wind;
  PlayerExpression expression;
  PlayerExtras extras;
  [[nodiscard]] PlayerFamily family() const { return playerInstrumentInfo(instrument).family; }
};

// The named, cited profile each instrument plays by default.
[[nodiscard]] inline PlayerRig builtInPlayer(PlayerInstrument i) {
  using I = PlayerInstrument;
  PlayerRig r;
  r.instrument = i;
  switch (i) {
    case I::Violin: r.bowed = violinistPlayerProfile(); break;
    case I::Viola: r.bowed = violistPlayerProfile(); break;
    case I::Cello: r.bowed = cellistPlayerProfile(); break;
    case I::DoubleBassArco: r.bowed = arcoBassistPlayerProfile(); break;
    case I::ClassicalGuitar: r.plucked = guitaristPlayerProfile(); break;
    case I::DoubleBassPizz: r.plucked = bassistPlayerProfile(); break;
    case I::Harp: r.plucked = harpistPlayerProfile(); break;
    case I::Harpsichord: r.plucked = harpsichordistPlayerProfile(); break;
    case I::ElectricGuitar: r.plucked = electricGuitaristPlayerProfile(); break;
    case I::Piano: r.struck = pianistPlayerProfile(); break;
    case I::Glockenspiel: r.struck = glockenspielPlayerProfile(); break;
    case I::Celesta: r.struck = celestaPlayerProfile(); break;
    case I::TubularBells: r.struck = tubularBellsPlayerProfile(); break;
    case I::Handbells: r.struck = handbellRingerPlayerProfile(); break;
    case I::SnareDrum: r.struck = snareDrummerPlayerProfile(); break;
    case I::DrumKit: r.kit = drumKitPlayerProfile(); break;
    case I::SleighBells: r.shaken = sleighBellPlayerProfile(); break;
    case I::Flute: r.wind = flutePlayerProfile(); break;
    case I::Clarinet: r.wind = clarinetPlayerProfile(); break;
    case I::AltoSaxophone: r.wind = saxophonePlayerProfile(); break;
    case I::Trumpet: r.wind = trumpetPlayerProfile(); break;
    case I::Trombone: r.wind = trombonePlayerProfile(); break;
  }
  return r;
}
[[nodiscard]] inline const char* builtInPlayerName(const PlayerRig& r) {
  switch (r.family()) {
    case PlayerFamily::Bowed: case PlayerFamily::Cello: return r.bowed.name;
    case PlayerFamily::Plucked: return r.plucked.name;
    case PlayerFamily::Struck: return r.struck.name;
    case PlayerFamily::Kit: return r.kit.name;
    case PlayerFamily::Shaken: return r.shaken.name;
    case PlayerFamily::Wind: return r.wind.name;
  }
  return "default";
}

// The voicing a bowed string plays on (its vibrato is the habit's default).
[[nodiscard]] inline BowedVoicing playerBowedVoicing(PlayerInstrument i) {
  return i == PlayerInstrument::Violin ? violinBowedVoicing()
         : i == PlayerInstrument::Viola ? violaBowedVoicing()
                                        : doubleBassBowedVoicing();
}

// Harness values a profile's "0 = default" stands for (the renderers in
// app/player_audition.h and tools/demo/band use the same numbers).
inline constexpr double kGuitarFingerDampSec = 0.010;      // band2 renderGuitar re-pluck damp, VERIFY
inline constexpr double kHarpFingerDampSec = 0.05;         // band3 hand fade, UNSOURCED
inline constexpr double kHarpsichordDamperSec = 0.03;      // band3 damper, UNSOURCED
inline constexpr double kPhraseTongueClosureSec = 0.040;   // phrase_player.h 1920 frames at 48 kHz

enum class FieldSource { Measured, Verify, Tuned, Unsourced };
[[nodiscard]] inline const char* fieldSourceLabel(FieldSource s) {
  switch (s) {
    case FieldSource::Measured: return "measured";
    case FieldSource::Verify: return "measured, verify";
    case FieldSource::Tuned: return "tuned";
    case FieldSource::Unsourced: return "unsourced";
  }
  return "";
}

struct PlayerField {
  const char* key = "";      // stable id in the saved file
  const char* label = "";    // plain English
  const char* tooltip = "";  // one line
  const char* group = "";    // slider group heading
  double minValue = 0.0, maxValue = 1.0;  // safe range, stored units
  double defaultValue = 0.0;              // the built-in profile's value
  const char* unit = "";                  // display unit
  double displayScale = 1.0;              // shown = stored x this
  FieldSource source = FieldSource::Unsourced;
  const char* citation = "";              // where the default comes from
  bool toggle = false;                    // on/off (0 or 1)
  bool integer = false;                   // whole numbers only
  double (*get)(const PlayerRig&) = nullptr;
  void (*set)(PlayerRig&, double) = nullptr;
};

[[nodiscard]] inline double clampFieldValue(const PlayerField& f, double v) {
  if (!std::isfinite(v)) return f.defaultValue;
  if (f.toggle) return v >= 0.5 ? 1.0 : 0.0;
  const double c = std::clamp(v, f.minValue, f.maxValue);
  return f.integer ? std::round(c) : c;
}

namespace player_builder_detail {
inline PlayerFeel& feelOf(PlayerRig& r) {
  switch (r.family()) {
    case PlayerFamily::Bowed: case PlayerFamily::Cello: return r.bowed.feel;
    case PlayerFamily::Plucked: return r.plucked.feel;
    case PlayerFamily::Struck: return r.struck.feel;
    case PlayerFamily::Kit: return r.kit.feel;
    case PlayerFamily::Shaken: return r.shaken.feel;
    case PlayerFamily::Wind: break;
  }
  return r.bowed.feel;  // winds have no PlayerFeel; not reached by any field
}
inline const PlayerFeel& feelOf(const PlayerRig& r) { return feelOf(const_cast<PlayerRig&>(r)); }

inline double bowedVibHz(const PlayerRig& r) {
  return r.bowed.strings.vibratoHz >= 0.0 ? r.bowed.strings.vibratoHz : playerBowedVoicing(r.instrument).vibratoHz;
}
inline double bowedVibCents(const PlayerRig& r) {
  return r.bowed.strings.vibratoCents >= 0.0 ? r.bowed.strings.vibratoCents : playerBowedVoicing(r.instrument).vibratoCents;
}
inline double bowedVibOnset(const PlayerRig& r) {
  return r.bowed.strings.vibratoOnsetSeconds >= 0.0 ? r.bowed.strings.vibratoOnsetSeconds
                                                    : playerBowedVoicing(r.instrument).vibratoOnsetSeconds;
}
inline BowedVoicing voicingFor(PlayerInstrument i) {
  return i == PlayerInstrument::Cello ? celloBowedVoicing() : playerBowedVoicing(i);
}
inline double orDefault(double v, double d) { return v >= 0.0 ? v : d; }
inline double fingerDampDefault(PlayerInstrument i) {
  switch (i) {
    case PlayerInstrument::ClassicalGuitar: return kGuitarFingerDampSec;
    case PlayerInstrument::DoubleBassPizz: return kDoubleBassPizzDampSec;
    case PlayerInstrument::Harpsichord: return kHarpsichordDamperSec;
    default: return kHarpFingerDampSec;
  }
}
inline PlayerField make(const char* key, const char* label, const char* tip, const char* group, double lo, double hi,
                        const char* unit, double scale, FieldSource src, const char* cite,
                        double (*get)(const PlayerRig&), void (*set)(PlayerRig&, double), bool toggle = false) {
  PlayerField f;
  f.key = key; f.label = label; f.tooltip = tip; f.group = group;
  f.minValue = lo; f.maxValue = hi; f.unit = unit; f.displayScale = scale;
  f.source = src; f.citation = cite; f.get = get; f.set = set; f.toggle = toggle;
  return f;
}
}  // namespace player_builder_detail

// The adjustable habits of instrument i, in display order, with the
// built-in profile's values as defaults.
[[nodiscard]] inline std::vector<PlayerField> playerFields(PlayerInstrument i) {
  using namespace player_builder_detail;
  using I = PlayerInstrument;
  using S = FieldSource;
  std::vector<PlayerField> v;
  const PlayerFamily fam = playerInstrumentInfo(i).family;
  const char* kFeel = "Timing & feel";

  // --- Shared feel (every non-wind family) ------------------------------
  if (fam != PlayerFamily::Wind) {
    const bool harpsichord = i == I::Harpsichord;
    const bool handbells = i == I::Handbells;
    const bool snare = i == I::SnareDrum;
    if (!harpsichord) v.push_back(make("velocityScale", "Overall loudness", "How hard every note is played (1 = as written).", kFeel,
                     0.3, 2.0, "x", 1.0, S::Unsourced, "1 = the written dynamics",
                     [](const PlayerRig& r) { return feelOf(r).velocityScale; },
                     [](PlayerRig& r, double x) { feelOf(r).velocityScale = x; }));
    if (!harpsichord) v.push_back(make("velocityCv", "Loudness variety", "How much the loudness wobbles from note to note.", kFeel,
                     0.0, 0.30, "%", 100.0, S::Verify,
                     "Goebl & Palmer 2013, PLoS ONE 8 e50901 (pianists)",
                     [](const PlayerRig& r) { return feelOf(r).velocityCv; },
                     [](PlayerRig& r, double x) { feelOf(r).velocityCv = x; }));
    v.push_back(make("timingCv", "Timing looseness", "How far notes drift off the beat, as a share of the beat.", kFeel,
                     0.0, 0.20, "%", 100.0, handbells ? S::Verify : S::Measured,
                     handbells ? "Hofmann, Wesolowski & Goebl 2017, PLoS ONE (between players)"
                               : "Goebl & Palmer 2008, Exp. Brain Res. 186 (lower bound)",
                     [](const PlayerRig& r) { return feelOf(r).timingCv; },
                     [](PlayerRig& r, double x) { feelOf(r).timingCv = x; }));
    v.push_back(make("timingSdMaxSec", "Drift limit", "Most a note may drift, however long the beat (0 = no limit).", kFeel,
                     0.0, 0.080, "ms", 1000.0, S::Verify, "Rasch 1979, Acustica 43 (ensemble asynchrony)",
                     [](const PlayerRig& r) { return feelOf(r).timingSdMaxSec; },
                     [](PlayerRig& r, double x) { feelOf(r).timingSdMaxSec = x; }));
    v.push_back(make("timingOffsetSec", "Rushing / dragging", "Plays every note early (-) or late (+).", kFeel,
                     -0.050, 0.050, "ms", 1000.0, snare ? S::Measured : S::Unsourced,
                     snare ? "Fujii et al. 2011, Music Perception 28 (snare 10 ms ahead)" : "0 = on the beat",
                     [](const PlayerRig& r) { return feelOf(r).timingOffsetSec; },
                     [](PlayerRig& r, double x) { feelOf(r).timingOffsetSec = x; }));
    v.push_back(make("seed", "Performance take", "Pick a different take: same habits, different random wobbles.", kFeel,
                     1.0, 999.0, "", 1.0, S::Unsourced, "the profile's own take",
                     [](const PlayerRig& r) { return double(feelOf(r).seed % 1000 == 0 ? 1 : feelOf(r).seed % 1000); },
                     [](PlayerRig& r, double x) {
                       const auto want = static_cast<std::uint64_t>(std::llround(x));
                       if (want != (feelOf(r).seed % 1000 == 0 ? 1 : feelOf(r).seed % 1000)) feelOf(r).seed = want;
                     }));
    v.back().integer = true;
  }

  // --- Bowed: violin, viola, arco bass ------------------------------------
  if (fam == PlayerFamily::Bowed || fam == PlayerFamily::Cello) {
    const bool vc = fam == PlayerFamily::Cello;
    const char* kBow = "Bow";
    const char* kVib = "Vibrato";
    const char* bowCite = "Askenfelt 1989 / Schoonderwaldt 2009: x1 = the voicing's measured stroke";
    if (vc) {
      v.push_back(make("bowForceScale", "Bow pressure", "How hard the bow presses on the string.", kBow, 0.5, 2.0, "x", 1.0,
                       S::Verify, bowCite, [](const PlayerRig& r) { return r.bowed.cello.bowForceScale; },
                       [](PlayerRig& r, double x) { r.bowed.cello.bowForceScale = x; }));
      v.push_back(make("bowSpeedScale", "Bow speed", "How fast the bow moves: faster is louder and brighter.", kBow, 0.5, 2.0, "x", 1.0,
                       S::Verify, bowCite, [](const PlayerRig& r) { return r.bowed.cello.bowSpeedScale; },
                       [](PlayerRig& r, double x) { r.bowed.cello.bowSpeedScale = x; }));
      v.push_back(make("contactPointScale", "Bow position", "Nearer the bridge (below 1, edgier) or the fingerboard (above 1, softer).", kBow,
                       0.6, 1.6, "x", 1.0, S::Verify, bowCite, [](const PlayerRig& r) { return r.bowed.cello.contactPointScale; },
                       [](PlayerRig& r, double x) { r.bowed.cello.contactPointScale = x; }));
      v.push_back(make("vibratoHz", "Vibrato speed", "Pitch wobbles per second.", kVib, 3.0, 8.0, "per sec", 1.0, S::Verify,
                       "Geringer & Allen 2004, JRME 52 (5.5 Hz)", [](const PlayerRig& r) { return r.bowed.cello.vibratoHz; },
                       [](PlayerRig& r, double x) { r.bowed.cello.vibratoHz = x; }));
      v.push_back(make("vibratoCents", "Vibrato depth", "How wide the pitch wobble is, either side of the note.", kVib, 0.0, 40.0, "cents", 1.0,
                       S::Verify, "Geringer & Allen 2004; MacLeod 2010 (+/-11 c)", [](const PlayerRig& r) { return r.bowed.cello.vibratoCents; },
                       [](PlayerRig& r, double x) { r.bowed.cello.vibratoCents = x; }));
      v.push_back(make("vibratoOnsetSeconds", "Vibrato delay", "How long a note is held before the wobble starts.", kVib, 0.0, 1.0, "s", 1.0,
                       S::Unsourced, "kCelloVibratoOnsetSeconds", [](const PlayerRig& r) { return r.bowed.cello.vibratoOnsetSeconds; },
                       [](PlayerRig& r, double x) { r.bowed.cello.vibratoOnsetSeconds = x; }));
    } else {
      const char* vibCite = i == I::DoubleBassArco ? "Double bass vibrato study 2024, String Research J. (5.17 Hz, +/-9.5 c)"
                                                   : "MacLeod 2008, JRME 56 (violin 5.71 Hz +/-25.5 c, viola 5.49 Hz +/-20.5 c)";
      v.push_back(make("bowForceScale", "Bow pressure", "How hard the bow presses on the string.", kBow, 0.5, 2.0, "x", 1.0,
                       S::Verify, bowCite, [](const PlayerRig& r) { return r.bowed.strings.bowForceScale; },
                       [](PlayerRig& r, double x) { r.bowed.strings.bowForceScale = x; }));
      v.push_back(make("bowSpeedScale", "Bow speed", "How fast the bow moves: faster is louder and brighter.", kBow, 0.5, 2.0, "x", 1.0,
                       S::Verify, bowCite, [](const PlayerRig& r) { return r.bowed.strings.bowSpeedScale; },
                       [](PlayerRig& r, double x) { r.bowed.strings.bowSpeedScale = x; }));
      v.push_back(make("contactPointScale", "Bow position", "Nearer the bridge (below 1, edgier) or the fingerboard (above 1, softer).", kBow,
                       0.6, 1.6, "x", 1.0, S::Verify, bowCite, [](const PlayerRig& r) { return r.bowed.strings.contactPointScale; },
                       [](PlayerRig& r, double x) { r.bowed.strings.contactPointScale = x; }));
      v.push_back(make("vibratoHz", "Vibrato speed", "Pitch wobbles per second.", kVib, 3.0, 8.0, "per sec", 1.0, S::Measured, vibCite,
                       [](const PlayerRig& r) { return bowedVibHz(r); }, [](PlayerRig& r, double x) { r.bowed.strings.vibratoHz = x; }));
      v.push_back(make("vibratoCents", "Vibrato depth", "How wide the pitch wobble is, either side of the note.", kVib, 0.0, 40.0, "cents", 1.0,
                       S::Measured, vibCite, [](const PlayerRig& r) { return bowedVibCents(r); },
                       [](PlayerRig& r, double x) { r.bowed.strings.vibratoCents = x; }));
      v.push_back(make("vibratoOnsetSeconds", "Vibrato delay", "How long a note is held before the wobble starts.", kVib, 0.0, 1.0, "s", 1.0,
                       S::Unsourced, "the voicing's vibrato onset", [](const PlayerRig& r) { return bowedVibOnset(r); },
                       [](PlayerRig& r, double x) { r.bowed.strings.vibratoOnsetSeconds = x; }));
    }
  }

  // --- Plucked ---------------------------------------------------------------
  if (fam == PlayerFamily::Plucked) {
    const char* kPluck = "Plucking hand";
    // The harpsichord's plectrum sets the loudness (Fletcher & Rossing sec.
    // 11.10), so its touch has no loudness habits; the plectrum strength
    // stands in for a harder or softer quill.
    v.push_back(make("pluckForceScale", i == I::Harpsichord ? "Plectrum strength" : "Pluck strength",
                     i == I::Harpsichord ? "How stiff the quill is: how far it pulls the string before letting go."
                                         : "How far the finger pulls the string before letting go.", kPluck, 0.25, 2.5, "x", 1.0,
                     S::Unsourced, "1 = the harness's velocity mapping", [](const PlayerRig& r) { return r.plucked.pluckForceScale; },
                     [](PlayerRig& r, double x) { r.plucked.pluckForceScale = x; }));
    if (i == I::Harp) {
      v.push_back(make("pluckFraction", "Pluck spot", "Where along the string it is plucked: low = near the soundboard, brighter.", kPluck,
                       0.10, 0.50, "of length", 1.0, S::Measured, "Chadefaux et al. 2012, JASA 131 (1/3 to 2/5)",
                       [](const PlayerRig& r) { return r.plucked.pluckFraction; },
                       [](PlayerRig& r, double x) { r.plucked.pluckFraction = x; }));
    } else if (i != I::Harpsichord) {
      const double lo = i == I::DoubleBassPizz ? 0.08 : 0.03, hi = i == I::DoubleBassPizz ? 0.60 : 0.30;
      const char* cite = i == I::ClassicalGuitar ? "Traube & Smith 2000/2001 (12-14 cm)"
                         : i == I::ElectricGuitar ? "Zollner, Physics of the Electric Guitar 1.5 (6-10 cm)"
                                                  : "kDoubleBassPizzFromBridgeM, teaching practice (VERIFY)";
      PlayerField f = make("pluckFromBridgeM", "Pluck spot", "Distance from the bridge: nearer is thinner and brighter.", kPluck, lo, hi, "cm",
                           100.0, i == I::DoubleBassPizz ? S::Verify : S::Measured, cite,
                           [](const PlayerRig& r) { return r.plucked.pluckFromBridgeM; },
                           [](PlayerRig& r, double x) { r.plucked.pluckFromBridgeM = x; });
      v.push_back(f);
    }
    const bool hc = i == I::Harpsichord;
    v.push_back(make("fingerDampSec", hc ? "Damper speed" : "Mute speed",
                     hc ? "How quickly the damper stops a note when the key is let go."
                        : "How quickly the hand stops a ringing string at the end of a note.",
                     kPluck, 0.003, 0.25, "ms", 1000.0, i == I::ClassicalGuitar ? S::Verify : S::Unsourced,
                     i == I::DoubleBassPizz ? "kDoubleBassPizzDampSec" : "the band harness's fade",
                     [](const PlayerRig& r) { return r.plucked.fingerDampSec > 0.0 ? r.plucked.fingerDampSec : fingerDampDefault(r.instrument); },
                     [](PlayerRig& r, double x) { r.plucked.fingerDampSec = x; }));
  }

  // --- Piano -------------------------------------------------------------------
  if (i == I::Piano) {
    const char* kKeys = "Keys";
    v.push_back(make("pianoKeyAction", "Louder notes sound first", "Real key action: a harder key reaches the string a little sooner.", kKeys,
                     0.0, 1.0, "", 1.0, S::Measured, "Goebl 2001, JASA 110 (Askenfelt & Jansson 1991)",
                     [](const PlayerRig& r) { return r.struck.pianoKeyAction ? 1.0 : 0.0; },
                     [](PlayerRig& r, double x) { r.struck.pianoKeyAction = x >= 0.5; }, true));
    v.push_back(make("continuousHammerSpeed", "Smooth loudness", "Every small change in touch is heard (off = six loudness steps).", kKeys,
                     0.0, 1.0, "", 1.0, S::Unsourced, "interpolates Askenfelt & Jansson's six hammer speeds",
                     [](const PlayerRig& r) { return r.struck.continuousHammerSpeed ? 1.0 : 0.0; },
                     [](PlayerRig& r, double x) { r.struck.continuousHammerSpeed = x >= 0.5; }, true));
  }

  // --- Drum kit ------------------------------------------------------------------
  if (fam == PlayerFamily::Kit) {
    const char* kLimbs = "Hands and feet";
    const char* fujii = "Fujii et al. 2011, Music Perception 28";
    v.push_back(make("limbRightHandSec", "Hi-hat hand early / late", "Right hand (hi-hat, cymbal): early (-) or late (+).", kLimbs,
                     -0.040, 0.040, "ms", 1000.0, S::Verify, fujii, [](const PlayerRig& r) { return r.kit.limbOffsetSec[0]; },
                     [](PlayerRig& r, double x) { r.kit.limbOffsetSec[0] = x; }));
    v.push_back(make("limbLeftHandSec", "Snare hand early / late", "Left hand (snare, toms): early (-) or late (+).", kLimbs,
                     -0.040, 0.040, "ms", 1000.0, S::Measured, fujii, [](const PlayerRig& r) { return r.kit.limbOffsetSec[1]; },
                     [](PlayerRig& r, double x) { r.kit.limbOffsetSec[1] = x; }));
    v.push_back(make("limbRightFootSec", "Kick foot early / late", "Right foot (bass drum): early (-) or late (+).", kLimbs,
                     -0.040, 0.040, "ms", 1000.0, S::Measured, fujii, [](const PlayerRig& r) { return r.kit.limbOffsetSec[2]; },
                     [](PlayerRig& r, double x) { r.kit.limbOffsetSec[2] = x; }));
    v.push_back(make("crashStickSpeedAt127", "Crash cymbal swing", "How fast the stick hits the crash cymbal on the loudest note.", kLimbs,
                     2.0, 15.0, "m/s", 1.0, S::Verify, "Dahl, Grossbach & Altenmueller 2011 (f strokes 9-12.5 m/s)",
                     [](const PlayerRig& r) { return r.kit.crashStickSpeedAt127; },
                     [](PlayerRig& r, double x) { r.kit.crashStickSpeedAt127 = x; }));
  }

  // --- Sleigh bells ----------------------------------------------------------------
  if (fam == PlayerFamily::Shaken) {
    v.push_back(make("shakeAmplitudeScale", "Shake size", "How big the steady shaking movement is (1 = the recording's).", "Shaking",
                     0.25, 2.0, "x", 1.0, S::Measured, "the reference recording's shake (kSleighBellShakeHz timeline)",
                     [](const PlayerRig& r) { return r.shaken.shakeAmplitudeScale; },
                     [](PlayerRig& r, double x) { r.shaken.shakeAmplitudeScale = x; }));
  }

  // --- Winds and brass --------------------------------------------------------------
  if (fam == PlayerFamily::Wind) {
    const bool brass = isBrass(i);
    const char* kBreath = "Breath";
    const char* kTongue = "Note starts & ends";
    const char* kVib = "Vibrato";
    const char* kNotes = "Note starts & ends";
    v.push_back(make("pressureScale", "Breath strength", "How hard the player blows (1 = the instrument's usual pressure).", kBreath,
                     0.75, 1.35, "x", 1.0, S::Measured, "Fuks & Sundberg 1999; Bianco et al. 2012 (x1 inside the measured range)",
                     [](const PlayerRig& r) { return r.wind.pressureScale; }, [](PlayerRig& r, double x) { r.wind.pressureScale = x; }));
    v.push_back(make("tongueClosureSec", "Gap between tongued notes", "How long the tongue stops the air before a tongued note.", kTongue,
                     0.015, 0.100, "ms", 1000.0, S::Verify, "Bertsch & Hoole, fast single tonguing 40-64 ms",
                     [](const PlayerRig& r) { return r.wind.tongueClosureSec > 0.0 ? r.wind.tongueClosureSec : kPhraseTongueClosureSec; },
                     [](PlayerRig& r, double x) { r.wind.tongueClosureSec = x; }));
    v.push_back(make("tongueReleaseSec", "Tongue release", "How quickly the tongue lets the air go: fast is crisp, slow is soft.", kTongue,
                     0.002, 0.030, "ms", 1000.0, S::Measured, "Li, Almeida, Smith & Wolfe 2016, JASA 140 (8 ms)",
                     [](const PlayerRig& r) { return r.wind.tongueReleaseSec; }, [](PlayerRig& r, double x) { r.wind.tongueReleaseSec = x; }));
    if (brass) {
      v.push_back(make("attackBuildUpPa", "Attack push", "Extra air pressure built behind the tongue: a punchier start.", kTongue,
                       0.0, 2000.0, "Pa", 1.0, S::Measured, "Bianco et al. 2012, JNMR 41 (0.56 +/- 0.48 kPa)",
                       [](const PlayerRig& r) { return r.wind.attackBuildUpPa; }, [](PlayerRig& r, double x) { r.wind.attackBuildUpPa = x; }));
      v.push_back(make("attackDropSec", "Attack push length", "How long the extra push takes to fade after the note starts.", kTongue,
                       0.010, 0.150, "ms", 1000.0, S::Verify, "Bianco et al. 2012 Fig. 8 trend; 60 ms swept on Jingle Bells",
                       [](const PlayerRig& r) { return r.wind.attackDropSec > 0.0 ? r.wind.attackDropSec : 0.060; },
                       [](PlayerRig& r, double x) { r.wind.attackDropSec = x; }));
      v.push_back(make("brassVibratoHz", "Vibrato speed", "Pitch wobbles per second.", kVib, 3.0, 8.0, "per sec", 1.0, S::Measured,
                       "VSCO 2 CE trumpet sustain-vibrato (5.84 Hz)", [](const PlayerRig& r) { return r.wind.brassVibratoHz; },
                       [](PlayerRig& r, double x) { r.wind.brassVibratoHz = x; }));
      v.push_back(make("brassVibratoCents", "Vibrato depth", "How wide the pitch wobble is.", kVib, 0.0, 35.0, "cents", 1.0, S::Tuned,
                       "kBrassVibratoCents, TUNED to the VSCO extent", [](const PlayerRig& r) { return r.wind.brassVibratoCents; },
                       [](PlayerRig& r, double x) { r.wind.brassVibratoCents = x; }));
    } else {
      const char* vcite = i == I::Flute ? "Fletcher 1975, JASA 57 (about 5 Hz, breath vibrato)"
                          : i == I::AltoSaxophone ? "concert saxophone vibrato thesis 2013 (5.96 Hz; depth TUNED)"
                                                  : "classical clarinet plays straight (practice, VERIFY)";
      v.push_back(make("vibratoHz", "Vibrato speed", "Pitch wobbles per second.", kVib, 3.0, 8.0, "per sec", 1.0,
                       i == I::Clarinet ? S::Unsourced : S::Measured, vcite, [](const PlayerRig& r) { return r.wind.vibratoHz; },
                       [](PlayerRig& r, double x) { r.wind.vibratoHz = x; }));
      v.push_back(make("vibratoCents", "Vibrato depth", "How wide the pitch wobble is (0 = straight tone).", kVib, 0.0, 40.0, "cents", 1.0,
                       i == I::AltoSaxophone ? S::Tuned : S::Verify, vcite, [](const PlayerRig& r) { return r.wind.vibratoCents; },
                       [](PlayerRig& r, double x) { r.wind.vibratoCents = x; }));
      v.push_back(make("vibratoPressureFraction", "Breath vibrato", "Wobble made with the breath (loudness pulses), as a share of the air.", kVib,
                       0.0, 0.25, "%", 100.0, i == I::Flute ? S::Measured : S::Unsourced,
                       i == I::Flute ? "Fletcher 1975 (10 % pressure)" : "0 = none", [](const PlayerRig& r) { return r.wind.vibratoPressureFraction; },
                       [](PlayerRig& r, double x) { r.wind.vibratoPressureFraction = x; }));
    }
    v.push_back(make("vibratoOnsetSec", "Vibrato delay", "How long a note is held before the wobble starts.", kVib, 0.0, 1.0, "s", 1.0,
                     S::Unsourced, "phrase player default 0.20 s", [](const PlayerRig& r) { return r.wind.vibratoOnsetSec; },
                     [](PlayerRig& r, double x) { r.wind.vibratoOnsetSec = x; }));
    if (!brass)
      v.push_back(make("slurGlideSec", "Slur smoothness", "How long a slurred note takes to slide into the next.", kNotes, 0.005, 0.100, "ms",
                       1000.0, S::Unsourced, "woodwind bore glide 35 ms", [](const PlayerRig& r) { return r.wind.slurGlideSec; },
                       [](PlayerRig& r, double x) { r.wind.slurGlideSec = x; }));
    if (brass) {
      v.push_back(make("lipRatioScale", "Lip tightness", "Tighter lips (above 1) push the pitch and tone up.", "Tone", 0.92, 1.08, "x", 1.0,
                       S::Unsourced, "1 = the instrument row's lip resonance", [](const PlayerRig& r) { return r.wind.lipRatioScale; },
                       [](PlayerRig& r, double x) { r.wind.lipRatioScale = x; }));
    }
    v.push_back(make("intonationCents", "Sharp / flat habit", "Plays every note a little sharp (+) or flat (-).", "Tone", -30.0, 30.0, "cents",
                     1.0, S::Unsourced, "0 = in tune", [](const PlayerRig& r) { return r.wind.intonationCents; },
                     [](PlayerRig& r, double x) { r.wind.intonationCents = x; }));
  }

  // --- Step 23 scope addition: the other realism controls -------------------
  const char* kPhr = "Phrasing (shaping the line)";
  const bool hasExpression = fam != PlayerFamily::Wind && fam != PlayerFamily::Shaken;
  if (hasExpression) {
    v.push_back(make("tempoArch", "Phrase push and pull", "Speeds up into the middle of each phrase and relaxes at its ends.", kPhr,
                     0.0, 0.30, "%", 100.0, S::Unsourced, "KTH phrase-arch rule, Friberg et al. 2006 (depth UNSOURCED; 0 = off)",
                     [](const PlayerRig& r) { return r.expression.tempoArch; }, [](PlayerRig& r, double x) { r.expression.tempoArch = x; }));
    const bool fixedLevel = i == PlayerInstrument::Harpsichord;  // one loudness: swells do nothing
    const bool fixedRing = i == PlayerInstrument::Glockenspiel || i == PlayerInstrument::Celesta || i == PlayerInstrument::TubularBells ||
                           i == PlayerInstrument::Handbells || i == PlayerInstrument::SnareDrum || i == PlayerInstrument::DrumKit ||
                           i == PlayerInstrument::ClassicalGuitar;  // the guitar lets notes ring until the string is replayed
    if (!fixedLevel) v.push_back(make("swellDb", "Phrase swell", "Gets louder toward the middle of each phrase and softer at its ends.", kPhr,
                     0.0, 12.0, "dB", 1.0, S::Verify, "KTH phrase-arch rule, Friberg et al. 2006 (a few dB; 0 = off)",
                     [](const PlayerRig& r) { return r.expression.swellDb; }, [](PlayerRig& r, double x) { r.expression.swellDb = x; }));
    if (!fixedLevel) v.push_back(make("phraseLevelSdDb", "Phrase-to-phrase loudness", "Plays each phrase a little louder or softer than the last.", kPhr,
                     0.0, 6.0, "dB", 1.0, S::Unsourced, "Repp 1992, JASA 92 (repeats vary; size UNSOURCED; 0 = off)",
                     [](const PlayerRig& r) { return r.expression.phraseLevelSdDb; }, [](PlayerRig& r, double x) { r.expression.phraseLevelSdDb = x; }));
    if (!fixedRing) v.push_back(make("noteLength", "Note length", "1 = notes held their full length; lower = shorter, separated notes.", "Note starts & ends",
                     0.3, 1.0, "x", 1.0, S::Unsourced, "TimingRules detachedRatio (1 = as written)",
                     [](const PlayerRig& r) { return r.expression.noteLength; }, [](PlayerRig& r, double x) { r.expression.noteLength = x; }));
    v.push_back(make("endingSlowdown", "Slow down at the end", "How much the last notes slow down (1 = no slowing).", kPhr,
                     1.0, 2.0, "x", 1.0, S::Unsourced, "final-ritard rule (Friberg & Sundberg 1999 shape; 1 = off)",
                     [](const PlayerRig& r) { return r.expression.endingSlowdown; }, [](PlayerRig& r, double x) { r.expression.endingSlowdown = x; }));
  }
  if (fam == PlayerFamily::Bowed || fam == PlayerFamily::Cello) {
    const char* kEnds = "Note starts & ends";
    v.push_back(make("intonationSdCents", "Out-of-tune wobble", "How far each note lands off pitch, at random (0 = perfectly in tune).", "Mistakes",
                     0.0, 25.0, "cents", 1.0, S::Unsourced, "TimingRules intonationSdCents (0 = off)",
                     [](const PlayerRig& r) { return r.expression.intonationSdCents; }, [](PlayerRig& r, double x) { r.expression.intonationSdCents = x; }));
    v.push_back(make("bowAttackSec", "Bow start", "How long the bow takes to get the string going: short = crisp, long = soft.", kEnds,
                     0.005, 0.25, "ms", 1000.0, S::Tuned, "the voicing's attack (kCello/kDoubleBassArcoAttackSeconds)",
                     [](const PlayerRig& r) { return orDefault(r.extras.bowAttackSec, voicingFor(r.instrument).attackSeconds); },
                     [](PlayerRig& r, double x) { r.extras.bowAttackSec = x; }));
    v.push_back(make("bowReleaseSec", "Bow lift", "How gently the bow leaves the string at the end of a note.", kEnds,
                     0.01, 0.40, "ms", 1000.0, S::Tuned, "the voicing's release",
                     [](const PlayerRig& r) { return orDefault(r.extras.bowReleaseSec, voicingFor(r.instrument).releaseSeconds); },
                     [](PlayerRig& r, double x) { r.extras.bowReleaseSec = x; }));
    v.push_back(make("vibratoRampSec", "Vibrato build-up", "How long the wobble takes to grow to full width once it starts.", "Vibrato",
                     0.0, 1.0, "s", 1.0, S::Unsourced, "the voicing's vibrato ramp",
                     [](const PlayerRig& r) { return orDefault(r.extras.vibratoRampSec, voicingFor(r.instrument).vibratoRampSeconds); },
                     [](PlayerRig& r, double x) { r.extras.vibratoRampSec = x; }));
    v.push_back(make("bowNoise", "Bow hair noise", "Scratchy grain from the bow hair and rosin (0 = perfectly smooth).", "Tone",
                     0.0, 0.30, "%", 100.0, S::Unsourced, "setForceJitter: no published value (TUNED in the Caprice harness), 0 = off",
                     [](const PlayerRig& r) { return r.extras.bowNoise; }, [](PlayerRig& r, double x) { r.extras.bowNoise = x; }));
  }
  if (fam == PlayerFamily::Wind) {
    v.push_back(make("breathNoiseScale", "Breath noise", "How much air hiss is heard in the tone (1 = the instrument's usual).", "Tone",
                     // The flute's jet needs some noise to start and stops speaking at 2x
                     // (box probe), so its range is 0.1x..1.5x.
                     i == I::Flute ? 0.1 : 0.0, i == I::Flute ? 1.5 : 3.0, "x", 1.0, S::Tuned, "the instrument's reed / jet turbulence (wind_presets.h, TUNED)",
                     [](const PlayerRig& r) { return r.extras.breathNoiseScale; }, [](PlayerRig& r, double x) { r.extras.breathNoiseScale = x; }));
    // Step 24.2a: the onset embouchure error that lets the lips / reed / jet
    // lock onto a neighbouring resonance for a moment (phrase_player.h). No
    // measured player rate was found, so 0 = off for every named player.
    v.push_back(make("crackSdCents", "Cracked notes", "How often a note cracks: the lips, reed or air jet catch the wrong note for a moment (0 = never).", "Mistakes",
                     0.0, 1200.0, "cents", 1.0, S::Unsourced, "onset embouchure error (Wolfe 2022; Velut et al. 2017); no measured rate, 0 = off",
                     [](const PlayerRig& r) { return r.wind.crackSdCents; }, [](PlayerRig& r, double x) { r.wind.crackSdCents = x; }));
    // Step 24.2b: scoops, a slide up into the note made by the embouchure
    // (phrase_player.h). No measured rate or depth for wind players, so
    // 0 = off for every named player.
    v.push_back(make("scoopCents", "Scoop depth", "How far below the note the player starts and slides up into it, like a jazz scoop (0 = no scoops; 100 = a half step).", "Note starts & ends",
                     0.0, isBrass(i) ? 150.0 : 200.0, "cents", 1.0, S::Unsourced,
                     "sung scoops 50-150 c (Larrouy-Maestri & Pfordresher 2018); wind depth UNSOURCED, 0 = off",
                     [](const PlayerRig& r) { return r.wind.scoopCents; }, [](PlayerRig& r, double x) { r.wind.scoopCents = x; }));
    v.push_back(make("scoopSec", "Scoop time", "How long the slide up into the note takes (0 = the usual, about 0.2 s). Needs some scoop depth.", "Note starts & ends",
                     0.0, 0.40, "ms", 1000.0, S::Unsourced, "0 = kScoopDefaultSec, 220 ms (Larrouy-Maestri & Pfordresher 2018)",
                     [](const PlayerRig& r) { return r.wind.scoopSec; }, [](PlayerRig& r, double x) { r.wind.scoopSec = x; }));
    v.push_back(make("scoopChance", "Scoop how often", "Share of tongued notes that get a scoop (100% = every one). Needs some scoop depth.", "Note starts & ends",
                     0.0, 1.0, "%", 100.0, S::Unsourced, "jazz soloists: 0.8% of tones, Bechet ~15% (Abesser et al., ISMIR 2015); default 100% of the depth's notes",
                     [](const PlayerRig& r) { return r.wind.scoopChance; }, [](PlayerRig& r, double x) { r.wind.scoopChance = x; }));
    // Step 24.2c: fall-offs, the end of a note dropping in pitch as the
    // breath fades (phrase_player.h). No measured depth or length for wind
    // players, so 0 = off for every named player.
    v.push_back(make("fallCents", "Fall-off depth", "How far the pitch drops at the end of a note, like a jazz fall (0 = no fall-offs; 100 = a half step, 1200 = an octave).", "Note starts & ends",
                     0.0, 1200.0, "cents", 1.0, S::Unsourced,
                     "jazz fall-offs annotated, no depth published (Abesser et al., ISMIR 2015); 0 = off",
                     [](const PlayerRig& r) { return r.wind.fallCents; }, [](PlayerRig& r, double x) { r.wind.fallCents = x; }));
    v.push_back(make("fallSec", "Fall-off length", "How long the drop at the end of the note takes (0 = the usual, 0.3 s). Needs some fall-off depth.", "Note starts & ends",
                     0.0, 1.0, "ms", 1000.0, S::Unsourced, "0 = kFallDefaultSec, 300 ms (UNSOURCED)",
                     [](const PlayerRig& r) { return r.wind.fallSec; }, [](PlayerRig& r, double x) { r.wind.fallSec = x; }));
    v.push_back(make("fallChance", "Fall-off how often", "Share of the notes that can fall that do (100% = every one). Needs some fall-off depth.", "Note starts & ends",
                     0.0, 1.0, "%", 100.0, S::Unsourced, "jazz soloists: 0.17% of tones (Abesser et al., ISMIR 2015); default 100%",
                     [](const PlayerRig& r) { return r.wind.fallChance; }, [](PlayerRig& r, double x) { r.wind.fallChance = x; }));
    v.push_back(make("fallPhraseEndsOnly", "Fall-offs only at phrase ends", "On: only the last note of a phrase (or a long held note) falls. Off: any note that is not slurred into the next can.", "Note starts & ends",
                     0.0, 1.0, "", 1.0, S::Unsourced, "phrase-end heuristic (phrase_player.h, UNSOURCED); on by default",
                     [](const PlayerRig& r) { return r.wind.fallPhraseEndsOnly ? 1.0 : 0.0; },
                     [](PlayerRig& r, double x) { r.wind.fallPhraseEndsOnly = x >= 0.5; }, true));
    if (!isBrass(i) && i != I::Flute)
      v.push_back(make("reedReleaseSec", "Note ending fade", "How gently the breath stops at the end of a phrase.", "Note starts & ends",
                       0.03, 0.60, "ms", 1000.0, S::Tuned, "kReedBreathReleaseSec, TUNED to Li et al. 2016",
                       [](const PlayerRig& r) { return orDefault(r.extras.reedReleaseSec, kReedBreathReleaseSec); },
                       [](PlayerRig& r, double x) { r.extras.reedReleaseSec = x; }));
    if (isBrass(i))
      v.push_back(make("lipMassScale", "Lip heaviness", "Heavier lips (same tuning) respond more slowly and sound darker.", "Tone",
                       0.6, 1.6, "x", 1.0, S::Unsourced, "x the instrument's lip mass and stiffness (wp1 WP_MASS sweep)",
                       [](const PlayerRig& r) { return r.extras.lipMassScale; }, [](PlayerRig& r, double x) { r.extras.lipMassScale = x; }));
  }
  if (i == I::SnareDrum || fam == PlayerFamily::Kit)
    v.push_back(make("stickSpeedAt127", "Stick speed (loudest hit)", "How fast the stick comes down on the loudest note; softer notes scale down.", "Tone",
                     2.0, 10.0, "m/s", 1.0, S::Unsourced, "band harness 6 (v/127)^2 m/s (Dahl 2004: about 1 pp to 6 ff, VERIFY)",
                     [](const PlayerRig& r) { return r.extras.stickSpeedAt127; }, [](PlayerRig& r, double x) { r.extras.stickSpeedAt127 = x; }));
  if (fam == PlayerFamily::Shaken) {
    v.push_back(make("shakeHz", "Shake speed", "Shakes per second of the steady shaking.", "Shaking", 1.5, 6.0, "per sec", 1.0, S::Measured,
                     "kSleighBellShakeHz, measured on the reference recording (3.36 Hz)",
                     [](const PlayerRig& r) { return r.extras.shakeHz; }, [](PlayerRig& r, double x) { r.extras.shakeHz = x; }));
    v.push_back(make("smoothStrokes", "Smooth hand strokes", "On: stop-and-go strokes like a real hand. Off: an even, machine-like swing.", "Shaking",
                     0.0, 1.0, "", 1.0, S::Measured, "minimum-jerk strokes (Flash & Hogan 1985), duty measured",
                     [](const PlayerRig& r) { return r.extras.smoothStrokes ? 1.0 : 0.0; },
                     [](PlayerRig& r, double x) { r.extras.smoothStrokes = x >= 0.5; }, true));
  }
  if (i == I::Handbells)
    v.push_back(make("handbellSwingMps", "Ringing swing", "Swing the bell to ring it (m/s of the swing); 0 = a fixed tap of the clapper.", "Tone",
                     0.0, 3.0, "m/s", 1.0, S::Unsourced, "0 = the harness's fixed clapper (kHandbellRingerSwingMps is the ringer's swing)",
                     [](const PlayerRig& r) { return r.extras.handbellSwingMps; }, [](PlayerRig& r, double x) { r.extras.handbellSwingMps = x; }));
  if (i == I::Glockenspiel)
    v.push_back(make("strikeSpotScale", "Strike spot", "Where the mallet hits each bar: 1 = the usual sweet spot.", "Tone",
                     0.5, 1.5, "x", 1.0, S::Measured, "glockenspielStrikeFraction (the bar's usual strike point)",
                     [](const PlayerRig& r) { return r.extras.strikeSpotScale; }, [](PlayerRig& r, double x) { r.extras.strikeSpotScale = x; }));

  const PlayerRig base = builtInPlayer(i);
  for (PlayerField& f : v) f.defaultValue = f.get(base);
  return v;
}

// Slider values by field key.
using PlayerValues = std::map<std::string, double>;

[[nodiscard]] inline PlayerValues defaultValues(PlayerInstrument i) {
  PlayerValues out;
  for (const PlayerField& f : playerFields(i)) out[f.key] = f.defaultValue;
  return out;
}
// Read every field of a rig back as values (clamped).
[[nodiscard]] inline PlayerValues valuesOf(const PlayerRig& r) {
  PlayerValues out;
  for (const PlayerField& f : playerFields(r.instrument)) out[f.key] = clampFieldValue(f, f.get(r));
  return out;
}
// Clamp every value to its field's safe range; unknown keys are dropped
// and missing ones take the default.
[[nodiscard]] inline PlayerValues clampValues(PlayerInstrument i, const PlayerValues& in) {
  PlayerValues out;
  for (const PlayerField& f : playerFields(i)) {
    const auto it = in.find(f.key);
    out[f.key] = it == in.end() ? f.defaultValue : clampFieldValue(f, it->second);
  }
  return out;
}
// The built-in player with these habits (each clamped to its safe range).
// A field left at its default leaves the built-in profile untouched.
[[nodiscard]] inline PlayerRig makePlayer(PlayerInstrument i, const PlayerValues& values) {
  PlayerRig r = builtInPlayer(i);
  for (const PlayerField& f : playerFields(i)) {
    const auto it = values.find(f.key);
    if (it == values.end()) continue;
    const double x = clampFieldValue(f, it->second);
    if (x == f.defaultValue) continue;
    f.set(r, x);
  }
  return r;
}

// --- Named custom players: save and load ----------------------------------
// Step 24.1: the players file, version 2. Nothing in a file is ever lost:
// lines and whole player blocks the app can't use (a hand-edit typo, an
// instrument or setting from a newer version) are kept verbatim and
// written back; every problem is reported with its line number; a file
// that exists but can't be read is never treated as empty, so no save or
// delete runs until it reads; each save re-reads the file and applies just
// that change (two app windows don't overwrite each other) and keeps the
// previous file as players.txt.bak.
struct CustomPlayer {
  std::string name;
  PlayerInstrument instrument = PlayerInstrument::Violin;
  PlayerValues values;
  // Lines of this player's block the app could not use (an unknown setting,
  // a value that isn't a number, a comment), verbatim, written back after
  // the values.
  std::vector<std::string> keptLines;
  bool operator==(const CustomPlayer&) const = default;
};

// The longest player name, in characters (UTF-8 code points). A display
// limit from step 23 (then 60 bytes), not a physical value.
inline constexpr std::size_t kPlayerNameMaxChars = 60;

namespace player_builder_detail {
// Byte length of the valid UTF-8 sequence starting at s[i], or 0 when the
// bytes there are not valid UTF-8 (overlong forms, surrogates and values
// above U+10FFFF are invalid, RFC 3629).
inline std::size_t utf8SequenceLength(const std::string& s, std::size_t i) {
  const auto byte = [&](std::size_t k) { return static_cast<unsigned>(static_cast<unsigned char>(s[k])); };
  const unsigned c = byte(i);
  if (c < 0x80u) return 1;
  std::size_t n = 0;
  unsigned cp = 0;
  if (c >= 0xC2u && c <= 0xDFu) { n = 2; cp = c & 0x1Fu; }
  else if (c >= 0xE0u && c <= 0xEFu) { n = 3; cp = c & 0x0Fu; }
  else if (c >= 0xF0u && c <= 0xF4u) { n = 4; cp = c & 0x07u; }
  else return 0;
  if (i + n > s.size()) return 0;
  for (std::size_t k = 1; k < n; ++k) {
    if ((byte(i + k) & 0xC0u) != 0x80u) return 0;
    cp = (cp << 6) | (byte(i + k) & 0x3Fu);
  }
  if ((n == 3 && cp < 0x800u) || (n == 4 && (cp < 0x10000u || cp > 0x10FFFFu)) || (cp >= 0xD800u && cp <= 0xDFFFu)) return 0;
  return n;
}
}  // namespace player_builder_detail

// A name safe for the one-line file format and for display: control
// characters become spaces, bytes that are not valid UTF-8 become U+FFFD,
// trimmed, and cut to kPlayerNameMaxChars characters without splitting one.
[[nodiscard]] inline std::string cleanPlayerName(const std::string& in) {
  std::vector<std::string> chars;
  for (std::size_t i = 0; i < in.size();) {
    const std::size_t n = player_builder_detail::utf8SequenceLength(in, i);
    if (n == 0) {
      chars.emplace_back("\xEF\xBF\xBD");
      ++i;
      continue;
    }
    const unsigned char c = static_cast<unsigned char>(in[i]);
    chars.emplace_back(n == 1 && (c < 32 || c == 127) ? std::string(" ") : in.substr(i, n));
    i += n;
  }
  std::size_t a = 0, b = chars.size();
  while (a < b && chars[a] == " ") ++a;
  while (b > a && chars[b - 1] == " ") --b;
  b = std::min(b, a + kPlayerNameMaxChars);
  while (b > a && chars[b - 1] == " ") --b;
  std::string s;
  for (std::size_t k = a; k < b; ++k) s += chars[k];
  return s;
}

namespace player_builder_detail {
inline std::string formatExact(double x) {
  char buf[64];
  const auto res = std::to_chars(buf, buf + sizeof buf, x);  // shortest round-trip form
  return std::string(buf, res.ptr);
}
inline bool parseExact(const std::string& s, double& x) {
  const auto res = std::from_chars(s.data(), s.data() + s.size(), x);
  return res.ec == std::errc{} && res.ptr == s.data() + s.size();
}
inline std::string envValue(const char* name) {
#if defined(_MSC_VER)
  char* buffer = nullptr;
  std::size_t length = 0;
  if (_dupenv_s(&buffer, &length, name) != 0 || buffer == nullptr) return {};
  std::string value(buffer);
  std::free(buffer);
  return value;
#else
  const char* value = std::getenv(name);
  return value ? std::string(value) : std::string();
#endif
}
inline const PlayerField* findField(const std::vector<PlayerField>& fields, const std::string& key) {
  for (const PlayerField& f : fields)
    if (key == f.key) return &f;
  return nullptr;
}
}  // namespace player_builder_detail

// The file format version this build writes and fully understands.
// Version 1 (step 23) had no "format" line; its settings are the same, so
// it reads with every value unchanged.
inline constexpr int kPlayersFormatVersion = 2;
inline constexpr const char* kPlayersFormatTag = "mmathEn-players";

struct PlayersFileWarning {
  int line = 0;  // 1-based line in the file
  std::string message;
  bool operator==(const PlayersFileWarning&) const = default;
};
struct PlayersFile {
  int version = kPlayersFormatVersion;  // as read; 1 = a step-23 file
  std::vector<CustomPlayer> players;
  // Blocks and loose lines the app could not use, verbatim (one string per
  // line), in file order; written back after the players.
  std::vector<std::string> keptText;
  std::vector<PlayersFileWarning> warnings;
};

// Text form (UTF-8):
//   # comment line(s)
//   format mmathEn-players 2
//   player <instrument key> <name to end of line>
//   <field key> <value>
//   [kept lines]
//   end
//   [kept blocks / lines]
// Values are written in the shortest form that reads back to the same
// double, so a save and load round-trips exactly.
[[nodiscard]] inline std::string playersToText(const PlayersFile& file) {
  std::string out = "# mmathEn custom players (Player builder). Hand edits are fine: lines the app can't use are kept.\n";
  out += std::string("format ") + kPlayersFormatTag + " " + std::to_string(kPlayersFormatVersion) + "\n";
  for (const CustomPlayer& p : file.players) {
    out += "player ";
    out += playerInstrumentInfo(p.instrument).key;
    out += ' ';
    out += cleanPlayerName(p.name);
    out += '\n';
    for (const auto& [k, x] : p.values) out += k + " " + player_builder_detail::formatExact(x) + "\n";
    for (const std::string& line : p.keptLines) out += line + "\n";
    out += "end\n";
  }
  for (const std::string& line : file.keptText) out += line + "\n";
  return out;
}
[[nodiscard]] inline std::string playersToText(const std::vector<CustomPlayer>& players) {
  PlayersFile f;
  f.players = players;
  return playersToText(f);
}

// Reads every player it can; see PlayersFile. Never throws.
[[nodiscard]] inline PlayersFile playersFileFromText(std::string text) {
  using player_builder_detail::findField;
  using player_builder_detail::formatExact;
  PlayersFile out;
  out.version = 1;  // until a format line says otherwise
  if (text.rfind("\xEF\xBB\xBF", 0) == 0) text.erase(0, 3);  // UTF-8 byte-order mark (Notepad)
  std::vector<std::string> lines;
  {
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      lines.push_back(line);
    }
  }
  auto warn = [&](std::size_t idx, std::string msg) { out.warnings.push_back({static_cast<int>(idx + 1), std::move(msg)}); };
  enum class Mode { None, Player, Kept };
  Mode mode = Mode::None;
  bool sawPlayer = false;
  CustomPlayer cur;
  PlayerValues raw;
  std::vector<PlayerField> fields;
  std::map<std::string, std::size_t> seenKey;
  std::size_t blockStart = 0;
  auto finishPlayer = [&] {
    if (mode != Mode::Player) return;
    cur.values = clampValues(cur.instrument, raw);
    out.players.push_back(cur);
    mode = Mode::None;
  };
  auto openBlock = [&](std::size_t idx) {
    sawPlayer = true;
    blockStart = idx;
    const std::string rest = lines[idx].substr(7);
    const auto sp = rest.find(' ');
    const std::string key = rest.substr(0, sp);
    PlayerInstrument inst{};
    const std::string name = sp == std::string::npos ? std::string() : cleanPlayerName(rest.substr(sp + 1));
    std::string problem;
    if (!playerInstrumentFromKey(key, inst)) problem = "unknown instrument \"" + key + "\"";
    else if (name.empty()) problem = "this player has no name";
    else
      for (const CustomPlayer& q : out.players)
        if (q.instrument == inst && q.name == name) problem = "a second player with the same instrument and name";
    if (!problem.empty()) {
      warn(idx, problem + ": the block is kept unchanged but not shown");
      out.keptText.push_back(lines[idx]);
      mode = Mode::Kept;
      return;
    }
    cur = CustomPlayer{};
    cur.instrument = inst;
    cur.name = name;
    raw.clear();
    seenKey.clear();
    fields = playerFields(inst);
    mode = Mode::Player;
  };
  for (std::size_t idx = 0; idx < lines.size(); ++idx) {
    const std::string& line = lines[idx];
    const bool isPlayer = line.rfind("player ", 0) == 0;
    if (mode == Mode::Kept) {
      if (isPlayer) {
        warn(blockStart, "no \"end\" before the next player (kept as written)");
        mode = Mode::None;
      } else {
        out.keptText.push_back(line);
        if (line == "end") mode = Mode::None;
        continue;
      }
    }
    if (mode == Mode::Player) {
      if (isPlayer) {
        warn(blockStart, "no \"end\" before the next player (read as if it had one)");
        finishPlayer();
      } else if (line == "end") {
        finishPlayer();
        continue;
      } else {
        if (line.empty()) continue;
        if (line[0] == '#') { cur.keptLines.push_back(line); continue; }
        const auto sp = line.find(' ');
        const std::string key = line.substr(0, sp);
        const PlayerField* f = findField(fields, key);
        double x = 0.0;
        if (f == nullptr) {
          warn(idx, "\"" + key + "\" is not a " + std::string(playerInstrumentInfo(cur.instrument).label) +
                        " setting in this version (kept unchanged)");
          cur.keptLines.push_back(line);
        } else if (sp == std::string::npos || !player_builder_detail::parseExact(line.substr(sp + 1), x) || !std::isfinite(x)) {
          warn(idx, "\"" + key + "\" has no readable number (kept unchanged; the built-in value is used)");
          cur.keptLines.push_back(line);
        } else {
          if (seenKey.count(key)) warn(idx, "\"" + key + "\" is set twice (line " + std::to_string(seenKey[key] + 1) + "); this later value is used");
          seenKey[key] = idx;
          const double c = clampFieldValue(*f, x);
          if (c != x)
            warn(idx, "\"" + key + "\" " + formatExact(x) + " is outside the safe range; " + formatExact(c) + " is used");
          raw[key] = x;
        }
        continue;
      }
    }
    // Between blocks.
    if (isPlayer) { openBlock(idx); continue; }
    if (line.empty()) continue;
    if (line.rfind("# mmathEn custom players", 0) == 0) continue;  // the header comment the app writes
    if (line.rfind("format ", 0) == 0) {
      std::istringstream in(line.substr(7));
      std::string tag, extra;
      int v = 0;
      if (!sawPlayer && (in >> tag >> v) && tag == kPlayersFormatTag && v >= 1 && !(in >> extra)) {
        out.version = v;
        if (v > kPlayersFormatVersion)
          warn(idx, "this file was saved by a newer version of the app (format " + std::to_string(v) +
                        "); it is shown read-only so nothing in it is lost");
        continue;
      }
      warn(idx, "unrecognised format line (kept unchanged)");
      out.keptText.push_back(line);
      continue;
    }
    if (line[0] != '#') warn(idx, "line outside any player (kept unchanged)");
    out.keptText.push_back(line);
  }
  if (mode == Mode::Player) {
    warn(blockStart, "the file ends before this player's \"end\" (read as if it had one)");
    finishPlayer();
  } else if (mode == Mode::Kept) {
    warn(blockStart, "the file ends before this kept block's \"end\"");
  }
  return out;
}
// The players only (see playersFileFromText).
[[nodiscard]] inline std::vector<CustomPlayer> playersFromText(const std::string& text) {
  return playersFileFromText(text).players;
}

// %APPDATA%\mmathEn\players.txt on Windows; $XDG_CONFIG_HOME/mmathEn or
// ~/.config/mmathEn elsewhere; the temp folder when none is set.
[[nodiscard]] inline std::filesystem::path defaultPlayersPath() {
  using player_builder_detail::envValue;
  std::filesystem::path dir;
#if defined(_WIN32)
  const std::string appData = envValue("APPDATA");
  if (!appData.empty()) dir = std::filesystem::path(appData) / "mmathEn";
#else
  const std::string xdg = envValue("XDG_CONFIG_HOME"), home = envValue("HOME");
  if (!xdg.empty()) dir = std::filesystem::path(xdg) / "mmathEn";
  else if (!home.empty()) dir = std::filesystem::path(home) / ".config" / "mmathEn";
#endif
  if (dir.empty()) {
    std::error_code ec;
    dir = std::filesystem::temp_directory_path(ec) / "mmathEn";
  }
  return dir / "players.txt";
}

// A path as UTF-8 text for messages and UIs. path::string() uses the Windows
// code page and can throw on names it can't show (step 24.2).
inline std::string pathText(const std::filesystem::path& p) {
  const std::u8string u = p.u8string();
  return std::string(reinterpret_cast<const char*>(u.data()), u.size());
}

namespace player_builder_detail {
// The operating system's text for the last C-library error (step 24.2:
// failed saves show the real reason).
inline std::string lastOsErrorText() {
  const int e = errno;
  return e != 0 ? std::generic_category().message(e) : std::string("the system gave no reason");
}
// Writes text via a temporary file and a rename, so a failed write never
// leaves a half file.
inline bool writeFileAtomic(const std::filesystem::path& path, const std::string& text, std::string* error = nullptr) {
  std::error_code ec;
  if (path.has_parent_path()) {
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) {
      if (error) *error = "could not create the folder " + pathText(path.parent_path()) + ": " + ec.message();
      return false;
    }
  }
  std::filesystem::path tmp = path;
  tmp += ".tmp";
  {
    errno = 0;
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) {
      if (error) *error = "could not create " + pathText(tmp) + ": " + lastOsErrorText();
      return false;
    }
    errno = 0;
    f << text;
    if (!f.flush()) {
      if (error) *error = "could not write " + pathText(tmp) + ": " + lastOsErrorText();
      return false;
    }
  }
  std::filesystem::rename(tmp, path, ec);
  if (ec) {
    if (error) *error = "could not replace " + pathText(path) + ": " + ec.message();
    std::error_code ec2;
    std::filesystem::remove(tmp, ec2);
    return false;
  }
  return true;
}
}  // namespace player_builder_detail

// Writes the whole list as a new version-2 file. Returns false on any I/O
// error. (The app saves through editPlayersFile, which keeps what it can't
// read and the previous file.)
[[nodiscard]] inline bool savePlayers(const std::filesystem::path& path, const std::vector<CustomPlayer>& players) {
  return player_builder_detail::writeFileAtomic(path, playersToText(players));
}

enum class PlayersFileState {
  Missing,     // no file yet: an empty list, saving creates it
  Ok,          // read
  Unreadable,  // exists but could not be read: nothing may be saved over it
  Newer,       // written by a newer version: shown read-only
};
struct PlayersLoad {
  PlayersFileState state = PlayersFileState::Missing;
  PlayersFile file;
  std::string error;  // why it could not be read (Unreadable)
  [[nodiscard]] bool canSave() const { return state == PlayersFileState::Missing || state == PlayersFileState::Ok; }
};

[[nodiscard]] inline PlayersLoad loadPlayersFile(const std::filesystem::path& path) {
  PlayersLoad r;
  std::error_code ec;
  const std::filesystem::file_status st = std::filesystem::status(path, ec);
  if (st.type() == std::filesystem::file_type::not_found) return r;
  r.state = PlayersFileState::Unreadable;
  if (ec) {
    r.error = ec.message();
    return r;
  }
  if (!std::filesystem::is_regular_file(st)) {
    r.error = "it is not a file";
    return r;
  }
  std::ifstream f(path, std::ios::binary);
  if (!f) {
    r.error = "it could not be opened (another program may be using it, or there is no permission)";
    return r;
  }
  std::stringstream ss;
  ss << f.rdbuf();
  if (f.bad()) {
    r.error = "reading it failed part-way";
    return r;
  }
  r.file = playersFileFromText(ss.str());
  r.state = r.file.version > kPlayersFormatVersion ? PlayersFileState::Newer : PlayersFileState::Ok;
  return r;
}
// The players in the file; an empty list when it does not exist or cannot
// be read (use loadPlayersFile to tell those apart).
[[nodiscard]] inline std::vector<CustomPlayer> loadPlayers(const std::filesystem::path& path) {
  return loadPlayersFile(path).file.players;
}

// Adds the player, replacing one with the same instrument and name (whose
// kept lines for settings this version does not know are carried over).
inline void upsertPlayer(std::vector<CustomPlayer>& list, CustomPlayer p) {
  p.name = cleanPlayerName(p.name);
  p.values = clampValues(p.instrument, p.values);
  for (CustomPlayer& q : list)
    if (q.instrument == p.instrument && q.name == p.name) {
      const auto fields = playerFields(p.instrument);
      for (const std::string& line : q.keptLines) {
        const std::string key = line.substr(0, line.find(' '));
        // A kept line for a known setting held an unreadable value; the
        // saved value replaces it.
        if (!line.empty() && line[0] != '#' && player_builder_detail::findField(fields, key) != nullptr) continue;
        if (std::find(p.keptLines.begin(), p.keptLines.end(), line) == p.keptLines.end()) p.keptLines.push_back(line);
      }
      q = std::move(p);
      return;
    }
  list.push_back(std::move(p));
}
inline bool removePlayer(std::vector<CustomPlayer>& list, PlayerInstrument i, const std::string& name) {
  const auto it = std::find_if(list.begin(), list.end(), [&](const CustomPlayer& q) { return q.instrument == i && q.name == name; });
  if (it == list.end()) return false;
  list.erase(it);
  return true;
}

struct PlayersEdit {
  bool ok = false;
  std::string error;  // plain English, for the status line
  PlayersLoad after;  // the file as it now is on disk
};
// One change to the players file: re-reads it (so another window's
// changes are kept), applies change(players), keeps the previous file as
// <name>.bak and writes the result. Refused, with nothing written, when the
// file exists but can't be read or is from a newer version.
template <class Change>
[[nodiscard]] PlayersEdit editPlayersFile(const std::filesystem::path& path, Change&& change) {
  PlayersEdit r;
  PlayersLoad cur = loadPlayersFile(path);
  if (cur.state == PlayersFileState::Unreadable) {
    r.error = "Not saved: " + pathText(path) + " can't be read (" + cur.error + "). Nothing was changed.";
    r.after = std::move(cur);
    return r;
  }
  if (cur.state == PlayersFileState::Newer) {
    r.error = "Not saved: " + pathText(path) + " was saved by a newer version of the app, so it is read-only here.";
    r.after = std::move(cur);
    return r;
  }
  change(cur.file.players);
  if (cur.state == PlayersFileState::Ok) {
    std::filesystem::path bak = path;
    bak += ".bak";
    std::error_code ec;
    std::filesystem::copy_file(path, bak, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
      r.error = "Not saved: could not keep a backup at " + pathText(bak) + " (" + ec.message() + ").";
      r.after = loadPlayersFile(path);
      return r;
    }
  }
  std::string why;
  if (!player_builder_detail::writeFileAtomic(path, playersToText(cur.file), &why)) {
    r.error = "Not saved: " + why + ".";
    r.after = loadPlayersFile(path);
    return r;
  }
  r.ok = true;
  r.after = loadPlayersFile(path);
  return r;
}

// Finds a saved player by "instrument:name" (e.g. "violin:Warm") or by name
// alone when only one player has it. Returns its index, or -1 with error set.
[[nodiscard]] inline int findPlayer(const std::vector<CustomPlayer>& list, const std::string& spec, std::string& error) {
  error.clear();
  const auto colon = spec.find(':');
  PlayerInstrument inst{};
  if (colon != std::string::npos && playerInstrumentFromKey(spec.substr(0, colon), inst)) {
    const std::string name = cleanPlayerName(spec.substr(colon + 1));
    for (std::size_t k = 0; k < list.size(); ++k)
      if (list[k].instrument == inst && list[k].name == name) return static_cast<int>(k);
  }
  const std::string name = cleanPlayerName(spec);
  int found = -1, count = 0;
  for (std::size_t k = 0; k < list.size(); ++k)
    if (list[k].name == name) {
      if (found < 0) found = static_cast<int>(k);
      ++count;
    }
  if (count == 1) return found;
  if (count == 0) {
    error = "No saved player named \"" + spec + "\"";
  } else {
    error = "More than one saved player is named \"" + name + "\"; use instrument:name, e.g. " +
            std::string(playerInstrumentInfo(list[static_cast<std::size_t>(found)].instrument).key) + ":" + name;
  }
  return -1;
}

}  // namespace mmathen
