# D27–D28 — Pitch Detection, Time Stretch and Pitch Shift (complete)

Status: 368/368 test cases (640,468 assertions) on MSVC Release+Debug, UCRT64 GCC Release+Debug, GCC-13 Release+Debug, Clang-18 Release+Debug, GCC ASan+UBSan (zero reports; test binary plus all 20 examples clean), Clang-18 TSan (zero reports) · Probes: 10/10

D27 adds time-domain pitch detection (autocorrelation and YIN). D28 adds
time-scale and pitch modification, in both the frequency domain (phase
vocoder) and the time domain (TD-PSOLA). PSOLA uses D27's YIN for its
pitch marks; the vocoder's shift uses D23's Butterworth as its
anti-alias filter.

## New code

| Header | Contents |
|---|---|
| `analysis/pitch.h` (D27) | `PitchEstimate`, `PitchSearch`; `autocorrelationPitch` (normalised ACF); `yinPitch` (YIN steps 1–5); `trackPitch` |
| `dsp/time_pitch.h` (D28) | `phaseVocoderStretch` (identity phase locking optional); `phaseVocoderPitchShift`; `psola` (pitch ratio and time ratio) |

## D27 — pitch detection

- **NACF**: r(τ) normalised by both windows' energies, over an equal
  number of products at every lag. It takes the **smallest** lag whose
  local maximum is within 90% of the global maximum, which avoids octave
  errors.
- **YIN** (de Cheveigné & Kawahara 2002): difference function, cumulative
  mean normalisation, absolute threshold 0.1, then down to the local
  minimum. Parabolic refinement uses the **raw** difference function d:
  measured on a 1760 Hz sine, refining on d′ is off by 5.6e-4 and on d by
  2.7e-5. Step 6 (best local estimate) is not implemented.

Evidence (`[d27pitch]`, 5 cases): sines from 55 to 1760 Hz within 1e-4
(measured ≤ 2.7e-5); a strong 2nd harmonic and a missing fundamental both
read as the fundamental; the D10 plucked stiff string reads f0·√(1 + B)
within 0.2%; noise is unvoiced; the tracker follows 220 → 330 Hz; invalid
searches are refused.

## D28 — phase vocoder and PSOLA

- **Vocoder**: per-bin heterodyned phase advance, instantaneous
  frequency, synthesis phase ψ += ω·Hs, using the **actual** integer
  analysis hop, so rounding never becomes phase error. Identity phase
  locking (Laroche & Dolson 1999) locks each bin to its nearest peak. The
  output is divided by the exact sum of squared windows. At stretch 1 it
  reconstructs its input.
- **Pitch shift** = stretch by p, then Catmull-Rom resampling by p.
  For p > 1 an 8th-order Butterworth at 0.45·fs/p is applied first. It
  moves formants: the "chipmunk" effect.
- **TD-PSOLA** (Moulines & Charpentier 1990): YIN track (pYIN since
  D35, which removes octave errors in the marks), pitch marks
  aligned to waveform peaks, synthesis marks every T/pitchRatio, Hann
  grains of 2T scaled by spacing/T. It keeps each period's waveform, so
  **formants stay put**. Unvoiced regions use a 10 ms pseudo-period and
  are not shifted. It is monophonic only (not for chords or mixes).

Evidence (`[d28tp]`, 5 cases): stretch 1 reconstructs to 1.9e-13 with and
without locking; stretches 0.75/1.5/2 keep 440 Hz within 1.2e-6 Hz and the
level within 0.0003 dB, at exact lengths; +7 and −7 semitone shifts are
exact to 2e-8; on a vowel-like tone (200 Hz pulses through a fixed 1 kHz
resonance) PSOLA ×1.25 reads 250 Hz (YIN, 3e-7) with the strongest
harmonic **still at 1000 Hz**, while the vocoder's shift moves it to
**1250 Hz**; the PSOLA level changes by 0.37 dB; PSOLA down a minor
third and a 2× stretch keep their targets within 1e-4.

## Limits

- Both D28 methods are offline (whole-signal) functions. The desktop app
  runs them on a worker thread.
- PSOLA on polyphonic material (the app's three-note demo, for example)
  runs, but it follows only the dominant period.
- `detail::fftInPlace` computes its trig per butterfly: accurate but slow.
  A 3-minute stereo vocoder render takes seconds, not milliseconds.
