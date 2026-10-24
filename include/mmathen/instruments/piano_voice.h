#pragma once
// Piano note: any key of a piano from the Chabassier Table III strings,
// struck by a Hunt-Crossley felt hammer, radiated through a modal
// soundboard (docs/48, band pro check). Each string of the unison is a
// HammeredStringVoice (struck_string.h); all of them load one board at one
// bridge point (renderUnison), so the mistuned unison couples there
// (Weinreich 1977). The strings of other keys do not share it.
// This extends PR #2's hammered piano (tests/test_contact_voice.cpp: one
// Table III string, linear felt, a single body mode at f0), which scored
// worse on the VSCO scoreboard (band-scoreboard2.png); the class is the same.
//
// Strings. chabassier2013Strings() prints five rows (D#1 C2 F3 C#5 G6).
// Other keys are INTERPOLATED on the key axis between neighbouring rows:
// log speaking length, core diameter, log winding factor (extrapolated
// from the end pair outside D#1-G6). The tension is then the one that
// puts the ideal fundamental on equal temperament, T = mu (2 L f)^2; the
// interpolation is not a published scale.
//
// Unison. Plain wire: three strings; wound (factor > 1): two; factor
// above kPianoMonochordFactor: one. Mistuning across a unison
// kPianoUnisonCents (Kirk, JASA 31, 1644 (1959): preferred unison
// mistuning 0-2 cents, VERIFY). The hammer mass and felt stiffness K are
// shared equally, so each string sees the same contact time as the whole
// hammer on one string (VERIFY: no hammer twist, equal string heights).
//
// Hammer. A. Chaigne & A. Askenfelt, JASA 95, 1112 and 1631 (1994),
// Table I (VERIFY each value): C2 mass 4.9 g, K 4.0e8 N/m^p, p 2.3,
// strike at 0.116 L; C4 2.97 g, 4.5e9, 2.5, 0.12 L; C7 2.2 g, 1.0e11, 3.0,
// 0.0625 L. Other keys interpolated (log mass, log K, p and strike linear
// in key). Hammer speed by dynamic: A. Askenfelt & E. V. Jansson, JASA 90,
// 2383 (1991): about 1 m/s pp to 5-6 m/s ff (the five steps are a reading
// of that range, VERIFY).
//
// String loss: lossFromT60 at f0 and 6 f0. TUNED to the VSCO 2 CE upright
// piano (Simon Dalzell / Ivy Audio, redistributable by Versilian Studios,
// Keys/Upright Piano, dyn2); see pianoT60AtF0.
//
// Soundboard. A simply supported orthotropic plate (Leissa, Vibration of
// Plates, NASA SP-160, 1969): f_mn = (pi/2) sqrt(1/(rho h)) sqrt(Dx (m/a)^4
// + 2 H (m/a)^2 (n/b)^2 + Dy (n/b)^4), H = sqrt(Dx Dy), D = E h^3/12.
// Spruce along the grain E_L 11.6 GPa, density 420 kg/m^3 (VERIFY, wood
// handbook values); the ribs make the board nearly isotropic, Dy =
// kPianoRibStiffnessRatio Dx (TUNED, UNSOURCED). Upright board 1.40 x 1.00
// m, 8 mm (UNSOURCED, typical). Modal Q kPianoSoundboardQ (Ege, Boutillon
// & Rebillat, JSV 332, 1288 (2013): modal loss factors of an upright board
// about 1-3 %, VERIFY). The modal mass at the bridge point is rho h a b /4
// / phi^2 and the effective area is (integral of phi)/phi, both from the
// mode shape, so the monopole cancellation of even modes is the plate's.
// The plate alone is too mobile (bridge and ribs add mass and
// stiffness): the modal masses are scaled together so that the mean
// |Y| at the bridge point, over f0/1.5 to 1.5 f0 of the key (kept inside
// 100 Hz-2.5 kHz), is kPianoBridgeMobility, 5e-4 m/(N s), i.e. 2x10^3 kg/s (Giordano,
// JASA 102, 1159 (1997) and JASA 103, 2128 (1998); Wogram 1980;
// Askenfelt 2006: bridge impedance near 10^3 kg/s from 100 Hz to about
// 2.5 kHz on upright pianos, varying by about a factor 2 with position).
// Sweep at x0.1 felt: 1e-3 / 5e-4 / 2.5e-4 gave median T60 -1.40 / -0.79
// / -0.45 oct against the reference; 5e-4 is the stiffest inside the
// cited factor 2, so it is used (TUNED within the range).
// Bridge point for a key: a straight treble bridge from (0.25 a, 0.80 b)
// at C8 to (0.78 a, 0.30 b) at A0 (UNSOURCED geometry).
//
// Damper: a damped key's strings fall to silence over kPianoDamperSec
// (UNSOURCED); keys from kPianoFirstUndampedKey up have no damper.
#include <algorithm>
#include <array>
#include <memory>
#include <cmath>
#include <vector>
#include "../math/constants.h"
#include "../physics/materials.h"
#include "piano.h"
#include "struck_string.h"

namespace mmathen {

inline constexpr double kPianoMonochordFactor = 4.0;
inline constexpr double kPianoUnisonCents = 1.0;  // full spread, Kirk 1959 VERIFY
inline constexpr double kPianoDamperSec = 0.12;   // UNSOURCED
inline constexpr int kPianoFirstUndampedKey = 89; // F6, VERIFY
inline constexpr double kPianoSoundboardQ = 30.0;
inline constexpr double kPianoRibStiffnessRatio = 0.5;
inline constexpr double kPianoBoardA = 1.40, kPianoBoardB = 1.00, kPianoBoardH = 0.008;
inline constexpr double kSpruceEL = 11.6e9, kSpruceRho = 420.0;
inline constexpr double kPianoBoardMaxHz = 10000.0;  // jb10: was 3000 (no board above 3 kHz)
inline constexpr int kPianoBoardMaxModes = 3000;
// Chaigne & Askenfelt's K is a grand's C2/C4/C7 hammer. Scaled down,
// TUNED to the VSCO upright (dyn2) spectral centroid, at bridge mobility
// 5e-4: x1.0 +0.86 oct, x0.1 +0.34, x0.03 +0.02, x0.01 -0.45 oct (median
// A1-A6). At x0.03 A6 is a thump (centroid 272 Hz vs 1493), so the scale
// rises to 0.1 from A5 (key 81) to A6 (key 93). Softer/worn upright felt
// is a guess at the cause, UNSOURCED.
[[nodiscard]] inline double pianoUprightFeltScale(int midi) {
  const double t = std::clamp((midi - 81) / 12.0, 0.0, 1.0);
  return 0.03 * std::pow(0.1 / 0.03, t);
}
inline constexpr double kPianoBridgeMobility = 5.0e-4;  // m/(N s), Giordano 1997/1998, factor 2 stiffer than 1e-3

// mp: pp p mp mf f ff -> m/s (Askenfelt & Jansson 1991 range, VERIFY).
[[nodiscard]] inline double pianoHammerSpeed(int dynamicStep) {
  constexpr std::array<double, 6> v{1.0, 1.5, 2.0, 2.5, 3.5, 5.0};
  return v[static_cast<std::size_t>(std::clamp(dynamicStep, 0, 5))];
}

namespace piano_detail {
inline double lerp(double a, double b, double t) { return a + (b - a) * t; }
// Piecewise in key (semitones from C4) over tabulated x.
template <std::size_t N>
inline double interp(const std::array<double, N>& x, const std::array<double, N>& y, double s, bool logy) {
  std::size_t i = 0;
  while (i + 2 < N && s > x[i + 1]) ++i;
  const double t = (s - x[i]) / (x[i + 1] - x[i]);
  return logy ? std::exp(lerp(std::log(y[i]), std::log(y[i + 1]), t)) : lerp(y[i], y[i + 1], t);
}
}  // namespace piano_detail

[[nodiscard]] inline Result<PianoStringSpec, QuantityError> pianoStringForKey(int midi, double cents = 0.0) {
  using R = Result<PianoStringSpec, QuantityError>;
  if (midi < 21 || midi > 108) return R::failure(QuantityError::RangeError);
  const auto rows = chabassier2013Strings();
  std::array<double, 5> s{}, L{}, d{}, F{};
  for (std::size_t i = 0; i < 5; ++i) {
    s[i] = rows[i].semitonesFromC4; L[i] = rows[i].length.value();
    d[i] = rows[i].diameter.value(); F[i] = rows[i].densityFactor;
  }
  const double k = midi - 60;
  const double len = piano_detail::interp(s, L, k, true);
  const double dia = piano_detail::interp(s, d, k, false);
  double fac = piano_detail::interp(s, F, k, true);
  if (k >= s[2]) fac = 1.0;
  if (!(len > 0.0) || !(dia > 0.0) || !(fac >= 1.0)) return R::failure(QuantityError::RangeError);
  const WireMaterial steel = steelPianoWireChabassier2013();
  const double mu = steel.densityKgPerM3 * fac * constants::pi * dia * dia / 4.0;
  const double f = 440.0 * std::pow(2.0, (midi - 69) / 12.0 + cents / 1200.0);
  const double T = mu * std::pow(2.0 * len * f, 2.0);
  return R::success(PianoStringSpec{"key", DistanceMeters{len}, DistanceMeters{dia}, fac,
                                    TensionNewtons{T}, FrequencyHz{f}, midi - 60, fac == 1.0});
}

struct PianoHammer { double massKg, stiffness, exponent, strikeFraction; };
[[nodiscard]] inline PianoHammer pianoHammerForKey(int midi) {
  constexpr std::array<double, 3> s{-24.0, 0.0, 36.0};
  constexpr std::array<double, 3> m{4.9e-3, 2.97e-3, 2.2e-3};
  constexpr std::array<double, 3> K{4.0e8, 4.5e9, 1.0e11};
  constexpr std::array<double, 3> p{2.3, 2.5, 3.0};
  constexpr std::array<double, 3> b{0.116, 0.12, 0.0625};
  const double k = std::clamp(static_cast<double>(midi - 60), -39.0, 48.0);
  return PianoHammer{piano_detail::interp(s, m, k, true), piano_detail::interp(s, K, k, true),
                     piano_detail::interp(s, p, k, false),
                     std::clamp(piano_detail::interp(s, b, k, false), 0.04, 0.2)};
}

// Intrinsic string T60 at f0 (s), before the soundboard drain. TUNED to
// the VSCO 2 CE upright (dyn2): its early fundamental decay (0.1-1.2 s) is
// 6-9 s at A2, A3, A5 and A6. A 6 s (A2) to 3 s (A5) curve gave the model
// 5.5/3.7/1.3/1.0 s there; the flat 12 s leaves the board to set the slope.
[[nodiscard]] inline double pianoT60AtF0(int /*midi*/) { return 12.0; }
inline constexpr double kPianoT60Ratio6F0 = 0.35;  // TUNED

// Radiating area of one board mode. The volume (monopole) area
// integral/phi alone makes every mode with an even index silent and lets
// only about one mode per 90 Hz radiate, so a key whose f0 falls between
// them came out up to 20 dB quieter than its neighbours (jb10 audit:
// D3 -46, D#3 -48 dB next to C3 -32, F3 -32). A finite board radiates
// from its edges and corners below coincidence and fully above it.
// Band-averaged radiation efficiency of a simply supported plate:
// G. Maidanik, JASA 34, 809 (1962), as corrected by M. J. Crocker & A. J.
// Price, JSV 9, 469 (1969) (also Beranek & Ver, Noise and Vibration
// Control Engineering, ch. 9): below fc, sigma = (lc^2/S) g1 + (P lc/S)
// g2, g1 = 4/pi^4 (1-2a^2)/(a sqrt(1-a^2)) for f < fc/2 else 0, g2 =
// [(1-a^2) ln((1+a)/(1-a)) + 2a] / (4 pi^2 (1-a^2)^1.5), a = sqrt(f/fc),
// lc = c/fc; above fc, sigma = 1/sqrt(1-fc/f) (capped at 3). fc = c^2/(2
// pi) sqrt(rho h / H) from the plate's own stiffness (about 1.8 kHz).
// A mode of bridge-point velocity v has mean-square plate velocity
// v^2/(4 phi^2); the monopole of the same power, rho c sigma S <v^2> =
// rho c k^2 U^2/(4 pi), has area A = sqrt(pi sigma S)/(k |phi|). The
// larger of the two areas is used, with the volume area's sign (VERIFY:
// a band average used per mode). Above 3 kHz the board had no modes,
// so nothing above 2 kHz radiated: 2-4 kHz sat 15-25 dB and 4-8 kHz
// 25-40 dB under the VSCO upright; the plate's modes are kept to 10 kHz.
[[nodiscard]] inline double pianoModeArea(double f, double phi, double volumeArea, double rhoh, double H) {
  const double c0 = 343.0, pi = constants::pi;
  const double a = kPianoBoardA, b = kPianoBoardB, S = a * b, P = 2.0 * (a + b);
  const double fc = c0 * c0 / (2.0 * pi) * std::sqrt(rhoh / H), lc = c0 / fc;
  double sigma;
  if (f > fc) {
    sigma = std::min(1.0 / std::sqrt(1.0 - fc / f), 3.0);
  } else {
    const double al = std::min(std::sqrt(f / fc), 0.999), al2 = al * al;
    const double g1 = f < fc / 2.0 ? 4.0 / std::pow(pi, 4) * (1.0 - 2.0 * al2) / (al * std::sqrt(1.0 - al2)) : 0.0;
    const double g2 = ((1.0 - al2) * std::log((1.0 + al) / (1.0 - al)) + 2.0 * al) / (4.0 * pi * pi * std::pow(1.0 - al2, 1.5));
    sigma = lc * lc / S * g1 + P * lc / S * g2;
  }
  const double k = 2.0 * pi * f / c0;
  const double aeq = std::sqrt(pi * sigma * S) / (k * std::fabs(phi));
  if (!(aeq > std::fabs(volumeArea))) return volumeArea;
  return volumeArea < 0.0 ? -aeq : aeq;
}

struct PianoBoard { std::vector<BodyModeSpec> modes; std::vector<double> areas; };
[[nodiscard]] inline PianoBoard pianoBoardAtKey(int midi) {
  const double a = kPianoBoardA, b = kPianoBoardB, h = kPianoBoardH;
  const double rhoh = kSpruceRho * h;
  const double Dx = kSpruceEL * h * h * h / 12.0, Dy = kPianoRibStiffnessRatio * Dx, H = std::sqrt(Dx * Dy);
  const double t = std::clamp((108.0 - midi) / 87.0, 0.0, 1.0);
  const double xb = piano_detail::lerp(0.25, 0.78, t) * a, yb = piano_detail::lerp(0.80, 0.30, t) * b;
  struct M { double f, mass, area; };
  std::vector<M> all;
  for (int m = 1; m <= 40; ++m)
    for (int n = 1; n <= 40; ++n) {
      const double km = m / a, kn = n / b;
      const double f = 0.5 * constants::pi * std::sqrt((Dx * std::pow(km, 4) + 2 * H * km * km * kn * kn + Dy * std::pow(kn, 4)) / rhoh);
      if (f > kPianoBoardMaxHz) continue;
      const double phi = std::sin(m * constants::pi * xb / a) * std::sin(n * constants::pi * yb / b);
      if (std::fabs(phi) < 0.15) continue;
      const double integral = ((m % 2) && (n % 2)) ? 4.0 * a * b / (m * n * constants::pi * constants::pi) : 0.0;
      all.push_back(M{f, rhoh * a * b / 4.0 / (phi * phi), pianoModeArea(f, phi, integral / phi, rhoh, H)});
    }
  std::sort(all.begin(), all.end(), [](const M& x, const M& y) { return x.f < y.f; });
  if (all.size() > static_cast<std::size_t>(kPianoBoardMaxModes)) all.resize(kPianoBoardMaxModes);
  double meanY = 0.0;
  constexpr int kPoints = 64;
  const double f0 = 440.0 * std::pow(2.0, (midi - 69) / 12.0);
  const double lo = std::clamp(f0 / 1.5, 100.0, 1100.0), hi = std::clamp(f0 * 1.5, 225.0, 2500.0);
  for (int i = 0; i < kPoints; ++i) {
    const double w = 2.0 * constants::pi * lo * std::pow(hi / lo, i / double(kPoints - 1));
    double re = 0.0, im = 0.0;  // Y = j w / (M (w0^2 - w^2 + j w w0/Q))
    for (const M& x : all) {
      const double w0 = 2.0 * constants::pi * x.f, dr = w0 * w0 - w * w, di = w * w0 / kPianoSoundboardQ;
      const double den = x.mass * (dr * dr + di * di);
      re += w * di / den; im += w * dr / den;
    }
    meanY += std::hypot(re, im) / kPoints;
  }
  const double massScale = meanY > 0.0 ? meanY / kPianoBridgeMobility : 1.0;
  for (M& x : all) x.mass *= massScale;
  PianoBoard out;
  for (const M& x : all) { out.modes.push_back(BodyModeSpec{x.f, kPianoSoundboardQ, x.mass}); out.areas.push_back(x.area); }
  return out;
}

// All strings of one key, struck at hammer speed v (m/s).
class PianoNoteVoice {
 public:
  [[nodiscard]] static Result<PianoNoteVoice, QuantityError> create(int midi, double speed, SampleRateHz rate,
                                                                    double distanceMeters = 1.0,
                                                                    double feltStiffnessScale = 0.0) {  // 0: pianoUprightFeltScale(midi)
    using R = Result<PianoNoteVoice, QuantityError>;
    auto base = pianoStringForKey(midi);
    if (!base.ok()) return R::failure(base.error());
    const double fac = base.value().densityFactor;
    const int strings = fac > kPianoMonochordFactor ? 1 : (fac > 1.0 ? 2 : 3);
    const PianoHammer hammer = pianoHammerForKey(midi);
    if (!(feltStiffnessScale > 0.0)) feltStiffnessScale = pianoUprightFeltScale(midi);
    PianoNoteVoice note;
    note.board_ = std::make_unique<PianoBoard>(pianoBoardAtKey(midi));
    if (note.board_->modes.empty()) return R::failure(QuantityError::RangeError);
    for (int i = 0; i < strings; ++i) {
      const double c = strings == 1 ? 0.0 : kPianoUnisonCents * (i / double(strings - 1) - 0.5);
      auto spec = pianoStringForKey(midi, c);
      if (!spec.ok()) return R::failure(spec.error());
      auto course = tunePianoString(spec.value());
      if (!course.ok()) return R::failure(course.error());
      auto stiff = tunedPianoString(spec.value());
      if (!stiff.ok()) return R::failure(stiff.error());
      auto ideal = courseString(course.value());
      if (!ideal.ok()) return R::failure(ideal.error());
      const double f = spec.value().publishedF0.value(), T1 = pianoT60AtF0(midi);
      auto loss = lossFromT60(ideal.value().speed, FrequencyHz{f}, DurationSeconds{T1}, FrequencyHz{6.0 * f},
                              DurationSeconds{T1 * kPianoT60Ratio6F0});
      if (!loss.ok()) return R::failure(loss.error());
      const HammerBodyDrive body{std::span<const BodyModeSpec>(note.board_->modes), 1.0, distanceMeters, 101325.0,
                                 293.15, std::span<const double>(note.board_->areas)};
      auto v = HammeredStringVoice::create(course.value(), HammerStrike{hammer.strikeFraction, hammer.massKg / strings, speed},
                                           loss.value(), stiff.value().stiffness, feltStiffnessScale * hammer.stiffness / strings, hammer.exponent, 0.0,
                                           4, 4, rate, body);
      if (!v.ok()) return R::failure(v.error());
      note.voices_.push_back(std::move(v).value());
    }
    return R::success(std::move(note));
  }
  // Adds the note into out[0..n): every string of the unison on the first
  // string's board (HammeredStringVoice::renderUnison).
  [[nodiscard]] Result<FrameCount, QuantityError> renderAdd(double* out, std::size_t n) {
    std::vector<double> tmp(n);
    auto r = voices_.front().renderUnison(std::span<HammeredStringVoice>(voices_).subspan(1), tmp.data(), FrameCount{n});
    if (!r.ok()) return r;
    for (std::size_t i = 0; i < n; ++i) out[i] += tmp[i];
    return Result<FrameCount, QuantityError>::success(FrameCount{n});
  }
  [[nodiscard]] int strings() const { return static_cast<int>(voices_.size()); }
 private:
  std::unique_ptr<PianoBoard> board_;
  std::vector<HammeredStringVoice> voices_;
};

}  // namespace mmathen
