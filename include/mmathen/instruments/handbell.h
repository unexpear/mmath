#pragma once

// English handbell: a bronze bell held mouth-up and rung, its spring
// clapper striking the inside of the soundbow. Modal shell, no samples.
//
// Modes. (2,0) is the fundamental; English tuning puts (3,0) at 3.0 times it
// (Rossing and Sathoff, "Modes of vibration and sound radiation from tuned
// handbells", JASA 68(6), 1600-1607, 1980). Higher modes are not tuned and
// their order depends on size (Rossing and Perrin, "Vibrations of bells",
// Applied Acoustics 20, 1987, table 1); their ratios here are measured from
// a brass F3 handbell recording (Freesound 557142, CC0), labelled MEASURED,
// with their (m, n) assignment VERIFY.
//
// Strike. The clapper is a point mass on a felt or rubber pad, power-law
// contact F = K d^p (felt exponent p = 2.5, Chaigne and Askenfelt, JASA
// 95(2), 1994), at the soundbow, where every (m, n) here has a rim
// antinode on the strike meridian. Modal mass mu M_bell (shell modes with
// a cos(m theta) rim pattern: mu about 1/4 of the moving rim mass, VERIFY).
//
// Radiation. Mode (m, 0) radiates as a 2^m-pole around the bell axis; its
// far-field pressure per unit rim acceleration is taken as
// rho a^2 (k a)^(m-1) / (4 pi r 2^(m-1) (m-1)!) (low-ka multipole order
// counting, Morse and Ingard sec. 7.2; VERIFY prefactor), saturating above
// ka = 1. Rossing and
// Sathoff measured each mode radiating also at twice its frequency
// (strongest on axis). Here that comes from the mouth's area change: a rim
// r(theta) = a + q cos(m theta) encloses pi a^2 + (pi/2) q^2, so the
// mouth breathes a volume (pi/2) q^2 h_e (h_e an effective rim height,
// VERIFY) and radiates as a monopole, rho/(4 pi r) d^2/dt^2 of it: second
// harmonic from (2,0), sixth from (3,0) (each mode's own q^2; cross terms
// between modes of different m vanish around the rim).
//
// Losses. A constant decay rate per bell (measured: F3 handbell sigma
// 0.35-0.39 1/s on (2,0), (3,0) and the 6.1 mode), scaled with the
// fundamental as sqrt(f1 / 175 Hz) to the Freesound radwoc G6 bell
// (sigma 1.2 1/s); TUNED.

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "../core/result.h"
#include "../math/constants.h"
#include "../physics/oscillator.h"
#include "../render/voice.h"
#include "../units/physical_units.h"

namespace mmathen {

struct HandbellMode {
  double ratio;   // to (2,0)
  int m;          // nodal meridian pairs
  int n;          // nodal circles
};
inline constexpr std::array<HandbellMode, 8> kHandbellModes = {{
    {1.00, 2, 0},   // (2,0) fundamental
    {3.00, 3, 0},   // (3,0) English tuning, Rossing and Sathoff 1980
    {4.83, 4, 0},   // MEASURED (Freesound 557142), (m,n) VERIFY
    {5.06, 3, 1},   // MEASURED, VERIFY
    {6.10, 4, 1},   // MEASURED, VERIFY
    {7.52, 5, 0},   // MEASURED, VERIFY
    {8.05, 5, 1},   // MEASURED, VERIFY
    {9.12, 6, 0},   // MEASURED, VERIFY
}};
// Size: diameter proportional to f^-1/2 (Rossing, "Acoustics of
// handbells", JASA 91, 2450, 1992); C5 mouth diameter 0.114 m and
// bell mass 0.45 kg (VERIFY, Malmark catalogue range); mass scaled as
// diameter^3 (h / d^2 proportional to f, same paper, gives d^3 for the
// shell mass at fixed shape).
inline constexpr double kHandbellC5DiameterM = 0.114;  // VERIFY
inline constexpr double kHandbellC5MassKg = 0.45;      // VERIFY
inline constexpr double kHandbellModalMassFraction = 0.25;  // VERIFY
inline constexpr double kHandbellRimHeightFraction = 0.25;  // h_e / a, VERIFY
inline constexpr double kHandbellSigmaAtF3 = 0.37;     // 1/s, TUNED (measured)
// Clapper: 25 g head at F3 (VERIFY) on a felt pad, K TUNED to the
// (3,0)/(2,0) balance of the F3 recording (-6 dB; model -4) at 1 m/s.
// Smaller bells carry smaller, harder clappers: the head mass is scaled
// with the bell mass, (f1/F3)^-1.5 (UNSOURCED), and K as (f1/F3)^1.5
// (TUNED: the radwoc G6 bell, English-tuned, (3,0) at -7 dB; model -10).
inline constexpr double kHandbellClapperMassKg = 0.025;  // at F3, VERIFY
inline constexpr double kHandbellClapperExponent = 2.5;  // Chaigne-Askenfelt felt
inline constexpr double kHandbellClapperStiffness = 1.0e9;  // N/m^2.5 at F3, TUNED
inline constexpr double kHandbellClapperRefHz = 174.61;
inline double handbellClapperMassKg(double f1) {
  return kHandbellClapperMassKg * std::pow(f1 / kHandbellClapperRefHz, -1.5);
}
inline double handbellClapperStiffness(double f1) {
  return kHandbellClapperStiffness * std::pow(f1 / kHandbellClapperRefHz, 1.5);  // TUNED exponent
}

class HandbellVoice final : public IVoice {
 public:
  // f1 the (2,0) frequency (the note); clapper speed in m/s; listener 1 m
  // off the bell's side (UNSOURCED). Clapper stiffness and mass of zero
  // take the size-scaled defaults above.
  [[nodiscard]] static Result<HandbellVoice, QuantityError> create(
      double f1, double speedMps, SampleRateHz rate, double distanceM = 1.0,
      double clapperStiffness = 0.0, int oversample = 4,
      double clapperMassKg = 0.0) {
    using R = Result<HandbellVoice, QuantityError>;
    if (!(f1 > 20.0) || !(speedMps > 0.0) || !(rate.value() > 0.0) || !(distanceM > 0.0) ||
        !(clapperStiffness >= 0.0) || oversample < 1 || !(clapperMassKg >= 0.0))
      return R::failure(QuantityError::InvalidArgument);
    HandbellVoice v;
    v.rate_ = rate;
    v.factor_ = oversample;
    v.dt_ = 1.0 / (rate.value() * oversample);
    v.speed_ = speedMps;
    v.K_ = clapperStiffness > 0.0 ? clapperStiffness : handbellClapperStiffness(f1);
    v.clapperMass_ = clapperMassKg > 0.0 ? clapperMassKg : handbellClapperMassKg(f1);
    const double rho0 = 1.204, c = 343.2;
    const double d = kHandbellC5DiameterM * std::sqrt(523.25 / f1), a = 0.5 * d;
    const double mass = kHandbellC5MassKg * std::pow(d / kHandbellC5DiameterM, 3.0);
    const double sigma = kHandbellSigmaAtF3 * std::sqrt(f1 / 174.61);
    v.he_ = kHandbellRimHeightFraction * a;
    v.monopole_ = rho0 / (4.0 * constants::pi * distanceM);
    for (const HandbellMode& hm : kHandbellModes) {
      const double hz = hm.ratio * f1;
      if (hz > 0.45 * rate.value()) continue;
      Mode o;
      o.omega = constants::twoPi * hz;
      o.sigma = sigma;
      o.mass = kHandbellModalMassFraction * mass;
      o.m = hm.m;
      const double ka = o.omega / c * a;
      double fact = 1.0;
      for (int j = 2; j < hm.m; ++j) fact *= j;
      // Low-ka multipole growth h = (ka)^(m-1) / (2^(m-1) (m-1)!),
      // saturating as h / sqrt(1 + h^2): above the coincidence every
      // order radiates with the same efficiency (a sphere's modal
      // radiation resistance tends to rho c for every order once ka
      // exceeds the order, Morse and Ingard sec. 7.2), so the order
      // prefactor saturates with the ka growth (VERIFY).
      const double h = std::pow(ka, hm.m - 1) / (std::pow(2.0, hm.m - 1) * fact);
      o.radiate = rho0 * a * a * h / std::sqrt(1.0 + h * h) /
                  (4.0 * constants::pi * distanceM);
      if (hm.n > 0) o.radiate *= 0.5;  // a nodal circle splits the rim region (VERIFY)
      v.modes_.push_back(o);
    }
    v.reset();
    return R::success(std::move(v));
  }

  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }
  [[nodiscard]] std::size_t modeCount() const noexcept { return modes_.size(); }
  [[nodiscard]] double contactSeconds() const noexcept { return contact_; }

  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(double* out, FrameCount n) override {
    using R = Result<FrameCount, QuantityError>;
    const auto count = static_cast<std::size_t>(n.value());
    if (count == 0) return R::success(n);
    if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
    for (std::size_t frame = 0; frame < count; ++frame) {
      double sum = 0.0;
      for (int sub = 0; sub < factor_; ++sub) {
        double u = 0.0;
        for (const Mode& m : modes_) u += m.s.displacement;
        const double pen = y_ - u;
        double F = 0.0;
        if (pen > 0.0 && armed_) {
          F = K_ * std::pow(pen, kHandbellClapperExponent);
          contact_ += dt_;
        } else if (contact_ > 0.0 && vy_ < 0.0) {
          armed_ = false;  // one stroke: the clapper spring holds it off after the bounce
        }
        double p = 0.0, breath = 0.0;
        for (Mode& m : modes_) {
          const double force = F / m.mass;
          const double acc = force - 2.0 * m.sigma * m.s.velocity - m.omega * m.omega * m.s.displacement;
          auto st = stepConstantForceOscillator(m.s, m.omega, m.sigma, force, dt_);
          if (!st.ok()) return R::failure(st.error());
          m.s = st.value();
          p += m.radiate * acc;
          // d^2/dt^2 of (pi/2) q^2 h_e = pi h_e (q'^2 + q q'')
          breath += constants::pi * he_ * (m.s.velocity * m.s.velocity + m.s.displacement * acc);
        }
        p += monopole_ * breath;
        vy_ += -F / clapperMass_ * dt_;
        y_ += vy_ * dt_;
        if (!std::isfinite(p)) return R::failure(QuantityError::RangeError);
        sum += p;
      }
      out[frame] = sum / factor_;
    }
    return R::success(n);
  }

  void reset() override {
    for (Mode& m : modes_) m.s = {};
    y_ = 0.0;
    vy_ = speed_;
    contact_ = 0.0;
    armed_ = true;
  }

 private:
  struct Mode {
    double omega = 0.0, sigma = 0.0, mass = 1.0, radiate = 0.0;
    int m = 2;
    OscillatorState s;
  };
  std::vector<Mode> modes_;
  SampleRateHz rate_{48000.0};
  int factor_ = 1;
  double dt_ = 0.0, speed_ = 0.0, K_ = 0.0, clapperMass_ = kHandbellClapperMassKg, he_ = 0.0, monopole_ = 0.0;
  double y_ = 0.0, vy_ = 0.0, contact_ = 0.0;
  bool armed_ = true;
};

}  // namespace mmathen
