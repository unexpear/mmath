#pragma once

// D29 — lock-free triple buffer: hands the LATEST value of a settings
// struct from one writer thread (the UI) to one reader thread (audio),
// never blocking either side and never tearing a value.
//
// Three slots of T (trivially copyable). The writer owns one slot (back),
// the reader owns one (front), and the third (middle) is exchanged
// atomically together with a "new data" bit:
//   write(v): copy v into back; back <- exchange(middle, back | NEW)
//   read(out): if middle has NEW: front <- exchange(middle, front);
//              out = copy of front; return true
// The exchange is acq_rel, so every byte the writer copied into a slot
// happens-before the reader's copy out of it. Intermediate values may be
// skipped (only the newest matters); a value is never seen half-written
// and never seen twice as "new". Verified by a two-thread test that writes
// internally consistent structs with increasing counters and checks every
// read is consistent and never goes backwards.

#include <atomic>
#include <type_traits>

namespace mmathen {

template <typename T>
class TripleBuffer {
  static_assert(std::is_trivially_copyable_v<T>,
                "TripleBuffer holds trivially copyable values");

 public:
  explicit TripleBuffer(const T& initial = T{}) noexcept
      : slots_{initial, initial, initial} {}

  TripleBuffer(const TripleBuffer&) = delete;
  TripleBuffer& operator=(const TripleBuffer&) = delete;

  // Writer thread only.
  void write(const T& value) noexcept {
    slots_[back_] = value;
    back_ = middle_.exchange(back_ | kNew, std::memory_order_acq_rel) & kIndex;
  }

  // Reader thread only: true (and out updated) when a newer value exists.
  bool read(T& out) noexcept {
    if ((middle_.load(std::memory_order_relaxed) & kNew) == 0) return false;
    front_ = middle_.exchange(front_, std::memory_order_acq_rel) & kIndex;
    out = slots_[front_];
    return true;
  }

 private:
  static constexpr unsigned kIndex = 3u;
  static constexpr unsigned kNew = 4u;
  T slots_[3];
  unsigned front_ = 0;              // reader-owned
  unsigned back_ = 2;               // writer-owned
  std::atomic<unsigned> middle_{1};  // shared: index | NEW
};

}  // namespace mmathen
