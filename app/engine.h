#pragma once

// mmathEn desktop app — the audio engine (runs on SDL's audio threads).
//
// Chain per block, identical for file playback and the microphone:
//   source -> EQ (BiquadCascade, up to 32 sections: graphic, parametric or
//   match designs) -> compressor (linked stereo) -> volume -> limiter
//   (look-ahead, ceiling guaranteed) -> clip count -> output, and the same
//   stereo frames to the UI (analyser, loudness and true-peak meters).
// Every stage has an on/off switch that CROSSFADES (20 ms GainRamp between
// the stage's input and output) instead of cutting; the limiter's "off"
// path is its input delayed by the same look-ahead, so latency never jumps.
// File playback goes through SpliceReader, so seeks and loop seams are
// crossfades, and play/pause ramps the level over 5 ms (no clicks).
//
// Threads: the UI publishes a complete EngineParams snapshot through a
// TripleBuffer (lock-free, never torn); the callback picks up the newest at
// the start of each block and retunes allocation-free (setSections keeps
// filter state; setParams keeps detector state). Transport requests are
// atomics. Rare structural changes (load a track, change rate, rebuild the
// reader) happen with the stream locked (SDL_LockAudioStream), which SDL
// also holds while running the callback.

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <mmathen/core/spsc_ring.h>
#include <mmathen/core/triple_buffer.h>
#include <mmathen/dsp/biquad.h>
#include <mmathen/dsp/crossfade.h>
#include <mmathen/dsp/dynamics.h>
#include <mmathen/dsp/filters.h>

namespace app {

using namespace mmathen;

constexpr std::size_t kChunk = 512;
constexpr std::size_t kMaxSections = 32;
constexpr double kMicRate = 48000.0;
constexpr double kLimiterLookaheadMs = 5.0;
enum Mode : int { kNone = 0, kFile = 1, kMic = 2 };

// Everything the UI can change continuously. Trivially copyable.
struct EngineParams {
  std::array<BiquadCoeffs, kMaxSections> eq{};
  int eqCount = 0;
  bool eqOn = true;
  CompressorParams comp{};
  bool compOn = false;
  LimiterParams lim{-1.0, kLimiterLookaheadMs, 80.0};
  bool limOn = true;
  double volumeDb = 0.0;
};

// Fixed-length delay (the limiter-off path).
struct DelayLine {
  std::vector<double> buf;
  std::size_t pos = 0;
  void resize(std::size_t n) {
    buf.assign(std::max<std::size_t>(n, 1), 0.0);
    pos = 0;
  }
  double push(double x) {
    const double y = buf[pos];
    buf[pos] = x;
    pos = (pos + 1) % buf.size();
    return y;
  }
};

struct Engine {
  // ---- structural state: changed only under the stream's lock ----------
  std::vector<float> track;  // interleaved stereo
  std::size_t trackFrames = 0;
  double rate = 48000.0;
  std::optional<SpliceReader<float>> reader;
  std::optional<BiquadCascade> eq[2];
  std::optional<Compressor> comp;
  std::optional<Limiter> lim;
  DelayLine limDry[2];
  bool truePeakMode = false;  // limiter mode (sets its latency)

  // ---- shared with the UI -------------------------------------------
  std::atomic<int> mode{kNone};
  std::atomic<bool> playing{false};
  std::atomic<bool> loop{true};
  std::atomic<long long> seekTo{-1};
  std::atomic<long long> loopA{-1}, loopB{-1};
  std::atomic<unsigned> loopVersion{0};
  TripleBuffer<EngineParams> params;
  std::atomic<float> compGrDb{0.0f}, limGrDb{0.0f};
  std::atomic<std::uint64_t> shownPos{0};
  std::atomic<std::uint64_t> clips{0};
  std::atomic<bool> ended{false};
  std::unique_ptr<SpscRing> ring;

  // ---- audio-thread state ---------------------------------------------
  EngineParams cur{};
  bool haveParams = false;
  unsigned loopSeen = 0;
  GainRamp playRamp{0.0};
  GainRamp eqMix{1.0}, compMix{0.0}, limMix{1.0};
  std::array<double, 2 * kChunk> inter{};
  std::array<double, kChunk> l{}, r{}, dl{}, dr{};
  std::array<double, 2 * kChunk> tap{};  // interleaved output for the UI
  std::array<float, 2 * kChunk> out{};
  std::array<float, kChunk> micIn{};

  std::size_t ms(double millis) const {
    return static_cast<std::size_t>(std::llround(millis * 1e-3 * rate));
  }

  // Build the per-rate processors (UI thread, stream locked or idle).
  bool buildProcessors(double fs) {
    rate = fs;
    for (auto& c : eq) {
      auto e = BiquadCascade::create(kMaxSections);
      if (!e.ok()) return false;
      c = std::move(e).value();
    }
    auto c = Compressor::create(CompressorParams{}, SampleRateHz{fs});
    auto li = Limiter::create(
        LimiterParams{-1.0, kLimiterLookaheadMs, 80.0, truePeakMode},
        SampleRateHz{fs});
    if (!c.ok() || !li.ok()) return false;
    comp = std::move(c).value();
    lim = std::move(li).value();
    for (auto& d : limDry) d.resize(lim->latencyFrames());
    haveParams = false;  // re-apply the current snapshot on the next block
    return true;
  }

  // Pick up the newest parameters (audio thread, allocation-free).
  void applyParams() {
    EngineParams p;
    if (params.read(p)) {
      cur = p;
      haveParams = false;
    }
    if (haveParams) return;
    haveParams = true;
    const auto n = static_cast<std::size_t>(std::clamp(cur.eqCount, 0, 32));
    for (auto& c : eq)
      if (c) (void)c->setSections(cur.eq.data(), n);
    if (comp) (void)comp->setParams(cur.comp);
    if (lim) (void)lim->setParams(cur.lim);
    const std::size_t fade = ms(20.0);
    if (eqMix.target() != (cur.eqOn ? 1.0 : 0.0))
      eqMix.setTarget(cur.eqOn ? 1.0 : 0.0, fade);
    if (compMix.target() != (cur.compOn ? 1.0 : 0.0))
      compMix.setTarget(cur.compOn ? 1.0 : 0.0, fade);
    if (limMix.target() != (cur.limOn ? 1.0 : 0.0))
      limMix.setTarget(cur.limOn ? 1.0 : 0.0, fade);
  }

  // The processing chain on l/r (n frames), then clip count, analyser feed.
  void chain(std::size_t n, bool stereo) {
    if (!stereo)
      for (std::size_t i = 0; i < n; ++i) r[i] = l[i];
    const FrameCount count{static_cast<std::uint64_t>(n)};
    // EQ, crossfaded on/off.
    dl = l;
    dr = r;
    eq[0]->processBlock(l.data(), l.data(), count);
    eq[1]->processBlock(r.data(), r.data(), count);
    for (std::size_t i = 0; i < n; ++i) {
      const double w = eqMix.next();
      l[i] = dl[i] + (l[i] - dl[i]) * w;
      r[i] = dr[i] + (r[i] - dr[i]) * w;
    }
    // Compressor, crossfaded on/off.
    dl = l;
    dr = r;
    comp->processStereo(l.data(), r.data(), l.data(), r.data(), count);
    for (std::size_t i = 0; i < n; ++i) {
      const double w = compMix.next();
      l[i] = dl[i] + (l[i] - dl[i]) * w;
      r[i] = dr[i] + (r[i] - dr[i]) * w;
    }
    // Volume, then the limiter (always running, so its delay is constant).
    const double g = std::pow(10.0, cur.volumeDb / 20.0);
    for (std::size_t i = 0; i < n; ++i) {
      l[i] *= g;
      r[i] *= g;
      dl[i] = limDry[0].push(l[i]);
      dr[i] = limDry[1].push(r[i]);
    }
    lim->processStereo(l.data(), r.data(), l.data(), r.data(), count);
    std::uint64_t clipped = 0;
    for (std::size_t i = 0; i < n; ++i) {
      const double w = limMix.next();
      double a = dl[i] + (l[i] - dl[i]) * w;
      double b = dr[i] + (r[i] - dr[i]) * w;
      if (std::fabs(a) > 1.0) {
        ++clipped;
        a = a > 0.0 ? 1.0 : -1.0;
      }
      if (std::fabs(b) > 1.0) {
        ++clipped;
        b = b > 0.0 ? 1.0 : -1.0;
      }
      l[i] = a;
      r[i] = b;
      tap[2 * i] = a;
      tap[2 * i + 1] = b;
    }
    if (clipped != 0) clips.fetch_add(clipped, std::memory_order_relaxed);
    compGrDb.store(static_cast<float>(comp->gainReductionDb()),
                   std::memory_order_relaxed);
    limGrDb.store(static_cast<float>(lim->gainReductionDb()),
                  std::memory_order_relaxed);
    // Whole stereo frames only: a split frame would swap the channels for
    // the rest of the session. available() can only shrink while we run
    // (the UI consumes), so the free space computed here is a lower bound.
    const std::size_t freeFrames = (ring->capacity() - ring->available()) / 2;
    (void)ring->write(tap.data(), 2 * std::min(n, freeFrames));
  }

  // SDL playback callback body.
  void playback(SDL_AudioStream* stream, int additional) {
    if (mode.load() != kFile || !reader || trackFrames == 0 || additional <= 0 ||
        !eq[0] || !comp || !lim)
      return;
    applyParams();
    const long long s = seekTo.exchange(-1);
    if (s >= 0) reader->jumpTo(static_cast<std::size_t>(s));
    reader->setLooping(loop.load(std::memory_order_relaxed));
    const unsigned lv = loopVersion.load(std::memory_order_acquire);
    if (lv != loopSeen) {
      loopSeen = lv;
      const long long a = loopA.load(), b = loopB.load();
      if (a >= 0 && b > a)
        (void)reader->setLoopRegion(static_cast<std::size_t>(a),
                                    static_cast<std::size_t>(b));
      else
        (void)reader->setLoopRegion(0, trackFrames);
    }
    const bool want = playing.load();
    if (want && playRamp.target() != 1.0) playRamp.setTarget(1.0, ms(5.0));
    if (!want && playRamp.target() != 0.0) playRamp.setTarget(0.0, ms(5.0));
    if (!want && !playRamp.ramping() && playRamp.value() == 0.0) return;
    std::size_t need =
        static_cast<std::size_t>(additional) / (2 * sizeof(float));
    while (need > 0) {
      const std::size_t n = std::min(need, kChunk);
      const std::size_t got = reader->read(inter.data(), n);
      for (std::size_t i = 0; i < n; ++i) {
        const double g = playRamp.next();
        l[i] = i < got ? inter[2 * i] * g : 0.0;
        r[i] = i < got ? inter[2 * i + 1] * g : 0.0;
      }
      if (got < n) {  // the track ended (not looping)
        playing.store(false);
        ended.store(true);
        playRamp.setTarget(0.0, 0);
      }
      chain(n, true);
      for (std::size_t i = 0; i < n; ++i) {
        out[2 * i] = static_cast<float>(l[i]);
        out[2 * i + 1] = static_cast<float>(r[i]);
      }
      SDL_PutAudioStreamData(stream, out.data(),
                             static_cast<int>(n * 2 * sizeof(float)));
      need -= n;
      if (got < n) break;
      if (!playRamp.ramping() && playRamp.value() == 0.0) break;
    }
    shownPos.store(reader->position(), std::memory_order_relaxed);
  }

  // SDL recording callback body.
  void recording(SDL_AudioStream* stream) {
    for (;;) {
      const int bytes = SDL_GetAudioStreamData(
          stream, micIn.data(), static_cast<int>(sizeof(float) * kChunk));
      if (bytes <= 0) break;
      const std::size_t n = static_cast<std::size_t>(bytes) / sizeof(float);
      if (mode.load() != kMic || !eq[0] || !comp || !lim) continue;
      applyParams();
      for (std::size_t i = 0; i < n; ++i) l[i] = static_cast<double>(micIn[i]);
      chain(n, false);
    }
  }
};

inline void SDLCALL playCallback(void* user, SDL_AudioStream* stream,
                                 int additional, int) {
  static_cast<Engine*>(user)->playback(stream, additional);
}

inline void SDLCALL micCallback(void* user, SDL_AudioStream* stream, int,
                                int) {
  static_cast<Engine*>(user)->recording(stream);
}

}  // namespace app
