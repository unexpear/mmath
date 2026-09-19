// D29 — triple buffer (Catch2): newest value wins, nothing is torn or
// repeated, across two threads.

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <cstdint>
#include <thread>

#include <mmathen/core/triple_buffer.h>

using namespace mmathen;

namespace {
struct Block {
  std::uint64_t counter = 0;
  std::array<std::uint64_t, 63> copies{};  // all equal to counter
};
Block make(std::uint64_t c) {
  Block b;
  b.counter = c;
  b.copies.fill(c);
  return b;
}
}  // namespace

TEST_CASE("triple buffer: newest wins, read once", "[d29tb]") {
  TripleBuffer<Block> tb(make(0));
  Block out;
  CHECK_FALSE(tb.read(out));  // nothing new yet
  tb.write(make(1));
  tb.write(make(2));
  tb.write(make(3));
  REQUIRE(tb.read(out));
  CHECK(out.counter == 3);  // intermediate values skipped
  CHECK_FALSE(tb.read(out));  // not new twice
  CHECK(out.counter == 3);
  tb.write(make(4));
  REQUIRE(tb.read(out));
  CHECK(out.counter == 4);
}

TEST_CASE("triple buffer: two threads, no tearing, monotone", "[d29tb]") {
  TripleBuffer<Block> tb(make(0));
  constexpr std::uint64_t kWrites = 200000;
  std::atomic<bool> done{false};
  std::thread writer([&] {
    for (std::uint64_t c = 1; c <= kWrites; ++c) tb.write(make(c));
    done.store(true);
  });
  std::uint64_t last = 0, reads = 0, torn = 0, backwards = 0;
  Block out;
  for (;;) {
    // Snapshot first: once the writer is done, one more drain sees its
    // final value (its writes happen-before the done store).
    const bool finished = done.load();
    while (tb.read(out)) {
      ++reads;
      for (auto v : out.copies)
        if (v != out.counter) ++torn;
      if (out.counter <= last) ++backwards;
      last = out.counter;
    }
    if (finished) break;
  }
  writer.join();
  CHECK(torn == 0);
  CHECK(backwards == 0);
  CHECK(last == kWrites);  // the newest value is always delivered
  CHECK(reads > 0);
}
