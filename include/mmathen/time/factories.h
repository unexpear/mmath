#pragma once

// Phase 1 — Function1D construction: constant values and wrapped callables
// share one interface, so architecture always assumes continuous variation.
// Inspectability is caller-supplied formula strings in this phase; structured
// expression nodes are deferred. Composition adapters (scale/offset/map) are
// deliberately absent: they belong to the Phase-2 signal layer, not here.

#include <functional>
#include <string>
#include <utility>

#include "function.h"

namespace mmathen {

// Constant function: at(t) == v for every t. Formula pins as "const".
template <typename Output>
[[nodiscard]] Function1D<Output> constant(Output v) {
  return Function1D<Output>{[v](TimePointSeconds) { return v; }, "const"};
}

// Time-varying function. The caller supplies the inspectable formula string
// (e.g. "220 + 220*t", "440 + 10*sin(2*pi*5*t)"); the engine stores it
// verbatim and never parses it in this phase.
template <typename Output, typename Fn>
[[nodiscard]] Function1D<Output> wrap(Fn&& fn, std::string formula) {
  return Function1D<Output>{
      std::function<Output(TimePointSeconds)>(std::forward<Fn>(fn)),
      std::move(formula)};
}

}  // namespace mmathen
