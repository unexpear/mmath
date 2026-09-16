#pragma once

// M4 — meter as subdivision math, not just numerator/denominator.
//
// Convention: TempoBPM counts abstract Beat units, and one Beat equals one
// quarter note. A meter's beat unit converts: beat length in Beats = 4/unit
// (4/4 -> 1 Beat per beat; 6/8 -> 0.5). Measure/beat arguments below are
// 1-based (musical counting); out-of-range arguments are InvalidArgument.
// Additive meters (3+2+2) are group counts in the same unit system.

#include <string>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../units/conversions.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"

namespace mmathen {

class Meter {
 public:
  [[nodiscard]] static Result<Meter, QuantityError> create(int beatsPerMeasure,
                                                           int beatUnit) {
    using R = Result<Meter, QuantityError>;
    if (beatsPerMeasure <= 0 || beatUnit <= 0)
      return R::failure(QuantityError::InvalidArgument);
    return R::success(Meter{beatsPerMeasure, beatUnit});
  }

  // Beats per measure, e.g. 4/4 -> 4.0, 7/8 -> 3.5.
  [[nodiscard]] double measureBeats() const noexcept {
    return beats_ * 4.0 / unit_;
  }
  [[nodiscard]] double beatBeats() const noexcept { return 4.0 / unit_; }

  [[nodiscard]] Result<DurationSeconds, QuantityError> measureDuration(
      TempoBPM bpm) const {
    return beatDuration(Beat{measureBeats()}, bpm);
  }

  // (measure, beat) in 1-based musical counting -> absolute beats (0-based).
  [[nodiscard]] Result<Beat, QuantityError> measureBeatToBeats(int measure,
                                                              int beat) const {
    using R = Result<Beat, QuantityError>;
    if (measure < 1 || beat < 1 || beat > beats_)
      return R::failure(QuantityError::InvalidArgument);
    return R::success(Beat{(measure - 1) * measureBeats() +
                           (beat - 1) * beatBeats()});
  }

  [[nodiscard]] int beatsPerMeasure() const noexcept { return beats_; }
  [[nodiscard]] int beatUnit() const noexcept { return unit_; }

 private:
  Meter(int beats, int unit) : beats_(beats), unit_(unit) {}
  int beats_;
  int unit_;
};

class AdditiveMeter {
 public:
  // Groups in units, e.g. {3,2,2} eighths with unit 8.
  [[nodiscard]] static Result<AdditiveMeter, QuantityError> create(
      std::vector<int> groups, int unit) {
    using R = Result<AdditiveMeter, QuantityError>;
    if (groups.empty() || unit <= 0)
      return R::failure(QuantityError::InvalidArgument);
    for (int g : groups) {
      if (g <= 0) return R::failure(QuantityError::InvalidArgument);
    }
    return R::success(AdditiveMeter{std::move(groups), unit});
  }

  [[nodiscard]] double totalBeats() const noexcept {
    double total = 0.0;
    for (int g : groups_) total += g;
    return total * 4.0 / unit_;
  }

  [[nodiscard]] const std::vector<int>& groups() const noexcept {
    return groups_;
  }

 private:
  AdditiveMeter(std::vector<int> groups, int unit)
      : groups_(std::move(groups)), unit_(unit) {}
  std::vector<int> groups_;
  int unit_;
};

}  // namespace mmathen
