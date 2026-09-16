#pragma once

// M2 — musical structures as DERIVED objects.
//
// A chord is a root plus frequency relationships (NOT a foundation):
//   C major (just)  = {1, 5/4, 3/2}
//   C major (12-TET) = {1, 2^(4/12), 2^(7/12)}
// A scale is a tuning plus root degree plus step offsets. Notes are events
// (pitch + absolute start/duration + dynamics + open articulation string),
// never sounds: C4 on piano vs violin share the symbol, not the physics.
// Melody/Rhythm/Progression are sequences with formula provenance.
// Durations stay signed (render acceptance is a separate rule).

#include <string>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"
#include "pitch.h"
#include "tuning.h"

namespace mmathen {

struct NoteEvent {
  Pitch pitch;
  TimePointSeconds start{0.0};
  DurationSeconds duration{0.0};
  AmplitudeLinear dynamicLevel{0.0};
  std::string articulation;  // open vocabulary in M2 ("", "staccato", ...)

  bool operator==(const NoteEvent&) const = default;
};

// Ratios-only collection. Voicing and inversion are transforms, not state.
class Chord {
 public:
  [[nodiscard]] static Result<Chord, QuantityError> create(
      FrequencyHz root, std::vector<FrequencyRatio> ratios,
      std::string formula = "chord") {
    using R = Result<Chord, QuantityError>;
    if (!std::isfinite(root.value()) || !(root.value() > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    if (ratios.empty())
      return R::failure(QuantityError::InvalidArgument);
    for (const auto& r : ratios) {
      if (!std::isfinite(r.value()) || !(r.value() > 0.0))
        return R::failure(QuantityError::InvalidArgument);
    }
    return R::success(Chord{root, std::move(ratios), std::move(formula)});
  }

  // Sounding frequencies: root * ratio each (RangeError on overflow).
  [[nodiscard]] Result<std::vector<FrequencyHz>, QuantityError> frequencies()
      const {
    using R = Result<std::vector<FrequencyHz>, QuantityError>;
    std::vector<FrequencyHz> out;
    out.reserve(ratios_.size());
    for (const auto& r : ratios_) {
      const double f = root_.value() * r.value();
      if (!std::isfinite(f)) return R::failure(QuantityError::RangeError);
      out.push_back(FrequencyHz{f});
    }
    return R::success(std::move(out));
  }

  [[nodiscard]] FrequencyHz root() const noexcept { return root_; }
  [[nodiscard]] const std::vector<FrequencyRatio>& ratios() const noexcept {
    return ratios_;
  }
  [[nodiscard]] const std::string& formula() const noexcept { return formula_; }

  bool operator==(const Chord&) const = default;

 private:
  Chord(FrequencyHz root, std::vector<FrequencyRatio> ratios,
        std::string formula)
      : root_(root), ratios_(std::move(ratios)), formula_(std::move(formula)) {}
  FrequencyHz root_;
  std::vector<FrequencyRatio> ratios_;
  std::string formula_;
};

// Scale: a tuning plus the tuning-degree that sounds the root plus step
// offsets. freq(i) = tuning(rootDegree + steps[i]). The tuning is stored
// by value (concrete type, zero-cost); no runtime polymorphism.
template <TuningSystem T>
class Scale {
 public:
  [[nodiscard]] static Result<Scale, QuantityError> create(
      T tuning, int rootDegree, std::vector<int> steps,
      std::string formula = "scale") {
    using R = Result<Scale, QuantityError>;
    if (steps.empty())
      return R::failure(QuantityError::InvalidArgument);
    return R::success(
        Scale{std::move(tuning), rootDegree, std::move(steps),
              std::move(formula)});
  }

  [[nodiscard]] Result<FrequencyHz, QuantityError> degreeFrequency(
      std::size_t index) const {
    using R = Result<FrequencyHz, QuantityError>;
    if (index >= steps_.size())
      return R::failure(QuantityError::InvalidArgument);
    // Promoted before adding (audit F1); the tuning interface takes int,
    // so out-of-range sums are defined errors, not overflow.
    const long long degree =
        static_cast<long long>(rootDegree_) + steps_[index];
    if (degree < -2147483648LL || degree > 2147483647LL)
      return R::failure(QuantityError::InvalidArgument);
    return tuning_.degreeToFrequency(static_cast<int>(degree));
  }

  [[nodiscard]] std::size_t size() const noexcept { return steps_.size(); }
  [[nodiscard]] const std::string& formula() const noexcept { return formula_; }
  [[nodiscard]] const T& tuning() const noexcept { return tuning_; }

 private:
  Scale(T tuning, int rootDegree, std::vector<int> steps, std::string formula)
      : tuning_(std::move(tuning)),
        rootDegree_(rootDegree),
        steps_(std::move(steps)),
        formula_(std::move(formula)) {}
  T tuning_;
  int rootDegree_;
  std::vector<int> steps_;
  std::string formula_;
};

struct Melody {
  std::vector<NoteEvent> notes;
  std::string formula = "melody";

  bool operator==(const Melody&) const = default;
};

struct Rhythm {
  TimePointSeconds start{0.0};
  std::vector<DurationSeconds> durations;
  std::string formula = "rhythm";

  // Absolute onsets derived from cumulative durations.
  [[nodiscard]] std::vector<TimePointSeconds> onsets() const {
    std::vector<TimePointSeconds> out;
    out.reserve(durations.size());
    TimePointSeconds t = start;
    for (const auto& d : durations) {
      out.push_back(t);
      t = TimePointSeconds{t.value() + d.value()};
    }
    return out;
  }

  bool operator==(const Rhythm&) const = default;
};

struct Progression {
  std::vector<Chord> chords;
  std::string formula = "progression";

  bool operator==(const Progression&) const = default;
};

}  // namespace mmathen
