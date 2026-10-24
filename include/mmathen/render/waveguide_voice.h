#pragma once

// One stateful voice: a tuned waveguide stepped at the sample rate.
//
// The sample is the string displacement at one interior rail node,
// read and then advanced. There is no gain envelope. A lossy loop
// decays because its bridge filter has |H| < 1, which is the same
// one-pole already in tuned_waveguide.h. This is not an instrument.
// No violin, guitar, or harp length is stored here.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "../physics/tuned_waveguide.h"
#include "voice.h"

namespace mmathen {

class WaveguideVoice final : public IVoice {
 public:
  // pickupFraction is the rail position in (0, 1). The node is the
  // nearest interior point, never a fixed end. The shape's system
  // must already be the one the filters were designed for.
  [[nodiscard]] static Result<WaveguideVoice, QuantityError> create(
      const InitialShape& shape, const LoopFilters& filters,
      SampleRateHz rate, double pickupFraction) {
    using R = Result<WaveguideVoice, QuantityError>;
    const double fs = rate.value();
    if (!std::isfinite(fs) || !(fs > 0.0) || !std::isfinite(pickupFraction) ||
        !(pickupFraction > 0.0) || !(pickupFraction < 1.0))
      return R::failure(QuantityError::InvalidArgument);
    auto guide = TunedWaveguide::create(shape, filters);
    if (!guide.ok()) return R::failure(guide.error());
    const std::size_t intervals = filters.intervals;
    if (intervals < 2) return R::failure(QuantityError::InvalidArgument);
    const double scaled =
        pickupFraction * static_cast<double>(intervals);
    auto node = static_cast<std::size_t>(std::llround(scaled));
    if (node < 1) node = 1;
    if (node >= intervals) node = intervals - 1;
    return R::success(WaveguideVoice{std::move(guide).value(), rate, node});
  }

  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }

  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(
      double* out, FrameCount n) override {
    using R = Result<FrameCount, QuantityError>;
    const std::uint64_t count = static_cast<std::uint64_t>(n.value());
    if (count == 0) return R::success(n);
    if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
    for (std::uint64_t i = 0; i < count; ++i) {
      auto sample = guide_.valueAt(node_);
      if (!sample.ok()) return R::failure(sample.error());
      out[i] = sample.value();
      guide_.step();
    }
    return R::success(n);
  }

  void reset() override { guide_.reset(); }

  [[nodiscard]] std::size_t pickupNode() const noexcept { return node_; }

 private:
  WaveguideVoice(TunedWaveguide guide, SampleRateHz rate, std::size_t node)
      : guide_(std::move(guide)), rate_(rate), node_(node) {}

  TunedWaveguide guide_;
  SampleRateHz rate_;
  std::size_t node_;
};

}  // namespace mmathen
