#pragma once

// D3 — constructive Moments of Symmetry (Carey/Clough lineage, bounded).
//
// Stack a generator `count` times modulo the octave, sort, and measure the
// step multiset: a MOS has EXACTLY two distinct step sizes (Myhill's
// property territory; the full Carey/Clough formalism with well-formedness
// proofs is deferred with citation). Steps cluster with 1e-9 relative
// tolerance — stacked floating-point sums need not be bitwise identical.
// Degenerate inputs (generator ≡ 0 mod octave, duplicate pitch classes)
// are InvalidArgument, not silent single-note "scales".

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "../core/result.h"
#include "../units/music_units.h"

namespace mmathen {

struct MosScale {
  std::vector<double> pitchClasses;            // sorted, in [0, 1200)
  std::vector<double> steps;                   // count steps incl. wrap
  std::vector<std::pair<double, int>> sizes;   // (step size, multiplicity)
  bool isMOS = false;                          // exactly two step sizes
};

[[nodiscard]] inline Result<MosScale, QuantityError> mosWord(Cents generator,
                                                             int count) {
  using R = Result<MosScale, QuantityError>;
  const double g = generator.value();
  if (!std::isfinite(g) || count < 1)
    return R::failure(QuantityError::InvalidArgument);
  MosScale out;
  out.pitchClasses.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    double pc = std::fmod(i * g, 1200.0);
    if (pc < 0.0) pc += 1200.0;
    if (pc >= 1200.0) pc -= 1200.0;  // normalize -0.0-style edge to unison
    if (!std::isfinite(pc)) return R::failure(QuantityError::RangeError);
    out.pitchClasses.push_back(pc);
  }
  std::sort(out.pitchClasses.begin(), out.pitchClasses.end());
  for (int i = 1; i < count; ++i) {
    if (out.pitchClasses[static_cast<std::size_t>(i)] ==
        out.pitchClasses[static_cast<std::size_t>(i - 1)])
      return R::failure(QuantityError::InvalidArgument);  // degenerate stack
  }
  out.steps.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    const double a = out.pitchClasses[static_cast<std::size_t>(i)];
    const double b = (i + 1 < count)
                         ? out.pitchClasses[static_cast<std::size_t>(i + 1)]
                         : out.pitchClasses[0] + 1200.0;
    out.steps.push_back(b - a);
  }
  // Cluster step sizes: same size within 1e-9 relative.
  std::vector<double> sorted = out.steps;
  std::sort(sorted.begin(), sorted.end());
  for (double s : sorted) {
    if (!out.sizes.empty() &&
        std::fabs(s - out.sizes.back().first) <=
            1e-9 * (s > out.sizes.back().first ? s : out.sizes.back().first)) {
      ++out.sizes.back().second;
    } else {
      out.sizes.emplace_back(s, 1);
    }
  }
  out.isMOS = (out.sizes.size() == 2);
  return R::success(std::move(out));
}

}  // namespace mmathen
