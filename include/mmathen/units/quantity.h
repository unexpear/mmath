#pragma once

// Phase 0 — strict quantity carrier.
//
// Quantity<Value, Dimension, Tag> is the storage layer for measured values.
// Rules:
//   - explicit construction, no implicit conversion to/from raw arithmetic;
//   - NO generic arithmetic: the only operators are scalar scaling for tags
//     that opt in via allows_scalar_scaling, plus the explicit per-type
//     overloads in the unit headers (the operator table). Unlisted
//     operations (TimePoint+TimePoint, Frequency+Duration, Frequency*Duration,
//     Quantity*Quantity, ...) have no overload and fail to compile;
//   - == and <=> are exact equality of the represented value. Approximate
//     comparison is the separate nearlyEqual() (comparison.h), never ==.

#include <compare>
#include <concepts>
#include <type_traits>

namespace mmathen {

template <typename Value, typename Dimension, typename Tag>
class Quantity {
 public:
  using value_type = Value;
  using dimension = Dimension;
  using tag = Tag;

  Quantity() = delete;
  explicit constexpr Quantity(Value v) : v_(v) {}

  [[nodiscard]] constexpr Value value() const noexcept { return v_; }

  constexpr bool operator==(const Quantity&) const = default;
  constexpr auto operator<=>(const Quantity&) const = default;

 private:
  Value v_;
};

// Opt-in scalar scaling for linear quantities. Defaults to false; unit
// headers specialize this true for their scalable tags (Duration, Frequency,
// AngularFrequency, Phase, Pressure, Cents, Beat, Amplitude, Gain,
// SignedAudioValue). TimePoint, SampleRate, FrameIndex, FrameCount,
// FrequencyRatio, TempoBPM are NOT scalable: scaling them is meaningless or
// suspicious, so no overload exists.
template <typename Tag>
struct allows_scalar_scaling : std::false_type {};

template <typename Q>
concept ScalarScalable = allows_scalar_scaling<typename Q::tag>::value;

template <ScalarScalable Q, typename S>
  requires std::is_arithmetic_v<S>
constexpr Q operator*(Q q, S s) {
  using V = typename Q::value_type;
  return Q{static_cast<V>(q.value() * static_cast<V>(s))};
}

template <ScalarScalable Q, typename S>
  requires std::is_arithmetic_v<S>
constexpr Q operator*(S s, Q q) {
  return q * s;
}

template <ScalarScalable Q, typename S>
  requires std::is_arithmetic_v<S>
constexpr Q operator/(Q q, S s) {
  using V = typename Q::value_type;
  return Q{static_cast<V>(q.value() / static_cast<V>(s))};
}

}  // namespace mmathen
