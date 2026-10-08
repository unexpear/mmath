#pragma once

// Shared bore network: tone-hole lengths, a lattice, and extra tubing.
//
// The lengths are the published low-frequency T-section, not a stored
// fingering chart. Keefe, JASA 72 (1982) and JASA 88 (1990), as transcribed
// by Scavone (tmmTonehole, 'Keefe1990'): series length
//   ta = 0.47 b δ^4 / (coth|tanh(1.84 tgh/b) + 0.62 δ^2 + 0.64 δ),
// closed on coth and open on tanh, with
//   tgh = t + b δ (1 + 0.172 δ^2) / 8.
// Lefebvre and Scavone, JASA 131 (2012), eqs. (8), (31)–(34): matching
// volume tm = b δ (1 + 0.207 δ^3) / 8, inner length the degree-5
// polynomial in δ, and the signed series lengths in tanh(t/b). Dalmont,
// Nederveen, and Dubos: ti = b (0.82 − 1.4 δ^2 + 0.75 δ^2.7), open
// ta = −0.28 b δ^2, closed ta from the coth chimney of Dubos et al. 1999.
// δ = b/a. A negative series length shortens the main bore. Hole
// radiation resistance (kb)^2/4 is not a constant and is not stepped;
// the bore's own open end remains the radiating load. Viscous wall loss
// and evanescent coupling of holes closer than about two diameters are
// not in this lattice.
//
// The junction is the lumped limit. A closed hole is the chimney
// compliance C = S_h h / (ρ c^2). An open hole is the inertance
// M = ρ h / S_h. Equal characteristic impedances and C = 0 pass the
// waves through. M → 0 is a pressure release that cuts the bore. A
// small open hole (a register key) is a large inertance. Radii, chimney
// height, and position are arguments.
//
// Brass valves and the trombone F attachment are the same extra length
// on the cone: the caller sums the engaged slides. Zero is the cone
// without that tubing. No piston chart is stored.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include "../math/constants.h"
#include "../physics/radiation.h"
#include "../render/voice.h"
#include "bore_valve.h"
#include "clarinet.h"
#include "flute.h"
#include "reed.h"
#include "saxophone.h"
#include "trombone.h"
#include "trumpet.h"

namespace mmathen {

enum class ToneHoleModel { Keefe1990, Lefebvre2012, Dalmont2002 };

// seriesMeters is signed: negative shortens the main bore (Za = −j ω ρ ta
// when ta > 0 in Keefe's notation, so this field stores −ta).
// closedHeightMeters is the compliance length. openHeightMeters is the
// inertance length. frequencyHz == 0 is the k → 0 limit. A positive
// frequency applies Lefebvre's G(δ, ka) to the inner length and Keefe's
// open-shunt formula; it does not add a wall loss.
struct ToneHoleBranch {
  double seriesMeters = 0.0;
  double closedHeightMeters = 0.0;
  double openHeightMeters = 0.0;
};

[[nodiscard]] inline Result<double, QuantityError> sumEngagedLengths(
    std::span<const double> lengths, std::span<const bool> engaged) {
  using R = Result<double, QuantityError>;
  if (lengths.size() != engaged.size()) return R::failure(QuantityError::InvalidArgument);
  double sum = 0.0;
  for (std::size_t i = 0; i < lengths.size(); ++i) {
    if (!std::isfinite(lengths[i]) || lengths[i] < 0.0)
      return R::failure(QuantityError::InvalidArgument);
    if (engaged[i]) sum += lengths[i];
  }
  if (!std::isfinite(sum)) return R::failure(QuantityError::RangeError);
  return R::success(sum);
}

[[nodiscard]] inline Result<ToneHoleBranch, QuantityError> toneHoleBranch(
    ToneHoleModel model, double boreRadius, double holeRadius, double chimneyHeight,
    double frequencyHz, double speed) {
  using R = Result<ToneHoleBranch, QuantityError>;
  if (!std::isfinite(boreRadius) || !(boreRadius > 0.0) || !std::isfinite(holeRadius) ||
      !(holeRadius > 0.0) || !std::isfinite(chimneyHeight) || chimneyHeight < 0.0 ||
      !std::isfinite(frequencyHz) || frequencyHz < 0.0)
    return R::failure(QuantityError::InvalidArgument);
  if (frequencyHz > 0.0 && (!std::isfinite(speed) || !(speed > 0.0)))
    return R::failure(QuantityError::InvalidArgument);
  const double delta = holeRadius / boreRadius;
  if (!std::isfinite(delta) || !(delta > 0.0))
    return R::failure(QuantityError::RangeError);
  const double delta2 = delta * delta;
  auto hyperbolic = [](double x) {
    const double t = std::tanh(x);
    return std::fabs(t) > 1.0e-12 ? t : 0.0;
  };
  auto coth = [&](double x) {
    const double t = hyperbolic(x);
    return t == 0.0 ? std::numeric_limits<double>::infinity() : 1.0 / t;
  };

  ToneHoleBranch branch;
  if (model == ToneHoleModel::Keefe1990) {
    const double tgh =
        chimneyHeight + holeRadius * delta * (1.0 + 0.172 * delta2) / 8.0;
    const double numer = 0.47 * holeRadius * delta2 * delta2;
    const double wall = 0.62 * delta2 + 0.64 * delta;
    const double closedDen = coth(1.84 * tgh / holeRadius) + wall;
    if (!(closedDen > 0.0) || !std::isfinite(closedDen))
      return R::failure(QuantityError::RangeError);
    // toneHoleSeries selects the open tanh denominator. This field is
    // the closed coth value.
    branch.seriesMeters = -(numer / closedDen);
    branch.closedHeightMeters = tgh;
    if (frequencyHz == 0.0) {
      branch.openHeightMeters = tgh + holeRadius * (1.4 - 0.58 * delta2);
    } else {
      const double k = constants::twoPi * frequencyHz / speed;
      const double tang = std::tan(tgh * k);
      const double denom = 1.0 - 0.61 * holeRadius * k * tang;
      if (!(std::fabs(denom) > 1.0e-8) || !(k > 0.0))
        return R::failure(QuantityError::RangeError);
      branch.openHeightMeters = (tang / k + holeRadius * (1.4 - 0.58 * delta2)) / denom;
    }
  } else if (model == ToneHoleModel::Lefebvre2012) {
    const double tm = holeRadius * delta * (1.0 + 0.207 * delta * delta2) / 8.0;
    double ti = holeRadius * (0.822 - 0.095 * delta - 1.566 * delta2 + 2.138 * delta2 * delta -
                              1.640 * delta2 * delta2 + 0.502 * delta2 * delta2 * delta);
    if (frequencyHz > 0.0) {
      const double ka = constants::twoPi * frequencyHz / speed * boreRadius;
      const double h = 1.0 - 4.56 * delta + 6.55 * delta2;
      const double ka2 = ka * ka;
      const double eye = 0.17 * ka + 0.92 * ka2 + 0.16 * ka2 * ka - 0.29 * ka2 * ka2;
      ti *= 1.0 + h * eye;
    }
    const double ratio = chimneyHeight / holeRadius;
    branch.closedHeightMeters = chimneyHeight + tm;
    branch.openHeightMeters = chimneyHeight + tm + ti + 0.61 * holeRadius;
    branch.seriesMeters =
        holeRadius * delta2 * (-0.12 - 0.17 * hyperbolic(2.4 * ratio));
    (void)ratio;
  } else {
    const double tm = holeRadius * delta * (1.0 + 0.207 * delta * delta2) / 8.0;
    const double ti =
        holeRadius * (0.82 - 1.4 * delta2 + 0.75 * std::pow(delta, 2.7));
    branch.closedHeightMeters = chimneyHeight + tm;
    branch.openHeightMeters = chimneyHeight + tm + ti + 0.61 * holeRadius;
    const double ratio = chimneyHeight / holeRadius;
    const double den =
        1.78 * coth(1.84 * ratio) + 0.94 + 0.54 * delta + 0.285 * delta2;
    if (!(den > 0.0) || !std::isfinite(den))
      return R::failure(QuantityError::RangeError);
    branch.seriesMeters = -(holeRadius * delta2) / den;
  }
  if (!std::isfinite(branch.seriesMeters) || !std::isfinite(branch.closedHeightMeters) ||
      !std::isfinite(branch.openHeightMeters) || !(branch.closedHeightMeters > 0.0) ||
      !(branch.openHeightMeters > 0.0))
    return R::failure(QuantityError::RangeError);
  return R::success(branch);
}

// The signed series length for the hole's open or closed state.
[[nodiscard]] inline Result<double, QuantityError> toneHoleSeries(
    ToneHoleModel model, bool open, double boreRadius, double holeRadius,
    double chimneyHeight) {
  using R = Result<double, QuantityError>;
  auto branch = toneHoleBranch(model, boreRadius, holeRadius, chimneyHeight, 0.0, 0.0);
  if (!branch.ok()) return R::failure(branch.error());
  if (model == ToneHoleModel::Keefe1990) {
    const double delta = holeRadius / boreRadius;
    const double delta2 = delta * delta;
    const double tgh = branch.value().closedHeightMeters;
    const double numer = 0.47 * holeRadius * delta2 * delta2;
    const double wall = 0.62 * delta2 + 0.64 * delta;
    const double x = 1.84 * tgh / holeRadius;
    const double t = std::tanh(x);
    const double shape = open ? (std::fabs(t) > 1.0e-12 ? t : 0.0)
                              : (std::fabs(t) > 1.0e-12 ? 1.0 / t
                                                       : std::numeric_limits<double>::infinity());
    const double den = shape + wall;
    if (!(den > 0.0) || !std::isfinite(den)) return R::failure(QuantityError::RangeError);
    return R::success(-(numer / den));
  }
  if (model == ToneHoleModel::Lefebvre2012) {
    const double delta = holeRadius / boreRadius;
    const double ratio = chimneyHeight / holeRadius;
    const double t = std::tanh(open ? 2.7 * ratio : 2.4 * ratio);
    const double coeff = open ? (-0.35 + 0.06 * t) : (-0.12 - 0.17 * t);
    return R::success(holeRadius * delta * delta * coeff);
  }
  if (!open) return R::success(branch.value().seriesMeters);
  const double delta = holeRadius / boreRadius;
  return R::success(-0.28 * holeRadius * delta * delta);
}

// Lumped tone hole or a plain area step (closed, zero compliance).
class ToneHoleJunction {
 public:
  [[nodiscard]] static Result<ToneHoleJunction, QuantityError> create(
      bool open, double impedanceLeft, double impedanceRight, double compliance,
      double inertance) {
    using R = Result<ToneHoleJunction, QuantityError>;
    if (!std::isfinite(impedanceLeft) || !(impedanceLeft > 0.0) ||
        !std::isfinite(impedanceRight) || !(impedanceRight > 0.0) ||
        !std::isfinite(compliance) || compliance < 0.0)
      return R::failure(QuantityError::InvalidArgument);
    if (open && (!std::isfinite(inertance) || !(inertance > 0.0)))
      return R::failure(QuantityError::InvalidArgument);
    ToneHoleJunction hole;
    hole.open_ = open;
    hole.zLeft_ = impedanceLeft;
    hole.zRight_ = impedanceRight;
    hole.compliance_ = compliance;
    hole.inertance_ = inertance;
    return R::success(hole);
  }

  void reset() noexcept {
    pressure_ = 0.0;
    flow_ = 0.0;
  }

  void step(double incomingLeft, double incomingRight, double dt, double& outgoingLeft,
            double& outgoingRight) {
    const double g = 1.0 / zLeft_ + 1.0 / zRight_;
    const double known = 2.0 * incomingLeft / zLeft_ + 2.0 * incomingRight / zRight_;
    double pressure = 0.0;
    if (open_) {
      const double admit = dt / inertance_;
      pressure = (known - flow_) / (g + admit);
      flow_ += admit * pressure;
    } else if (compliance_ > 0.0) {
      const double admit = compliance_ / dt;
      pressure = (known + admit * pressure_) / (g + admit);
      pressure_ = pressure;
    } else {
      pressure = known / g;
    }
    outgoingLeft = pressure - incomingLeft;
    outgoingRight = pressure - incomingRight;
  }

 private:
  bool open_ = false;
  double zLeft_ = 1.0;
  double zRight_ = 1.0;
  double compliance_ = 0.0;
  double inertance_ = 0.0;
  double pressure_ = 0.0;
  double flow_ = 0.0;
};

struct LatticeHole {
  double positionFromMouthMeters = 0.0;
  double holeRadiusMeters = 0.0;
  double chimneyHeightMeters = 0.0;
  bool open = false;
  ToneHoleModel model = ToneHoleModel::Lefebvre2012;
};

// Cylinder or stepped cone, split at the caller's holes. areaSteps > 1
// inserts area samples on a cone; a cylinder ignores them. The saxophone
// uses S(x) = S_throat ((x0 + x) / x0)^2. Lefebvre's cylindrical-hole
// lengths are used on that cone; a taper correction is not added.
class BoreLatticeVoice final : public IVoice {
 public:
  [[nodiscard]] static Result<BoreLatticeVoice, QuantityError> fromClarinet(
      const Clarinet& clarinet, const ValveBlow& blow, std::span<const LatticeHole> holes,
      SampleRateHz rate) {
    return make(ValveLaw::InwardReed, clarinet.reed, clarinet.inertia, Flute{},
                Trumpet{}, clarinet.bore.lengthMeters, 0.0, clarinet.bore.crossSectionM2,
                clarinet.bore.soundSpeedMetersPerSecond, clarinet.bore.airDensityKgPerM3,
                false, holes, 1, blow, rate);
  }

  [[nodiscard]] static Result<BoreLatticeVoice, QuantityError> fromFlute(
      const Flute& flute, const ValveBlow& blow, std::span<const LatticeHole> holes,
      SampleRateHz rate) {
    ReedCharacteristic unused{};
    ReedInertia inertia{};
    return make(ValveLaw::Jet, unused, inertia, flute, Trumpet{}, flute.lengthMeters, 0.0,
                flute.crossSectionM2, flute.soundSpeedMetersPerSecond,
                flute.airDensityKgPerM3, false, holes, 1, blow, rate);
  }

  [[nodiscard]] static Result<BoreLatticeVoice, QuantityError> fromSaxophone(
      const Saxophone& saxophone, const ValveBlow& blow, std::span<const LatticeHole> holes,
      SampleRateHz rate, int areaSteps) {
    Flute unused{};
    return make(ValveLaw::InwardReed, saxophone.reed, saxophone.inertia, unused, Trumpet{},
                saxophone.lengthMeters, saxophone.apexDistanceMeters, saxophone.throatAreaM2,
                saxophone.soundSpeedMetersPerSecond, saxophone.airDensityKgPerM3, true, holes,
                areaSteps, blow, rate);
  }

  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }

  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(double* out,
                                                             FrameCount n) override {
    using R = Result<FrameCount, QuantityError>;
    const std::uint64_t count = static_cast<std::uint64_t>(n.value());
    if (count == 0) return R::success(n);
    if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
    const double dt = 1.0 / rate_.value();
    const std::size_t segments = right_.size();
    for (std::uint64_t i = 0; i < count; ++i) {
      for (std::size_t s = 0; s < segments; ++s) {
        atRight_[s] = right_[s][pos_[s]];
        atLeft_[s] = left_[s][pos_[s]];
      }
      const double farOut =
          radiating_ ? openEnd_.step(atRight_.back()) : reflection_ * atRight_.back();
      if (!std::isfinite(farOut)) return R::failure(QuantityError::RangeError);
      double mouthOut = 0.0;
      double sample = 0.0;
      auto mouth = stepMouth(atLeft_.front(), mouthOut, sample, dt);
      if (!mouth.ok()) return R::failure(mouth.error());
      for (std::size_t j = 0; j < junctions_.size(); ++j) {
        junctions_[j].step(atRight_[j], atLeft_[j + 1], dt, toLeft_[j], toRight_[j]);
        if (!std::isfinite(toLeft_[j]) || !std::isfinite(toRight_[j]))
          return R::failure(QuantityError::RangeError);
      }
      intoRight_[0] = mouthOut;
      intoLeft_[segments - 1] = farOut;
      if (segments > 1) {
        intoLeft_[0] = toLeft_[0];
        intoRight_[segments - 1] = toRight_[segments - 2];
        for (std::size_t s = 1; s + 1 < segments; ++s) {
          intoRight_[s] = toRight_[s - 1];
          intoLeft_[s] = toLeft_[s];
        }
      }
      for (std::size_t s = 0; s < segments; ++s) {
        right_[s][pos_[s]] = intoRight_[s];
        left_[s][pos_[s]] = intoLeft_[s];
        pos_[s] = (pos_[s] + 1) % right_[s].size();
      }
      out[i] = sample;
      ++frame_;
    }
    return R::success(n);
  }

  void reset() override {
    for (auto& rail : right_) std::fill(rail.begin(), rail.end(), 0.0);
    for (auto& rail : left_) std::fill(rail.begin(), rail.end(), 0.0);
    std::fill(pos_.begin(), pos_.end(), 0);
    for (auto& hole : junctions_) hole.reset();
    openEnd_.reset();
    std::fill(jetDelay_.begin(), jetDelay_.end(), initialDisplacement_);
    jetPos_ = 0;
    displacement_ = initialDisplacement_;
    velocity_ = 0.0;
    flueY_ = initialDisplacement_;
    frame_ = 0;
  }

  [[nodiscard]] double fundamentalHz() const noexcept { return fundamentalHz_; }
  [[nodiscard]] double reflectionDecay() const noexcept { return decay_; }
  [[nodiscard]] bool radiating() const noexcept { return radiating_; }
  [[nodiscard]] int holeCount() const noexcept { return holeCount_; }

 private:
  struct ReedPack {
    ReedCharacteristic reed;
    ReedInertia inertia;
  };

  explicit BoreLatticeVoice(SampleRateHz rate) : rate_(rate) {}

  [[nodiscard]] static Result<BoreLatticeVoice, QuantityError> make(
      ValveLaw law, const ReedCharacteristic& reed, const ReedInertia& inertia,
      const Flute& flute, const Trumpet&, double length, double apex, double throatArea,
      double speed, double density, bool conical, std::span<const LatticeHole> holes,
      int areaSteps, const ValveBlow& blow, SampleRateHz rate) {
    using R = Result<BoreLatticeVoice, QuantityError>;
    const double fs = rate.value();
    if (!std::isfinite(blow.pressurePa) || !(blow.pressurePa > 0.0) ||
        !std::isfinite(blow.reflectionMagnitude) || !(blow.reflectionMagnitude > 0.0) ||
        blow.reflectionMagnitude > 1.0 || !std::isfinite(blow.initialDisplacementMeters) ||
        !std::isfinite(fs) || !(fs > 0.0) || !std::isfinite(length) || !(length > 0.0) ||
        !std::isfinite(throatArea) || !(throatArea > 0.0) || !std::isfinite(speed) ||
        !(speed > 0.0) || !std::isfinite(density) || !(density > 0.0) || areaSteps < 1 ||
        !std::isfinite(blow.endCorrectionMeters) || blow.endCorrectionMeters < 0.0)
      return R::failure(QuantityError::InvalidArgument);
    if (conical && (!std::isfinite(apex) || !(apex > 0.0)))
      return R::failure(QuantityError::InvalidArgument);
    for (std::size_t i = 0; i < holes.size(); ++i) {
      if (!std::isfinite(holes[i].positionFromMouthMeters) ||
          !(holes[i].positionFromMouthMeters > 0.0) ||
          !(holes[i].positionFromMouthMeters < length))
        return R::failure(QuantityError::InvalidArgument);
      if (i > 0 &&
          !(holes[i].positionFromMouthMeters > holes[i - 1].positionFromMouthMeters))
        return R::failure(QuantityError::InvalidArgument);
    }

    struct Mark {
      double x;
      int hole = -1;
    };
    std::vector<Mark> marks;
    marks.push_back(Mark{0.0, -1});
    marks.push_back(Mark{length, -1});
    for (std::size_t i = 0; i < holes.size(); ++i)
      marks.push_back(Mark{holes[i].positionFromMouthMeters, static_cast<int>(i)});
    if (conical) {
      for (int step = 1; step < areaSteps; ++step)
        marks.push_back(Mark{length * static_cast<double>(step) / static_cast<double>(areaSteps),
                             -1});
    }
    std::sort(marks.begin(), marks.end(),
              [](const Mark& a, const Mark& b) { return a.x < b.x; });
    std::vector<Mark> splits;
    for (const Mark& mark : marks) {
      if (!splits.empty() && std::fabs(mark.x - splits.back().x) < 1.0e-9) {
        if (mark.hole >= 0) splits.back().hole = mark.hole;
      } else {
        splits.push_back(mark);
      }
    }

    auto areaAt = [&](double x) {
      if (!conical) return throatArea;
      const double scale = (apex + x) / apex;
      return throatArea * scale * scale;
    };
    std::vector<double> series(holes.size(), 0.0);
    for (std::size_t i = 0; i < holes.size(); ++i) {
      const double radius = std::sqrt(areaAt(holes[i].positionFromMouthMeters) / constants::pi);
      auto signedSeries =
          toneHoleSeries(holes[i].model, holes[i].open, radius, holes[i].holeRadiusMeters,
                         holes[i].chimneyHeightMeters);
      if (!signedSeries.ok()) return R::failure(signedSeries.error());
      series[i] = signedSeries.value();
    }

    BoreLatticeVoice voice(rate);
    voice.law_ = law;
    voice.pressure_ = blow.pressurePa;
    voice.blowFrames_ = blow.frames;
    voice.initialDisplacement_ = blow.initialDisplacementMeters;
    voice.displacement_ = blow.initialDisplacementMeters;
    voice.flueY_ = blow.initialDisplacementMeters;
    voice.density_ = density;
    voice.speed_ = speed;
    voice.reflection_ = -blow.reflectionMagnitude;
    voice.holeCount_ = static_cast<int>(holes.size());
    voice.reed_.reed = reed;
    voice.reed_.inertia = inertia;
    voice.closing_ = inertia.closingDisplacementMeters;
    int oneWay = 0;
    for (std::size_t s = 0; s + 1 < splits.size(); ++s) {
      const double left = splits[s].x;
      const double right = splits[s + 1].x;
      double acoustic = right - left;
      if (splits[s].hole >= 0) acoustic += 0.5 * series[static_cast<std::size_t>(splits[s].hole)];
      if (splits[s + 1].hole >= 0)
        acoustic += 0.5 * series[static_cast<std::size_t>(splits[s + 1].hole)];
      if (!(acoustic > 0.0) || !std::isfinite(acoustic))
        return R::failure(QuantityError::RangeError);
      const double samples = fs * acoustic / speed;
      if (!std::isfinite(samples) ||
          samples > static_cast<double>(std::numeric_limits<int>::max() / 8))
        return R::failure(QuantityError::RangeError);
      int delay = static_cast<int>(std::llround(samples));
      if (delay < 1) delay = 1;
      oneWay += delay;
      voice.right_.emplace_back(static_cast<std::size_t>(delay), 0.0);
      voice.left_.emplace_back(static_cast<std::size_t>(delay), 0.0);
      voice.pos_.push_back(0);
      if (s + 1 < splits.size() - 1) {
        const double zLeft = density * speed / areaAt(0.5 * (left + right));
        const double nextLeft = splits[s + 1].x;
        const double nextRight = splits[s + 2].x;
        const double zRight = density * speed / areaAt(0.5 * (nextLeft + nextRight));
        bool open = false;
        double compliance = 0.0;
        double inertance = 0.0;
        if (splits[s + 1].hole >= 0) {
          const LatticeHole& hole = holes[static_cast<std::size_t>(splits[s + 1].hole)];
          const double radius = std::sqrt(areaAt(hole.positionFromMouthMeters) / constants::pi);
          auto branch = toneHoleBranch(hole.model, radius, hole.holeRadiusMeters,
                                       hole.chimneyHeightMeters, 0.0, speed);
          if (!branch.ok()) return R::failure(branch.error());
          const double height = hole.open ? branch.value().openHeightMeters
                                          : branch.value().closedHeightMeters;
          const double holeArea = constants::pi * hole.holeRadiusMeters * hole.holeRadiusMeters;
          open = hole.open;
          if (open)
            inertance = density * height / holeArea;
          else
            compliance = holeArea * height / (density * speed * speed);
        }
        auto junction = ToneHoleJunction::create(open, zLeft, zRight, compliance, inertance);
        if (!junction.ok()) return R::failure(junction.error());
        voice.junctions_.push_back(std::move(junction).value());
      }
    }
    if (voice.right_.empty() || oneWay < 1) return R::failure(QuantityError::RangeError);
    const std::size_t segments = voice.right_.size();
    voice.atRight_.assign(segments, 0.0);
    voice.atLeft_.assign(segments, 0.0);
    voice.intoRight_.assign(segments, 0.0);
    voice.intoLeft_.assign(segments, 0.0);
    voice.toLeft_.assign(voice.junctions_.size(), 0.0);
    voice.toRight_.assign(voice.junctions_.size(), 0.0);

    const bool openOpen = law == ValveLaw::Jet;
    voice.radiating_ = blow.radiusMeters > 0.0;
    if (voice.radiating_) {
      auto filter = OpenEndReflection::create(blow.radiusMeters, blow.endCorrectionMeters, speed,
                                              rate);
      if (!filter.ok()) return R::failure(filter.error());
      voice.openEnd_ = std::move(filter).value();
      auto solved = voice.openEnd_.resonanceHz(oneWay * 2, openOpen);
      if (!solved.ok()) return R::failure(solved.error());
      auto response = voice.openEnd_.response(solved.value());
      if (!response.ok()) return R::failure(response.error());
      const double magnitude = std::hypot(response.value().real, response.value().imag);
      if (!(magnitude > 0.0) || magnitude > 1.0) return R::failure(QuantityError::RangeError);
      voice.fundamentalHz_ = solved.value();
      const double trip = openOpen ? 1.0 / voice.fundamentalHz_ : 1.0 / (2.0 * voice.fundamentalHz_);
      voice.decay_ = -std::log(magnitude) / trip;
    } else {
      const double roundTrip = static_cast<double>(2 * oneWay);
      voice.decay_ = -std::log(blow.reflectionMagnitude) * fs / roundTrip;
      voice.fundamentalHz_ = openOpen ? fs / roundTrip : fs / (2.0 * roundTrip);
    }
    if (!std::isfinite(voice.fundamentalHz_) || !(voice.fundamentalHz_ > 0.0) ||
        !std::isfinite(voice.decay_))
      return R::failure(QuantityError::RangeError);

    if (law == ValveLaw::Jet) {
      auto travel = fluteConvectionTime(flute, blow.pressurePa);
      if (!travel.ok()) return R::failure(travel.error());
      const double samples = fs * travel.value();
      if (!std::isfinite(samples) ||
          samples > static_cast<double>(std::numeric_limits<int>::max() / 4))
        return R::failure(QuantityError::RangeError);
      int convection = static_cast<int>(std::llround(samples));
      if (convection < 1) convection = 1;
      voice.jetArea_ = flute.jetAreaM2;
      voice.jetThickness_ = flute.jetThicknessMeters;
      voice.flueToEdge_ = flute.flueToEdgeMeters;
      voice.area_ = flute.crossSectionM2;
      voice.impedance_ = density * speed / throatArea;
      voice.jetDelay_.assign(static_cast<std::size_t>(convection),
                             blow.initialDisplacementMeters);
      voice.flowOutput_ = true;
    } else {
      voice.impedance_ = density * speed / throatArea;
      voice.area_ = throatArea;
    }
    return R::success(std::move(voice));
  }

  [[nodiscard]] double bernoulli(double opening, double deltaP) const {
    if (!(opening > 0.0) || deltaP == 0.0) return 0.0;
    const double flowSpeed = std::sqrt(2.0 * std::fabs(deltaP) / density_);
    return std::copysign(opening * flowSpeed, deltaP);
  }

  [[nodiscard]] Result<double, QuantityError> solveDelta(double opening, double target) const {
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

  [[nodiscard]] Result<bool, QuantityError> stepMouth(double incoming, double& outgoing,
                                                     double& sample, double dt) {
    using R = Result<bool, QuantityError>;
    if (law_ == ValveLaw::Jet) return stepJet(incoming, outgoing, sample, dt);
    const bool open = frame_ < blowFrames_;
    const double mouth = open ? pressure_ : 0.0;
    if (!open) {
      displacement_ = law_ == ValveLaw::InwardReed ? closing_ : 0.0;
      velocity_ = 0.0;
    }
    double opening = 0.0;
    if (law_ == ValveLaw::InwardReed) {
      auto area = reedAreaAtDisplacement(reed_.reed, reed_.inertia, displacement_);
      if (!area.ok()) return R::failure(area.error());
      opening = area.value();
    }
    const double target = mouth - 2.0 * incoming;
    auto delta = solveDelta(opening, target);
    if (!delta.ok()) return R::failure(delta.error());
    const double pressure = mouth - delta.value();
    outgoing = pressure - incoming;
    sample = pressure;
    if (!std::isfinite(outgoing) || !std::isfinite(sample))
      return R::failure(QuantityError::RangeError);
    if (open && law_ == ValveLaw::InwardReed) {
      auto acceleration =
          reedTipAcceleration(reed_.inertia, displacement_, velocity_, delta.value());
      if (!acceleration.ok()) return R::failure(acceleration.error());
      velocity_ += acceleration.value() * dt;
      displacement_ += velocity_ * dt;
      if (!std::isfinite(velocity_) || !std::isfinite(displacement_))
        return R::failure(QuantityError::RangeError);
    }
    return R::success(true);
  }

  [[nodiscard]] Result<bool, QuantityError> stepJet(double incoming, double& outgoing,
                                                   double& sample, double dt) {
    using R = Result<bool, QuantityError>;
    (void)dt;
    const bool blowing = frame_ < blowFrames_;
    const double mouth = blowing ? pressure_ : 0.0;
    double jetFlow = 0.0;
    if (mouth > 0.0) {
      auto flow = fluteJetFlow(jetFlute(), mouth);
      if (!flow.ok()) return R::failure(flow.error());
      jetFlow = flow.value();
    }
    const double yEdge = jetDelay_.empty() ? flueY_ : jetDelay_[jetPos_];
    auto fraction = fluteJetFraction(jetFlute(), yEdge);
    if (!fraction.ok()) return R::failure(fraction.error());
    outgoing = -incoming + impedance_ * jetFlow * (2.0 * fraction.value() - 1.0);
    const double boreFlow = (outgoing - incoming) / impedance_;
    sample = boreFlow;
    if (!std::isfinite(outgoing) || !std::isfinite(sample))
      return R::failure(QuantityError::RangeError);
    if (blowing && mouth > 0.0 && area_ > 0.0) {
      auto jetSpeed = fluteJetSpeed(jetFlute(), mouth);
      if (!jetSpeed.ok()) return R::failure(jetSpeed.error());
      if (jetSpeed.value() > 0.0) {
        flueY_ = -(boreFlow / area_) * (flueToEdge_ / jetSpeed.value());
        if (!std::isfinite(flueY_)) return R::failure(QuantityError::RangeError);
      }
    } else if (!blowing) {
      flueY_ = 0.0;
    }
    if (!jetDelay_.empty()) {
      jetDelay_[jetPos_] = flueY_;
      jetPos_ = (jetPos_ + 1) % jetDelay_.size();
    }
    return R::success(true);
  }

  [[nodiscard]] Flute jetFlute() const {
    Flute flute;
    flute.lengthMeters = 1.0;
    flute.crossSectionM2 = area_;
    flute.soundSpeedMetersPerSecond = speed_;
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
  double speed_ = 0.0;
  double area_ = 0.0;
  double closing_ = 0.0;
  double jetArea_ = 0.0;
  double jetThickness_ = 0.0;
  double flueToEdge_ = 0.0;
  double fundamentalHz_ = 0.0;
  double decay_ = 0.0;
  int holeCount_ = 0;
  bool radiating_ = false;
  bool flowOutput_ = false;
  OpenEndReflection openEnd_{};
  ReedPack reed_{};
  std::vector<double> jetDelay_;
  std::size_t jetPos_ = 0;
  std::vector<std::vector<double>> right_;
  std::vector<std::vector<double>> left_;
  std::vector<std::size_t> pos_;
  std::vector<ToneHoleJunction> junctions_;
  std::vector<double> atRight_;
  std::vector<double> atLeft_;
  std::vector<double> intoRight_;
  std::vector<double> intoLeft_;
  std::vector<double> toLeft_;
  std::vector<double> toRight_;
  std::uint64_t frame_ = 0;
};

}  // namespace mmathen
