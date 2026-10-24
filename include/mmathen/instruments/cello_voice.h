#pragma once

// Bowed cello voice for the band: one bowed string course of cello()
// (Guettler strings, 0.690 m) through the generic stick-slip engine
// (OversampledBowedStringVoice), its rigid-bridge transverse force driving
// a modal cello body that radiates as a compact monopole.
//
// Bridge coupling: the string sees a rigid bridge and the body is driven
// by F = 2 Z v_arriving (Cremer, The Physics of the Violin, ch. 3). The
// feedback of body motion on the string is left out: the body's
// driving-point mobility is small against the string's 1/Z. This is the
// one-way approximation, labelled here, not a measured property.
//
// Body modes. Frequencies below 250 Hz are from holographic studies of
// the cello: Bynum and Rossing, "Holographic studies of cello vibrations"
// (Proc. IOA 19, 1997) and Rossing, ASA 135th meeting lay paper (1998):
// A0 (f-hole Helmholtz) 104 Hz, T1 ~140 Hz, C3 ~180 Hz, C4 195 Hz, A1
// air 226 Hz. Higher modes are a quarter-octave family to 4 kHz
// (UNSOURCED spacing, as in double_bass_pizz.h). The cello bridge's first
// in-plane resonance sits near 1 kHz (Reinicke 1973, as quoted by Cremer
// ch. 9: VERIFY); above it the bridge passes less force, modelled by modal
// mass rising as f / 1 kHz (TUNED; (f/1 kHz)^2 left 2-2.6 kHz 9 dB low). Q, mass, and areas are labelled
// at each constant.

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <vector>

#include "../physics/oscillator.h"
#include "bowed_string.h"
#include "cello.h"

namespace mmathen {

struct CelloBodyMode {
  double hz;
  double q;
  double massKg;
  double areaM2;
};

// 1 kg modal mass: peak mobility Q/(2 pi f m) ~ 0.02-0.05 m/(N s) for the
// low modes, the order of measured cello bridge admittance peaks
// (Askenfelt 1982, KTH STL-QPSR: VERIFY). Areas are signed effective
// radiating areas (TUNED to the Commons PD Sarabande spectra).
inline constexpr double kCelloModalMassKg = 1.0;    // VERIFY
inline constexpr double kCelloLowAreaM2 = 0.08;     // TUNED
inline constexpr double kCelloHighAreaM2 = 0.05;    // TUNED
inline constexpr double kCelloBodyQ = 25.0;         // VERIFY (violin body Q 10-50, Bissinger 2008)
inline constexpr double kCelloBridgeHillHz = 1000.0;  // VERIFY (Reinicke 1973)

[[nodiscard]] inline std::vector<CelloBodyMode> celloBodyModes() {
  std::vector<CelloBodyMode> m;
  // Bynum & Rossing 1997 / Rossing 1998 frequencies. A0 and A1 are air
  // modes; their areas stand for f-hole volume flow (TUNED).
  m.push_back({104.0, 20.0, kCelloModalMassKg, kCelloLowAreaM2});
  m.push_back({140.0, kCelloBodyQ, kCelloModalMassKg, kCelloLowAreaM2});
  m.push_back({180.0, kCelloBodyQ, kCelloModalMassKg, -kCelloLowAreaM2});
  m.push_back({195.0, kCelloBodyQ, kCelloModalMassKg, kCelloLowAreaM2});
  m.push_back({226.0, kCelloBodyQ, kCelloModalMassKg, -0.5 * kCelloLowAreaM2});
  // Quarter-octave family 260 Hz - 4 kHz (UNSOURCED), alternating sign.
  int k = 0;
  for (double f = 260.0; f <= 4000.0; f *= std::pow(2.0, 0.25), ++k) {
    const double rise = std::max(1.0, f / kCelloBridgeHillHz);
    m.push_back({f, kCelloBodyQ, kCelloModalMassKg * rise,
                 (k % 2 ? -1.0 : 1.0) * kCelloHighAreaM2});
  }
  return m;
}

// Force-driven modal body: m (x'' + 2 sigma x' + w^2 x) = F per mode;
// pressure p = rho / (4 pi r) d/dt sum A_n v_n (compact monopole),
// delayed r / c. Same integrator as the bowed-string body
// (stepConstantForceOscillator).
class CelloBody {
 public:
  [[nodiscard]] static Result<CelloBody, QuantityError> create(SampleRateHz rate,
                                                               double distanceMeters = 1.0) {
    using R = Result<CelloBody, QuantityError>;
    if (!(rate.value() > 0.0) || !(distanceMeters > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    CelloBody b;
    b.fs_ = rate.value();
    for (const CelloBodyMode& s : celloBodyModes()) {
      if (s.hz >= 0.45 * b.fs_) continue;
      const double w = 2.0 * constants::pi * s.hz;
      b.modes_.push_back(Mode{w, w / (2.0 * s.q), s.massKg, s.areaM2, {}});
    }
    // Air at 20 C: rho 1.204 kg/m^3, c 343 m/s (standard values).
    b.scale_ = 1.204 / (4.0 * constants::pi * distanceMeters);
    b.delay_.assign(std::max<std::size_t>(1, static_cast<std::size_t>(
                                                 std::llround(b.fs_ * distanceMeters / 343.0))),
                    0.0);
    return R::success(std::move(b));
  }
  // out[i] += pressure for force[i].
  [[nodiscard]] bool processAdd(const double* force, double* out, std::size_t n) {
    const double dt = 1.0 / fs_;
    for (std::size_t i = 0; i < n; ++i) {
      double flow = 0.0;
      for (Mode& m : modes_) {
        auto s = stepConstantForceOscillator(m.state, m.omega, m.sigma, force[i] / m.mass, dt);
        if (!s.ok()) return false;
        m.state = s.value();
        flow += m.area * m.state.velocity;
      }
      const double p = scale_ * (flow - lastFlow_) * fs_;
      lastFlow_ = flow;
      out[i] += delay_[pos_];
      delay_[pos_] = p;
      pos_ = (pos_ + 1) % delay_.size();
      if (!std::isfinite(out[i])) return false;
    }
    return true;
  }
  [[nodiscard]] std::size_t modeCount() const { return modes_.size(); }

 private:
  struct Mode {
    double omega, sigma, mass, area;
    OscillatorState state;
  };
  double fs_ = 0.0, scale_ = 0.0, lastFlow_ = 0.0;
  std::vector<Mode> modes_;
  std::vector<double> delay_;
  std::size_t pos_ = 0;
};

// String loss: T60 at f0 and at kCelloT60HighHz (TUNED to the Sarabande
// note tails and harmonic slopes; cello strings ring longer than violin
// strings).
//
// Step 10: the high-frequency anchor is a fixed frequency, not 6 f0 of each
// course. A string's loss factor is a function of absolute frequency (air
// viscosity, internal friction and bending losses: Valette, "The mechanics
// of vibrating strings", in Mechanics of Musical Instruments, Springer
// 1995; Woodhouse, Acta Acustica 90, 2004, the form woodhouseLossFactor
// uses for the guitar). Anchored at 6 f0, every course got the same
// damping per harmonic number, so the C string (6 f0 = 392 Hz) lost its
// partials near 2.6 kHz (harmonic 40, T60 about 10 ms) eleven times faster
// than the A string at the same frequency (harmonic 12, about 110 ms): the
// low notes were dark (LTAS of C2-B2 3-12 dB under the Commons PD
// Sarabande at 2.5-8 kHz; centroid / f0 2.09 against 2.52).
inline constexpr double kCelloT60LowSeconds = 3.0;    // TUNED
inline constexpr double kCelloT60HighSeconds = 0.4;   // TUNED
// 6 x A3: the A string keeps the loss the two TUNED values were fitted with.
inline constexpr double kCelloT60HighHz = 6.0 * 220.0;  // TUNED anchor (see above)
[[nodiscard]] inline Result<LossModel, QuantityError> celloStringLoss(const StringCourse& c) {
  using R = Result<LossModel, QuantityError>;
  const double f = c.tuning.frequency.value();
  auto ideal = courseString(c);
  if (!ideal.ok()) return R::failure(ideal.error());
  return lossFromT60(ideal.value().speed, FrequencyHz{f}, DurationSeconds{kCelloT60LowSeconds},
                     FrequencyHz{kCelloT60HighHz}, DurationSeconds{kCelloT60HighSeconds});
}

// Vibrato measured on the Commons PD Sarabande (Bach BWV 1007/IV, PD
// recording, 39 sustained notes, pyin): median 6.0 Hz, median extent
// +/-9 cents. Onset / ramp UNSOURCED.
inline constexpr double kCelloVibratoRateHz = 6.0;
inline constexpr double kCelloVibratoCents = 9.0;
inline constexpr double kCelloVibratoOnsetSeconds = 0.15;  // UNSOURCED
inline constexpr double kCelloVibratoRampSeconds = 0.25;   // UNSOURCED
// Bow-bridge distance: cellists play nearer beta 0.1 at forte (Schelleng
// 1973 diagrams; VERIFY); soft playing further out.
inline constexpr double kCelloBetaForte = 0.07;  // VERIFY (0.09 gave env mean dev 3.1 dB vs 2.8)
inline constexpr double kCelloBetaPiano = 0.11;  // VERIFY
inline constexpr double kCelloBowSpeed = 0.15;   // m/s, VERIFY (Askenfelt 1989 range 0.05-1)
inline constexpr double kCelloAttackSeconds = 0.08;   // UNSOURCED
// Bow force as a fraction of Schelleng's maximum (the violin stock is
// 0.2). Guettler (Acta Acustica 88, 2002): the Helmholtz build-up shortens
// as force rises inside the playable region. TUNED: 0.2 gave a 10-90 %
// harmonic rise of 951 ms on open C2 and 780 ms on D2 (Sarabande PD
// reference, notes C2-B2 after a gap: median 565 ms); 0.4 gives 387 /
// 264 ms; 0.6 made the rise irregular (F2 346 ms, G4 283 ms).
inline constexpr double kCelloBowForceFraction = 0.4;  // TUNED
// Player intonation. A cellist tunes the open strings and places the
// fingers by ear, so the stop is set to sound the written pitch. The
// engine's bowed pitch sits above the loop tuning by these amounts per
// string (measured on this voice at force 0.4, beta 0.11, vibrato
// averaged, parabolic FFT peak over H1-H4): the stop is lowered by them.
// TUNED (measured on the model, not a property of real strings). Step 10
// re-measured after the loss anchor moved to kCelloT60HighHz (less loss on
// C, G and D left those strings 7.0, 4.5 and 2.4 c flatter, the A string
// unchanged): was {10.5, 7.8, 4.9, 4.4}.
inline constexpr std::array<double, 4> kCelloIntonationCents = {3.5, 3.4, 2.5, 4.4};  // TUNED
inline constexpr double kCelloReleaseSeconds = 0.12;  // UNSOURCED

// A cellist's bowing and vibrato habits (player_profile.h,
// BowedPlayerProfile). The default is the constants above, so a note made
// without habits and one made with CelloPlayerHabits{} are identical.
struct CelloPlayerHabits {
  double bowForceScale = 1.0;     // on kCelloBowForceFraction's force
  double bowSpeedScale = 1.0;     // on kCelloBowSpeed x level
  double contactPointScale = 1.0; // on the bow-bridge distance beta
  double vibratoHz = kCelloVibratoRateHz;
  double vibratoCents = kCelloVibratoCents;
  double vibratoOnsetSeconds = kCelloVibratoOnsetSeconds;
};

// Highest open string at or below f (the fingering a hymn line uses).
[[nodiscard]] inline int celloCourseFor(const StringInstrument& c, double hz) {
  int best = 0;
  for (int k = 0; k < kStringInstrumentCourses; ++k)
    if (c.courses[static_cast<std::size_t>(k)].tuning.frequency.value() <= hz * 1.001) best = k;
  return best;
}

// One bowed note: string force output (rigid bridge). Add the force into a
// buffer and run it through CelloBody.
[[nodiscard]] inline Result<OversampledBowedStringVoice, QuantityError> makeCelloNote(
    const StringInstrument& cello, double hz, double seconds, double level, SampleRateHz rate,
    bool vibrato = true, const CelloPlayerHabits& habits = CelloPlayerHabits{}) {
  using R = Result<OversampledBowedStringVoice, QuantityError>;
  const int k = celloCourseFor(cello, hz);
  // The player's peg tuning: the whole course is lowered by the
  // intonation offset (tension scaled with frequency squared).
  StringCourse c = cello.courses[static_cast<std::size_t>(k)];
  const double tune = std::pow(2.0, -kCelloIntonationCents[static_cast<std::size_t>(k)] / 1200.0);
  c.tuning.frequency = FrequencyHz{c.tuning.frequency.value() * tune};
  c.tension = TensionNewtons{c.tension.value() * tune * tune};
  const double lv = std::clamp(level, 0.05, 2.0);
  const double ldb = 20.0 * std::log10(lv);
  const double x = std::clamp(0.5 - ldb / 24.0, 0.0, 1.0);  // +-12 dB span (UNSOURCED)
  const double beta = (kCelloBetaForte + x * (kCelloBetaPiano - kCelloBetaForte)) * habits.contactPointScale;
  auto loss = celloStringLoss(c);
  if (!loss.ok()) return R::failure(loss.error());
  BowStroke stroke = violinBowStroke(c, beta, kCelloBowSpeed * lv * habits.bowSpeedScale);
  stroke.normalForceNewtons *= kCelloBowForceFraction / kViolinBowForceFraction;
  stroke.normalForceNewtons *= habits.bowForceScale;
  auto v = OversampledBowedStringVoice::create(
      c, stroke, loss.value(), 0.0, 4, 0, rate,
      static_cast<std::uint64_t>(std::max(0.0, seconds) * rate.value()), 4);
  if (!v.ok()) return R::failure(v.error());
  OversampledBowedStringVoice voice = std::move(v).value();
  BowExpression e{vibrato ? habits.vibratoHz : 0.0, vibrato ? habits.vibratoCents : 0.0,
                  habits.vibratoOnsetSeconds, kCelloVibratoRampSeconds, kCelloAttackSeconds,
                  0.2, kCelloReleaseSeconds};
  if (!voice.setExpression(e)) return R::failure(QuantityError::InvalidArgument);
  voice.setBridgeForceOutput(true);
  if (!voice.setStoppedFrequency(std::max(hz * tune, c.tuning.frequency.value()))) return R::failure(QuantityError::DomainError);
  return R::success(std::move(voice));
}

}  // namespace mmathen
