# D39–D43 — MFCC, GMM, HMM/Viterbi, Markov Chains and n-grams, Chord Recognition (complete)

Initial certification: 424/424 test cases (660,121 assertions) on MSVC Release+Debug, UCRT64 GCC Release+Debug, GCC-13 Release+Debug, Clang-18 Release+Debug, GCC ASan+UBSan (zero reports; test binary plus all 20 examples clean), Clang-18 TSan (zero reports)

Review follow-up (2026-09-19): six defects fixed, with six new regression
cases (`[review-regression]`, 179 assertions). The full MSVC Release and
Clang-18 Release suites each pass 430 cases / 660,300 assertions. The new cases also pass under GCC
ASan+UBSan with zero reports. These follow-up checks are separate from the
initial ten-configuration certification above.

The fixes preserve the public function signatures:
- HMM trainers validate model dimensions and probability rows before
  indexing them. The shared decoding boundary also validates the HMM.
  Empty/malformed models return `InvalidArgument`; zero-probability
  transitions and unreachable states remain valid.
- `deltas` and `cepstralMeanNormalize` throw `std::invalid_argument` for
  inconsistent frame dimensions. CMN validates before modifying any data.
- Chord analysis rejects nonfinite audio, including unused trailing
  samples, instead of reporting it as silence.
- The chord gate uses window energy for summed spectral power:
  the full-scale sine reference is `N * sum(w*w) / 4`. The previous
  peak-bin reference overestimated a Hann-windowed sine's power by
  `10*log10(1.5)` (about 1.76 dB). Tests cover both sides of the gate,
  two FFT sizes, and bin-centered and off-bin tones. See the normalization
  distinction in [SciPy's spectral documentation](https://docs.scipy.org/doc/scipy/reference/generated/scipy.signal.csd.html).
- MFCC options reject nonfinite lifter/log-floor/frequency limits and
  invalid negative values. Zero lifter and zero maximum-frequency default
  remain supported.
- Markov fitting rejects nonfinite pseudocounts with `InvalidArgument`
  and overflowing finite row totals with `RangeError`.

These are the pieces of the classical statistical pipeline. Speech
recognition ran on it from the 1980s until about 2012 (HTK, Sphinx,
Kaldi): FFT → MFCC → GMM emissions → HMM → Viterbi, with an n-gram
language model. Music information retrieval uses the same tools: MFCCs
for timbre, HMMs for chords and score following, and n-grams or Markov
chains for progressions and melodies. Everything here is header-only with
no dependencies. It is checked against closed forms, published examples,
brute-force enumeration, or a source whose true statistics are known
exactly.

## New code

| Header | Contents |
|---|---|
| `analysis/mfcc.h` (D39) | `hzToMel`/`melToHz` (HTK and Slaney), `melFilterbank` (HTK or Slaney-area normalisation), orthonormal `dctII`/`dctIII`, `mfcc` (pre-emphasis, Hamming/Hann, power or magnitude, log, DCT, HTK lifter), `deltas`, `cepstralMeanNormalize` |
| `stats/random.h` (D40) | `SplitMix64` (uniform, Box–Muller normal, categorical), `logSumExp`. The std:: distributions differ between standard libraries, so pinned samples need their own. |
| `stats/gmm.h` (D40) | diagonal `Gmm` (log-likelihood, posteriors, sampling), `trainGmm` (k-means++ seeding, Lloyd, EM, variance floor, per-iteration log-likelihood), `gmmBic` |
| `stats/hmm.h` (D41) | `Hmm`, `forwardLogLikelihood`, `forwardBackward`, `viterbi`, `pathLogProbability`; `DiscreteHmm` with sampling and `baumWelchDiscrete`; `GmmHmm` with `baumWelchGmm` |
| `stats/markov.h` (D42) | `MarkovChain`: stationary law (direct solve), n-step law, irreducibility, period, entropy rate, sampling, fitting |
| `stats/ngram.h` (D42) | `Vocabulary`; `NgramModel` of any order with MLE, add-k or interpolated modified Kneser–Ney; perplexity; sampling |
| `analysis/chords.h` (D43) | `chromagram`, `chordScores`, `recognizeChords` (frame-wise and HMM/Viterbi, optional chord bigram), `chordSegments` |

## D39 — MFCC

The pipeline follows Davis & Mermelstein (1980) and the HTK Book, ch. 5.
The conventions of HTK and librosa can be selected.

Evidence (`[d39mfcc]`, 5 cases):
- **Mel scales.** HTK mel(1000 Hz) = 999.98553. Slaney mel(1000) = 15 and
  mel(6400) = 42 exactly. Both invert to roundoff.
- **Filterbank.** Unnormalised filters sum to 1 (to 1e-12) between the
  first and last centres. Slaney-normalised filters have unit area (1e-3
  on a fine grid).
- **DCT.** The orthonormal DCT is exactly invertible, satisfies Parseval,
  and maps a constant to c0 = value·√M alone.
- **Against an independent pipeline.** The MFCCs match a brute-force
  pipeline written separately in the test (explicit O(N²) DFT, the filter
  formula, and the DCT sum) within 1e-8.
- **Gain.** A gain g changes c0 alone, by exactly 2 ln(g)·√M (1e-9); every
  other coefficient is unchanged. Deltas of a linear ramp equal its slope.
  CMN zeroes the mean. A 1 kHz tone peaks in the filter whose centre is
  nearest.

## D40 — Gaussian mixture models

EM follows Dempster, Laird & Rubin (1977) and Bishop's PRML, sec. 9.2,
seeded with k-means++. It is deterministic for a given seed on every
compiler.

Evidence (`[d40gmm]`, 2 cases):
- K = 1 equals the closed-form MLE (1e-12).
- A 2-component 2-D mixture is recovered from 20,000 samples: weights and
  means within 4 standard errors, variances within 5.
- EM's log-likelihood never decreases.
- BIC picks K = 2 over K = 1 and K = 3.
- Posteriors sum to 1, and a point at 10⁶ still has a finite
  log-likelihood.
- The same seed gives a bit-identical model.

The variance floor is a constraint EM doesn't know about, so it can break
the monotone guarantee. The fit reports whether the floor ever applied.

## D41 — HMM: forward-backward, Viterbi, Baum-Welch

The implementation follows Rabiner (1989), in the log domain throughout.
That means no scaling factors, no underflow, and forbidden transitions are
exact. Emissions are a matrix of log-likelihoods, so one decoder serves
discrete symbols, GMMs, chord templates, or pYIN candidates.

Evidence (`[d41hmm]`, 5 cases):
- **Published example.** The Wikipedia "Healthy/Fever" example decodes to
  H H F with probability **0.01512**, the published value, to 1e-12. Its
  forward probability equals the sum over all 8 paths.
- **Brute force.** On 5 random models the forward likelihood, every
  posterior, and the Viterbi path and score equal enumeration over all
  3⁷ = 2,187 paths (1e-12).
- **Long sequences.** A 200,000-step sequence gives log P ≈ −10⁵ with no
  underflow; forward and forward-backward agree.
- **Forbidden transitions.** A left-to-right model never decodes backwards.
  An impossible sequence returns DomainError, not NaN.
- **Baum-Welch (discrete).** The log-likelihood never decreases. It ends
  at or above the true model's likelihood, and recovers every transition
  and emission probability within 0.04 (60 × 500 samples).
- **Baum-Welch (GMM-HMM)** (Juang 1985): monotone. It decodes the hidden
  states of 20 × 300 samples at over 97 % and recovers the 0.95
  self-transition within 0.03.

## D42 — Markov chains and n-gram language models

**Markov chains** (`[d42markov]`, 2 cases):
- The two-state chain's stationary law, n-step law and entropy rate equal
  their closed forms (1e-14).
- Periods 1, 2 and 3 are classified correctly.
- Two closed classes (no unique stationary law) are refused. A transient
  state gets exactly 0.
- Sampling agrees within 5σ, with the variance corrected for
  autocorrelation. Fitting recovers P.

**n-grams.** The model covers MLE, add-k and interpolated modified
Kneser–Ney (Chen & Goodman 1998). Lower orders use continuation counts
(KenLM's rule for n-grams starting with <s>), with per-order discounts
taken from the counts of counts.

**Found while testing.** On small, non-Zipfian data the modified-KN
discount formulas give D₂ ≤ 0. A zero discount leaves unseen words no
probability at all: the first run gave infinite perplexity. KenLM refuses
such data. Here, that order falls back to the original single-discount KN
(Ney, Essen & Kneser 1994, D = n₁/(n₁+2n₂)). So every D is in (0, i], and
unseen words always keep positive probability.

Evidence (`[d42ngram]`, 3 cases):
- Every conditional distribution sums to 1 (1e-12) for all three
  smoothings and orders 1–3, over seen and unseen contexts.
- MLE and add-k equal their formulas. A huge k gives perplexity V − 1.
- KN equals a hand calculation on a 3-sentence corpus, with exact
  fractions written in the test: P(c|a) = 2/9 + 23/147, and so on.
- **Accuracy against known truth.** A second-order source over 6 symbols
  has an entropy rate computed exactly from the 36-state pair chain's
  stationary law: 1.271206 nats. Trained on 200,000 tokens, the KN trigram
  scores held-out cross-entropy 1.274950. The source's own probabilities
  score 1.274080 on the same held-out data, so the trigram is **within
  0.068 %** of the truth. A bigram scores 1.655792 (+29.9 %), because it
  can't represent a second-order source.
- With little data (1,000 tokens), MLE gives infinite perplexity and KN
  beats add-one (3.92 vs 4.07).

## D43 — Chord recognition (all of it together)

The pipeline is FFT → chroma (Fujishima 1999; log compression as in
Müller's FMP) → cosine scores against 24 triad templates plus N → HMM
(Sheh & Ellis 2003) whose chord changes can come from an n-gram chord
model → Viterbi. The template scores are used as log-emissions scaled by a
sharpness factor. That's standard for template recognisers, but it is not
a fitted density.

**What testing changed.** The first run was only 67.8 % correct frame by
frame. 307 of the 407 errors were the flat "no chord" template winning:
log compression flattens a busy chroma. Two changes fixed it. N is now
penalised by 0.2 on frames above the energy gate, and silence still goes
to N. The sharpness is 10.

These settings were chosen on one piece and then tested on **four
held-out pieces** in other keys. Each piece has 40 chords, plucked
harmonic triads with bass, a melody of which half the notes are passing
tones, noise at −30 dB, and drum bursts on every beat:

| Held-out piece (key) | Frame by frame | HMM | HMM + chord bigram | Segments: frame / HMM (truth 40) |
|---|---|---|---|---|
| G | 85.9 % | 93.5 % | 94.9 % | 127 / 46 |
| C | 88.3 % | 95.6 % | 96.2 % | 131 / 44 |
| F | 89.1 % | 95.6 % | 95.4 % | 118 / 43 |
| A♯ | 82.5 % | 91.2 % | 92.9 % | 158 / 50 |

- **The HMM gains 6.5–8.7 points on every piece.** It also cuts the
  segment count from 3–4× the truth to within 25 % of it.
- **The chord bigram helps less.** It is an `NgramModel` trained on 500
  progressions in all keys. It helps on three pieces, costs 0.25 points on
  one, and gains 0.9 points on average.
- **Most remaining errors are at chord changes.** On frames whose
  186 ms window lies entirely inside one chord, the HMM scores
  94.2–98.5 %.

Evidence (`[d43chords]`, 2 cases): clean triads are right on every frame,
silence is N, and names and pitch classes are correct; the held-out table
above.

## What this means for speech recognition, honestly

Every statistical component of a classical recogniser is now here and
tested: MFCC with deltas and CMN, GMM-HMMs trained by Baum-Welch, Viterbi,
and a Kneser–Ney n-gram language model. It is still not a speech
recogniser. That would also need:

- **Data.** Hours of transcribed speech. Nothing here can learn without
  it.
- **A pronunciation dictionary.** Words must map to phones.
- **Context-dependent phone models.** Triphones with decision-tree state
  tying.
- **A decoder over the lexicon and language model together.** Token
  passing or a WFST.

Even complete, this family is well known to be substantially less
accurate than neural recognisers. That is why the field replaced GMMs
with DNNs (Hinton et al., IEEE Signal Processing Magazine 2012). So it
doesn't replace running Whisper's own weights for transcription.

It does fit well, dependency-free, for these jobs:
- isolated-word or command recognition from a few recorded examples per
  word;
- speaker or instrument identification (GMMs on MFCCs, the classic
  Reynolds approach);
- chords, keys, and score following in music;
- generative Markov/n-gram models of melody and harmony.

## Limits

- **MFCC speed.** The MFCC's DCT is O(M·C) per frame. That's fine for 26
  filters, but it isn't an FFT-based DCT.
- **Diagonal covariances only.** Full covariances aren't implemented.
- **No Viterbi training or beam pruning.** Baum-Welch is the only
  trainer, and Viterbi keeps the full N×T back-pointer table.
- **n-gram size.** n-grams use `std::map`, which is fine for thousands of
  types. It isn't meant for web-scale corpora, where KenLM-style hashed
  tries would be needed.
- **Chord vocabulary and data.** Chords are triads only (no sevenths or
  inversions), with equal-tempered chroma and fixed A4 tuning. The
  held-out evaluation uses synthesised pieces, not recordings. Accuracy
  on real recordings isn't measured here.
