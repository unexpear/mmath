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
#include <vector>

#include "../math/constants.h"
#include "bore_valve.h"
#include "cone_valve.h"
#include "player_airway.h"
#include "wind_presets.h"

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
};

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
                               PlayerAirway* airway = nullptr, double sampleRateHz = 48000.0) {
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
  constexpr std::uint64_t kTongue = 1920;
  const double depth = std::pow(2.0, kVibratoCents / 1200.0) - 1.0;
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
    const PhraseNote& note = notes[static_cast<std::size_t>(sounding)];
    if (airway != nullptr) {
      const auto next = static_cast<std::size_t>(active) + 1;
      const PhraseNote& here = notes[static_cast<std::size_t>(active)];
      if (here.lipHz == 0.0 && !here.rest) {
        const std::uint64_t noteEnd =
            next < notes.size() ? static_cast<std::uint64_t>(notes[next].startSec * rateHz) : scoreFrames;
        const bool restNext = next >= notes.size() || notes[next].rest;
        if (restNext && noteEnd >= kTongue && frame >= noteEnd - kTongue) tongueShut = true;
      }
      airway->setTongueTarget(tongueShut ? 0.0 : 1.0);
    }
    const double time = static_cast<double>(frame) / rateHz;
    bool stolenBreath = false;
    if (airway != nullptr && !afterScore && !note.rest) {
      const auto next = static_cast<std::size_t>(active) + 1;
      if (next < notes.size() && !notes[next].rest) {
        const auto join = static_cast<std::uint64_t>(notes[next].startSec * rateHz);
        const auto steal = static_cast<std::uint64_t>(kBreathStealSec * rateHz);
        if (breathing && frame < join) stolenBreath = true;
        else if (join > steal && frame >= join - steal && frame < join &&
                 airway->needsBreath(kBreathMarginM3)) {
          breathing = true; stolenBreath = true;
        }
        if (frame + count >= join) breathing = false;
      }
    }
    // The note after a breath is tongued, as after a written rest.
    if (stolenBreath) airway->setTongueTarget(0.0);
    const bool breathOff = note.rest || afterScore || stolenBreath;
    // Once the breath is off, the lip seals and the wall is Kirchhoff
    // at this fingering's impedance peak. The next blow restores the
    // mild playing pole. The radiated sample is not scaled.
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
    double env = breathOff ? 0.0 : note.gain;
    const bool dipping = airway == nullptr && fluteJet && note.tongue && sounding > 0 && !note.rest &&
                         time < note.startSec + 0.020;
    if (dipping) env *= 0.55;
    double length = note.lengthM;
    // A lip locked to one partial explodes if the bore glides through
    // another. Brass changes length at the note. Woodwind slurs glide.
    if (sounding > 0 && note.lipHz == 0.0 && !note.rest) {
      const double glide = note.tongue ? 0.010 : 0.035;
      double alpha = (time - note.startSec) / glide;
      if (alpha < 0.0) alpha = 0.0;
      if (alpha > 1.0) alpha = 1.0;
      alpha = alpha * alpha * (3.0 - 2.0 * alpha);
      const PhraseNote& prev = notes[static_cast<std::size_t>(sounding - 1)];
      length = prev.lengthM + (note.lengthM - prev.lengthM) * alpha;
    }
    if (note.vibrato && !note.rest && time > note.startSec + 0.20) {
      length *= 1.0 - depth * std::sin(constants::twoPi * kVibratoHz * time);
    }
    if (!(voice.setSoundingLength(length))) return {};
    if (!(voice.setBlowingPressure(breathOff ? 0.0 : note.pressure * env))) return {};
    if (note.lipHz > 0.0) if (!(voice.setLipResonanceHz(note.lipHz))) return {};
    if constexpr (requires(Voice& probe) { probe.setJetDelaySamples(0.0); }) {
      if (!fluteJet || note.rest) {
        if (fluteJet && note.jetSamples > 0.0) if (!(voice.setJetDelaySamples(note.jetSamples))) return {};
      } else {
      double jetNote = note.jetSamples;
      if (sounding > 0 && notes[static_cast<std::size_t>(sounding - 1)].jetSamples > 0.0) {
        const double glide = note.tongue ? 0.010 : 0.035;
        double alpha = (time - note.startSec) / glide;
        if (alpha < 0.0) alpha = 0.0;
        if (alpha > 1.0) alpha = 1.0;
        alpha = alpha * alpha * (3.0 - 2.0 * alpha);
        const double prev = notes[static_cast<std::size_t>(sounding - 1)].jetSamples;
        jetNote = prev + (note.jetSamples - prev) * alpha;
      }
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
