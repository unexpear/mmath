#pragma once

// A mallet, one or two drum heads, a closed air cavity, and a monopole.
//
// The cavity pressure at long wavelength is the acoustic compliance
// already used for a mouthpiece cup, C_a = V/(ρ c²), so
// p = (ρ c² / V) ΔV. V is π a² times the caller's shell depth. No port
// radius is stored. Positive modal displacement points into the cavity.
// The volume coupling of mode (m, n) is ∫ φ dA. For φ = J_m(α r/a) that
// integral is zero when m ≠ 0, and 2 π a² J_1(α)/α when m = 0.
//
// The compliance is a rank-one stiffness on the modal coordinates. Those
// axes are diagonalized once. Each normal mode then steps with
// stepConstantForceOscillator. A lossless spring does not add decay:
// σ stays the membrane's ω_bare/(2Q). Degenerate heads keep the
// direction orthogonal to the volume coupling at the bare frequency.
//
// The sample is the batter head's outward volume acceleration, delayed
// by r/c:
//   p(t) = ρ a_out(t − r/c) / (4 π r).
// The resonant head is driven by the cavity and is not struck. Snare
// wires are not in this step. A bass-drum port is not invented.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

#include "../math/constants.h"
#include "../physics/impedance.h"
#include "../physics/medium.h"
#include "../physics/modes.h"
#include "../physics/oscillator.h"
#include "../render/voice.h"
#include "drum.h"
#include "mallet_voice.h"

namespace mmathen {

class CavityDrumVoice final : public IVoice {
 public:
  // `resonant` may be null: the far end is then a rigid wall. Both heads
  // must use the shell radius. Depth, air pressure, temperature, and
  // listening distance are arguments. maxNodalDiameters and maxRadial
  // match MembraneMalletVoice.
  [[nodiscard]] static Result<CavityDrumVoice, QuantityError> create(
      const MembraneHead& batter, const MembraneHead* resonant,
      const MembraneShell& shell, const MalletStrike& strike, int maxNodalDiameters,
      int maxRadial, SampleRateHz rate, double airPressurePa,
      double temperatureKelvin, double distanceMeters) {
    using R = Result<CavityDrumVoice, QuantityError>;
    const double fs = rate.value();
    const double depth = shell.shellDepth.value();
    const double radius = shell.headRadius.value();
    if (!std::isfinite(fs) || !(fs > 0.0) || !mallet_detail::strikeSane(strike) ||
        maxNodalDiameters < 0 || maxRadial < 1 || !std::isfinite(depth) ||
        !(depth > 0.0) || !std::isfinite(radius) || !(radius > 0.0) ||
        !std::isfinite(airPressurePa) || !(airPressurePa > 0.0) ||
        !std::isfinite(distanceMeters) || !(distanceMeters > 0.0) ||
        batter.radius.value() != radius)
      return R::failure(QuantityError::InvalidArgument);
    if (resonant != nullptr && resonant->radius.value() != radius)
      return R::failure(QuantityError::InvalidArgument);
    std::vector<Axis> axes;
    auto built = appendHead(axes, batter, strike, maxNodalDiameters, maxRadial, true);
    if (!built.ok()) return R::failure(built.error());
    if (resonant != nullptr) {
      auto second =
          appendHead(axes, *resonant, strike, maxNodalDiameters, maxRadial, false);
      if (!second.ok()) return R::failure(second.error());
    }
    if (axes.empty()) return R::failure(QuantityError::InvalidArgument);
    const double volume = constants::pi * radius * radius * depth;
    auto air = makeAir(TemperatureKelvin{temperatureKelvin});
    if (!air.ok()) return R::failure(air.error());
    auto speed = speedOfSound(air.value());
    if (!speed.ok()) return R::failure(speed.error());
    auto rho = airDensity(PressurePascals{airPressurePa},
                          TemperatureKelvin{temperatureKelvin});
    if (!rho.ok()) return R::failure(rho.error());
    const double kappa = rho.value().value() * speed.value().value() *
                         speed.value().value() / volume;
    if (!std::isfinite(kappa) || !(kappa > 0.0))
      return R::failure(QuantityError::RangeError);
    auto normals = couple(axes, kappa);
    if (!normals.ok()) return R::failure(normals.error());
    const double travel = distanceMeters / speed.value().value();
    if (!std::isfinite(travel) || travel < 0.0)
      return R::failure(QuantityError::RangeError);
    const double samples = fs * travel;
    if (samples > static_cast<double>(std::numeric_limits<int>::max() / 4))
      return R::failure(QuantityError::RangeError);
    int delay = static_cast<int>(std::llround(samples));
    if (delay < 1) delay = 1;
    const double scale =
        rho.value().value() / (4.0 * constants::pi * distanceMeters);
    if (!std::isfinite(scale) || !(scale > 0.0))
      return R::failure(QuantityError::RangeError);
    double lowest = normals.value().front().frequencyHz;
    for (const Normal& mode : normals.value())
      lowest = std::min(lowest, mode.frequencyHz);
    return R::success(CavityDrumVoice{rate, strike, scale, delay, lowest,
                                      std::move(axes), std::move(normals).value()});
  }

  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }

  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(
      double* out, FrameCount n) override {
    using R = Result<FrameCount, QuantityError>;
    const std::uint64_t count = static_cast<std::uint64_t>(n.value());
    if (count == 0) return R::success(n);
    if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
    const double dt = 1.0 / rate_.value();
    const std::size_t nAxis = axes_.size();
    const std::size_t nMode = modes_.size();
    for (std::uint64_t frame = 0; frame < count; ++frame) {
      std::vector<double> q(nAxis, 0.0);
      std::vector<double> vq(nAxis, 0.0);
      for (std::size_t i = 0; i < nAxis; ++i) {
        const double root = std::sqrt(axes_[i].mass);
        for (std::size_t k = 0; k < nMode; ++k) {
          q[i] += modes_[k].y[i] * eta_[k].displacement / root;
          vq[i] += modes_[k].y[i] * eta_[k].velocity / root;
        }
      }
      double atStrike = 0.0;
      double rateAtStrike = 0.0;
      for (std::size_t i = 0; i < nAxis; ++i) {
        if (!axes_[i].batter) continue;
        atStrike += q[i] * axes_[i].phiStrike;
        rateAtStrike += vq[i] * axes_[i].phiStrike;
      }
      auto force = mallet_detail::feltForce(malletY_ - atStrike,
                                            malletV_ - rateAtStrike, strike_);
      if (!force.ok()) return R::failure(force.error());
      std::vector<double> specific(nMode, 0.0);
      std::vector<double> acceleration(nMode, 0.0);
      for (std::size_t k = 0; k < nMode; ++k) {
        double drive = 0.0;
        for (std::size_t i = 0; i < nAxis; ++i) {
          const double root = std::sqrt(axes_[i].mass);
          drive += modes_[k].y[i] * force.value() * axes_[i].phiStrike / root;
        }
        specific[k] = drive;
        const double omega = modes_[k].omega;
        acceleration[k] = drive - 2.0 * modes_[k].sigma * eta_[k].velocity -
                          omega * omega * eta_[k].displacement;
        auto stepped = stepConstantForceOscillator(
            eta_[k], omega, modes_[k].sigma, drive, dt);
        if (!stepped.ok()) return R::failure(stepped.error());
        eta_[k] = stepped.value();
      }
      double outward = 0.0;
      for (std::size_t i = 0; i < nAxis; ++i) {
        if (!axes_[i].batter || axes_[i].volume == 0.0) continue;
        double qdd = 0.0;
        const double root = std::sqrt(axes_[i].mass);
        for (std::size_t k = 0; k < nMode; ++k)
          qdd += modes_[k].y[i] * acceleration[k] / root;
        outward += -axes_[i].volume * qdd;
      }
      if (!std::isfinite(outward)) return R::failure(QuantityError::RangeError);
      malletV_ += (-force.value() / strike_.massKg) * dt;
      malletY_ += malletV_ * dt;
      if (!std::isfinite(malletY_) || !std::isfinite(malletV_))
        return R::failure(QuantityError::RangeError);
      const double delayed = radiation_[radiationPos_];
      radiation_[radiationPos_] = outward;
      radiationPos_ = (radiationPos_ + 1) % radiation_.size();
      out[frame] = scale_ * delayed;
    }
    return R::success(n);
  }

  void reset() override {
    for (OscillatorState& state : eta_) state = {};
    malletY_ = 0.0;
    malletV_ = strike_.speedMetersPerSecond;
    std::fill(radiation_.begin(), radiation_.end(), 0.0);
    radiationPos_ = 0;
  }

  [[nodiscard]] double fundamentalHz() const noexcept { return fundamentalHz_; }
  [[nodiscard]] std::size_t normalCount() const noexcept { return modes_.size(); }
  [[nodiscard]] double normalHz(std::size_t index) const noexcept {
    return index < modes_.size() ? modes_[index].frequencyHz : 0.0;
  }
  [[nodiscard]] double normalDecay(std::size_t index) const noexcept {
    return index < modes_.size() ? modes_[index].sigma : 0.0;
  }

 private:
  struct Axis {
    double omegaBare = 0.0;
    double sigma = 0.0;
    double mass = 0.0;
    double volume = 0.0;
    double phiStrike = 0.0;
    bool batter = true;
  };

  struct Normal {
    double omega = 0.0;
    double sigma = 0.0;
    double frequencyHz = 0.0;
    std::vector<double> y;
  };

  CavityDrumVoice(SampleRateHz rate, MalletStrike strike, double scale, int delay,
                  double fundamentalHz, std::vector<Axis> axes,
                  std::vector<Normal> modes)
      : rate_(rate),
        strike_(strike),
        scale_(scale),
        fundamentalHz_(fundamentalHz),
        axes_(std::move(axes)),
        modes_(std::move(modes)),
        eta_(modes_.size()),
        radiation_(static_cast<std::size_t>(delay), 0.0),
        malletV_(strike.speedMetersPerSecond) {}

  [[nodiscard]] static Result<double, QuantityError> appendHead(
      std::vector<Axis>& axes, const MembraneHead& head, const MalletStrike& strike,
      int maxNodalDiameters, int maxRadial, bool batter) {
    using R = Result<double, QuantityError>;
    const double radius = head.radius.value();
    const double density = head.arealDensity.value();
    for (int m = 0; m <= maxNodalDiameters; ++m) {
      for (int n = 1; n <= maxRadial; ++n) {
        auto alpha = besselJZero(m, n);
        if (!alpha.ok()) return R::failure(alpha.error());
        auto hertz = circularMembraneMode(head.radius, head.tension, head.arealDensity,
                                          m, n);
        if (!hertz.ok()) return R::failure(hertz.error());
        const double norm = mallet_detail::membraneModeNorm(m, radius, alpha.value());
        if (!(norm > 0.0) || !std::isfinite(norm))
          return R::failure(QuantityError::RangeError);
        const double mass = density * norm;
        const double omega = constants::twoPi * hertz.value().value();
        const double sigma = omega / (2.0 * strike.qualityFactor);
        if (!(mass > 0.0) || !(sigma < omega))
          return R::failure(QuantityError::RangeError);
        double volume = 0.0;
        if (m == 0) {
          const double radial = std::cyl_bessel_j(1.0, alpha.value());
          volume = constants::twoPi * radius * radius * radial / alpha.value();
        }
        const double strikeR = strike.radialFraction * radius;
        const double phi =
            batter ? std::cyl_bessel_j(static_cast<double>(m),
                                      alpha.value() * strikeR / radius)
                   : 0.0;
        if (!std::isfinite(phi) || !std::isfinite(volume))
          return R::failure(QuantityError::RangeError);
        axes.push_back(Axis{omega, sigma, mass, volume, phi, batter});
      }
    }
    return R::success(0.0);
  }

  [[nodiscard]] static Result<std::vector<Normal>, QuantityError> couple(
      const std::vector<Axis>& axes, double kappa) {
    using R = Result<std::vector<Normal>, QuantityError>;
    const std::size_t n = axes.size();
    std::vector<Normal> modes;
    std::vector<std::size_t> live;
    for (std::size_t i = 0; i < n; ++i) {
      if (axes[i].volume == 0.0) {
        Normal mode;
        mode.omega = axes[i].omegaBare;
        mode.sigma = axes[i].sigma;
        mode.y.assign(n, 0.0);
        mode.y[i] = 1.0;
        mode.frequencyHz = dampedHz(mode.omega, mode.sigma);
        modes.push_back(std::move(mode));
      } else {
        live.push_back(i);
      }
    }
    if (live.empty()) return R::success(std::move(modes));
    std::sort(live.begin(), live.end(), [&](std::size_t a, std::size_t b) {
      return axes[a].omegaBare < axes[b].omegaBare;
    });
    struct Group {
      double omega2 = 0.0;
      double u2 = 0.0;
      std::vector<std::size_t> members;
      std::vector<double> u;
    };
    std::vector<Group> groups;
    for (std::size_t index : live) {
      const double omega2 = axes[index].omegaBare * axes[index].omegaBare;
      const double u = axes[index].volume / std::sqrt(axes[index].mass);
      if (!groups.empty() &&
          std::fabs(omega2 - groups.back().omega2) <= 1.0e-8 * omega2) {
        groups.back().members.push_back(index);
        groups.back().u.push_back(u);
        groups.back().u2 += u * u;
      } else {
        groups.push_back(Group{omega2, u * u, {index}, {u}});
      }
    }
    for (const Group& group : groups) {
      if (group.members.size() < 2) continue;
      const std::size_t g = group.members.size();
      std::vector<std::vector<double>> kept;
      for (std::size_t i = 0; i < g; ++i) {
        std::vector<double> v(g, 0.0);
        v[i] = 1.0;
        double dotU = 0.0;
        for (std::size_t j = 0; j < g; ++j) dotU += v[j] * group.u[j];
        for (std::size_t j = 0; j < g; ++j)
          v[j] -= dotU / group.u2 * group.u[j];
        for (const std::vector<double>& prev : kept) {
          double dot = 0.0;
          for (std::size_t j = 0; j < g; ++j) dot += v[j] * prev[j];
          for (std::size_t j = 0; j < g; ++j) v[j] -= dot * prev[j];
        }
        double norm = 0.0;
        for (double value : v) norm += value * value;
        norm = std::sqrt(norm);
        if (!(norm > 1.0e-10)) continue;
        for (double& value : v) value /= norm;
        kept.push_back(v);
        Normal mode;
        mode.omega = std::sqrt(group.omega2);
        mode.y.assign(n, 0.0);
        double sigma = 0.0;
        for (std::size_t j = 0; j < g; ++j) {
          mode.y[group.members[j]] = v[j];
          sigma += v[j] * v[j] * axes[group.members[j]].sigma;
        }
        mode.sigma = sigma;
        if (!(mode.sigma < mode.omega)) return R::failure(QuantityError::DomainError);
        mode.frequencyHz = dampedHz(mode.omega, mode.sigma);
        modes.push_back(std::move(mode));
      }
    }
    std::vector<double> roots;
    auto secular = [&](double lambda) {
      double sum = 0.0;
      for (const Group& group : groups) {
        const double denom = group.omega2 - lambda;
        sum += group.u2 / denom;
      }
      return 1.0 + kappa * sum;
    };
    for (std::size_t g = 0; g + 1 < groups.size(); ++g) {
      double lo = groups[g].omega2;
      double hi = groups[g + 1].omega2;
      const double span = hi - lo;
      lo += span * 1.0e-9;
      hi -= span * 1.0e-9;
      if (!(secular(lo) < 0.0) || !(secular(hi) > 0.0))
        return R::failure(QuantityError::RangeError);
      for (int iter = 0; iter < 80; ++iter) {
        const double mid = 0.5 * (lo + hi);
        if (secular(mid) > 0.0) hi = mid;
        else lo = mid;
      }
      roots.push_back(0.5 * (lo + hi));
    }
    double lo = groups.back().omega2 * (1.0 + 1.0e-8);
    double hi = lo + kappa * groups.back().u2 + groups.back().omega2 + 1.0;
    for (int grow = 0; grow < 40 && !(secular(hi) > 0.0); ++grow) hi *= 2.0;
    if (!(secular(lo) < 0.0) || !(secular(hi) > 0.0))
      return R::failure(QuantityError::RangeError);
    for (int iter = 0; iter < 80; ++iter) {
      const double mid = 0.5 * (lo + hi);
      if (secular(mid) > 0.0) hi = mid;
      else lo = mid;
    }
    roots.push_back(0.5 * (lo + hi));
    for (double lambda : roots) {
      Normal mode;
      mode.omega = std::sqrt(lambda);
      mode.y.assign(n, 0.0);
      double norm = 0.0;
      double sigma = 0.0;
      for (const Group& group : groups) {
        const double denom = group.omega2 - lambda;
        for (std::size_t j = 0; j < group.members.size(); ++j) {
          const double y = group.u[j] / denom;
          mode.y[group.members[j]] = y;
          norm += y * y;
          sigma += y * y * axes[group.members[j]].sigma;
        }
      }
      if (!(norm > 0.0)) return R::failure(QuantityError::RangeError);
      norm = std::sqrt(norm);
      for (double& value : mode.y) value /= norm;
      mode.sigma = sigma / (norm * norm);
      if (!(mode.sigma < mode.omega) || !std::isfinite(mode.omega))
        return R::failure(QuantityError::DomainError);
      mode.frequencyHz = dampedHz(mode.omega, mode.sigma);
      modes.push_back(std::move(mode));
    }
    std::sort(modes.begin(), modes.end(), [](const Normal& a, const Normal& b) {
      return a.frequencyHz < b.frequencyHz;
    });
    return R::success(std::move(modes));
  }

  [[nodiscard]] static double dampedHz(double omega, double sigma) {
    return std::sqrt(omega * omega - sigma * sigma) / constants::twoPi;
  }

  SampleRateHz rate_;
  MalletStrike strike_;
  double scale_ = 0.0;
  double fundamentalHz_ = 0.0;
  std::vector<Axis> axes_;
  std::vector<Normal> modes_;
  std::vector<OscillatorState> eta_;
  std::vector<double> radiation_;
  std::size_t radiationPos_ = 0;
  double malletY_ = 0.0;
  double malletV_ = 0.0;
};

}  // namespace mmathen
