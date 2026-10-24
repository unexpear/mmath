#pragma once

// One pressure-controlled valve on a cylindrical bore delay (docs/48).
//
// The bore is a round trip of fs * 2L/c samples. The integer part is a
// delay line; the fraction is a first-order Thiran interpolator, or, when
// the far end is the Levine–Schwinger load, that fraction is added to the
// end correction so it is not applied twice. One sample per step is the
// Courant condition for a wave moving at c. The far-end pressure
// reflection is the bore's open-end sign, −1, times the caller's
// magnitude in (0, 1]. A positive radiusMeters replaces that constant
// with the unflanged open-end filter in physics/radiation.h: the
// Levine–Schwinger load, end correction and radius both arguments.
// |R| = 1 does not decay. After the blow, a reed
// or a lip is held shut, so the mouth is a closed end and the ringing
// samples are that delay. Decay of the traveling wave is −ln(|R|) per
// round trip. Holding the valve shut is the player stopping, not an
// amplitude envelope. attackFrames and releaseFrames are a raised-cosine
// ramp of the blowing pressure. Zero leaves a step.
//
// The sample is the radiated pressure of the bell, not the mouth
// pressure. A compact open end is a monopole, p = ρ (dU_bell/dt) / (4 π r),
// with r = listenerDistanceMeters. The derivative is the radiation
// high-pass: a steady mouth pressure does not radiate. r is an argument.
// The omitted value is one metre.
//
// Three mouths share the junction. An inward reed solves
// Z U + ΔP = p_m − 2 p_in with U the odd Bernoulli jet through the
// opening S(y). y is the reed mass on Dalmont's spring, stopped at the
// lay. The embouchure damping is the caller's: if it is light, the reed
// can ring at its own resonance instead of the bore. The lip keeps its
// mass: opening is clamp(S0 + w y, 0, Smax) and the acceleration is
// trumpetLipAcceleration. The spring continues past those area limits,
// as trumpet.h states: there is no contact solver on this cylinder.
// The cone voice stops the displacement at the rails, because otherwise
// a high blowing pressure winds y up and pins the opening at Smax.
// Outward-striking lips oscillate when their resonance sits below a
// bore peak. A quasi-static outward lip does not.
//
// A flute jet is not that closed junction. The embouchure reflection is
// −1, and the acoustic jet flow is added through the bore impedance.
// The edge sees a Boussinesq jet: the captured fraction is tanh of the
// delayed deflection over the jet thickness (the integral of a sech²
// profile), minus the value at the caller's aim. Aim zero is an odd
// function of deflection and matches the small-signal slope of
// 2 U_j y / h. A nonzero aim is an even-harmonic operating point.
// y at the edge is the flue displacement delayed by W/(σ U). The flue
// displacement is the acoustic velocity times W/U, with the sign that
// pulls the jet out of the bore when flow enters the pipe. The
// mouthpiece cup, the saxophone cone, and a bell flare are not in this
// delay.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "../math/constants.h"
#include "../physics/medium.h"
#include "../physics/radiation.h"
#include "../render/voice.h"
#include "clarinet.h"
#include "flute.h"
#include "player_airway.h"
#include "saxophone.h"
#include "trombone.h"
#include "trumpet.h"

namespace mmathen {

enum class ValveLaw { InwardReed, OutwardLip, Jet };

// pressurePa is the blowing pressure while the valve is open. frames is
// how long it stays open. reflectionMagnitude is |R| at the far end.
// initialDisplacementMeters is the reed or lip displacement, or the
// flute's centerline displacement, at the start. All four are arguments.
// radiusMeters and endCorrectionMeters are the unflanged-pipe load.
// Both are arguments. A non-positive radius means the far end stays the
// constant magnitude above.
// attackFrames ramps the blowing pressure through attackPressureFraction:
// a quick rise to the threshold region, a creep across it, then the
// rest of the published duration. releaseFrames stays a raised cosine.
// listenerDistanceMeters is the free-field distance of the radiated
// sample. jetAimMeters shifts the flute jet off the edge center;
// zero is centered. reflectionCutoffHz is the low-pass reflection
// corner. Zero leaves the constant far-end magnitude and does not apply
// wall loss, so an ideal bore keeps its pitch and its |R|. A positive
// corner also enables Kirchhoff's wide-tube wall loss on the round
// trip. jetNoiseMeters is a turbulent perturbation of the flute
// centerline. Zero is a silent jet. radiationHz is the ka = 1 smoother
// on the radiated pressure, separate from the loop cutoff. Zero keeps
// the previous rule: a positive reflection cutoff supplies that corner.
// tongueReleaseFrames keeps a reed or a lip on its closed stop until
// that frame. A reed is then placed at initialDisplacementMeters. A lip
// stays shut and the blowing pressure accelerates it: snapping a lip to
// its rest opening is an already-open valve. Zero leaves the valve free
// from the first sample. Mouth pressure ramps on its own.
// reedTurbulence is the fraction of the reed-gap jet, or of a flue
// jet, added as a zero-mean disturbance. On a reed, one sample of
// that fraction of the mean aperture flow, over the reed period, is
// Z δU on the outgoing wave (reedGapSeedPressure): the sub-threshold
// stream and the seed the bore grows from. The same fraction is also
// radiated after the loop (reedApertureBreath). Below threshold the
// monopole uses the reed period 1/f_res; once the tone is louder
// than that jet the scale is the round-trip root-mean-square, which
// keeps the breath floor of a locked note. Wolfe's UNSW clarinet and
// saxophone pages describe the sub-threshold stream as breathy and
// print no ratio. Li, Almeida, Smith, and Wolfe (JASA 139, 2016)
// measured the fundamental growing out of the flow seed at a rate r;
// a soft note is the slow end of that rate. Bergeot, arXiv:1510.04793,
// is the rise of blowing pressure from silence with no tongue.
// Guillemain, JASA 121, 2396–2406 (2007), is the breath attack.
// A product with the instantaneous sample sits on the harmonics and
// the bins between them stay at the converter floor. A disturbance
// of the blowing pressure is cancelled by the pressure search, and
// on the shorter saxophone tubes that pressure locks the second
// mode, so the seed is a slice of the aperture flow and not that
// pressure. A lip uses the same seed and the same radiated breath,
// with the lip period 2π√(m/k) in place of the reed period: Fletcher
// and Rossing (chap. 13) treat the lip jet as turbulent as well.
// A flue jet does not add that pressure slice. On this pipe it pulls
// the fifth partial under −40 dB. The bore seed is the turbulent
// centerline offset, jetNoiseMeters. The radiated monopole uses the
// mixing-layer transit h/U until the standing wave is louder than
// that jet (Fletcher, Acustica 34, 1976; Coltman). Neither source
// prints a noise-to-harmonic ratio, so the fraction is an argument.
// Zero is the deterministic valve.
// brassiness is the fraction of the cylindrical Burgers advance
// applied to the traveling wave as it reaches the bell. β = (γ+1)/2
// and δt = β p L / (ρ c³). Hirschberg, Gilbert, Msallam, and Wijnands
// (JASA 99, 1754–1758, 1996) measured the shocks that advance produces
// in trombones. The steepened sample is what radiates; it is not
// written back into the delay. The one-sample Taylor step is not a
// conservative scheme, and feeding it back grew a length-change
// transient without bound and pulled the trumpet's limit cycle darker.
// On these notes the advance is about two samples at the peaks, so the
// bore does not shock; the clamp keeps that step inside one sample.
// A flare accumulates less than a cylinder, and that paper does not
// print a waveguide gain, so the fraction is an argument. Zero leaves
// the linear cone. A reed or a jet ignores it.
// spectrumBreakHz is Benade
// and Lutgen's break frequency fb, JASA 83, 1900–1907 (1988). Zero
// leaves the radiated pressure without that transfer.
struct ValveBlow {
  double pressurePa = 0.0;
  std::uint64_t frames = 0;
  double reflectionMagnitude = 0.0;
  double initialDisplacementMeters = 0.0;
  double radiusMeters = 0.0;
  double endCorrectionMeters = 0.0;
  std::uint64_t attackFrames = 0;
  std::uint64_t releaseFrames = 0;
  double listenerDistanceMeters = 1.0;
  double jetAimMeters = 0.0;
  double reflectionCutoffHz = 0.0;
  double jetNoiseMeters = 0.0;
  double radiationHz = 0.0;
  std::uint64_t tongueReleaseFrames = 0;
  double reedTurbulence = 0.0;
  // Fraction of the locked tone radiated as breath once the standing
  // wave is louder than the jet. Negative reuses reedTurbulence. The
  // flute uses a level read off the University of Iowa MIS flute
  // (measurement reference only); no paper prints this ratio.
  double lockedBreathFraction = -1.0;
  double brassiness = 0.0;
  // When positive, a lip voice steepens its bell wave with the Burgers
  // line over this fraction of the sounding length
  // (ConeValveVoice::setBoreBurgers), in place of `brassiness`.
  double burgersFraction = 0.0;
  // When positive, a lip voice holds this lip quality factor Q = m omega / r
  // as the lip resonance moves (ConeValveVoice::setLipQualityFactor).
  double lipQualityFactor = 0.0;
  double spectrumBreakHz = 0.0;
  // When positive, Kirchhoff's round-trip loss is fitted at this
  // frequency on the cylindrical bore radius, instead of the default
  // one-pole matched near 4 kHz on the cone's log-mean radius. Zero
  // keeps that default. The frequency is the caller's mode, not a
  // stored bandwidth.
  double wallMatchHz = 0.0;
  // Cylinder radius for Kirchhoff's ring, when the caller has one.
  // Zero uses the bore area. A cited slide bore can differ from the
  // cone area the lengths were tuned on.
  double wallRadiusMeters = 0.0;
  // When true (a cylindrical reed bore), the wall is a flat gain times a
  // one-pole, fitted to Kirchhoff at both the sounding fundamental and the
  // default match frequency, and refitted on every setSoundingLength.
  // Kirchhoff's loss grows as sqrt(f) and a lone one-pole's as f^2, so the
  // lone pole matched near 4 kHz loses about 1/16 of Kirchhoff at the
  // fundamental. False keeps the lone pole.
  bool wallFitFundamental = false;
  // Flared bell only (ConeValveVoice with a Bessel bell): when true the
  // radiated source is the wave the bell transmits, (1 + R) p+, rather
  // than the throat flow (p+ - R p+) / Z. With the bell's reflectance R
  // near -1 at low frequency and falling in magnitude above the flare
  // cutoff, the transmitted wave is a high-pass, the bell's measured
  // behaviour (Benade, Fundamentals of Musical Acoustics, ch. 20; Fletcher
  // and Rossing ch. 14: brass radiate little below the bell cutoff). The
  // throat flow is (1 - R) p+ / Z, a low-pass that is near 2 p+ / Z at the
  // fundamental. False keeps the throat flow.
  bool bellTransmission = false;
  // Bessel flare carried by the bore, a(x) = b (x + x0)^{−γ} with x
  // measured from the mouth. A positive length replaces a straight-pipe
  // Levine–Schwinger termination at radiusMeters with the input
  // reflectance of that horn. radiusMeters is still the mouth at which
  // Levine–Schwinger ends the horn. Zero leaves a straight pipe of
  // radiusMeters when that radius is positive. The four numbers are
  // the caller's profile. No bell curve is stored.
  double bellLengthMeters = 0.0;
  double bellFlare = 0.0;
  double bellCoefficient = 0.0;
  double bellMouthOffsetMeters = 0.0;
};

namespace valve_detail {

// Fraction of the full pressure during an attack, x in [0, 1].
//
// A single raised cosine is steepest at half pressure. These demo
// embouchures start to oscillate near that middle, so the cosine
// crosses threshold at its greatest slope and the fundamental then
// jumps (clarinet ~880 dB/s). Bergeot et al. (arXiv:1510.04793) show that
// a fast blowing-pressure rise delays the bifurcation and the amplitude
// then jumps; a gradual rise lets the
// oscillation grow while the pressure is still near threshold. Li,
// Almeida, Smith, and Wolfe (JASA 139, 2016) measured that near-threshold
// growth at tens to hundreds of dB/s on a soft note. The time and
// pressure fractions are arguments: those papers do not print this
// waveform. The slow segment is the middle of the published attack
// duration, not a longer attack.
[[nodiscard]] inline double attackPressureFraction(double x) {
  auto cosine = [](double u) { return 0.5 * (1.0 - std::cos(constants::pi * u)); };
  // Reach the region just under threshold quickly, creep through the
  // pressures where these reeds actually grow, then settle. A cosine on
  // the creep would be steepest in the middle of that band.
  constexpr double kTimeLo = 0.18;
  constexpr double kTimeHi = 0.72;
  constexpr double kPressureLo = 0.50;
  constexpr double kPressureHi = 0.78;
  if (x <= 0.0) return 0.0;
  if (x >= 1.0) return 1.0;
  if (x < kTimeLo) return kPressureLo * cosine(x / kTimeLo);
  if (x < kTimeHi) {
    const double u = (x - kTimeLo) / (kTimeHi - kTimeLo);
    return kPressureLo + (kPressureHi - kPressureLo) * u;
  }
  const double u = (x - kTimeHi) / (1.0 - kTimeHi);
  return kPressureHi + (1.0 - kPressureHi) * cosine(u);
}

// Attack uses attackPressureFraction. Release stays a raised cosine, 0
// at the end of the blow. A zero attack or release is a step. Past
// blowFrames the player has stopped.
[[nodiscard]] inline double blowingPressure(double full, std::uint64_t blowFrames,
                                            std::uint64_t attackFrames,
                                            std::uint64_t releaseFrames,
                                            std::uint64_t frame) {
  if (!(full > 0.0) || frame >= blowFrames) return 0.0;
  if (attackFrames > 0 && frame < attackFrames) {
    const double x =
        static_cast<double>(frame) / static_cast<double>(attackFrames);
    return full * attackPressureFraction(x);
  }
  if (releaseFrames > 0 && blowFrames > releaseFrames &&
      frame + releaseFrames >= blowFrames) {
    const double x = static_cast<double>(blowFrames - frame) /
                     static_cast<double>(releaseFrames);
    return full * 0.5 * (1.0 - std::cos(constants::pi * x));
  }
  return full;
}

// Compact monopole of the bell flow, valid while ka is small:
// p = ρ (dU/dt) / (4 π r). Above ka = 1 the unflanged pipe stops
// getting brighter, so the derivative is smoothed with a one-pole at
// ω = c / a. The stored flow and state are the previous sample.
// Distance, corner, and dt are strictly positive.
[[nodiscard]] inline double radiateBell(double density, double distance, double flow,
                                        double& previousFlow, double& state,
                                        double cornerRadPerSec, double dt) {
  const double derivative = (flow - previousFlow) / dt;
  previousFlow = flow;
  const double gain = 1.0 - std::exp(-cornerRadPerSec * dt);
  state += gain * (derivative - state);
  return density * state / (4.0 * constants::pi * distance);
}

[[nodiscard]] inline bool blowShapeSane(const ValveBlow& blow) {
  if (!std::isfinite(blow.listenerDistanceMeters) ||
      !(blow.listenerDistanceMeters > 0.0) || !std::isfinite(blow.jetAimMeters))
    return false;
  if (blow.attackFrames > blow.frames || blow.releaseFrames > blow.frames)
    return false;
  if (blow.attackFrames > 0 && blow.releaseFrames > 0 &&
      blow.attackFrames + blow.releaseFrames > blow.frames)
    return false;
  if (!std::isfinite(blow.reflectionCutoffHz) || blow.reflectionCutoffHz < 0.0 ||
      !std::isfinite(blow.jetNoiseMeters) || blow.jetNoiseMeters < 0.0 ||
      !std::isfinite(blow.radiationHz) || blow.radiationHz < 0.0)
    return false;
  if (blow.tongueReleaseFrames > blow.frames) return false;
  if (!std::isfinite(blow.reedTurbulence) || blow.reedTurbulence < 0.0 ||
      blow.reedTurbulence > 1.0 || !std::isfinite(blow.brassiness) ||
      blow.brassiness < 0.0 || blow.brassiness > 1.0 ||
      !std::isfinite(blow.spectrumBreakHz) || blow.spectrumBreakHz < 0.0 ||
      !std::isfinite(blow.wallMatchHz) || blow.wallMatchHz < 0.0 ||
      !std::isfinite(blow.wallRadiusMeters) || blow.wallRadiusMeters < 0.0 ||
      !std::isfinite(blow.bellLengthMeters) || blow.bellLengthMeters < 0.0 ||
      !std::isfinite(blow.bellFlare) || blow.bellFlare < 0.0 ||
      !std::isfinite(blow.bellCoefficient) || blow.bellCoefficient < 0.0 ||
      !std::isfinite(blow.bellMouthOffsetMeters) || blow.bellMouthOffsetMeters < 0.0)
    return false;
  return true;
}

// Unit sample in (−1, 1] from a numerical recipes LCG. The sequence is
// the disturbance, not a stored spectrum.
[[nodiscard]] inline double unitNoise(std::uint32_t& state) {
  state = state * 1664525u + 1013904223u;
  return (static_cast<double>(state & 0x00ffffffu) / 16777215.0) * 2.0 - 1.0;
}

// Breath smoothed one octave under the 0.4 fs Nyquist guard. A flat
// draw to fs/2 puts a fifth of its power above that guard. The corner
// is 0.2 fs, so the gain does not depend on a stored hertz value.
[[nodiscard]] inline double breathSample(std::uint32_t& state, double& smoother) {
  const double white = unitNoise(state);
  const double gain = 1.0 - std::exp(-constants::twoPi * 0.2);
  smoother += gain * (white - smoother);
  return smoother;
}

// Pressure added to the outgoing bore wave by one sample of reed-gap
// turbulence. The mean aperture flow is scaled by reedTurbulence, the
// caller's fraction: Fletcher and Rossing (chap. 13) and Wolfe's UNSW
// saxophone and clarinet notes call that stream turbulent and print
// no ratio. The correlation time is the reed period 1/f_res.
// f_res is ReedInertia::resonanceHz, from Dalmont's stiffness and the
// caller's mass, not a stored musical frequency. A sample shorter
// than that period carries dt·f_res of the fraction. The product
// enters as Z δU on the outgoing wave, after the pressure search, so
// the search does not cancel it. Li, Almeida, Smith, and Wolfe, JASA
// 139 (2016): the flow pulse seeds the bore, and the reed's mechanical
// energy is already gone on that time scale. A seed written into the
// blowing pressure locks a short saxophone on its second mode; this
// term is a slice of the aperture flow, not that pressure.
[[nodiscard]] inline double reedGapSeedPressure(std::uint32_t& noise, double& smoother,
                                                double meanFlow, double turbulence,
                                                double impedance, double resonanceHz,
                                                double dt) {
  if (!(turbulence > 0.0) || !(meanFlow > 0.0) || !(impedance > 0.0) ||
      !(resonanceHz > 0.0) || !(dt > 0.0))
    return 0.0;
  const double slice = dt * resonanceHz;
  if (!(slice > 0.0) || !(slice < 1.0)) return 0.0;
  const double fluctuation = turbulence * meanFlow * slice * breathSample(noise, smoother);
  const double pressure = impedance * fluctuation;
  if (!std::isfinite(pressure)) return 0.0;
  return pressure;
}

// Far-field pressure of reed-gap turbulence for one sample.
//
// apertureFlow is the absolute Bernoulli volume flow through the
// mouthpiece (m³/s). radiated is the bell pressure before this term.
// flowMean and oscEnv are round-trip smoothers owned by the voice.
// Below the oscillation the jet radiates as ρ (U/τ) / (4 π r). τ is
// the reed period 1/f_res when the caller has one, otherwise the round
// trip. f_res is the spring's own resonance, not a stored hertz.
// Once the radiated tone is louder than that jet, the weight is
// reedTurbulence times the oscillation's round-trip root-mean-square,
// which is the breath floor a locked spectrum was tuned with. The two
// expressions meet when the tone equals the jet. Guillemain, JASA 121,
// 2396–2406 (2007), is the breath-attack synthesis. This function does
// not impose the exponential envelope; that growth is the bore.
[[nodiscard]] inline double reedApertureBreath(double& flowMean, double& oscEnv,
                                               std::uint32_t& noise, double& smoother,
                                               double apertureFlow, double radiated,
                                               double turbulence, double density,
                                               double listener, double tripSamples,
                                               double sampleRate, double reedPeriodSeconds,
                                               bool sustain = true,
                                               bool* lockedOut = nullptr,
                                               double lockedTurbulence = -1.0) {
  if (!(turbulence > 0.0) || !(tripSamples > 1.0) || !(sampleRate > 0.0) ||
      !(listener > 0.0) || !(density > 0.0))
    return 0.0;
  const double leak = 1.0 - std::exp(-1.0 / tripSamples);
  const double flow = std::fabs(apertureFlow);
  flowMean += leak * (flow - flowMean);
  if (flowMean < 0.0) flowMean = 0.0;
  oscEnv += leak * (radiated * radiated - oscEnv);
  if (oscEnv < 0.0) oscEnv = 0.0;
  const double tone = std::sqrt(oscEnv);
  double correlation = tripSamples / sampleRate;
  if (reedPeriodSeconds > 0.0 && std::isfinite(reedPeriodSeconds))
    correlation = reedPeriodSeconds;
  const double breathP =
      density * (flowMean / correlation) / (4.0 * constants::pi * listener);
  if (!std::isfinite(breathP) || !std::isfinite(tone)) return 0.0;
  const double jet = turbulence * breathP;
  if (lockedOut != nullptr) *lockedOut = tone >= breathP;
  const double lockedFraction = lockedTurbulence >= 0.0 ? lockedTurbulence : turbulence;
  const double locked = sustain ? lockedFraction * tone : 0.0;
  const double amp = tone >= breathP ? locked : jet;
  if (!std::isfinite(amp)) return 0.0;
  return amp * breathSample(noise, smoother);
}

// External spectrum of a beating saxophone reed, Benade and Lutgen,
// JASA 83, 1900–1907 (1988):
//
//   E(x) = [x / (1 + x^7)]^{1/2},   x = f / fb.
//
// Below fb the envelope rises about 3 dB/octave; above fb it falls
// about 18 dB/octave. fb is an argument. Their alto measurement is
// 837 Hz and their tenor measurement is 618 Hz; neither is stored
// here. The transfer is the length-512 frequency-sampling FIR of E,
// normalized so its peak is 1. A zero break frequency is a wire.
struct BreakEnvelope {
  static constexpr int kTaps = 512;

  void reset() { std::fill(memory.begin(), memory.end(), 0.0); }

  [[nodiscard]] bool active() const { return !impulse.empty(); }

  void design(double breakHz, double sampleRateHz) {
    impulse.clear();
    memory.clear();
    cursor = 0;
    if (!(breakHz > 0.0) || !(sampleRateHz > 2.0 * breakHz)) return;
    double magnitude[kTaps / 2 + 1] = {};
    double peak = 0.0;
    for (int k = 0; k <= kTaps / 2; ++k) {
      const double frequency = sampleRateHz * static_cast<double>(k) / kTaps;
      const double x = frequency / breakHz;
      const double value =
          k == 0 ? 0.0 : std::sqrt(x / (1.0 + std::pow(x, 7.0)));
      magnitude[k] = value;
      if (value > peak) peak = value;
    }
    if (!(peak > 0.0)) return;
    impulse.assign(static_cast<std::size_t>(kTaps), 0.0);
    for (int n = 0; n < kTaps; ++n) {
      const double center = static_cast<double>(n) - 0.5 * kTaps;
      double sum = magnitude[0];
      for (int k = 1; k < kTaps / 2; ++k) {
        sum += 2.0 * magnitude[k] *
               std::cos(constants::twoPi * static_cast<double>(k) * center / kTaps);
      }
      sum += magnitude[kTaps / 2] *
             std::cos(constants::pi * center);
      impulse[static_cast<std::size_t>(n)] = sum / (peak * kTaps);
    }
    memory.assign(static_cast<std::size_t>(kTaps), 0.0);
  }

  [[nodiscard]] double step(double x) {
    if (impulse.empty()) return x;
    memory[static_cast<std::size_t>(cursor)] = x;
    double y = 0.0;
    for (int i = 0; i < kTaps; ++i) {
      int index = cursor - i;
      if (index < 0) index += kTaps;
      y += impulse[static_cast<std::size_t>(i)] * memory[static_cast<std::size_t>(index)];
    }
    cursor += 1;
    if (cursor == kTaps) cursor = 0;
    return y;
  }

  std::vector<double> impulse;
  std::vector<double> memory;
  int cursor = 0;
};

// One-pole low-pass, unity DC gain: (1−p)/(1−p z^{−1}). p = 0 is a wire.
struct TripLoss {
  double pole = 0.0;
  double state = 0.0;
  double gain = 1.0;  // flat part of a fitted wall; 1 for a lone pole

  void reset() { state = 0.0; }

  [[nodiscard]] double step(double x) {
    // A wire still carries its state, so a pole engaged later (the
    // brass ring wall at a tongue stop) starts from the wave it is
    // given. A stale state there was a one-sample step in the bell
    // flow: winds pro6 trumpet clicks at every tongue stop, the bulk
    // of its 8-16 kHz band (-24.7 dB, -37.0 without them).
    if (!(pole > 0.0)) {
      state = x;
      return gain * x;
    }
    const double y = (1.0 - pole) * x + pole * state;
    state = y;
    return gain * y;
  }
};

// Pole in (0, 1) whose response magnitude at frequencyHz equals magnitude.
// The quadratic is p + 1/p = 2b, and the root below 1 is kept.
[[nodiscard]] inline double onePoleMatching(double magnitude, double frequencyHz,
                                            double sampleRateHz) {
  if (!(magnitude > 0.0) || !(magnitude < 1.0) || !(frequencyHz > 0.0) ||
      !(sampleRateHz > frequencyHz * 2.0))
    return 0.0;
  const double omega = constants::twoPi * frequencyHz / sampleRateHz;
  const double cosine = std::cos(omega);
  const double m2 = magnitude * magnitude;
  const double gap = 1.0 - m2;
  if (!(gap > 0.0)) return 0.0;
  const double b = (1.0 - m2 * cosine) / gap;
  const double disc = b * b - 1.0;
  if (!(disc > 0.0) || !std::isfinite(disc)) return 0.0;
  const double pole = b - std::sqrt(disc);
  if (!std::isfinite(pole) || !(pole > 0.0) || !(pole < 1.0)) return 0.0;
  return pole;
}

[[nodiscard]] inline double onePoleMagnitude(double pole, double frequencyHz,
                                             double sampleRateHz);

// Gain g and pole p with g |P(lowHz)| = lowMagnitude and
// g |P(highHz)| = highMagnitude (one-pole P, bisection on p). A ratio the
// pole cannot reach leaves p = 0 and a flat g = lowMagnitude.
inline void fitGainPole(double lowHz, double lowMagnitude, double highHz,
                        double highMagnitude, double sampleRateHz, double& pole,
                        double& gain) {
  pole = 0.0;
  gain = lowMagnitude;
  if (!(lowMagnitude > 0.0) || !(lowMagnitude <= 1.0) || !(highMagnitude > 0.0) ||
      !(lowHz > 0.0) || !(highHz > lowHz) || !(sampleRateHz > 2.0 * highHz))
    return;
  const double ratio = highMagnitude / lowMagnitude;
  if (!(ratio < 1.0)) return;
  double lo = 0.0, hi = 0.999999;
  for (int i = 0; i < 80; ++i) {
    const double mid = 0.5 * (lo + hi);
    const double r = onePoleMagnitude(mid, highHz, sampleRateHz) /
                     onePoleMagnitude(mid, lowHz, sampleRateHz);
    if (r > ratio) lo = mid; else hi = mid;
  }
  pole = 0.5 * (lo + hi);
  gain = lowMagnitude / onePoleMagnitude(pole, lowHz, sampleRateHz);
  if (!std::isfinite(gain) || !(gain > 0.0) || gain > 1.0) {
    pole = 0.0;
    gain = lowMagnitude;
  }
}

[[nodiscard]] inline double onePoleMagnitude(double pole, double frequencyHz,
                                             double sampleRateHz) {
  if (!(pole > 0.0) || !(frequencyHz > 0.0) || !(sampleRateHz > 0.0)) return 1.0;
  const double omega = constants::twoPi * frequencyHz / sampleRateHz;
  const double den =
      std::sqrt(1.0 + pole * pole - 2.0 * pole * std::cos(omega));
  if (!(den > 0.0) || !std::isfinite(den)) return 1.0;
  const double magnitude = (1.0 - pole) / den;
  if (!std::isfinite(magnitude) || !(magnitude > 0.0)) return 1.0;
  return magnitude;
}

// Samples of phase delay, −φ/ω, of the unity-DC one-pole.
[[nodiscard]] inline double onePolePhaseDelay(double pole, double frequencyHz,
                                              double sampleRateHz) {
  if (!(pole > 0.0) || !(pole < 1.0) || !(frequencyHz > 0.0) ||
      !(sampleRateHz > 0.0))
    return 0.0;
  const double omega = constants::twoPi * frequencyHz / sampleRateHz;
  if (!(omega > 0.0)) return 0.0;
  const double phase =
      -std::atan2(pole * std::sin(omega), 1.0 - pole * std::cos(omega));
  const double delay = -phase / omega;
  if (!std::isfinite(delay) || delay < 0.0) return 0.0;
  return delay;
}

// Round-trip transmission exp(−α 2L) of Kirchhoff's wide tube.
// μ(T) is Sutherland's law for dry air, μ0 = 1.716e−5 Pa·s at 273.15 K
// with S = 110.4 K. Prandtl's number for air is 0.71. T is recovered
// from c = sqrt(γ R T). α = sqrt(ω ν / 2) (1 + (γ−1)/sqrt(Pr)) / (a c).
[[nodiscard]] inline double kirchhoffTripMagnitude(double frequencyHz, double radius,
                                                   double length, double speed,
                                                   double density) {
  if (!(frequencyHz > 0.0) || !(radius > 0.0) || !(length > 0.0) ||
      !(speed > 0.0) || !(density > 0.0))
    return 1.0;
  const double temperature =
      (speed * speed) /
      (acoustic_constants::dryAirGamma * acoustic_constants::dryAirGasConstant);
  if (!(temperature > 150.0) || !(temperature < 2000.0)) return 1.0;
  const double t0 = 273.15;
  const double sutherland = 110.4;
  const double mu0 = 1.716e-5;
  const double mu = mu0 * std::pow(temperature / t0, 1.5) * (t0 + sutherland) /
                    (temperature + sutherland);
  if (!(mu > 0.0) || !std::isfinite(mu)) return 1.0;
  const double nu = mu / density;
  const double prandtl = 0.71;
  const double thermal =
      1.0 + (acoustic_constants::dryAirGamma - 1.0) / std::sqrt(prandtl);
  const double omega = constants::twoPi * frequencyHz;
  const double alpha =
      std::sqrt(omega * nu / 2.0) * thermal / (radius * speed);
  if (!std::isfinite(alpha) || alpha < 0.0) return 1.0;
  const double transmission = std::exp(-alpha * 2.0 * length);
  if (!std::isfinite(transmission) || !(transmission > 0.0) || transmission > 1.0)
    return 1.0;
  return transmission;
}

// Sample written `age` steps ago. age == line.size() is the slot the
// legacy loop reads, which is the full round trip. The fraction is a
// linear interpolation toward the next older sample.
[[nodiscard]] inline double delayRead(const std::vector<double>& line, std::size_t writePos,
                                      double age) {
  const int n = static_cast<int>(line.size());
  if (n < 1 || !std::isfinite(age)) return 0.0;
  if (age < 1.0) age = 1.0;
  if (age > static_cast<double>(n)) age = static_cast<double>(n);
  const int older = static_cast<int>(std::floor(age));
  const double fraction = age - static_cast<double>(older);
  auto at = [&](int steps) {
    int index = static_cast<int>(writePos) - steps;
    index %= n;
    if (index < 0) index += n;
    return line[static_cast<std::size_t>(index)];
  };
  const double recent = at(older);
  if (!(fraction > 1.0e-9) || older >= n) return recent;
  return (1.0 - fraction) * recent + fraction * at(older + 1);
}

}  // namespace valve_detail

class BoreValveVoice final : public IVoice {
 public:
  [[nodiscard]] static Result<BoreValveVoice, QuantityError> fromClarinet(
      const Clarinet& clarinet, const ValveBlow& blow, SampleRateHz rate) {
    return fromReed(clarinet.reed, clarinet.inertia, clarinet.bore.lengthMeters,
                    clarinet.bore.crossSectionM2,
                    clarinet.bore.soundSpeedMetersPerSecond,
                    clarinet.bore.airDensityKgPerM3,
                    clarinet.bore.acousticImpedance, blow, rate);
  }

  // The reed and the throat area of the saxophone, on a cylinder of the
  // saxophone's length. The apex distance, the bell area, and the cone
  // impedance are not read. This is not a truncated-cone stepper.
  [[nodiscard]] static Result<BoreValveVoice, QuantityError>
  fromSaxophoneCylinder(const Saxophone& saxophone, const ValveBlow& blow,
                        SampleRateHz rate) {
    return fromReed(saxophone.reed, saxophone.inertia, saxophone.lengthMeters,
                    saxophone.throatAreaM2, saxophone.soundSpeedMetersPerSecond,
                    saxophone.airDensityKgPerM3, saxophone.acousticImpedance,
                    blow, rate);
  }

  // The lip on the trumpet's cylinder. The cup compliance is not stepped.
  [[nodiscard]] static Result<BoreValveVoice, QuantityError> fromTrumpet(
      const Trumpet& trumpet, const ValveBlow& blow, SampleRateHz rate) {
    return fromLip(trumpet, blow, rate);
  }

  // The lip on the slide's air column L = L0 + 2x. The cup is not stepped.
  [[nodiscard]] static Result<BoreValveVoice, QuantityError> fromTrombone(
      const Trombone& trombone, const ValveBlow& blow, SampleRateHz rate) {
    return fromLip(trombone.bore, blow, rate);
  }

  [[nodiscard]] static Result<BoreValveVoice, QuantityError> fromFlute(
      const Flute& flute, const ValveBlow& blow, SampleRateHz rate) {
    using R = Result<BoreValveVoice, QuantityError>;
    if (!blowSane(blow) || !(blow.pressurePa > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    const double fs = rate.value();
    if (!std::isfinite(fs) || !(fs > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    auto travel = fluteConvectionTime(flute, blow.pressurePa);
    if (!travel.ok()) return R::failure(travel.error());
    const double samples = fs * travel.value();
    if (!std::isfinite(samples) ||
        samples > static_cast<double>(std::numeric_limits<int>::max() / 4))
      return R::failure(QuantityError::RangeError);
    int convection = static_cast<int>(std::llround(samples));
    if (convection < 1) convection = 1;
    auto built = makeCylinder(flute.lengthMeters, flute.crossSectionM2,
                              flute.soundSpeedMetersPerSecond,
                              flute.airDensityKgPerM3, flute.acousticImpedance,
                              ValveLaw::Jet, blow, rate);
    if (!built.ok()) return R::failure(built.error());
    BoreValveVoice voice = std::move(built).value();
    voice.jetArea_ = flute.jetAreaM2;
    voice.jetThickness_ = flute.jetThicknessMeters;
    voice.flueToEdge_ = flute.flueToEdgeMeters;
    voice.jetAim_ = blow.jetAimMeters;
    voice.jetDelay_.assign(static_cast<std::size_t>(convection),
                           blow.initialDisplacementMeters);
    voice.flueY_ = blow.initialDisplacementMeters;
    voice.flowOutput_ = false;
    return R::success(std::move(voice));
  }

  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }

  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(
      double* out, FrameCount n) override {
    using R = Result<FrameCount, QuantityError>;
    const std::uint64_t count = static_cast<std::uint64_t>(n.value());
    if (count == 0) return R::success(n);
    if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
    const double dt = 1.0 / rate_.value();
    for (std::uint64_t i = 0; i < count; ++i) {
      // The player's airway, when attached, turns the blowing-pressure
      // schedule into the lung target and returns the mouth pressure
      // with the tract's acoustic answer on it (player_airway.h).
      if (airway_ != nullptr)
        airMouth_ = airway_->mouthPressure(valve_detail::blowingPressure(
            pressure_, blowFrames_, attackFrames_, releaseFrames_, frame_));
      // Wall loss is on the trip. The reflection cutoff shapes only the
      // wave that returns: an open end radiates what it does not reflect,
      // the same split ConeValveVoice uses. Filtering the incident wave
      // before the bell deletes the harmonics the reed or the jet just
      // put on the bore.
      const double incident = readBore();
      const double reflected = cutoff_.step(incident);
      const double incoming =
          radiating_ ? openEnd_.step(reflected) : reflection_ * reflected;
      if (!std::isfinite(incoming)) return R::failure(QuantityError::RangeError);
      double outgoing = 0.0;
      if (law_ == ValveLaw::Jet) {
        auto stepped = stepJet(incoming, outgoing, dt);
        if (!stepped.ok()) return R::failure(stepped.error());
      } else if (law_ == ValveLaw::InwardReed) {
        auto stepped = stepReed(incoming, outgoing, dt);
        if (!stepped.ok()) return R::failure(stepped.error());
      } else {
        auto stepped = stepLip(incoming, outgoing, dt);
        if (!stepped.ok()) return R::failure(stepped.error());
      }
      if (!std::isfinite(outgoing)) return R::failure(QuantityError::RangeError);
      line_[pos_] = outgoing;
      pos_ = (pos_ + 1) % line_.size();
      const double bellFlow = (incident - incoming) / impedance_;
      double radiated = valve_detail::radiateBell(density_, listener_, bellFlow, bellFlow_,
                                                 radiationState_, radiationCorner_, dt);
      // After the loop, so the limit cycle is unchanged. Reed, lip, and
      // jet all use the aperture jet (reedApertureBreath). See
      // ValveBlow::reedTurbulence.
      const bool open = frame_ < blowFrames_;
      const bool tongued = tongueFrames_ > 0 && frame_ < tongueFrames_;
      const double mouth = mouthNow();
      if (reedTurbulence_ > 0.0 && law_ == ValveLaw::InwardReed && open && mouth > 0.0 &&
          !tongued) {
        const double trip =
            line_.empty() ? 1.0 : static_cast<double>(line_.size());
        const double reedPeriod =
            reed_.inertia.resonanceHz > 0.0 ? 1.0 / reed_.inertia.resonanceHz : 0.0;
        radiated += valve_detail::reedApertureBreath(
            flowMean_, breathEnv_, noise_, breathState_, apertureFlow_, radiated,
            reedTurbulence_, density_, listener_, trip, rate_.value(), reedPeriod, true,
            nullptr, lockedBreath_);
      } else if (reedTurbulence_ > 0.0 && law_ == ValveLaw::Jet && open && mouth > 0.0) {
        // Fletcher, Acustica 34 (1976), and Coltman: the jet and the
        // edge-tone noise are present before the pipe locks. The
        // correlation time is the mixing-layer transit h/U, not the
        // round-trip envelope of a tone that does not exist yet.
        // Once the standing wave is louder than that jet, the fraction
        // scales the round-trip envelope, as on the reed.
        const double trip =
            line_.empty() ? 1.0 : static_cast<double>(line_.size());
        const double jetPeriod =
            (jetSpeed_ > 0.0 && jetThickness_ > 0.0) ? jetThickness_ / jetSpeed_ : 0.0;
        radiated += valve_detail::reedApertureBreath(
            flowMean_, breathEnv_, noise_, breathState_, apertureFlow_, radiated,
            reedTurbulence_, density_, listener_, trip, rate_.value(), jetPeriod, true,
            nullptr, lockedBreath_);
      } else if (reedTurbulence_ > 0.0 && law_ == ValveLaw::OutwardLip && open && mouth > 0.0 &&
                 !tongued) {
        const double trip =
            line_.empty() ? 1.0 : static_cast<double>(line_.size());
        const double lipPeriod = lipPeriodSeconds();
        radiated += valve_detail::reedApertureBreath(
            flowMean_, breathEnv_, noise_, breathState_, apertureFlow_, radiated,
            reedTurbulence_, density_, listener_, trip, rate_.value(), lipPeriod);
      }
      out[i] = radiated;
      if (!std::isfinite(out[i])) return R::failure(QuantityError::RangeError);
      if (airway_ != nullptr) airway_->step(apertureFlow_);
      ++frame_;
    }
    return R::success(n);
  }

  // Couple the player's airway (not owned). Null detaches it; the mouth
  // pressure is then the blowing-pressure schedule itself.
  void attachAirway(PlayerAirway* airway) { airway_ = airway; }

  void reset() override {
    std::fill(line_.begin(), line_.end(), 0.0);
    pos_ = 0;
    boreX_ = 0.0;
    boreY_ = 0.0;
    openEnd_.reset();
    wall_.reset();
    cutoff_.reset();
    noise_ = 1u;
    breathEnv_ = 0.0;
    breathState_ = 0.0;
    flowMean_ = 0.0;
    apertureFlow_ = 0.0;
    std::fill(jetDelay_.begin(), jetDelay_.end(), initialDisplacement_);
    jetPos_ = 0;
    displacement_ = initialDisplacement_;
    velocity_ = 0.0;
    flueY_ = initialDisplacement_;
    bellFlow_ = 0.0;
    radiationState_ = 0.0;
    frame_ = 0;
  }

  [[nodiscard]] ValveLaw law() const noexcept { return law_; }
  [[nodiscard]] bool outputsFlow() const noexcept { return flowOutput_; }
  [[nodiscard]] double fundamentalHz() const noexcept { return fundamentalHz_; }
  [[nodiscard]] double reflectionDecay() const noexcept {
    return reflectionDecay_;
  }
  [[nodiscard]] int delaySamples() const noexcept { return delaySamples_; }

  // Mouth pressure after the attack. The raised cosine still uses this
  // value as its full pressure.
  bool setBlowingPressure(double pressurePa) {
    if (!std::isfinite(pressurePa) || pressurePa < 0.0) return false;
    pressure_ = pressurePa;
    return true;
  }

  // Absolute frame at which the tongue lets the valve go. Until that
  // frame a reed or a lip stays on its closed stop. Zero clears it.
  // The jet has no tongue stop; a flute attack is the pressure ramp.
  void scheduleTongue(std::uint64_t releaseFrame) { tongueFrames_ = releaseFrame; }

  // Sounding length in metres. The delay line is allocated at
  // construction and cannot grow, so this only shortens the round trip
  // already allocated. It is the air column to an open hole, or a slide
  // position, not a stepped tone-hole lattice and not a valve chart.
  // Until this succeeds, readBore is the construction delay.
  bool setSoundingLength(double lengthMeters) {
    if (!std::isfinite(lengthMeters) || !(lengthMeters > 0.0) || !(speed_ > 0.0))
      return false;
    const double fs = rate_.value();
    double air = lengthMeters;
    if (!radiating_) air += endCorrection_;
    if (!(air > 0.0) || !std::isfinite(fs) || !(fs > 0.0)) return false;
    // Same round-trip accounting as makeCylinder. There is no half-sample
    // Nyquist average on this bore, so none is removed here.
    double timed = fs * 2.0 * air / speed_;
    const double geometric = (law_ == ValveLaw::Jet) ? speed_ / (2.0 * air)
                                                     : speed_ / (4.0 * air);
    if (wallFit_) {
      // Refit the wall to this air column (blow.wallFitFundamental).
      const double match = std::min(0.3 * fs, std::max(12.0 * geometric, 4000.0));
      const double high = valve_detail::kirchhoffTripMagnitude(
          match, wallFitRadius_, air, speed_, density_);
      const double low = valve_detail::kirchhoffTripMagnitude(
          geometric, wallFitRadius_, air, speed_, density_);
      double pole = 0.0, gain = 1.0;
      if (geometric < match)
        valve_detail::fitGainPole(geometric, low, match, high, fs, pole, gain);
      if (pole > 0.0 || gain < 1.0) {
        wall_.pole = pole;
        wall_.gain = gain;
      }
    }
    if (!radiating_) {
      timed -= valve_detail::onePolePhaseDelay(wall_.pole, geometric, fs);
      timed -= valve_detail::onePolePhaseDelay(cutoff_.pole, geometric, fs);
    }
    if (!std::isfinite(timed) || !(timed > 1.0)) return false;
    const double limit = static_cast<double>(line_.size());
    if (timed > limit + 2.0) return false;
    if (timed > limit) timed = limit;
    variableAge_ = timed;
    variableRead_ = true;
    return true;
  }

  // Unloaded lip resonance. The spring is k = m (2 π f)^2, the same
  // relation makeTrumpet stores. A reed or a jet has no lip spring.
  bool setLipResonanceHz(double hertz) {
    if (law_ != ValveLaw::OutwardLip) return false;
    if (!std::isfinite(hertz) || !(hertz > 0.0) || !(trumpet_.lipMassKg > 0.0))
      return false;
    const double omega = constants::twoPi * hertz;
    const double stiffness = trumpet_.lipMassKg * omega * omega;
    if (!std::isfinite(stiffness) || !(stiffness > 0.0)) return false;
    trumpet_.lipStiffnessNewtonsPerMeter = stiffness;
    return true;
  }

  // Convection delay of the jet, in samples. The buffer is the delay
  // allocated from the construction pressure and cannot grow. The
  // default read stays the integer buffer until this succeeds.
  bool setJetDelaySamples(double samples) {
    if (law_ != ValveLaw::Jet || jetDelay_.empty()) return false;
    if (!std::isfinite(samples)) return false;
    const double limit = static_cast<double>(jetDelay_.size());
    if (samples < 1.0 || samples > limit + 2.0) return false;
    if (samples > limit) samples = limit;
    jetAge_ = samples;
    jetVariable_ = true;
    return true;
  }

 private:
  struct ReedPack {
    ReedCharacteristic reed;
    ReedInertia inertia;
  };

  BoreValveVoice(SampleRateHz rate, ValveLaw law, ValveBlow blow, double zeta,
                 double density, double area, int delay, double fundamentalHz,
                 double reflectionDecay)
      : rate_(rate),
        law_(law),
        pressure_(blow.pressurePa),
        blowFrames_(blow.frames),
        initialDisplacement_(blow.initialDisplacementMeters),
        displacement_(blow.initialDisplacementMeters),
        flueY_(blow.initialDisplacementMeters),
        reflection_(zeta),
        density_(density),
        area_(area),
        listener_(blow.listenerDistanceMeters),
        attackFrames_(blow.attackFrames),
        releaseFrames_(blow.releaseFrames),
        tongueFrames_(blow.tongueReleaseFrames),
        jetAim_(blow.jetAimMeters),
        delaySamples_(delay),
        fundamentalHz_(fundamentalHz),
        reflectionDecay_(reflectionDecay),
        line_(static_cast<std::size_t>(delay), 0.0) {}

  [[nodiscard]] static bool blowSane(const ValveBlow& blow) {
    return std::isfinite(blow.pressurePa) && blow.pressurePa >= 0.0 &&
           std::isfinite(blow.reflectionMagnitude) &&
           blow.reflectionMagnitude > 0.0 && blow.reflectionMagnitude <= 1.0 &&
           std::isfinite(blow.initialDisplacementMeters) &&
           valve_detail::blowShapeSane(blow);
  }

  [[nodiscard]] static Result<BoreValveVoice, QuantityError> makeCylinder(
      double length, double area, double speed, double density, double impedance,
      ValveLaw law, const ValveBlow& blow, SampleRateHz rate) {
    using R = Result<BoreValveVoice, QuantityError>;
    const double fs = rate.value();
    if (!blowSane(blow) || !std::isfinite(fs) || !(fs > 0.0) ||
        !std::isfinite(length) || !(length > 0.0) || !std::isfinite(area) ||
        !(area > 0.0) || !std::isfinite(speed) || !(speed > 0.0) ||
        !std::isfinite(density) || !(density > 0.0) ||
        !std::isfinite(impedance) || !(impedance > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    // A non-radiating bore can still carry Levine's unflanged end
    // correction, 0.61 a, as extra length. A positive radius already
    // puts that correction inside OpenEndReflection, so it is not added
    // again. Zero correction leaves the geometric length.
    double airColumn = length;
    if (!(blow.radiusMeters > 0.0)) airColumn += blow.endCorrectionMeters;
    if (!std::isfinite(airColumn) || !(airColumn > 0.0))
      return R::failure(QuantityError::RangeError);
    const double roundTrip = 2.0 * airColumn / speed;
    const double geometric = (law == ValveLaw::Jet) ? speed / (2.0 * airColumn)
                                                    : speed / (4.0 * airColumn);
    const bool lossy = blow.reflectionCutoffHz > 0.0;
    double wallPole = 0.0;
    double wallGain = 1.0;
    double cutPole = 0.0;
    double timed = fs * roundTrip;
    if (lossy) {
      const double radius = std::sqrt(area / constants::pi);
      // Match Kirchhoff at the 4th harmonic so the one-pole does not
      // invent a cutoff below the bore. The caller's corner is the
      // radiation reflection, a separate section.
      const double match = std::min(0.3 * fs, std::max(12.0 * geometric, 4000.0));
      const double wall = valve_detail::kirchhoffTripMagnitude(
          match, radius, airColumn, speed, density);
      wallPole = valve_detail::onePoleMatching(wall, match, fs);
      if (blow.wallFitFundamental && geometric < match) {
        const double low = valve_detail::kirchhoffTripMagnitude(
            geometric, radius, airColumn, speed, density);
        valve_detail::fitGainPole(geometric, low, match, wall, fs, wallPole, wallGain);
      }
      cutPole = std::exp(-constants::twoPi * blow.reflectionCutoffHz / fs);
      if (!std::isfinite(cutPole) || !(cutPole > 0.0) || !(cutPole < 1.0))
        return R::failure(QuantityError::RangeError);
      if (!(blow.radiusMeters > 0.0)) {
        timed -= valve_detail::onePolePhaseDelay(wallPole, geometric, fs);
        timed -= valve_detail::onePolePhaseDelay(cutPole, geometric, fs);
      }
    }
    const double samples = timed;
    if (!std::isfinite(roundTrip) || !(roundTrip > 0.0) || !std::isfinite(samples) ||
        !(samples > 1.0) ||
        samples > static_cast<double>(std::numeric_limits<int>::max() / 4))
      return R::failure(QuantityError::RangeError);
    int delay = static_cast<int>(std::floor(samples));
    double fraction = samples - static_cast<double>(delay);
    if (delay < 1) {
      delay = 1;
      fraction = 0.0;
    }
    if (fraction < 1.0e-9) fraction = 0.0;
    if (fraction > 1.0 - 1.0e-9) {
      fraction = 0.0;
      ++delay;
    }
    const bool radiating = blow.radiusMeters > 0.0;
    double zeta = -blow.reflectionMagnitude;
    double decay = 0.0;
    double hertz = 0.0;
    double boreThiran = 0.0;
    bool useBoreThiran = false;
    OpenEndReflection openEnd;
    if (radiating) {
      const double fractionMeters = fraction * speed / (2.0 * fs);
      auto filter = OpenEndReflection::create(
          blow.radiusMeters, blow.endCorrectionMeters + fractionMeters, speed, rate);
      if (!filter.ok()) return R::failure(filter.error());
      openEnd = std::move(filter).value();
      const bool openOpen = law == ValveLaw::Jet;
      auto solved = openEnd.resonanceHz(delay, openOpen);
      if (!solved.ok()) return R::failure(solved.error());
      auto response = openEnd.response(solved.value());
      if (!response.ok()) return R::failure(response.error());
      const double magnitude =
          std::hypot(response.value().real, response.value().imag);
      if (!(magnitude > 0.0) || !(magnitude <= 1.0))
        return R::failure(QuantityError::RangeError);
      hertz = solved.value();
      const double trip = openOpen ? 1.0 / hertz : 1.0 / (2.0 * hertz);
      decay = -std::log(magnitude) / trip;
      zeta = -1.0;
    } else {
      if (fraction > 0.0) {
        boreThiran = (1.0 - fraction) / (1.0 + fraction);
        if (!std::isfinite(boreThiran) || !(std::fabs(boreThiran) < 1.0))
          return R::failure(QuantityError::RangeError);
        useBoreThiran = true;
      }
      decay = -std::log(blow.reflectionMagnitude) / roundTrip;
      hertz = geometric;
    }
    if (lossy) {
      const double extra = wallGain * valve_detail::onePoleMagnitude(wallPole, hertz, fs) *
                           valve_detail::onePoleMagnitude(cutPole, hertz, fs);
      if (!(extra > 0.0) || !std::isfinite(extra))
        return R::failure(QuantityError::RangeError);
      decay -= std::log(extra) / roundTrip;
    }
    if (!std::isfinite(decay) || !std::isfinite(hertz) || !(hertz > 0.0))
      return R::failure(QuantityError::RangeError);
    BoreValveVoice voice(rate, law, blow, zeta, density, area, delay, hertz,
                         decay);
    voice.impedance_ = impedance;
    voice.radiating_ = radiating;
    voice.openEnd_ = std::move(openEnd);
    voice.boreThiran_ = boreThiran;
    voice.useBoreThiran_ = useBoreThiran;
    const double radius = std::sqrt(area / constants::pi);
    voice.wall_.pole = wallPole;
    voice.wall_.gain = wallGain;
    voice.wallFit_ = lossy && blow.wallFitFundamental;
    voice.wallFitRadius_ = radius;
    voice.cutoff_.pole = cutPole;
    voice.speed_ = speed;
    voice.endCorrection_ = radiating ? 0.0 : blow.endCorrectionMeters;
    voice.jetNoise_ = blow.jetNoiseMeters;
    voice.reedTurbulence_ = blow.reedTurbulence;
  voice.lockedBreath_ = blow.lockedBreathFraction;
    voice.radiationCorner_ = speed / radius;
    // The caller's corner is the bell/embouchure radiation cutoff. It
    // replaces c/a of the bore when the bore is not the radiating mouth.
    // radiationHz, when set, is that smoother alone. The loop cutoff
    // stays on the returning wave.
    if (blow.radiationHz > 0.0)
      voice.radiationCorner_ = constants::twoPi * blow.radiationHz;
    else if (lossy)
      voice.radiationCorner_ = constants::twoPi * blow.reflectionCutoffHz;
    if (!std::isfinite(voice.radiationCorner_) || !(voice.radiationCorner_ > 0.0))
      return R::failure(QuantityError::RangeError);
    return R::success(std::move(voice));
  }

  [[nodiscard]] static Result<BoreValveVoice, QuantityError> fromReed(
      const ReedCharacteristic& reed, const ReedInertia& inertia, double length,
      double area, double speed, double density, double impedance,
      const ValveBlow& blow, SampleRateHz rate) {
    using R = Result<BoreValveVoice, QuantityError>;
    if (!(blow.pressurePa > 0.0)) return R::failure(QuantityError::InvalidArgument);
    auto built = makeCylinder(length, area, speed, density, impedance,
                              ValveLaw::InwardReed, blow, rate);
    if (!built.ok()) return R::failure(built.error());
    BoreValveVoice voice = std::move(built).value();
    voice.reed_.reed = reed;
    voice.reed_.inertia = inertia;
    voice.closing_ = inertia.closingDisplacementMeters;
    return R::success(std::move(voice));
  }

  [[nodiscard]] static Result<BoreValveVoice, QuantityError> fromLip(
      const Trumpet& trumpet, const ValveBlow& blow, SampleRateHz rate) {
    using R = Result<BoreValveVoice, QuantityError>;
    if (!(blow.pressurePa > 0.0)) return R::failure(QuantityError::InvalidArgument);
    auto built = makeCylinder(
        trumpet.lengthMeters, trumpet.crossSectionM2,
        trumpet.soundSpeedMetersPerSecond, trumpet.airDensityKgPerM3,
        trumpet.acousticImpedance, ValveLaw::OutwardLip, blow, rate);
    if (!built.ok()) return R::failure(built.error());
    BoreValveVoice voice = std::move(built).value();
    voice.trumpet_ = trumpet;
    voice.restOpening_ = trumpet.restOpeningAreaM2;
    voice.maximumOpening_ = trumpet.maximumOpeningAreaM2;
    voice.slitWidth_ = trumpet.slitWidthMeters;
    return R::success(std::move(voice));
  }


  [[nodiscard]] double readBore() {
    double x = line_[pos_];
    if (variableRead_) {
      x = valve_detail::delayRead(line_, pos_, variableAge_);
    } else if (useBoreThiran_) {
      const double y = boreThiran_ * x + boreX_ - boreThiran_ * boreY_;
      boreX_ = x;
      boreY_ = y;
      x = y;
    }
    // The reflection cutoff is applied to the returning wave in
    // renderBlock, not to this incident read.
    return wall_.step(x);
  }

  [[nodiscard]] double mouthNow() const {
    if (airway_ != nullptr) return airMouth_;
    return valve_detail::blowingPressure(pressure_, blowFrames_, attackFrames_,
                                         releaseFrames_, frame_);
  }

  // Tongue contact on the valve (1 when no airway is attached).
  [[nodiscard]] double tongueOpen() const {
    return airway_ != nullptr ? airway_->tongueOpenFraction() : 1.0;
  }

  [[nodiscard]] Result<double, QuantityError> solveDelta(
      double opening, double target) const {
    using R = Result<double, QuantityError>;
    if (!(opening > 0.0)) return R::success(target);
    double lo = target < 0.0 ? target : 0.0;
    double hi = target > 0.0 ? target : 0.0;
    if (!(hi > lo)) return R::success(lo);
    for (int iteration = 0; iteration < 64; ++iteration) {
      const double mid = 0.5 * (lo + hi);
      const double flow = bernoulli(opening, mid);
      if (!std::isfinite(flow)) return R::failure(QuantityError::RangeError);
      const double residual = impedance_ * flow + mid - target;
      if (!std::isfinite(residual)) return R::failure(QuantityError::RangeError);
      if (residual > 0.0)
        hi = mid;
      else
        lo = mid;
    }
    return R::success(0.5 * (lo + hi));
  }

  // Lip period from the caller's mass and stiffness. Zero when this
  // voice is not a lip.
  [[nodiscard]] double lipPeriodSeconds() const {
    if (!(trumpet_.lipMassKg > 0.0) || !(trumpet_.lipStiffnessNewtonsPerMeter > 0.0))
      return 0.0;
    const double omega =
        std::sqrt(trumpet_.lipStiffnessNewtonsPerMeter / trumpet_.lipMassKg);
    if (!(omega > 0.0)) return 0.0;
    return constants::twoPi / omega;
  }

  [[nodiscard]] double bernoulli(double opening, double deltaP) const {
    if (!(opening > 0.0) || deltaP == 0.0) return 0.0;
    const double speed = std::sqrt(2.0 * std::fabs(deltaP) / density_);
    return std::copysign(opening * speed, deltaP);
  }

  [[nodiscard]] Result<bool, QuantityError> stepReed(double incoming, double& outgoing,
                                                     double dt) {
    using R = Result<bool, QuantityError>;
    const bool open = frame_ < blowFrames_;
    const double mouth = mouthNow();
    const bool tongued = tongueFrames_ > 0 && frame_ < tongueFrames_;
    if (!open || tongued) {
      displacement_ = closing_;
      velocity_ = 0.0;
    } else if (tongueFrames_ > 0 && frame_ == tongueFrames_) {
      displacement_ = initialDisplacement_;
      velocity_ = 0.0;
    }
    auto reedArea =
        reedAreaAtDisplacement(reed_.reed, reed_.inertia, displacement_);
    if (!reedArea.ok()) return R::failure(reedArea.error());
    // The tongue on the reed shuts part of the channel (Li et al. 2016).
    const Result<double, QuantityError> opening =
        Result<double, QuantityError>::success(reedArea.value() * tongueOpen());
    const double target = mouth - 2.0 * incoming;
    auto delta = solveDelta(opening.value(), target);
    if (!delta.ok()) return R::failure(delta.error());
    apertureFlow_ = 0.0;
    if (open && mouth > 0.0 && !tongued && opening.value() > 0.0)
      apertureFlow_ = std::fabs(bernoulli(opening.value(), delta.value()));
    const double pressure = mouth - delta.value();
    outgoing = pressure - incoming;
    if (apertureFlow_ > 0.0)
      outgoing += valve_detail::reedGapSeedPressure(
          noise_, breathState_, apertureFlow_, reedTurbulence_, impedance_,
          reed_.inertia.resonanceHz, dt);
    if (!std::isfinite(outgoing) || !std::isfinite(pressure))
      return R::failure(QuantityError::RangeError);
    if (open && mouth > 0.0 && !tongued) {
      // Exact spring step. Explicit Euler chatters on this mass; see
      // stepReedTip. The lay is an inelastic stop.
      auto moved = stepReedTip(reed_.inertia, displacement_, velocity_,
                               delta.value(), dt);
      if (!moved.ok()) return R::failure(moved.error());
      displacement_ = moved.value().displacementMeters;
      velocity_ = moved.value().velocityMetersPerSecond;
      if (displacement_ > closing_) {
        displacement_ = closing_;
        if (velocity_ > 0.0) velocity_ = 0.0;
      }
    }
    return R::success(true);
  }

  [[nodiscard]] Result<bool, QuantityError> stepLip(double incoming, double& outgoing,
                                                    double dt) {
    using R = Result<bool, QuantityError>;
    const bool open = frame_ < blowFrames_;
    const double mouth = mouthNow();
    const bool tongued = tongueFrames_ > 0 && frame_ < tongueFrames_;
    if (!open || tongued) {
      displacement_ = -restOpening_ / slitWidth_;
      velocity_ = 0.0;
    } else if (tongueFrames_ > 0 && frame_ == tongueFrames_) {
      // Stay on the closed stop. The pressure accelerates the lip;
      // the rest opening is not where a tongue lets go.
      velocity_ = 0.0;
    }
    double opening = restOpening_ + slitWidth_ * displacement_;
    if (opening < 0.0) opening = 0.0;
    if (opening > maximumOpening_) opening = maximumOpening_;
    // Same closed stop as the cone: a tongue is a shut lip, not a
    // roundoff slit. See ConeValveVoice.
    if (tongued || !open) opening = 0.0;
    opening *= tongueOpen();
    // Same roundoff slit as ConeValveVoice: the closed stop does not
    // multiply back to zero, and the pressure search then fails.
    if (restOpening_ > 0.0 && opening < restOpening_ * 1.0e-6) opening = 0.0;
    const double target = mouth - 2.0 * incoming;
    auto delta = solveDelta(opening, target);
    if (!delta.ok()) return R::failure(delta.error());
    const double pressure = mouth - delta.value();
    outgoing = pressure - incoming;
    apertureFlow_ = 0.0;
    if (open && mouth > 0.0 && !tongued && opening > 0.0)
      apertureFlow_ = std::fabs(bernoulli(opening, delta.value()));
    if (apertureFlow_ > 0.0) {
      // Fletcher and Rossing, chap. 13: the lip jet is turbulent. The
      // correlation frequency is the lip's own resonance, from the
      // caller's mass and stiffness, the same role f_res plays for a
      // reed. The fraction is an argument; that chapter prints no ratio.
      const double lipHz = lipPeriodSeconds() > 0.0 ? 1.0 / lipPeriodSeconds() : 0.0;
      outgoing += valve_detail::reedGapSeedPressure(
          noise_, breathState_, apertureFlow_, reedTurbulence_, impedance_, lipHz, dt);
    }
    if (!std::isfinite(outgoing) || !std::isfinite(pressure))
      return R::failure(QuantityError::RangeError);
    if (open && mouth > 0.0 && !tongued) {
      auto acceleration = trumpetLipAcceleration(
          trumpet_, displacement_, velocity_, delta.value());
      if (!acceleration.ok()) return R::failure(acceleration.error());
      velocity_ += acceleration.value() * dt;
      displacement_ += velocity_ * dt;
      if (!std::isfinite(velocity_) || !std::isfinite(displacement_))
        return R::failure(QuantityError::RangeError);
    }
    return R::success(true);
  }

  [[nodiscard]] Result<bool, QuantityError> stepJet(double incoming, double& outgoing,
                                                    double dt) {
    using R = Result<bool, QuantityError>;
    const bool open = frame_ < blowFrames_;
    const double mouth = mouthNow();
    double jetFlow = 0.0;
    if (mouth > 0.0) {
      auto flow = fluteJetFlow(jetFlute(), mouth);
      if (!flow.ok()) return R::failure(flow.error());
      // The tongue behind the teeth cuts the jet's supply area.
      jetFlow = flow.value() * tongueOpen();
    }
    double yEdge = flueY_;
    if (!jetDelay_.empty()) {
      yEdge = jetVariable_ ? valve_detail::delayRead(jetDelay_, jetPos_, jetAge_)
                           : jetDelay_[jetPos_];
    }
    // tanh is the captured fraction of a sech² jet. The factor 2 matches
    // the centered small-signal slope 2 U_j y / h of the top-hat jet.
    // A turbulent offset, argument jetNoiseMeters, is the breath noise
    // of the mixing layer. Zero leaves a deterministic jet.
    const double thickness = jetThickness_;
    if (!(thickness > 0.0)) return R::failure(QuantityError::InvalidArgument);
    if (open && mouth > 0.0 && jetNoise_ > 0.0) {
      noise_ = noise_ * 1664525u + 1013904223u;
      const double unit =
          (static_cast<double>(noise_ & 0x00ffffffu) / 16777215.0) * 2.0 - 1.0;
      yEdge += jetNoise_ * unit;
    }
    const double eta = std::tanh(yEdge / thickness + jetAim_ / thickness);
    const double eta0 = std::tanh(jetAim_ / thickness);
    const double acoustic = jetFlow * 2.0 * (eta - eta0);
    outgoing = -incoming + impedance_ * acoustic;
    apertureFlow_ = open && mouth > 0.0 ? std::fabs(jetFlow) : 0.0;
    jetSpeed_ = 0.0;
    if (open && mouth > 0.0) {
      auto speed = fluteJetSpeed(jetFlute(), mouth);
      if (!speed.ok()) return R::failure(speed.error());
      jetSpeed_ = speed.value();
    }
    // The turbulent centerline offset above is what seeds the bore.
    // Fletcher, Acustica 34 (1976), and Coltman: edge-tone noise is
    // present before the pipe locks. A pressure slice of the whole
    // jet, at U/h or at the convection rate U/W, pulls the fifth
    // partial of this note under −40 dB, so that slice is not added.
    // The audible breath before lock-in is the radiated monopole,
    // correlation h/U, in the jet branch of renderBlock.
    (void)dt;
    const double boreFlow = (outgoing - incoming) / impedance_;
    if (!std::isfinite(outgoing) || !std::isfinite(boreFlow))
      return R::failure(QuantityError::RangeError);
    // The flue channel sets the centerline from the acoustic velocity.
    // The scale W/U is the jet's own travel time across the mouth, not a
    // second time constant. Positive bore flow pulls the jet out of the
    // bore; the convection delay then carries that deflection to the edge.
    if (open && mouth > 0.0 && area_ > 0.0) {
      auto speed = fluteJetSpeed(jetFlute(), mouth);
      if (!speed.ok()) return R::failure(speed.error());
      if (speed.value() > 0.0) {
        flueY_ = -(boreFlow / area_) * (flueToEdge_ / speed.value());
        if (!std::isfinite(flueY_)) return R::failure(QuantityError::RangeError);
      }
    } else if (!open) {
      flueY_ = 0.0;
    }
    if (!jetDelay_.empty()) {
      jetDelay_[jetPos_] = flueY_;
      jetPos_ = (jetPos_ + 1) % jetDelay_.size();
    }
    return R::success(true);
  }

  // fluteJetFlow and fluteJetFraction need a Flute. The stored jet
  // numbers are copied onto a temporary that carries only those fields
  // plus the bore area already checked at construction.
  [[nodiscard]] Flute jetFlute() const {
    Flute flute;
    flute.lengthMeters = 1.0;
    flute.crossSectionM2 = area_;
    flute.soundSpeedMetersPerSecond = 1.0;
    flute.airDensityKgPerM3 = density_;
    flute.acousticImpedance = impedance_;
    flute.jetAreaM2 = jetArea_;
    flute.jetThicknessMeters = jetThickness_;
    flute.flueToEdgeMeters = 1.0;
    flute.convectionRatio = 1.0;
    flute.pressureReflection = -1.0;
    return flute;
  }

  SampleRateHz rate_;
  ValveLaw law_ = ValveLaw::InwardReed;
  double pressure_ = 0.0;
  std::uint64_t blowFrames_ = 0;
  double initialDisplacement_ = 0.0;
  double displacement_ = 0.0;
  double velocity_ = 0.0;
  double flueY_ = 0.0;
  double reflection_ = -1.0;
  double impedance_ = 0.0;
  double density_ = 0.0;
  double area_ = 0.0;
  double listener_ = 1.0;
  std::uint64_t attackFrames_ = 0;
  std::uint64_t releaseFrames_ = 0;
  std::uint64_t tongueFrames_ = 0;
  double jetAim_ = 0.0;
  double jetNoise_ = 0.0;
  double jetSpeed_ = 0.0;
  double reedTurbulence_ = 0.0;
  double lockedBreath_ = -1.0;
  valve_detail::TripLoss wall_{};
  bool wallFit_ = false;
  double wallFitRadius_ = 0.0;
  valve_detail::TripLoss cutoff_{};
  std::uint32_t noise_ = 1u;
  double breathEnv_ = 0.0;
  double breathState_ = 0.0;
  double flowMean_ = 0.0;
  double apertureFlow_ = 0.0;
  PlayerAirway* airway_ = nullptr;
  double airMouth_ = 0.0;
  double bellFlow_ = 0.0;
  double radiationState_ = 0.0;
  double radiationCorner_ = 1.0;
  double boreThiran_ = 0.0;
  double boreX_ = 0.0;
  double boreY_ = 0.0;
  bool useBoreThiran_ = false;
  double closing_ = 0.0;
  double restOpening_ = 0.0;
  double maximumOpening_ = 0.0;
  double slitWidth_ = 0.0;
  double jetArea_ = 0.0;
  double jetThickness_ = 0.0;
  double flueToEdge_ = 0.0;
  double speed_ = 0.0;
  double endCorrection_ = 0.0;
  double variableAge_ = 0.0;
  double jetAge_ = 0.0;
  bool variableRead_ = false;
  bool jetVariable_ = false;
  int delaySamples_ = 1;
  double fundamentalHz_ = 0.0;
  double reflectionDecay_ = 0.0;
  bool flowOutput_ = false;
  bool radiating_ = false;
  OpenEndReflection openEnd_{};
  ReedPack reed_{};
  Trumpet trumpet_{};
  std::vector<double> line_;
  std::vector<double> jetDelay_;
  std::size_t pos_ = 0;
  std::size_t jetPos_ = 0;
  std::uint64_t frame_ = 0;
};

}  // namespace mmathen
