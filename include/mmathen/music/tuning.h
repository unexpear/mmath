#pragma once

// M1 — tuning systems: degree -> frequency maps.
//
// A tuning system is a mapping n -> f with an inspectable formula. The
// interface is a C++20 concept (zero-cost, no vtables): any type providing
// degreeToFrequency(int) -> Result<FrequencyHz, QuantityError> plus a
// formula() string models TuningSystem. Pitch LABELS (A4, C#) are deferred
// to M2; here there are only integer degrees and frequencies.
//
// music/ depends on the shared foundation only (core, math, units) — never
// on signal/ or physics/. Adapters in examples/ wire frequencies to
// oscillators. All mappings are fallible: bad tables are rejected at
// creation, undefined degrees and overflow at evaluation.

#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../units/conversions.h"
#include "../units/music_units.h"
#include "../units/physical_units.h"

namespace mmathen {

template <typename T>
concept TuningSystem = requires(const T& t, int degree) {
  {
    t.degreeToFrequency(degree)
  } -> std::same_as<Result<FrequencyHz, QuantityError>>;
  { t.formula() } -> std::convertible_to<std::string>;
};

// Equal temperament: f(n) = f_ref * 2^((n - n_ref)/N).
template <int Divisions>
class EqualTemperament {
  static_assert(Divisions > 0, "equal temperament needs N > 0 divisions");

 public:
  explicit EqualTemperament(FrequencyHz reference, int referenceDegree = 0)
      : ref_(reference), refDegree_(referenceDegree) {}

  [[nodiscard]] Result<FrequencyHz, QuantityError> degreeToFrequency(
      int degree) const {
    using R = Result<FrequencyHz, QuantityError>;
    const double ref = ref_.value();
    if (!std::isfinite(ref) || !(ref > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    // Promoted before subtracting: int degree arithmetic can overflow for
    // extreme inputs (audit F1); the pow() below reports those as RangeError.
    const double steps =
        static_cast<double>(static_cast<long long>(degree) - refDegree_);
    const double r = std::pow(2.0, steps / static_cast<double>(Divisions));
    if (!std::isfinite(r)) return R::failure(QuantityError::RangeError);
    const double f = ref * r;
    if (!std::isfinite(f)) return R::failure(QuantityError::RangeError);
    return R::success(FrequencyHz{f});
  }

  [[nodiscard]] std::string formula() const {
    return "f_ref * 2^(n/" + std::to_string(Divisions) + ")";
  }
  [[nodiscard]] FrequencyHz reference() const noexcept { return ref_; }
  [[nodiscard]] int referenceDegree() const noexcept { return refDegree_; }

 private:
  FrequencyHz ref_;
  int refDegree_;
};

using TwelveTET = EqualTemperament<12>;
using NineteenTET = EqualTemperament<19>;
using ThirtyOneTET = EqualTemperament<31>;

// Just intonation: one period of ratios (e.g. an octave span ending on 2/1),
// repeating by octaves. Degree n maps to ratios_[n mod size] * 2^octave.
// The table is validated at creation; the class itself is then infallible
// except for overflow (RangeError).
class JustIntonation {
 public:
  [[nodiscard]] static Result<JustIntonation, QuantityError> create(
      FrequencyHz root, std::vector<FrequencyRatio> ratios) {
    using R = Result<JustIntonation, QuantityError>;
    if (!std::isfinite(root.value()) || !(root.value() > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    if (ratios.empty())
      return R::failure(QuantityError::InvalidArgument);
    for (const auto& r : ratios) {
      if (!std::isfinite(r.value()) || !(r.value() > 0.0))
        return R::failure(QuantityError::InvalidArgument);
    }
    // Count BEFORE the move below: argument evaluation order is unspecified,
    // so reading ratios.size() after std::move(ratios) is a latent bug
    // (caught on GCC, silent on MSVC).
    const std::size_t count = ratios.size();
    return R::success(JustIntonation{root, std::move(ratios),
                                     "just[" + std::to_string(count) +
                                         " ratios]"});
  }

  [[nodiscard]] Result<FrequencyHz, QuantityError> degreeToFrequency(
      int degree) const {
    using R = Result<FrequencyHz, QuantityError>;
    const long long size = static_cast<long long>(ratios_.size());
    long long q = static_cast<long long>(degree) / size;
    long long rem = static_cast<long long>(degree) % size;
    if (rem < 0) {
      rem += size;
      --q;
    }
    const double f =
        root_.value() *
        ratios_[static_cast<std::size_t>(rem)].value() *
        std::pow(2.0, static_cast<double>(q));
    if (!std::isfinite(f)) return R::failure(QuantityError::RangeError);
    return R::success(FrequencyHz{f});
  }

  [[nodiscard]] const std::string& formula() const noexcept { return formula_; }

 private:
  JustIntonation(FrequencyHz root, std::vector<FrequencyRatio> ratios,
                 std::string formula)
      : root_(root), ratios_(std::move(ratios)), formula_(std::move(formula)) {}
  FrequencyHz root_;
  std::vector<FrequencyRatio> ratios_;
  std::string formula_;
};

// Cents tuning: one period of cent offsets from the root, repeating by
// octaves (offsets are fifths-style deviations, the period still spans 2/1
// per repetition). Root must be finite and > 0 (checked at creation).
class CentsTuning {
 public:
  [[nodiscard]] static Result<CentsTuning, QuantityError> create(
      FrequencyHz root, std::vector<Cents> offsets) {
    using R = Result<CentsTuning, QuantityError>;
    if (!std::isfinite(root.value()) || !(root.value() > 0.0))
      return R::failure(QuantityError::InvalidArgument);
    if (offsets.empty())
      return R::failure(QuantityError::InvalidArgument);
    for (const auto& c : offsets) {
      if (!std::isfinite(c.value()))
        return R::failure(QuantityError::InvalidArgument);
    }
    const std::size_t count = offsets.size();
    return R::success(CentsTuning{root, std::move(offsets), count});
  }

  [[nodiscard]] Result<FrequencyHz, QuantityError> degreeToFrequency(
      int degree) const {
    using R = Result<FrequencyHz, QuantityError>;
    const long long size = static_cast<long long>(offsets_.size());
    long long q = static_cast<long long>(degree) / size;
    long long rem = static_cast<long long>(degree) % size;
    if (rem < 0) {
      rem += size;
      --q;
    }
    const auto ratioOut =
        ratio(offsets_[static_cast<std::size_t>(rem)]);
    if (!ratioOut.ok())
      return R::failure(ratioOut.error());  // unreachable for finite cents
    const double f = root_.value() * ratioOut.value().value() *
                     std::pow(2.0, static_cast<double>(q));
    if (!std::isfinite(f)) return R::failure(QuantityError::RangeError);
    return R::success(FrequencyHz{f});
  }

  [[nodiscard]] const std::string& formula() const noexcept { return formula_; }

 private:
  // NOTE: formula_ is built from the count parameter, never from the moved
  // vector: member init order would otherwise read a moved-from object.
  CentsTuning(FrequencyHz root, std::vector<Cents> offsets, std::size_t count)
      : root_(root),
        offsets_(std::move(offsets)),
        formula_("cents[" + std::to_string(count) + " offsets]") {}
  FrequencyHz root_;
  std::vector<Cents> offsets_;
  std::string formula_;
};

// Custom map: any pure int -> frequency callable with a stored formula
// string (e.g. "220 * 2^(n/19)"). The callable owns validation of its own
// outputs; the engine stores the formula verbatim and never parses it.
class CustomMap {
 public:
  explicit CustomMap(
      std::function<Result<FrequencyHz, QuantityError>(int)> fn,
      std::string formula)
      : fn_(std::move(fn)), formula_(std::move(formula)) {}

  [[nodiscard]] Result<FrequencyHz, QuantityError> degreeToFrequency(
      int degree) const {
    return fn_(degree);
  }

  [[nodiscard]] const std::string& formula() const noexcept { return formula_; }

 private:
  std::function<Result<FrequencyHz, QuantityError>(int)> fn_;
  std::string formula_;
};

}  // namespace mmathen
