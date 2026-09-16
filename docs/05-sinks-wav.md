# Sinks Design — Null + Minimal WAV (Phase 2.5)

Status: locked · Sinks are boundary, not core · Zero third-party deps

## Priority order

```text
Null
  ↓
WAV / File
  ↓
WASAPI (first realtime, Windows)
CoreAudio · ALSA/PipeWire
  ↓
JACK · ASIO (licensing/distribution permitting)
```

Do not build a universal backend abstraction until the offline pipeline
proves what the renderer actually needs.

## Rule: graph never names a sink

```cpp
renderer.render(graph, sink);
```

Never:

```cpp
signal.playOnWasapi();  // forbidden
```

Future shape (frame count authoritative at the low level):

```cpp
struct IAudioSink {
  virtual ~IAudioSink() = default;
  virtual void consume(const float* frames, FrameCount n) = 0;  // or double; fixed in MVP
};
// Rendering entry takes FrameCount N (+ t_0, fs); duration-based overloads
// are convenience wrappers with documented rounding. File I/O stays outside
// the processing boundary: render into supplied buffers, then write.
struct NullSink : IAudioSink { /* count, check peak/NaN/Inf/overload, discard */ };
struct WavSink  : IAudioSink { /* write RIFF/WAVE */ };
// later: WasapiSink, CoreAudioSink, AlsaSink, PipeWireSink, JackSink, AsioSink
```

One offline rendering path — Null and WAV consume the same generated buffers:

```text
Signal or prepared processor
            ↓
       Rendered buffer (owned by renderer, loaned to sink)
            ↓
   Null sink / WAV sink  (+ test-only memory sink for sample inspection;
                          memory sink is tooling, not a public subsystem)
```

Platform code stays at the boundary. Core (`math/units/time/signal`)
has no `#include` of any sink/backend header.

## NullSink (build first)

```cpp
NullSink sink;
sink.render(signal, duration);
```

Purpose: CI, benchmarks, determinism tests, sample counting,
NaN/Inf detection, clipping detection, profiling. Computes everything,
produces no audio, never touches threads/device clocks/drivers.

Must report: frames rendered, peak, NaN/Inf count, clipped-sample count.

## WavSink (minimal, ~100 lines, stdlib only)

Deterministic sink:

```text
Signal<double> → sample(t_n), t_n = n/fs → PCM conversion → WAV file
```

First call shape:

```cpp
renderWav(signal, "440hz.wav", SampleRateHz{48000}, TimeSeconds{1.0});
```

Fixed first-target format (golden artifact):

```text
mono · 48000 Hz · 16-bit PCM · 1.0 s · 48000 frames · 440 Hz sine · deterministic
```

Covers without involving realtime complexity:

```text
sampling, channel interleaving, PCM conversion, clipping,
file duration, time stepping, header/metadata correctness
```

No threads, no device clocks, no latency, no buffer underruns, no OS APIs.

## Implementation notes (explicit policies, all tested)

- Format fixed: **mono, signed 16-bit PCM, little-endian**. Verify header fields
  and data length independently (block alignment + byte rate per PCM spec).
- Rounding: document float→int16 rounding (e.g. round-half-away, symmetric);
  endpoint: N frames cover `t_0 … t_0+(N-1)/fs`; overload: clamp policy declared
  and counted, certification renders fail on unexpected overload; file size
  derived from `FrameCount × channels × bytesPerSample`.
- Sample loop derives time from integer index: `t = t_0 + n/fs`, never accumulated `t += dt`.
- Duration → frame count: documented rounding (round-to-nearest, ties specified);
  assert `N == 48000` for the golden case. Max render length declared + tested.
- Certification renders **fail** on non-finite values, unexpected overload, or any
  file-write failure (no success message on partial file; no silent defaults).
- Encoder tests use known sample-value fixtures. Writer+reader sharing one
  assumption is a known risk: verify with an **independent decoder** in dev tooling
  (external tool allowed — zero-dep rule covers shipped core, not verification tools).
- Block-size invariance (early acceptance test): same input rendered in blocks of
  64, 127, 512 frames incl. final partial block → identical sample sequence within
  declared tolerance. Catches phase resets / state bugs that hide at one buffer size.
- Stereo, float32, higher rates: explicitly post-MVP.
