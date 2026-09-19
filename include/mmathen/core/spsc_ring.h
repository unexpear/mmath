#pragma once

// D22 — lock-free single-producer / single-consumer ring buffer of doubles:
// the hand-off between an audio callback thread and a UI thread
// (FOUNDATION: no allocation, no locks, bounded work in the callback).
//
// Capacity is a power of two, fixed at construction (the only allocation).
// One producer thread calls write(); one consumer thread calls read(). Two
// monotonically increasing indices (head = written, tail = read) are
// std::atomic<std::size_t>; the producer publishes with a release store of
// head after copying samples in, the consumer acquires head before reading
// them (and symmetrically for tail), so every sample the consumer sees is
// fully written. write() stores what fits and returns the count (never
// blocks, never overwrites unread data); read() returns what is available.
// Index wrap after 2^64 samples is unsigned arithmetic, correct for any
// fill level below capacity.
//
// Verified single-threaded (FIFO order, wrap-around, full/empty behaviour)
// and by a two-thread stress test streaming a counting sequence (every value
// must arrive exactly once, in order), and that test plus the whole suite
// run clean under ThreadSanitizer (MMATHEN_SANITIZE_THREAD, CI job "tsan").

#include <atomic>
#include <cstddef>
#include <memory>
#include <vector>

#include "result.h"

namespace mmathen {

class SpscRing {
 public:
  // capacity: power of two in [2, 2^26]; otherwise InvalidArgument.
  [[nodiscard]] static Result<std::unique_ptr<SpscRing>, QuantityError> create(
      std::size_t capacity) {
    using R = Result<std::unique_ptr<SpscRing>, QuantityError>;
    if (capacity < 2 || capacity > (std::size_t{1} << 26) ||
        (capacity & (capacity - 1)) != 0)
      return R::failure(QuantityError::InvalidArgument);
    return R::success(std::unique_ptr<SpscRing>(new SpscRing(capacity)));
  }

  SpscRing(const SpscRing&) = delete;
  SpscRing& operator=(const SpscRing&) = delete;

  // Producer: copy up to n samples; returns how many were stored.
  std::size_t write(const double* in, std::size_t n) noexcept {
    const std::size_t head = head_.load(std::memory_order_relaxed);
    const std::size_t tail = tail_.load(std::memory_order_acquire);
    const std::size_t space = buf_.size() - (head - tail);
    const std::size_t count = n < space ? n : space;
    for (std::size_t i = 0; i < count; ++i)
      buf_[(head + i) & mask_] = in[i];
    head_.store(head + count, std::memory_order_release);
    return count;
  }

  // Consumer: copy up to n samples out; returns how many were read.
  std::size_t read(double* out, std::size_t n) noexcept {
    const std::size_t tail = tail_.load(std::memory_order_relaxed);
    const std::size_t head = head_.load(std::memory_order_acquire);
    const std::size_t avail = head - tail;
    const std::size_t count = n < avail ? n : avail;
    for (std::size_t i = 0; i < count; ++i)
      out[i] = buf_[(tail + i) & mask_];
    tail_.store(tail + count, std::memory_order_release);
    return count;
  }

  // Snapshot of the fill level (exact only when both sides are idle).
  [[nodiscard]] std::size_t available() const noexcept {
    return head_.load(std::memory_order_acquire) -
           tail_.load(std::memory_order_acquire);
  }
  [[nodiscard]] std::size_t capacity() const noexcept { return buf_.size(); }

 private:
  explicit SpscRing(std::size_t capacity)
      : buf_(capacity, 0.0), mask_(capacity - 1) {}
  std::vector<double> buf_;
  std::size_t mask_;
  // No alignas(64) cache-line padding: it would only avoid false sharing
  // (performance), and MSVC /W4 /WX rejects the resulting padding (C4324).
  std::atomic<std::size_t> head_{0};
  std::atomic<std::size_t> tail_{0};
};

}  // namespace mmathen
