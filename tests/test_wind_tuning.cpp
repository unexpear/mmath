// Step 24.2f: every semitone of the wind auditions in tune. One chromatic
// run per instrument (1 s per note, tongued, default player) through the
// audition's wind path; each note's pitch is read from its spectrum (the
// lowest peak within 30 dB of the strongest, so a note that sounds an
// octave or a twelfth up, or splits, reads as that wrong pitch) and must
// sit within 10 cents of the written pitch. The tolerance is TUNED: the
// usual just-noticeable mistuning between two held tones is about 5-10
// cents. Also checks that a forced crack near A4 on the alto saxophone
// comes back within the step 24.2a window and does not stick up.
#include <catch2/catch_test_macros.hpp>

#include "../app/player_audition.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <vector>

using namespace mmathen;
namespace au = app::audition;

namespace {
constexpr double kPi = 3.14159265358979323846;

void fftInPlace(std::vector<std::complex<double>>& a) {
  const std::size_t n = a.size();
  for (std::size_t i = 1, j = 0; i < n; ++i) {
    std::size_t b = n >> 1;
    for (; j & b; b >>= 1) j ^= b;
    j ^= b;
    if (i < j) std::swap(a[i], a[j]);
  }
  for (std::size_t len = 2; len <= n; len <<= 1) {
    const double ang = -2.0 * kPi / static_cast<double>(len);
    const std::complex<double> wl(std::cos(ang), std::sin(ang));
    for (std::size_t i = 0; i < n; i += len) {
      std::complex<double> w(1.0);
      for (std::size_t j = 0; j < len / 2; ++j) {
        const auto u = a[i + j], v = a[i + j + len / 2] * w;
        a[i + j] = u + v;
        a[i + j + len / 2] = u - v;
        w *= wl;
      }
    }
  }
}

// The sounding fundamental of mix[a, b) in cents from f: Hann window,
// 2^17-point FFT; the lowest local peak (+-8 Hz) at or above 0.4 f that
// is within 30 dB of the strongest peak, its
// pitch from the power centroid around it. Returns a large number when
// nothing qualifies.
double spectralCents(const std::vector<double>& mix, std::size_t a, std::size_t b, double f) {
  const std::size_t N = std::size_t{1} << 17;
  const std::size_t L = std::min<std::size_t>(b - a, N);
  std::vector<std::complex<double>> X(N);
  for (std::size_t i = 0; i < L; ++i)
    X[i] = mix[a + i] * (0.5 - 0.5 * std::cos(2.0 * kPi * static_cast<double>(i) / static_cast<double>(L)));
  fftInPlace(X);
  std::vector<double> mag(N / 2);
  for (std::size_t i = 0; i < N / 2; ++i) mag[i] = std::abs(X[i]);
  const double df = au::kRate / static_cast<double>(N);
  const auto lo = static_cast<std::size_t>(std::max(30.0, 0.4 * f) / df);
  const auto hi = static_cast<std::size_t>(8000.0 / df);
  std::size_t kmax = lo;
  for (std::size_t i = lo; i < hi; ++i)
    if (mag[i] > mag[kmax]) kmax = i;
  if (!(mag[kmax] > 0.0)) return 1e9;
  const auto w2 = static_cast<std::size_t>(8.0 / df);
  std::size_t k0 = kmax;
  for (std::size_t i = std::max(lo, w2 + 1); i < kmax; ++i) {
    if (mag[i] < mag[kmax] * 0.0316) continue;  // -30 dB
    bool peak = true;
    for (std::size_t j = i - w2; j <= i + w2; ++j)
      if (mag[j] > mag[i]) { peak = false; break; }
    if (peak) { k0 = i; break; }
  }
  // The vibrato spreads a high note over several close peaks, so the
  // pitch is the power centroid within +-3% (about +-50 cents) of the
  // chosen peak, not the single tallest bin.
  double sw = 0.0, swf = 0.0;
  const auto c0 = static_cast<std::size_t>(0.97 * static_cast<double>(k0)), c1 = static_cast<std::size_t>(1.03 * static_cast<double>(k0)) + 1;
  for (std::size_t i = c0; i <= c1 && i < mag.size(); ++i) {
    sw += mag[i] * mag[i];
    swf += mag[i] * mag[i] * static_cast<double>(i);
  }
  const double hz = swf / sw * df;
  return 1200.0 * std::log2(hz / f);
}

// Frame pitch by YIN (A. de Cheveigne & H. Kawahara, JASA 111, 1917-1930,
// 2002), as in test_wind_cracks.cpp so the step 24.2a crack window means
// the same here: first dip under 0.15 between fLo and fHi. 0 = no pitch.
double yinHz(const std::vector<double>& x, std::size_t a, double fs, double fLo, double fHi) {
  constexpr std::size_t n = 1024;
  const auto tMin = static_cast<std::size_t>(fs / fHi), tMax = static_cast<std::size_t>(fs / fLo);
  if (tMin < 2 || a + n + tMax + 1 > x.size()) return 0.0;
  std::vector<double> dn(tMax + 2, 1.0);
  double run = 0.0;
  for (std::size_t t = 1; t <= tMax + 1; ++t) {
    double d = 0.0;
    for (std::size_t k = 0; k < n; ++k) {
      const double e = x[a + k] - x[a + k + t];
      d += e * e;
    }
    run += d;
    dn[t] = run > 0.0 ? d * static_cast<double>(t) / run : 1.0;
  }
  std::size_t best = 0;
  for (std::size_t t = tMin; t <= tMax; ++t)
    if (dn[t] < 0.15) {
      while (t + 1 <= tMax && dn[t + 1] < dn[t]) ++t;
      best = t;
      break;
    }
  if (best == 0) {
    double m = 1e300;
    for (std::size_t t = tMin; t <= tMax; ++t)
      if (dn[t] < m) { m = dn[t]; best = t; }
    if (m > 0.5) return 0.0;
  }
  return fs / static_cast<double>(best);
}

PlayerInstrument instrumentOf(const char* key) {
  PlayerInstrument i{};
  REQUIRE(playerInstrumentFromKey(key, i));
  return i;
}

// Plays sounding MIDI lo..hi, one second each, and returns each note's
// error in cents (spectrum from 0.35 s into the note to 0.05 s before
// its end, past the attack).
std::vector<double> chromaticErrors(const char* key, int lo, int hi) {
  std::vector<PartNote> w;
  double t = 0.25;
  for (int m = lo; m <= hi; ++m) {
    PartNote n;
    n.startSec = t;
    n.durSec = 1.0;
    n.midi = m;
    w.push_back(n);
    t += 1.0;
  }
  const PlayerInstrument inst = instrumentOf(key);
  std::vector<double> mix(static_cast<std::size_t>((t + 1.5) * au::kRate), 0.0);
  REQUIRE(au::detail::renderWind(makePlayer(inst, defaultValues(inst)), w, false, mix));
  REQUIRE(std::all_of(mix.begin(), mix.end(), [](double x) { return std::isfinite(x); }));
  std::vector<double> out;
  for (const PartNote& n : w) {
    const double f = 440.0 * std::pow(2.0, (n.midi - 69) / 12.0);
    out.push_back(spectralCents(mix, static_cast<std::size_t>((n.startSec + 0.35) * au::kRate),
                                static_cast<std::size_t>((n.startSec + n.durSec - 0.05) * au::kRate), f));
  }
  return out;
}

void checkRange(const char* key, int lo, int hi, double tolCents) {
  const auto e = chromaticErrors(key, lo, hi);
  for (int m = lo; m <= hi; ++m) {
    INFO(key << " sounding MIDI " << m << ": " << e[static_cast<std::size_t>(m - lo)] << " cents");
    CHECK(std::fabs(e[static_cast<std::size_t>(m - lo)]) <= tolCents);
  }
}
}  // namespace

TEST_CASE("wind tuning: alto saxophone, every semitone of the written range", "[wind_tuning]") {
  // Eb alto: written Bb3..F#6 sounds Db3..A5 (MIDI 49..81).
  checkRange("alto_sax", 49, 81, 10.0);
}

TEST_CASE("wind tuning: trumpet, every semitone down to written F#3", "[wind_tuning]") {
  // Bb trumpet: written F#3..C6 sounds E3..Bb5 (MIDI 52..82).
  const auto e = chromaticErrors("trumpet", 52, 82);
  for (int m = 52; m <= 82; ++m) {
    INFO("trumpet sounding MIDI " << m << ": " << e[static_cast<std::size_t>(m - 52)] << " cents");
    CHECK(std::fabs(e[static_cast<std::size_t>(m - 52)]) <= 10.0);
  }
  // Named in the brief: written C4 (sounds Bb3, 58) and F#3 (sounds E3, 52).
  CHECK(std::fabs(e[58 - 52]) <= 10.0);
  CHECK(std::fabs(e[0]) <= 10.0);
}

TEST_CASE("wind tuning: trombone, clarinet and flute across their ranges", "[wind_tuning]") {
  SECTION("trombone E2..F5") { checkRange("trombone", 40, 77, 10.0); }
  // Clarinet: concert D3..Bb5 (written E3..C6). Above that the model has
  // no register hole or altissimo fingering and does not speak.
  SECTION("clarinet D3..Bb5") { checkRange("clarinet", 50, 82, 10.0); }
  SECTION("flute C4..C7") { checkRange("flute", 60, 96, 10.0); }
}

TEST_CASE("wind tuning: an alto saxophone crack near A4 comes back and does not stick", "[wind_tuning]") {
  // Forced onset errors on 0.6 s tongued notes around concert A4: any jump
  // to another regime is brief (<= 30 frames of 10 ms, the step 24.2a
  // window) and the last third of every note is back on pitch (> 0.8).
  const PlayerInstrument inst = instrumentOf("alto_sax");
  for (double err : {600.0, 1000.0, 1500.0}) {
    std::vector<PartNote> w;
    std::vector<double> errs;
    double t = 0.25;
    for (int m = 64; m <= 73; ++m) {
      PartNote n;
      n.startSec = t;
      n.durSec = 0.6;
      n.midi = m;
      w.push_back(n);
      errs.push_back(err);
      t += 0.6;
    }
    std::vector<double> mix(static_cast<std::size_t>((t + 1.5) * au::kRate), 0.0);
    REQUIRE(au::detail::renderWind(builtInPlayer(inst), w, false, mix, &errs));
    for (const PartNote& n : w) {
      const double f = 440.0 * std::pow(2.0, (n.midi - 69) / 12.0);
      std::vector<double> ratio;
      for (double s = n.startSec; s < n.startSec + n.durSec - 0.04; s += 0.01) {
        const double hz = yinHz(mix, static_cast<std::size_t>(s * au::kRate), au::kRate, f / 2.2, std::min(4000.0, f * 3.3));
        ratio.push_back(hz > 0.0 ? hz / f : 0.0);
      }
      int run = 0, worst = 0;
      for (double r : ratio) {
        if (r > 0.0 && std::fabs(1200.0 * std::log2(r)) > 300.0) worst = std::max(worst, ++run);
        else run = 0;
      }
      int ok = 0, all = 0;
      for (std::size_t i = ratio.size() * 2 / 3; i < ratio.size(); ++i) {
        if (!(ratio[i] > 0.0)) continue;
        ++all;
        if (std::fabs(1200.0 * std::log2(ratio[i])) <= 300.0) ++ok;
      }
      REQUIRE(all >= 5);
      INFO("error " << err << " cents, sounding MIDI " << n.midi << ": longest off run " << worst << " frames, back "
                    << static_cast<double>(ok) / all);
      CHECK(worst <= 30);
      CHECK(static_cast<double>(ok) / all > 0.8);
    }
  }
}
