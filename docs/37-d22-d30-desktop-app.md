# D22, D29, D30 — The Desktop App (complete)

Status: 368/368 test cases (640,468 assertions) on MSVC Release+Debug, UCRT64 GCC Release+Debug, GCC-13 Release+Debug, Clang-18 Release+Debug, GCC ASan+UBSan (zero reports; test binary plus all 20 examples clean), Clang-18 TSan (zero reports) · Probes: 10/10 · App: builds with no warnings on MSVC /W4 /WX and
UCRT64 GCC -Wall -Wextra -Werror, and on Linux (WSL, GCC 13, Wayland via
WSLg); verified by window captures on Windows and Linux (below).

A cross-platform desktop application (SDL3 + Dear ImGui) for WAV files,
with playback, and for a microphone. It is an **optional build**
(`-DMMATHEN_BUILD_APP=ON`, off by default): the core library stays header-only
with zero dependencies, and the tests and CI never fetch SDL or ImGui.
Every number the app draws or plays comes from the tested library headers.

## New code

| File | Contents |
|---|---|
| `sink/wav_reader.h` (D22) | `WavData`, `parseWav`, `readWavFile`: PCM 16/24/32, float32, EXTENSIBLE, chunk skipping; exact inverse of `WavSink`'s scaling; every error path typed |
| `core/spsc_ring.h` (D22) | lock-free single-producer/single-consumer ring (audio → UI samples) |
| `analysis/analyzer.h` (D22) | `BandAnalyzer`: streaming band levels with meter ballistics (instant attack, linear release, peak hold) |
| `core/triple_buffer.h` (D29) | lock-free newest-value hand-off (UI → audio settings), never torn |
| `app/engine.h` (D30) | the audio engine and its threading rules |
| `app/spectrum_app.cpp` (D30) | the UI; capture options for verification |
| `CMakeLists.txt` | `MMATHEN_BUILD_APP`: pinned SDL 3.4.16 (`fa2c02b…`) and Dear ImGui v1.92.9b (`f1cc2ae…`) via FetchContent; `SDL_X11_XTEST` off (unused, and SDL's configure otherwise fails on Linux hosts without libxtst-dev) |

Evidence: `[d22core]` (7 cases), with a byte-exact WAV round trip, every
accepted encoding and error path, the two-thread ring stress test, exact
release slopes and block-size invariance; `[d29tb]` (2 cases), where a
writer publishes 200,000 64-word structs, the reader sees no torn value,
never goes backwards, and always receives the newest (three runs clean).

## Engine (`app/engine.h`)

Chain, for both file and microphone:
EQ (`BiquadCascade`, up to 32 sections) → compressor → volume → limiter →
clip count → output + analyser.

- **Realtime rules**: the callbacks don't allocate, don't take locks of
  their own, and don't do I/O. The whole settings snapshot arrives through
  the triple buffer and is applied allocation-free (`setSections`,
  `setParams`), keeping filter and detector state.
- **No clicks**: seeks and loop seams go through `SpliceReader`; play and
  pause ramp over 5 ms; EQ, compressor and limiter on/off switches
  crossfade over 20 ms. The limiter always runs, and its "off" path is the
  input delayed by the same look-ahead, so latency never jumps.
- Structural changes (load, source rate, reader rebuild) happen with
  `SDL_LockAudioStream` held, which SDL also holds around the callback.

## UI (`app/spectrum_app.cpp`)

- **Transport**: open WAV (dialog or drag-drop), a physics demo, play,
  pause, stop, loop, A–B loop markers, Space to toggle play.
- **Views**: a clickable whole-track spectrogram (click seeks, with a
  crossfade), 1/3-octave or octave bars with peak hold, a spectrum line
  with the exact EQ curve, a live scrolling spectrogram, level meters with
  a clip light, hover readouts (Hz, note ± cents, dBFS), and SVG export of
  the track spectrogram.
- **EQ tab**: graphic (10 octave bands, presets, *exact* solved gains);
  parametric (8 bands: peak, shelves, 12–48 dB/oct Butterworth cuts,
  band-pass, notch; draggable nodes, mouse wheel for Q); match (reference
  WAV at any rate, amount, relative floor; plot of wanted vs predicted
  change). Auto headroom subtracts the exact response maximum.
- **Dynamics tab**: compressor controls, gain-reduction bar, static curve;
  limiter controls, true-peak mode (D31), gain-reduction bar.
- **Export**: processed 24-bit stereo WAV through the playback engine's
  own code, loudness and true peak reported (D32).
- **Pitch/Time tab**: PSOLA or phase vocoder, ±12 semitones, 50–200%
  tempo, rendered on a worker thread; Revert.
- **Settings tab**: crossfade length and curve; analyzer resolution and
  floor.
- **Tuner**: YIN on the newest 4096 samples, 15 times a second.
- **Loudness** (D31): BS.1770 momentary, short-term, integrated, LRA and
  true peak of the output, with a reset.

## Verification by capture

`mmathen_app --capture out.png [...]` runs a scenario, waits for any
background job, then saves the window and a statistics file. Measured on
MSVC Release, on the 4 s demo (three plucked strings):

| Scenario | Result |
|---|---|
| Graphic EQ, "Smile" | 10 sections, max boost +6.02 dB, auto headroom applied, 0 clips |
| Parametric example | 6 sections (a 24 dB/oct cut is 2), the dip node sits on the −6 dB curve |
| Compressor −30 dB, 6:1, with a seek | GR −4.9 dB, limiter GR −0.1 dB, 0 clips, seek crossfaded |
| PSOLA +4 semitones | worker render, track replaced, 0 clips |
| Match to `eq.wav` | 29 sections, predicted error reported (4.9 dB: one note matched to three) |
| Settings | tab renders; reader rebuild path |
| Microphone | "Access is denied" from the Windows privacy setting, shown in the status bar; the app keeps running |
| Linux (WSL, GCC 13) | same scenario as the first row: 133 analysis frames, 0 clips, loudness −26.0 LUFS integrated; the UI renders identically |
| Loudness readout (D31) | M/S/I/LRA/TP on the output; S reads "--" until 3 s have played |

## Later additions

D33–D38 added the Repair tab, de-esser, multiband, the ACGE graphic solver,
mid/side EQ targets, PGHI/hybrid pitch/time, a pYIN tuner, and linear-phase
and normalized export. See `39-d33-d38-repair-pitch-tsm-multiband.md`.

## Limits

- The microphone path is built and wired, but on this machine Windows
  denied access, so it couldn't be exercised.
- ThreadSanitizer (added after this record's first draft, option
  `MMATHEN_SANITIZE_THREAD`, CI job `tsan`) runs the whole suite clean,
  including both two-thread stress tests; a deliberate race was confirmed
  to be reported, so the clean result is meaningful. It covers the library
  primitives. The app's SDL callbacks aren't run under TSan (no audio
  device in CI).
- The match EQ is only as good as the material: tonal references with
  different notes produce large, honestly reported prediction errors.
