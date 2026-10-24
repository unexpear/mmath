// mmathEn desktop app (D22, extended in D30): spectrum analyzer, visualizer,
// equalizers, dynamics, smooth cuts, pitch tools — for WAV files (with
// playback) and a microphone.
//
// Optional build (MMATHEN_BUILD_APP): SDL3 for the window, audio devices and
// file dialogs; Dear ImGui for the interface. The built-in processing uses
// the header-only core and our local guitar recognition model:
//   analysis: BandAnalyzer (bars), Spectrum/Spectrogram (line, heat maps),
//             yinPitch (tuner)
//   dsp:      graphic / parametric / match EQ designs (equalizer.h,
//             filters.h, match_eq.h) run as a BiquadCascade; Compressor and
//             Limiter (dynamics.h); SpliceReader crossfades (crossfade.h);
//             phase vocoder and PSOLA (time_pitch.h)
//   core:     SpscRing (audio -> UI samples), TripleBuffer (UI -> audio
//             settings); plot/svg.h for the heat-map colours and SVG export.
// The audio engine and its threading rules are in engine.h.
//
// Command line (the capture options exist to verify the running app):
//   mmathen_app [file.wav]
//   mmathen_app --capture out.png [--seconds S] [--mute] [--mic]
//               [--tab eq|dyn|fx|set] [--eqmode graphic|parametric|match]
//               [--preset N] [--comp] [--seek SEC] [--match ref.wav]
//               [--shift SEMITONES] [--tempo PERCENT] [--method psola|pv]
//               [--export out.wav] [--flat] [--truepeak] [--normalize]
//               [--linear] [--method psola|pv|pghi|hybrid] [--tab repair]
//               [file.wav]
//   mmathen_app --reference-demo reference-suite.tsv
//   mmathen_app --builder [instrument-key] [--builder-play]   (step 23: opens
//               the Player builder, e.g. --builder cello; --builder-play
//               also renders and plays the phrase)
//               [--players file.txt] [--builder-load [INSTRUMENT:]NAME]
//               (another saved-players file; load a saved player, e.g.
//               violin:Warm, or by name alone when only one has it)
//   saves a PNG of the window after S seconds (after any background job has
//   finished), plus out.png.txt with run statistics, and exits.

#include "engine.h"
#include "reference_demo.h"
#include "precision_tuner.h"
#include "tuner_panel.h"
#include "audio_output.h"
#include "desktop_style.h"
#include "player_builder_panel.h"

#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <mmathen/analysis/analyzer.h>
#include <mmathen/analysis/bs1770.h>
#include <mmathen/analysis/pitch.h>
#include <mmathen/analysis/pyin.h>
#include <mmathen/analysis/spectrum.h>
#include <mmathen/dsp/acge.h>
#include <mmathen/dsp/denoise.h>
#include <mmathen/dsp/equalizer.h>
#include <mmathen/dsp/loudness_normalize.h>
#include <mmathen/dsp/match_eq.h>
#include <mmathen/dsp/time_pitch.h>
#include <mmathen/dsp/tsm.h>
#include <mmathen/music/pitch.h>
#include <mmathen/physics/wave1d.h>
#include <mmathen/plot/svg.h>
#include <mmathen/sink/wav_reader.h>
#include <mmathen/sink/wav_writer.h>

namespace {

using namespace mmathen;
using app::Engine;
using app::EngineParams;
using app::kFile;
using app::kMic;
using app::kNone;

constexpr float kEqRangeDb = 12.0f;

// ------------------------------------------------------------- helpers --

std::vector<FrequencyBand> octaveBandsFor(double rate) {
  auto b = fractionalOctaveBands(1, FrequencyHz{22.0},
                                 FrequencyHz{std::min(17000.0, 0.44 * rate)});
  return b.ok() ? std::move(b).value() : std::vector<FrequencyBand>{};
}

std::vector<FrequencyBand> thirdBandsFor(double rate) {
  auto b = fractionalOctaveBands(3, FrequencyHz{24.0},
                                 FrequencyHz{std::min(16500.0, 0.44 * rate)});
  if (!b.ok()) return {};
  auto v = std::move(b).value();
  if (v.size() > app::kMaxSections)
    v.erase(v.begin() + static_cast<std::ptrdiff_t>(app::kMaxSections), v.end());
  return v;
}

const char* errorText(QuantityError e) {
  switch (e) {
    case QuantityError::InvalidArgument:
      return "not a readable WAV file (or damaged)";
    case QuantityError::DomainError:
      return "unsupported WAV encoding (use 16/24/32-bit PCM or 32-bit float)";
    case QuantityError::RangeError:
      return "cannot open the file, or its data is truncated";
    default:
      return "unknown error";
  }
}

// IEC nominal label (R10 preferred numbers) for an exact frequency.
std::string bandLabel(double f) {
  static const double r10[] = {1.0, 1.25, 1.6, 2.0, 2.5, 3.15, 4.0, 5.0, 6.3, 8.0, 10.0};
  const double dec = std::pow(10.0, std::floor(std::log10(f)));
  double best = r10[0];
  for (double v : r10)
    if (std::fabs(std::log(f / (v * dec))) < std::fabs(std::log(f / (best * dec))))
      best = v;
  const double nominal = best * dec;
  char buf[32];
  if (nominal >= 1000.0)
    std::snprintf(buf, sizeof buf, "%gk", nominal / 1000.0);
  else
    std::snprintf(buf, sizeof buf, "%g", nominal);
  return buf;
}

std::string hzText(double f) {
  char buf[32];
  if (f >= 1000.0)
    std::snprintf(buf, sizeof buf, "%.2f kHz", f / 1000.0);
  else
    std::snprintf(buf, sizeof buf, "%.1f Hz", f);
  return buf;
}

// Nearest 12-TET note (A4 = 440 Hz) and the offset in cents.
std::string noteText(double f) {
  if (!(f > 8.0)) return "";
  const double midi = 69.0 + 12.0 * std::log2(f / 440.0);
  const int m = static_cast<int>(std::lround(midi));
  auto lab = midiToLabel(std::clamp(m, 0, 127));
  if (!lab.ok()) return "";
  char buf[48];
  std::snprintf(buf, sizeof buf, "%s %+.0f c", lab.value().c_str(),
                (midi - static_cast<double>(m)) * 100.0);
  return buf;
}

std::size_t pow2Ceil(double x) {
  std::size_t p = 16;
  while (static_cast<double>(p) < x && p < (std::size_t{1} << 16)) p *= 2;
  return p;
}

ImU32 heat(double t, int alpha = 255) {
  const auto c = heatColorRgb(t);
  return IM_COL32(c[0], c[1], c[2], alpha);
}

std::vector<double> monoOf(const std::vector<float>& st, std::size_t frames) {
  std::vector<double> m(frames);
  for (std::size_t i = 0; i < frames; ++i)
    m[i] = 0.5 * (static_cast<double>(st[2 * i]) + static_cast<double>(st[2 * i + 1]));
  return m;
}

// Three plucked stiff, lossy strings (D8-D10 physics, 40 exact modes each):
// root, fifth and octave entering one second apart, normalised to -6 dBFS.
// Loss values are illustrative, as in spectrum_demo.
WavData makeDemo() {
  constexpr double fs = 48000.0;
  const std::size_t frames = static_cast<std::size_t>(4.0 * fs);
  WavData w;
  w.rate = SampleRateHz{fs};
  w.channels = 1;
  w.bitsPerSample = 32;
  w.isFloat = true;
  w.frames = frames;
  w.samples.assign(frames, 0.0);
  const double ratios[3] = {1.0, 1.5, 2.0};
  for (int k = 0; k < 3; ++k) {
    const double tension = 97.344 * ratios[k] * ratios[k];
    auto base = idealString(DistanceMeters{0.65}, TensionNewtons{tension},
                            LinearDensityKgPerM{0.001});
    if (!base.ok()) return {};
    auto kappa = stiffnessFromB(base.value(), 4e-4);
    auto loss = lossFromT60(base.value().speed, FrequencyHz{240.0 * ratios[k]},
                            DurationSeconds{2.5}, FrequencyHz{2400.0},
                            DurationSeconds{0.4});
    if (!kappa.ok() || !loss.ok()) return {};
    auto stiff = withStiffness(base.value(), kappa.value());
    if (!stiff.ok()) return {};
    auto sys = withLoss(stiff.value(), loss.value());
    if (!sys.ok()) return {};
    auto pluck = InitialShape::pluck(sys.value(), DistanceMeters{0.1234}, 0.002);
    if (!pluck.ok()) return {};
    auto modal = ModalSolution::create(pluck.value(), 40);
    if (!modal.ok()) return {};
    const std::size_t onset = static_cast<std::size_t>(k) * 48000;
    for (std::size_t i = onset; i < frames; ++i) {
      auto y = modal.value().at(DistanceMeters{0.0845},
                                TimePointSeconds{static_cast<double>(i - onset) / fs});
      if (!y.ok()) return {};
      w.samples[i] += y.value();
    }
  }
  double peak = 0.0;
  for (double v : w.samples) peak = std::max(peak, std::fabs(v));
  if (!(peak > 0.0)) return {};
  for (double& v : w.samples) v *= 0.5 / peak;
  return w;
}

// ------------------------------------------------------------ EQ model --

enum class EqMode : int { Graphic = 0, Parametric = 1, Match = 2 };
enum class PType : int { Peak, LowShelf, HighShelf, LowCut, HighCut, BandPass, Notch };
const char* const kPTypeNames[] = {"Peak",     "Low shelf", "High shelf", "Low cut",
                                   "High cut", "Band-pass", "Notch"};

struct PBand {
  bool on = false;
  PType type = PType::Peak;
  float freq = 1000.0f;
  float gain = 0.0f;
  float q = 1.0f;
  int slope = 1;  // 0..3 -> 12/24/36/48 dB/oct (Butterworth order 2/4/6/8)
};

// Delivery loudness presets (D33). Values are the services' published
// normalization targets (AES TD1008) and the broadcast standards.
struct ExportTarget {
  const char* name;
  double lufs;
};
constexpr ExportTarget kExportTargets[] = {
    {"Streaming: -14 LUFS (Spotify, YouTube, Tidal, Amazon)", -14.0},
    {"Apple Music: -16 LUFS", -16.0},
    {"Broadcast EBU R128: -23 LUFS", -23.0},
    {"Broadcast ATSC A/85: -24 LKFS", -24.0}};

bool hasGain(PType t) {
  return t == PType::Peak || t == PType::LowShelf || t == PType::HighShelf;
}
bool hasSlope(PType t) { return t == PType::LowCut || t == PType::HighCut; }

// ------------------------------------------------------------ the app --

struct Job {
  enum Kind { None, Transform, MatchAnalysis, Export, Repair } kind = None;
  std::thread thread;
  std::atomic<bool> done{false};
  // Results (written by the worker before `done`, read after it).
  std::vector<float> track;
  std::size_t frames = 0;
  double rate = 0.0;
  Spectrum sourceLtas;
  std::vector<BandLevel> referenceLevels;
  std::string message;
  bool ok = false;
};

struct App {
  SDL_Window* window = nullptr;
  SDL_Renderer* renderer = nullptr;
  std::unique_ptr<Engine> engine = std::make_unique<Engine>();
  SDL_AudioStream* play = nullptr;
  SDL_AudioStream* mic = nullptr;
  double rate = 48000.0;
  double fileRate = 48000.0;
  app::AudioOutput output;

  // Source.
  std::string trackName;
  double trackSeconds = 0.0;
  std::vector<float> original;  // before any transform
  std::size_t originalFrames = 0;
  double originalRate = 48000.0;
  std::string originalName;
  bool loop = true;
  long long markA = -1, markB = -1;
  app::ReferenceDemo referenceDemo;
  bool referenceView = false;
  int referenceKind = 0; // chords, isolated guitar pitch, speech

  // EQ.
  EqMode eqMode = EqMode::Graphic;
  bool eqOn = true;
  bool autoHeadroom = true;
  float volumeDb = 0.0f;
  std::array<float, 16> gains{};
  int graphicSolver = 0;  // 0 ACGE (D38), 1 solved centres (D25), 2 plain (D21)
  int eqTarget = 0;       // 0 stereo, 1 mid, 2 side (D37)
  // D37 dynamics additions.
  DynamicEqBand deess{};
  bool deessOn = false;
  MultibandParams mb{};
  bool mbOn = false;
  float mbLowHz = 200.0f, mbHighHz = 2500.0f;
  // D34 repair.
  int denoiseMethod = 2;  // MMSE-LSA
  float denoiseReduction = 15.0f;
  std::optional<NoiseProfile> noiseProfile;
  std::vector<float> undoTrack;
  std::size_t undoFrames = 0;
  bool selecting = false, haveBox = false;
  double boxT0 = 0, boxT1 = 0, boxF0 = 0, boxF1 = 0, dragT = 0, dragF = 0;
  float boxGainDb = -20.0f;
  bool exportLinearPhase = false;
  app::PrecisionTuner streamingTuner;
  app::tuner::Panel tunerPanel;
  bool tunerView = false;
  // Step 23: the Player builder view and its loop setting to restore.
  app::builder::Panel builder;
  bool builderView = false;
  bool loopBeforeBuilder = true;
  int preset = 0;
  bool applyPreset = false;
  std::array<PBand, 8> pbands{};
  int dragBand = -1;
  std::string refName;
  Spectrum sourceLtas;
  std::vector<BandLevel> refLevels;
  std::optional<MatchEqDesign> match;
  float matchAmount = 1.0f;
  float matchRelFloor = 50.0f;
  std::string lastExport;
  bool exportNormalize = false;  // export = what you hear, unless asked
  int exportTarget = 0;
  float exportCeilingDb = -1.0f;
  std::vector<double> curveHz, curveDb;
  double eqMaxDb = 0.0;
  std::string eqNote;

  // Dynamics.
  bool compOn = false;
  CompressorParams comp{};
  bool autoMakeup = true;
  bool limOn = true;
  LimiterParams lim{-1.0, app::kLimiterLookaheadMs, 80.0};
  bool paramsDirty = true;

  // Smooth cuts.
  float fadeMs = 20.0f;
  int fadeCurve = 2;  // EqualPower

  // Transform.
  float shiftSemis = 0.0f;
  float tempoPct = 100.0f;
  int method = 0;  // 0 PSOLA, 1 phase vocoder
  std::unique_ptr<Job> job;

  // Analysis.
  int bandsPerOctave = 3;
  std::optional<BandAnalyzer> analyzer;
  std::size_t frameSize = 8192, hop = 1024, sinceHop = 0;
  std::vector<double> drain = std::vector<double>(8192);          // mono mix
  std::vector<double> stereo = std::vector<double>(2 * 8192);     // from ring
  std::vector<double> monoTap = std::vector<double>(8192);
  std::optional<LoudnessMeter> loudness;  // BS.1770 on the output
  int loudnessChannels = 2;
  std::uint64_t framesAnalysed = 0;
  float dbFloor = -100.0f;
  double meterRmsDb = -120.0, meterPeakDb = -120.0;
  std::uint64_t lastClips = 0;
  double clipLightUntil = 0.0;
  std::vector<float> lineDb;
  std::vector<double> hist = std::vector<double>(app::precisionHistorySize(48000,app::PitchRange::Music), 0.0);
  std::size_t samplesSinceTune = 0;
  std::size_t histPos = 0;
  double nextTune = 0.0;
  PitchEstimate tune;
  double tuneLevelDb = -120.0;

  // Live spectrogram texture.
  static constexpr int kGramW = 512;
  static constexpr int kGramH = 200;
  SDL_Texture* gramTex = nullptr;
  std::vector<unsigned char> gramPixels =
      std::vector<unsigned char>(static_cast<std::size_t>(kGramW) * kGramH * 4);
  bool gramDirty = true;
  std::vector<std::size_t> gramLo, gramHi;

  // Whole-track overview.
  std::optional<Spectrogram> overview;
  SDL_Texture* overviewTex = nullptr;
  int overviewW = 0;
  static constexpr int kOverH = 96;

  std::string status = "Open a WAV file, drop one on the window, or load the demo.";

  // Dialog results (dialog callbacks may run on another thread).
  enum class Pending { None, Open, Reference, ExportSvg, ExportWav, ReferenceSuite };
  std::mutex pendingMu;
  std::vector<std::pair<Pending, std::string>> pending;

  std::vector<SDL_AudioDeviceID> micIds;
  std::vector<std::string> micNames;
  int micChoice = 0;
  int forceTab = -1;

  double now() const { return static_cast<double>(SDL_GetTicksNS()) * 1e-9; }
  Engine& e() { return *engine; }

  // ================================================================ EQ ==
  // Sections for the current EQ mode (also the exact response curve).
  std::vector<BiquadCoeffs> designSections() {
    std::vector<BiquadCoeffs> s;
    eqNote.clear();
    const SampleRateHz fs{rate};
    if (eqMode == EqMode::Graphic) {
      const auto bands = octaveBandsFor(rate);
      std::vector<double> g(bands.size());
      for (std::size_t i = 0; i < g.size(); ++i) g[i] = static_cast<double>(gains[i]);
      if (graphicSolver == 0 && bands.size() >= 2) {
        // D38: Välimäki-Liski accurate cascade GEQ (centres and midpoints).
        auto d = designAcge(bands, g, fs);
        if (d.ok()) {
          eqNote = "ACGE (Valimaki-Liski): within ~1 dB at centres and midpoints";
          return finishCurve(d.value().sections);
        }
      }
      if (graphicSolver == 1 && !bands.empty()) {
        auto sol = solveGraphicEqGains(bands, g, fs, 15.0);
        if (sol.ok()) {
          char buf[128];
          std::snprintf(buf, sizeof buf, "solved centres: residual %.3f dB (%d it.); may ripple between",
                        sol.value().residualDb, sol.value().iterations);
          eqNote = buf;
          g = sol.value().gainsDb;
        }
      }
      for (std::size_t i = 0; i < bands.size(); ++i) {
        auto k = designEqSection(
            EqBand{EqShape::Peaking, bands[i].center, g[i], bandEdgeQ(bands[i])}, fs);
        if (k.ok()) s.push_back(k.value());
      }
    } else if (eqMode == EqMode::Parametric) {
      for (const auto& b : pbands) {
        if (!b.on) continue;
        const double f = std::clamp(static_cast<double>(b.freq), 10.0, 0.45 * rate);
        if (hasSlope(b.type)) {
          auto c = designButterworth(b.type == PType::LowCut, 2 * (b.slope + 1),
                                     FrequencyHz{f}, fs);
          if (c.ok())
            for (const auto& k : c.value())
              if (s.size() < app::kMaxSections) s.push_back(k);
          continue;
        }
        EqShape sh = EqShape::Peaking;
        if (b.type == PType::LowShelf) sh = EqShape::LowShelf;
        if (b.type == PType::HighShelf) sh = EqShape::HighShelf;
        if (b.type == PType::BandPass) sh = EqShape::BandPass;
        if (b.type == PType::Notch) sh = EqShape::Notch;
        auto k = designEqSection(EqBand{sh, FrequencyHz{f}, static_cast<double>(b.gain),
                                        std::max(0.1, static_cast<double>(b.q))},
                                 fs);
        if (k.ok() && s.size() < app::kMaxSections) s.push_back(k.value());
      }
    } else if (match) {
      for (std::size_t i = 0; i < match->bands.size() && i < app::kMaxSections; ++i) {
        auto k = designEqSection(EqBand{EqShape::Peaking, match->bands[i].center,
                                        match->solution.gainsDb[i],
                                        bandEdgeQ(match->bands[i])},
                                 fs);
        if (k.ok()) s.push_back(k.value());
      }
      char buf[128];
      std::snprintf(buf, sizeof buf,
                    "%zu third-octave bands; predicted band error %.2f dB",
                    match->bands.size(), match->predictedErrorDb);
      eqNote = buf;
    }
    return finishCurve(std::move(s));
  }

  // Exact response curve of the sections and its maximum (auto headroom).
  std::vector<BiquadCoeffs> finishCurve(std::vector<BiquadCoeffs> s) {
    const SampleRateHz fs{rate};
    curveHz.clear();
    curveDb.clear();
    eqMaxDb = 0.0;
    const double lo = 20.0, hi = std::min(20000.0, 0.499 * rate);
    for (int k = 0; k < 480; ++k) {
      const double f = lo * std::pow(hi / lo, k / 479.0);
      auto db = cascadeResponseDb(s, FrequencyHz{f}, fs);
      if (!db.ok()) continue;
      curveHz.push_back(f);
      curveDb.push_back(std::max(db.value(), -60.0));
      eqMaxDb = std::max(eqMaxDb, db.value());
    }
    return s;
  }

  void redesignMatch() {
    match.reset();
    if (sourceLtas.power.empty() || refLevels.empty()) return;
    const auto bands = thirdBandsFor(rate);
    if (bands.size() != refLevels.size()) return;
    MatchEqOptions o;
    o.amount = static_cast<double>(matchAmount);
    o.relativeFloorDb = static_cast<double>(matchRelFloor);
    auto d = designMatchEq(sourceLtas, refLevels, bands, SampleRateHz{rate}, o);
    if (d.ok()) match = std::move(d).value();
  }

  void publish() {
    e().params.write(makeParams());
    paramsDirty = false;
  }

  // The complete settings snapshot (shared by playback and export).
  EngineParams makeParams() {
    EngineParams p;
    const auto s = designSections();
    p.eqCount = static_cast<int>(std::min(s.size(), app::kMaxSections));
    for (std::size_t i = 0; i < static_cast<std::size_t>(p.eqCount); ++i) p.eq[i] = s[i];
    p.eqOn = eqOn;
    CompressorParams c = comp;
    if (autoMakeup) c.makeupDb = std::clamp(autoMakeupDb(c), -60.0, 60.0);
    p.comp = c;
    p.compOn = compOn;
    p.lim = lim;
    p.limOn = limOn;
    const double headroom = (autoHeadroom && eqOn) ? eqMaxDb : 0.0;
    p.volumeDb = static_cast<double>(volumeDb) - headroom;
    p.eqTarget = eqTarget;
    p.deess = deess;
    p.deessOn = deessOn;
    p.mb = mb;
    p.mbOn = mbOn;
    if (referenceView || tunerView || builderView) {
      // Reference playback uses the original signal; retain all user controls
      // so leaving this view restores their settings without changing them.
      p.eqOn = p.compOn = p.limOn = p.deessOn = p.mbOn = false;
      p.volumeDb = 0.0;
    }
    return p;
  }

  // Offline render of the whole track through a private Engine: the SAME
  // chain() the audio thread runs, with the on/off crossfades settled at
  // their targets, so the file is what you hear. The limiter's look-ahead
  // is compensated (L trailing zeros in, first L frames dropped), so the
  // export is sample-aligned with the source. 24-bit stereo, with loudness
  // measured on the way out.
  void startExport(std::string path) {
    if (busy() || e().trackFrames == 0) return;
    if (path.size() < 4 || path.substr(path.size() - 4) != ".wav") path += ".wav";
    job = std::make_unique<Job>();
    job->kind = Job::Export;
    Job* j = job.get();
    NormalizeTarget target;
    target.lufs = kExportTargets[std::clamp(exportTarget, 0, 3)].lufs;
    target.truePeakDb = static_cast<double>(exportCeilingDb);
    job->thread = std::thread([j, path, srcIn = e().track, frames = e().trackFrames, fs = rate,
                               p0 = makeParams(), normalize = exportNormalize, target,
                               linear = exportLinearPhase, mbLo = e().mbLowHz,
                               mbHi = e().mbHighHz] {
      EngineParams p = p0;
      std::vector<float> src = srcIn;
      // D37: linear-phase EQ replaces the EQ stage offline (same magnitude,
      // symmetric FIR), latency-compensated, honouring the mid/side target.
      std::string lpNote;
      if (linear && p.eqOn && p.eqCount > 0) {
        const std::vector<BiquadCoeffs> secs(p.eq.begin(), p.eq.begin() + p.eqCount);
        constexpr std::size_t kTaps = 8191, kBlock = 8192;
        auto fir = linearPhaseFromSections(secs, SampleRateHz{fs}, kTaps);
        if (fir.ok()) {
          auto filt = [&](std::vector<double>& x) {
            auto c = FftConvolver::create(fir.value(), kBlock);
            if (!c.ok()) return;
            FftConvolver conv = std::move(c).value();
            const std::size_t delay = kBlock + (kTaps - 1) / 2;
            std::vector<double> buf(x.size() + delay, 0.0);
            std::copy(x.begin(), x.end(), buf.begin());
            conv.process(buf.data(), buf.data(), buf.size());
            std::copy(buf.begin() + static_cast<std::ptrdiff_t>(delay), buf.end(), x.begin());
          };
          std::vector<double> a(frames), b(frames);
          for (std::size_t i = 0; i < frames; ++i) {
            a[i] = static_cast<double>(src[2 * i]);
            b[i] = static_cast<double>(src[2 * i + 1]);
          }
          if (p.eqTarget == 0) {
            filt(a);
            filt(b);
          } else {
            midSideEncode(a.data(), b.data(), a.data(), b.data(), frames);
            filt(p.eqTarget == 1 ? a : b);
            midSideDecode(a.data(), b.data(), a.data(), b.data(), frames);
          }
          for (std::size_t i = 0; i < frames; ++i) {
            src[2 * i] = static_cast<float>(a[i]);
            src[2 * i + 1] = static_cast<float>(b[i]);
          }
          p.eqOn = false;
          lpNote = "; linear-phase EQ (8191-tap FIR)";
        }
      }
      auto fail = [&](const char* why) {
        j->message = std::string("Export failed: ") + why;
        j->done.store(true);
      };
      app::Engine off;
      off.truePeakMode = p.lim.truePeak;
      off.mbLowHz = mbLo;
      off.mbHighHz = mbHi;
      off.eqTargetCur = p.eqTarget;  // no fade-switch in an offline render
      auto ring = SpscRing::create(std::size_t{1} << 14);
      if (!ring.ok() || !off.buildProcessors(fs)) return fail("processors");
      off.ring = std::move(ring).value();
      off.params.write(p);
      off.applyParams();
      off.eqMix.setTarget(p.eqOn ? 1.0 : 0.0, 0);
      off.compMix.setTarget(p.compOn ? 1.0 : 0.0, 0);
      off.limMix.setTarget(p.limOn ? 1.0 : 0.0, 0);
      off.deessMix.setTarget(p.deessOn ? 1.0 : 0.0, 0);
      off.mbMix.setTarget(p.mbOn ? 1.0 : 0.0, 0);
      const std::size_t lat = off.lim->latencyFrames();
      auto wr = WavWriter::open(path, SampleRateHz{fs}, FrameCount{frames}, 2, 24);
      if (!wr.ok()) return fail("cannot open the file");
      auto writer = std::move(wr).value();
      std::vector<double> drainBuf(std::size_t{1} << 14), all(2 * frames, 0.0);
      std::size_t skip = lat, written = 0;
      for (std::size_t pos = 0; pos < frames + lat; pos += app::kChunk) {
        const std::size_t n = std::min(app::kChunk, frames + lat - pos);
        for (std::size_t i = 0; i < n; ++i) {
          const std::size_t f = pos + i;
          off.l[i] = f < frames ? static_cast<double>(src[2 * f]) : 0.0;
          off.r[i] = f < frames ? static_cast<double>(src[2 * f + 1]) : 0.0;
        }
        off.chain(n, true);
        while (off.ring->read(drainBuf.data(), drainBuf.size()) > 0) {
        }
        for (std::size_t i = 0; i < n && written < frames; ++i) {
          if (skip > 0) {
            --skip;
            continue;
          }
          all[2 * written] = off.l[i];
          all[2 * written + 1] = off.r[i];
          ++written;
        }
      }
      // D33: optional delivery normalization of the rendered result.
      std::string norm;
      if (normalize) {
        auto rep = normalizeLoudness(all, 2, SampleRateHz{fs}, target);
        if (!rep.ok()) {
          norm = " (normalization skipped: nothing to measure)";
        } else {
          char nb[160];
          std::snprintf(nb, sizeof nb, "; normalized %+.1f dB%s%s", rep.value().gainDb,
                        rep.value().limited ? " with true-peak limiting" : "",
                        rep.value().reachedTarget ? "" : " (TARGET NOT REACHED under the ceiling)");
          norm = nb;
        }
      }
      auto meterR = LoudnessMeter::create(SampleRateHz{fs}, 2);
      if (!meterR.ok()) return fail("meter");
      LoudnessMeter meter = std::move(meterR).value();
      meter.push(all.data(), frames);
      if (!writer.write(all.data(), frames)) return fail("write error");
      if (!writer.close().ok()) return fail("could not finish the file");
      char buf[520];
      std::snprintf(buf, sizeof buf,
                    "Exported %s: %.2f s, integrated %.1f LUFS, true peak %.1f dBTP, "
                    "%llu clipped samples%s",
                    baseName(path).c_str(), static_cast<double>(frames) / fs,
                    meter.integratedLufs(), meter.truePeakDbtp(),
                    static_cast<unsigned long long>(off.clips.load() + writer.overloads()),
                    (lpNote + norm).c_str());
      j->message = buf;
      j->ok = true;
      j->done.store(true);
    });
    status = "Exporting...";
  }

  // ========================================================== analysis ==
  // meterChannels: 2 for files (the stereo output), 1 for the microphone.
  void setSourceRate(double fs, int meterChannels = 2) {
    hist.assign(app::precisionHistorySize(fs,tunerPanel.pitchRange()), 0.0);
    tunerPanel.calibration.reset();
    resetTuner();
    rate = fs;
    frameSize = pow2Ceil(0.15 * fs);
    hop = frameSize / 8;
    sinceHop = 0;
    rebuildAnalyzer();
    loudnessChannels = meterChannels;
    resetLoudness();
    if (eqMode == EqMode::Match) redesignMatch();
    paramsDirty = true;
    while (e().ring->read(stereo.data(), stereo.size()) > 0) {
    }
  }

  void rebuildAnalyzer() {
    analyzer.reset();
    auto a = BandAnalyzer::create(SampleRateHz{rate}, frameSize, hop, bandsPerOctave,
                                  FrequencyHz{20.0}, FrequencyHz{std::min(20000.0, 0.45 * rate)},
                                  Window::Hann, MeterBallistics{30.0, 1.5});
    if (a.ok()) analyzer = std::move(a).value();
    const double binHz = rate / static_cast<double>(frameSize);
    const double lo = 30.0, hi = std::min(20000.0, 0.5 * rate);
    gramLo.assign(kGramH, 0);
    gramHi.assign(kGramH, 0);
    for (int row = 0; row < kGramH; ++row) {
      const double f0 = lo * std::pow(hi / lo, 1.0 - (row + 1.0) / kGramH);
      const double f1 = lo * std::pow(hi / lo, 1.0 - static_cast<double>(row) / kGramH);
      std::size_t b0 = static_cast<std::size_t>(std::floor(f0 / binHz + 0.5));
      std::size_t b1 = static_cast<std::size_t>(std::floor(f1 / binHz + 0.5));
      b1 = std::max(b1, b0);
      gramLo[static_cast<std::size_t>(row)] = std::min(b0, frameSize / 2);
      gramHi[static_cast<std::size_t>(row)] = std::min(b1, frameSize / 2);
    }
    std::fill(gramPixels.begin(), gramPixels.end(), 0);
    for (std::size_t i = 3; i < gramPixels.size(); i += 4) gramPixels[i] = 255;
    gramDirty = true;
    lineDb.clear();
  }

  void addGramColumn() {
    const auto& amp = analyzer->spectrum().amplitude;
    if (amp.empty()) return;
    const std::size_t stride = static_cast<std::size_t>(kGramW) * 4;
    for (int row = 0; row < kGramH; ++row) {
      unsigned char* p = gramPixels.data() + static_cast<std::size_t>(row) * stride;
      std::memmove(p, p + 4, stride - 4);
      double a = 0.0;
      for (std::size_t k = gramLo[static_cast<std::size_t>(row)];
           k <= gramHi[static_cast<std::size_t>(row)] && k < amp.size(); ++k)
        a = std::max(a, amp[k]);
      const double db = a > 0.0 ? 20.0 * std::log10(a) : -300.0;
      const auto c = heatColorRgb((db - dbFloor) / (0.0 - static_cast<double>(dbFloor)));
      unsigned char* px = p + stride - 4;
      px[0] = c[0];
      px[1] = c[1];
      px[2] = c[2];
      px[3] = 255;
    }
    gramDirty = true;
  }

  void resetLoudness() {
    loudness.reset();
    auto m = LoudnessMeter::create(SampleRateHz{rate}, loudnessChannels);
    if (m.ok()) loudness = std::move(m).value();
  }

  void resetTuner() {
    streamingTuner.reset();
    tune = {};
    tuneLevelDb = -120;
    std::fill(hist.begin(), hist.end(), 0);
    histPos = 0;
    nextTune = 0;
    samplesSinceTune = 0;
  }

  void returnToFile() {
    stopMic();
    if (app::restoreFileSource(e(), play, fileRate)) {
      setSourceRate(fileRate);
      publish();
      status = "Ready: " + trackName;
    } else status = e().trackFrames == 0 ? "Open a WAV recording to begin." : std::string("Cannot restore file playback: ") + SDL_GetError();
  }

  void pumpAudio(double dt) {
    double peak = 0.0, sumSq = 0.0;
    std::size_t total = 0;
    for (;;) {
      // The engine writes whole stereo frames, so every read is even.
      const std::size_t values = e().ring->read(stereo.data(), stereo.size());
      if (values == 0) break;
      const std::size_t got = values / 2;
      for (std::size_t i = 0; i < got; ++i) {
        const double l = stereo[2 * i], r = stereo[2 * i + 1];
        drain[i] = 0.5 * (l + r);
        peak = std::max(peak, std::max(std::fabs(l), std::fabs(r)));
        sumSq += 0.5 * (l * l + r * r);
        hist[histPos] = drain[i];
        histPos = (histPos + 1) % hist.size();
        if (loudnessChannels == 1) monoTap[i] = l;
      }
      if (loudness)
        loudness->push(loudnessChannels == 1 ? monoTap.data() : stereo.data(), got);
      total += got;
      if (!analyzer) continue;
      std::size_t off = 0;
      while (off < got) {
        const std::size_t k = std::min(got - off, hop - sinceHop);
        const std::size_t frames = analyzer->push(drain.data() + off, k);
        sinceHop = (sinceHop + k) % hop;
        off += k;
        if (frames > 0) {
          framesAnalysed += frames;
          addGramColumn();
        }
      }
    }
    const double fall = 20.0 * dt;
    const double rmsDb = total > 0 && sumSq > 0.0
                             ? 10.0 * std::log10(sumSq / static_cast<double>(total))
                             : -120.0;
    const double pkDb = peak > 0.0 ? 20.0 * std::log10(peak) : -120.0;
    meterRmsDb = std::max(rmsDb, meterRmsDb - fall);
    meterPeakDb = std::max(pkDb, meterPeakDb - fall);
    const std::uint64_t c = e().clips.load(std::memory_order_relaxed);
    if (c != lastClips) {
      lastClips = c;
      clipLightUntil = now() + 1.0;
    }
    // Keep the short pYIN acquisition window. Fine tracking uses a slightly
    // longer, past-only context after four consistent readings.
    samplesSinceTune += total;
    if (now() >= nextTune && rate >= 8000.0) {
      nextTune = now() + 1.0 / 15.0;
      if (samplesSinceTune > 0) {
        std::vector<double> frame(hist.size());
        for (std::size_t i = 0; i < hist.size(); ++i) frame[i] = hist[(histPos + i) % hist.size()];
        tune = streamingTuner.process(frame, rate,tunerPanel.pitchRange());
        tuneLevelDb = streamingTuner.levelDb;
      } else {
        // A repeated stale buffer must never complete calibration.
        streamingTuner.reset(); tune = {}; tuneLevelDb = -120;
      }
      samplesSinceTune = 0;
      if (e().mode.load() == kMic) tunerPanel.calibration.observe(tune.frequencyHz,tune.voiced,tuneLevelDb);
    }
  }

  // =========================================================== sources ==
  void stopMic() {
    if (!mic) return;
    SDL_LockAudioStream(mic);
    e().mode.store(kNone);
    SDL_UnlockAudioStream(mic);
    SDL_DestroyAudioStream(mic);
    mic = nullptr;
  }

  void buildOverview() {
    overview.reset();
    if (overviewTex) {
      SDL_DestroyTexture(overviewTex);
      overviewTex = nullptr;
    }
    if (e().trackFrames < 4096) return;
    const auto mono = monoOf(e().track, e().trackFrames);
    const std::size_t hopO = std::max<std::size_t>(512, e().trackFrames / 1400);
    auto g = computeSpectrogram(mono, SampleRateHz{rate}, Window::Hann, 2048, hopO);
    if (!g.ok()) return;
    overview = std::move(g).value();
    overviewW = static_cast<int>(overview->frames);
    overviewTex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                    SDL_TEXTUREACCESS_STATIC, overviewW, kOverH);
    if (!overviewTex) return;
    SDL_SetTextureScaleMode(overviewTex, SDL_SCALEMODE_LINEAR);
    refreshOverviewPixels();
  }

  void refreshOverviewPixels() {
    if (!overview || !overviewTex) return;
    const auto& g = *overview;
    std::vector<unsigned char> px(static_cast<std::size_t>(overviewW) * kOverH * 4);
    const double lo = 30.0, hi = std::min(20000.0, 0.5 * rate);
    for (int row = 0; row < kOverH; ++row) {
      const double f0 = lo * std::pow(hi / lo, 1.0 - (row + 1.0) / kOverH);
      const double f1 = lo * std::pow(hi / lo, 1.0 - static_cast<double>(row) / kOverH);
      std::size_t b0 = static_cast<std::size_t>(f0 / g.binHz + 0.5);
      std::size_t b1 = std::max(b0, static_cast<std::size_t>(f1 / g.binHz + 0.5));
      b1 = std::min(b1, g.bins - 1);
      b0 = std::min(b0, b1);
      for (int col = 0; col < overviewW; ++col) {
        double db = -300.0;
        for (std::size_t b = b0; b <= b1; ++b)
          db = std::max(db, g.at(static_cast<std::size_t>(col), b));
        const auto c = heatColorRgb((db - dbFloor) / (0.0 - static_cast<double>(dbFloor)));
        unsigned char* p = px.data() + (static_cast<std::size_t>(row) * static_cast<std::size_t>(overviewW) +
                                        static_cast<std::size_t>(col)) * 4;
        p[0] = c[0];
        p[1] = c[1];
        p[2] = c[2];
        p[3] = 255;
      }
    }
    SDL_UpdateTexture(overviewTex, nullptr, px.data(), overviewW * 4);
  }

  std::size_t fadeFrames(double fs) const {
    return std::max<std::size_t>(1, static_cast<std::size_t>(static_cast<double>(fadeMs) * 1e-3 * fs));
  }

  // Install a stereo float track (UI thread).
  bool installTrack(std::vector<float> st, std::size_t frames, double fs, const std::string& name) {
    auto reader = SpliceReader<float>::create(2, fadeFrames(fs), static_cast<FadeCurve>(fadeCurve));
    if (!reader.ok() || frames == 0 || octaveBandsFor(fs).empty()) {
      status = "Cannot use " + name + ": empty, or sample rate too low.";
      return false;
    }
    stopMic();
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, static_cast<int>(fs)};
    SDL_LockAudioStream(play);
    e().playing.store(false);
    e().track.swap(st);
    e().trackFrames = frames;
    e().reader = std::move(reader).value();
    e().reader->setSource(e().track.data(), frames);
    e().reader->setLooping(loop);
    const bool built = e().buildProcessors(fs);
    e().playRamp.setTarget(0.0, 0);
    SDL_SetAudioStreamFormat(play, &spec, nullptr);
    SDL_ClearAudioStream(play);
    e().mode.store(built ? kFile : kNone);
    SDL_UnlockAudioStream(play);
    if (!built) {
      status = "Cannot build the processors for " + name + ".";
      return false;
    }
    markA = markB = -1;
    e().loopA.store(-1);
    e().loopB.store(-1);
    e().loopVersion.fetch_add(1);
    trackName = name;
    trackSeconds = static_cast<double>(frames) / fs;
    fileRate = fs;
    const bool rateChanged = fs != rate;
    sourceLtas = Spectrum{};
    match.reset();
    if (rateChanged) refLevels.clear();  // the band list depends on the rate
    if (!refName.empty()) refName += " (choose again for this track)";
    setSourceRate(fs);
    buildOverview();
    e().shownPos.store(0);
    e().ended.store(false);
    e().playing.store(true);
    return true;
  }

  void loadWav(const WavData& w, const std::string& name) {
    std::vector<float> st(w.frames * 2);
    const auto ch = static_cast<std::size_t>(w.channels);
    for (std::size_t i = 0; i < w.frames; ++i) {
      const double a = w.samples[i * ch];
      const double b = ch > 1 ? w.samples[i * ch + 1] : a;
      st[2 * i] = static_cast<float>(a);
      st[2 * i + 1] = static_cast<float>(b);
    }
    original = st;
    originalFrames = w.frames;
    originalRate = w.rate.value();
    originalName = name;
    if (!installTrack(std::move(st), w.frames, w.rate.value(), name)) return;
    char buf[512];
    std::snprintf(buf, sizeof buf, "%s: %d Hz, %d ch, %d-bit %s, %.2f s%s%s", name.c_str(),
                  static_cast<int>(w.rate.value()), w.channels, w.bitsPerSample,
                  w.isFloat ? "float" : "PCM", trackSeconds,
                  w.channels > 2 ? " (playing channels 1-2)" : "",
                  w.outOfRange > 0 ? " (some samples beyond full scale)" : "");
    status = buf;
  }

  static std::string baseName(const std::string& path) {
    const auto slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
  }

  void loadPath(const std::string& path) {
    auto w = readWavFile(path);
    if (!w.ok()) {
      status = baseName(path) + ": " + errorText(w.error());
      return;
    }
    referenceView = false;
    paramsDirty = true;
    loadWav(w.value(), baseName(path));
  }

  void loadDemo() {
    referenceView = false;
    paramsDirty = true;
    const WavData w = makeDemo();
    if (w.frames == 0) {
      status = "Demo synthesis failed.";
      return;
    }
    loadWav(w, "Demo: plucked string root, fifth, octave");
  }

  void seek(double seconds) {
    if (e().trackFrames == 0) return;
    resetTuner();
    const auto f = static_cast<long long>(
        std::clamp(seconds * rate, 0.0, static_cast<double>(e().trackFrames - 1)));
    e().seekTo.store(f);  // the audio thread crossfades to it
    e().shownPos.store(static_cast<std::uint64_t>(f));
  }

  void setPlaying(bool on) {
    if (on && e().ended.load()) {
      seek(0.0);
      e().ended.store(false);
    }
    e().playing.store(on);
  }

  void rebuildReader() {
    if (e().trackFrames == 0) return;
    auto r = SpliceReader<float>::create(2, fadeFrames(rate), static_cast<FadeCurve>(fadeCurve));
    if (!r.ok()) return;
    SDL_LockAudioStream(play);
    const std::size_t pos = e().reader ? e().reader->position() : 0;
    e().reader = std::move(r).value();
    e().reader->setSource(e().track.data(), e().trackFrames);
    e().reader->setLooping(loop);
    e().reader->setPosition(pos);
    e().loopVersion.fetch_add(1);  // re-apply A-B with the new fade length
    SDL_UnlockAudioStream(play);
    setLoopMarks();
  }

  void setLoopMarks() {
    const std::size_t minLen = 2 * fadeFrames(rate);
    if (markA >= 0 && markB > markA && static_cast<std::size_t>(markB - markA) >= minLen) {
      e().loopA.store(markA);
      e().loopB.store(markB);
    } else {
      e().loopA.store(-1);
      e().loopB.store(-1);
    }
    e().loopVersion.fetch_add(1, std::memory_order_release);
  }

  void refreshMics() {
    micIds.clear();
    micNames.clear();
    int count = 0;
    SDL_AudioDeviceID* ids = SDL_GetAudioRecordingDevices(&count);
    if (ids) {
      for (int i = 0; i < count; ++i) {
        const char* n = SDL_GetAudioDeviceName(ids[i]);
        micIds.push_back(ids[i]);
        micNames.push_back(n ? n : "(unnamed)");
      }
      SDL_free(ids);
    }
    if (micChoice > static_cast<int>(micIds.size())) micChoice = 0;
  }

  void startMic() {
    stopMic();
    SDL_LockAudioStream(play);
    e().playing.store(false);
    e().mode.store(kNone);
    SDL_UnlockAudioStream(play);
    if (!e().buildProcessors(app::kMicRate)) {
      status = "Cannot build the processors for the microphone.";
      return;
    }
    setSourceRate(app::kMicRate, 1);
    publish();
    e().mode.store(kMic);
    const SDL_AudioDeviceID dev = micChoice == 0 ? SDL_AUDIO_DEVICE_DEFAULT_RECORDING
                                                 : micIds[static_cast<std::size_t>(micChoice - 1)];
    const SDL_AudioSpec spec{SDL_AUDIO_F32, 1, static_cast<int>(app::kMicRate)};
    mic = SDL_OpenAudioDeviceStream(dev, &spec, app::micCallback, engine.get());
    if (!mic) {
      e().mode.store(kNone);
      status = std::string("Microphone unavailable: ") + SDL_GetError();
      return;
    }
    SDL_ResumeAudioStreamDevice(mic);
    status = "Listening: " +
             (micChoice == 0 ? std::string("default microphone")
                             : micNames[static_cast<std::size_t>(micChoice - 1)]) +
             " (analysis only, not played back)";
  }

  // ============================================================== jobs ==
  bool busy() const { return job != nullptr; }

  void startTransform() {
    if (busy() || original.empty()) return;
    job = std::make_unique<Job>();
    job->kind = Job::Transform;
    const double p = std::pow(2.0, static_cast<double>(shiftSemis) / 12.0);
    const double t = 100.0 / static_cast<double>(tempoPct);
    const bool usePsola = method == 0;
    // 1 classic locked vocoder (D28), 2 PGHI (D36), 3 hybrid HPSS (D36).
    const TsmMethod tsm = method == 2 ? TsmMethod::Pghi
                          : method == 3 ? TsmMethod::Hybrid
                                        : TsmMethod::ClassicLocked;
    Job* j = job.get();
    job->thread = std::thread([j, src = original, frames = originalFrames, fs = originalRate, p, t,
                               usePsola, tsm] {
      const SampleRateHz r{fs};
      auto run = [&](const std::vector<double>& x) -> Result<std::vector<double>, QuantityError> {
        if (usePsola) return psola(x, r, p, t);
        std::vector<double> y = x;
        if (std::fabs(t - 1.0) > 1e-9) {
          auto st = timeStretch(y, t, tsm);
          if (!st.ok()) return st;
          y = std::move(st).value();
        }
        if (std::fabs(p - 1.0) > 1e-9) return pitchShift(y, r, p, tsm);
        return Result<std::vector<double>, QuantityError>::success(std::move(y));
      };
      std::vector<double> l(frames), rr(frames);
      bool stereo = false;
      for (std::size_t i = 0; i < frames; ++i) {
        l[i] = static_cast<double>(src[2 * i]);
        rr[i] = static_cast<double>(src[2 * i + 1]);
        if (l[i] != rr[i]) stereo = true;
      }
      if (usePsola && stereo)  // PSOLA assumes one periodic source: mono mix
        for (std::size_t i = 0; i < frames; ++i) l[i] = 0.5 * (l[i] + rr[i]);
      auto a = run(l);
      if (!a.ok()) {
        j->message = "Transform failed (the signal may be too short or unvoiced).";
        j->done.store(true);
        return;
      }
      std::vector<double> b;
      if (stereo && !usePsola) {
        auto rb = run(rr);
        if (rb.ok()) b = std::move(rb).value();
      }
      const auto& ya = a.value();
      j->frames = ya.size();
      j->track.resize(2 * ya.size());
      for (std::size_t i = 0; i < ya.size(); ++i) {
        j->track[2 * i] = static_cast<float>(ya[i]);
        j->track[2 * i + 1] = static_cast<float>(b.size() == ya.size() ? b[i] : ya[i]);
      }
      j->rate = fs;
      j->ok = true;
      j->done.store(true);
    });
    status = usePsola ? "Rendering (PSOLA)..." : "Rendering (phase vocoder)...";
  }

  void startMatchAnalysis(const std::string& refPath) {
    if (busy() || e().trackFrames == 0) {
      status = "Load a track first, then pick a reference.";
      return;
    }
    job = std::make_unique<Job>();
    job->kind = Job::MatchAnalysis;
    Job* j = job.get();
    job->message = baseName(refPath);
    job->thread = std::thread([j, mono = monoOf(e().track, e().trackFrames), fs = rate,
                               bands = thirdBandsFor(rate), refPath] {
      auto s = longTermSpectrum(mono, SampleRateHz{fs}, 8192);
      auto w = readWavFile(refPath);
      if (!s.ok() || !w.ok()) {
        j->message = "Match: cannot analyse (" +
                     std::string(w.ok() ? "track too short" : errorText(w.error())) + ")";
        j->done.store(true);
        return;
      }
      auto rs = longTermSpectrum(w.value().mono(), w.value().rate, 8192);
      if (!rs.ok()) {
        j->message = "Match: reference too short.";
        j->done.store(true);
        return;
      }
      j->sourceLtas = std::move(s).value();
      j->referenceLevels = bandLevels(rs.value(), bands);
      j->ok = true;
      j->done.store(true);
    });
    status = "Analysing track and reference...";
  }

  void pollJob() {
    if (!job || !job->done.load()) return;
    job->thread.join();
    Job& j = *job;
    if (j.kind == Job::Transform) {
      if (j.ok) {
        char name[200];
        std::snprintf(name, sizeof name, "%s [%+.1f st, %.0f%%, %s]", originalName.c_str(),
                      static_cast<double>(shiftSemis), static_cast<double>(tempoPct),
                      (method == 0 ? "PSOLA" : method == 1 ? "vocoder" : method == 2 ? "PGHI" : "hybrid"));
        if (installTrack(std::move(j.track), j.frames, j.rate, name)) status = std::string("Rendered: ") + name;
      } else {
        status = j.message;
      }
    } else if (j.kind == Job::Repair) {
      if (j.ok) {
        const std::string keep = trackName;
        if (installTrack(std::move(j.track), j.frames, j.rate, keep)) status = "Repair: " + j.message;
      } else {
        status = j.message;
      }
    } else if (j.kind == Job::Export) {
      status = j.message;
      lastExport = j.message;
    } else if (j.kind == Job::MatchAnalysis) {
      if (j.ok) {
        sourceLtas = std::move(j.sourceLtas);
        refLevels = std::move(j.referenceLevels);
        refName = j.message;
        eqMode = EqMode::Match;
        redesignMatch();
        paramsDirty = true;
        status = match ? "Match EQ designed from " + refName : "Match EQ design failed.";
      } else {
        status = j.message;
      }
    }
    job.reset();
  }

  // ============================================================ drawing ==
  static float xOfFreq(double f, float x0, float w, double lo, double hi) {
    return x0 + w * static_cast<float>(std::log(f / lo) / std::log(hi / lo));
  }
  static double freqOfX(float x, float x0, float w, double lo, double hi) {
    return lo * std::pow(hi / lo, static_cast<double>((x - x0) / w));
  }
  static float yOfDb(double db, float y0, float h, double lo, double hi) {
    const double t = std::clamp((db - lo) / (hi - lo), 0.0, 1.0);
    return y0 + h * static_cast<float>(1.0 - t);
  }

  void drawDbGrid(ImDrawList* d, ImVec2 p0, ImVec2 p1, double lo, double hi, double step) {
    const float h = p1.y - p0.y;
    for (double db = std::ceil(lo / step) * step; db <= hi + 1e-9; db += step) {
      const float y = yOfDb(db, p0.y, h, lo, hi);
      d->AddLine(ImVec2(p0.x, y), ImVec2(p1.x, y), IM_COL32(255, 255, 255, 28));
      if (db >= hi - 1e-9) continue;
      char buf[16];
      std::snprintf(buf, sizeof buf, "%.0f", db);
      d->AddText(ImVec2(p0.x + 3.0f, y - ImGui::GetFontSize()), IM_COL32(200, 200, 210, 140), buf);
    }
  }

  static void tooltipAtMouse(const std::string& text) {
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(text.c_str());
    ImGui::EndTooltip();
  }

  void drawOverview(ImVec2 size) {
    ImDrawList* d = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("overview", size);
    const ImVec2 p1(p0.x + size.x, p0.y + size.y);
    d->AddRectFilled(p0, p1, IM_COL32(14, 12, 30, 255));
    if (overviewTex)
      d->AddImage(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<std::intptr_t>(overviewTex))), p0,
                  p1);
    if (trackSeconds <= 0.0 || e().trackFrames == 0) return;
    auto xOfFrame = [&](double f) {
      return p0.x + size.x * static_cast<float>(f / static_cast<double>(e().trackFrames));
    };
    if (markA >= 0 || markB >= 0) {
      const float xa = markA >= 0 ? xOfFrame(static_cast<double>(markA)) : p0.x;
      const float xb = markB >= 0 ? xOfFrame(static_cast<double>(markB)) : p1.x;
      d->AddRectFilled(ImVec2(xa, p0.y), ImVec2(xb, p1.y), IM_COL32(120, 200, 255, 40));
      if (markA >= 0) d->AddLine(ImVec2(xa, p0.y), ImVec2(xa, p1.y), IM_COL32(120, 200, 255, 220), 2.0f);
      if (markB >= 0) d->AddLine(ImVec2(xb, p0.y), ImVec2(xb, p1.y), IM_COL32(120, 200, 255, 220), 2.0f);
    }
    const float xp = xOfFrame(static_cast<double>(e().shownPos.load()));
    d->AddLine(ImVec2(xp, p0.y), ImVec2(xp, p1.y), IM_COL32(255, 255, 255, 230), 2.0f);
    d->AddText(ImVec2(p0.x + 4.0f, p0.y + 2.0f), IM_COL32(255, 255, 255, 170),
               "whole track (click: seek; shift+drag: select a region to repair)");
    const double lo = 30.0, hi = std::min(20000.0, 0.5 * rate);
    auto tOf = [&](float x) {
      return std::clamp(trackSeconds * static_cast<double>((x - p0.x) / size.x), 0.0, trackSeconds);
    };
    auto fOf = [&](float y) {
      return lo * std::pow(hi / lo, std::clamp(static_cast<double>((p1.y - y) / size.y), 0.0, 1.0));
    };
    auto yOfF = [&](double f) {
      return p1.y - size.y * static_cast<float>(std::log(f / lo) / std::log(hi / lo));
    };
    const ImVec2 m = ImGui::GetMousePos();
    // D34 spectral repair: shift+drag selects a time x frequency box.
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::GetIO().KeyShift) {
      selecting = true;
      dragT = tOf(m.x);
      dragF = fOf(m.y);
    }
    if (selecting) {
      const double t = tOf(m.x), f = fOf(m.y);
      boxT0 = std::min(dragT, t);
      boxT1 = std::max(dragT, t);
      boxF0 = std::min(dragF, f);
      boxF1 = std::max(dragF, f);
      haveBox = boxT1 - boxT0 > 0.01 && boxF1 / boxF0 > 1.05;
      if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) selecting = false;
    }
    if (haveBox) {
      const ImVec2 a(xOfFrame(boxT0 * rate), yOfF(boxF1)), b(xOfFrame(boxT1 * rate), yOfF(boxF0));
      d->AddRectFilled(a, b, IM_COL32(255, 220, 120, 45));
      d->AddRect(a, b, IM_COL32(255, 220, 120, 230), 0.0f, 0, 2.0f);
    }
    if (ImGui::IsItemHovered() && !selecting) {
      const double t = tOf(m.x), f = fOf(m.y);
      char buf[96];
      std::snprintf(buf, sizeof buf, "%.2f s, %s  (click: seek, crossfaded)", t, hzText(f).c_str());
      tooltipAtMouse(buf);
      if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyShift) seek(t);
    }
  }

  // ============================================================= repair ==
  void learnNoiseFromSelection() {
    if (e().trackFrames == 0) return;
    std::size_t a = 0, b = std::min<std::size_t>(e().trackFrames, static_cast<std::size_t>(rate));
    if (markA >= 0 && markB > markA) {
      a = static_cast<std::size_t>(markA);
      b = static_cast<std::size_t>(markB);
    }
    std::vector<double> mono(b - a);
    for (std::size_t i = a; i < b; ++i)
      mono[i - a] = 0.5 * (static_cast<double>(e().track[2 * i]) + static_cast<double>(e().track[2 * i + 1]));
    auto p = learnNoiseProfile(mono, 2048);
    if (p.ok()) {
      noiseProfile = std::move(p).value();
      char buf[128];
      std::snprintf(buf, sizeof buf, "Noise profile learned from %.2f s (%s).",
                    static_cast<double>(b - a) / rate, markA >= 0 && markB > markA ? "A-B" : "first second");
      status = buf;
    } else {
      status = "Noise profile: select at least 2048 samples of noise (A-B).";
    }
  }

  // Offline repair of the current track on the worker (kind Transform
  // installs the result; the previous track is kept for Undo).
  void startRepair(bool denoiseJob) {
    if (busy() || e().trackFrames == 0) return;
    if (denoiseJob && !noiseProfile) {
      status = "Learn a noise profile first.";
      return;
    }
    if (!denoiseJob && !haveBox) {
      status = "Shift+drag on the whole-track view to select a region first.";
      return;
    }
    undoTrack = e().track;
    undoFrames = e().trackFrames;
    job = std::make_unique<Job>();
    job->kind = Job::Repair;
    Job* j = job.get();
    const auto prof = noiseProfile ? *noiseProfile : NoiseProfile{};
    const auto meth = static_cast<DenoiseMethod>(denoiseMethod);
    const double red = static_cast<double>(denoiseReduction);
    const double t0 = boxT0, t1 = boxT1, f0 = boxF0, f1 = boxF1, g = static_cast<double>(boxGainDb);
    job->thread = std::thread([j, src = e().track, frames = e().trackFrames, fs = rate, prof, meth,
                               red, t0, t1, f0, f1, g, denoiseJob] {
      std::vector<double> ch[2] = {std::vector<double>(frames), std::vector<double>(frames)};
      for (std::size_t i = 0; i < frames; ++i) {
        ch[0][i] = static_cast<double>(src[2 * i]);
        ch[1][i] = static_cast<double>(src[2 * i + 1]);
      }
      for (auto& c : ch) {
        auto r = denoiseJob ? denoise(c, prof, meth, red)
                            : spectralGainRegion(c, SampleRateHz{fs}, t0, t1, f0, f1, g);
        if (!r.ok()) {
          j->message = "Repair failed.";
          j->done.store(true);
          return;
        }
        c = std::move(r).value();
      }
      j->track.resize(2 * frames);
      for (std::size_t i = 0; i < frames; ++i) {
        j->track[2 * i] = static_cast<float>(ch[0][i]);
        j->track[2 * i + 1] = static_cast<float>(ch[1][i]);
      }
      j->frames = frames;
      j->rate = fs;
      j->ok = true;
      j->message = denoiseJob ? "noise reduced" : "region repaired";
      j->done.store(true);
    });
    status = denoiseJob ? "Reducing noise..." : "Repairing region...";
  }

  void panelRepair() {
    ImGui::SeparatorText("Noise reduction (D34)");
    ImGui::TextWrapped("1. Set A-B around a stretch of noise only (or use the first second). "
                       "2. Learn. 3. Apply to the track.");
    ImGui::BeginDisabled(busy() || e().trackFrames == 0);
    if (ImGui::Button("Learn noise profile")) learnNoiseFromSelection();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", noiseProfile ? "profile ready" : "no profile");
    static const char* methods[] = {"Spectral subtraction (Boll/Berouti)", "MMSE-STSA (Ephraim-Malah)",
                                    "MMSE-LSA (Ephraim-Malah)"};
    ImGui::Combo("Method", &denoiseMethod, methods, 3);
    ImGui::SliderFloat("Reduction", &denoiseReduction, 3.0f, 40.0f, "%.0f dB max");
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("The gain floor: no bin is pulled down more than this.\n"
                        "Lower values keep more natural texture (less 'musical noise').");
    if (ImGui::Button("Apply noise reduction")) startRepair(true);
    ImGui::SeparatorText("Spectral repair");
    ImGui::TextWrapped("Shift+drag on the whole-track spectrogram to select a time x frequency "
                       "box, then attenuate it (e.g. a cough, a squeak, a hum burst).");
    if (haveBox)
      ImGui::TextDisabled("box: %.2f-%.2f s, %.0f-%.0f Hz", boxT0, boxT1, boxF0, boxF1);
    ImGui::SliderFloat("Gain in box", &boxGainDb, -60.0f, 0.0f, "%.0f dB");
    if (ImGui::Button("Apply to box")) startRepair(false);
    ImGui::SameLine();
    if (ImGui::Button("Clear box")) haveBox = false;
    ImGui::Separator();
    ImGui::BeginDisabled(undoTrack.empty());
    if (ImGui::Button("Undo last repair")) {
      auto u = std::move(undoTrack);
      undoTrack.clear();
      if (installTrack(std::move(u), undoFrames, rate, trackName)) status = "Repair undone.";
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
  }

  void drawBars(ImVec2 size) {
    ImDrawList* d = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("bars", size);
    const float labelH = ImGui::GetFontSize() + 4.0f;
    const ImVec2 p1(p0.x + size.x, p0.y + size.y - labelH);
    d->AddRectFilled(p0, ImVec2(p1.x, p0.y + size.y), IM_COL32(14, 12, 30, 255));
    const double lo = dbFloor, hi = 0.0;
    drawDbGrid(d, p0, p1, lo, hi, 10.0);
    if (!analyzer) return;
    const auto& disp = analyzer->display();
    const auto& peaks = analyzer->peaks();
    const auto& raw = analyzer->raw();
    const std::size_t n = disp.size();
    const float slot = (p1.x - p0.x - 36.0f) / static_cast<float>(n);
    const float gap = std::max(1.0f, slot * 0.18f);
    const float h = p1.y - p0.y;
    for (std::size_t i = 0; i < n; ++i) {
      const float x0 = p0.x + 36.0f + slot * static_cast<float>(i) + gap * 0.5f;
      const float x1 = x0 + slot - gap;
      if (raw[i].bins == 0) {
        d->AddRect(ImVec2(x0, p0.y + 2.0f), ImVec2(x1, p1.y), IM_COL32(120, 120, 140, 60));
      } else {
        const float y = yOfDb(disp[i], p0.y, h, lo, hi);
        const double t = std::clamp((disp[i] - lo) / (hi - lo), 0.0, 1.0);
        d->AddRectFilledMultiColor(ImVec2(x0, y), ImVec2(x1, p1.y), heat(0.25 + 0.75 * t),
                                   heat(0.25 + 0.75 * t), heat(0.2), heat(0.2));
        if (peaks[i] > lo) {
          const float yp = yOfDb(peaks[i], p0.y, h, lo, hi);
          d->AddLine(ImVec2(x0, yp), ImVec2(x1, yp), IM_COL32(252, 240, 170, 230), 2.0f);
        }
      }
      if (static_cast<int>(i) % (bandsPerOctave == 3 ? 3 : 1) == 0) {
        const std::string lab = bandLabel(raw[i].band.center.value());
        const float tw = ImGui::CalcTextSize(lab.c_str()).x;
        d->AddText(ImVec2(0.5f * (x0 + x1) - 0.5f * tw, p1.y + 2.0f), IM_COL32(200, 200, 210, 200),
                   lab.c_str());
      }
    }
    if (ImGui::IsItemHovered()) {
      const ImVec2 m = ImGui::GetMousePos();
      const int i = static_cast<int>((m.x - p0.x - 36.0f) / slot);
      if (i >= 0 && i < static_cast<int>(n)) {
        const auto k = static_cast<std::size_t>(i);
        char buf[128];
        std::snprintf(buf, sizeof buf, "%s Hz band: %.1f dB (peak %.1f dB)",
                      bandLabel(raw[k].band.center.value()).c_str(), disp[k], peaks[k]);
        tooltipAtMouse(buf);
      }
    }
  }

  void drawSpectrum(ImVec2 size, double dt) {
    ImDrawList* d = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("spectrum", size);
    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();
    const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
    const ImVec2 p1(p0.x + size.x, p0.y + size.y);
    d->AddRectFilled(p0, p1, IM_COL32(14, 12, 30, 255));
    const double lo = dbFloor, hi = 0.0;
    const double fLo = 20.0, fHi = std::min(20000.0, 0.5 * rate);
    const float w = size.x, h = size.y;
    drawDbGrid(d, p0, p1, lo, hi, 20.0);
    for (double f : {50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0}) {
      if (f > fHi) break;
      const float x = xOfFreq(f, p0.x, w, fLo, fHi);
      d->AddLine(ImVec2(x, p0.y), ImVec2(x, p1.y), IM_COL32(255, 255, 255, 22));
      d->AddText(ImVec2(x + 2.0f, p1.y - ImGui::GetFontSize() - 1.0f), IM_COL32(200, 200, 210, 140),
                 bandLabel(f).c_str());
    }
    const int cols = std::max(2, static_cast<int>(w));
    if (lineDb.size() != static_cast<std::size_t>(cols)) lineDb.assign(static_cast<std::size_t>(cols), -300.0f);
    if (analyzer && !analyzer->spectrum().amplitude.empty()) {
      const auto& s = analyzer->spectrum();
      for (int c = 0; c < cols; ++c) {
        const double f0 = fLo * std::pow(fHi / fLo, c / static_cast<double>(cols));
        const double f1 = fLo * std::pow(fHi / fLo, (c + 1) / static_cast<double>(cols));
        std::size_t k0 = static_cast<std::size_t>(std::floor(f0 / s.binHz + 0.5));
        std::size_t k1 = std::max(k0, static_cast<std::size_t>(std::floor(f1 / s.binHz + 0.5)));
        double a = 0.0;
        for (std::size_t k = k0; k <= k1 && k < s.amplitude.size(); ++k) a = std::max(a, s.amplitude[k]);
        const float db = a > 0.0 ? static_cast<float>(20.0 * std::log10(a)) : -300.0f;
        float& v = lineDb[static_cast<std::size_t>(c)];
        v = std::max(db, v - static_cast<float>(60.0 * dt));
      }
      std::vector<ImVec2> pts(static_cast<std::size_t>(cols));
      for (int c = 0; c < cols; ++c)
        pts[static_cast<std::size_t>(c)] = ImVec2(p0.x + static_cast<float>(c) + 0.5f,
                                                  yOfDb(lineDb[static_cast<std::size_t>(c)], p0.y, h, lo, hi));
      for (int c = 0; c + 1 < cols; ++c) {
        const auto& a = pts[static_cast<std::size_t>(c)];
        const auto& b = pts[static_cast<std::size_t>(c + 1)];
        d->AddQuadFilled(a, b, ImVec2(b.x, p1.y), ImVec2(a.x, p1.y), IM_COL32(205, 55, 80, 60));
      }
      d->AddPolyline(pts.data(), cols, IM_COL32(245, 150, 40, 255), ImDrawFlags_None, 1.5f);
    }
    // EQ response on a +/-18 dB right-hand scale.
    constexpr double kEqScale = 18.0;
    if (eqOn && !curveHz.empty()) {
      const float y0 = yOfDb(0.0, p0.y, h, -kEqScale, kEqScale);
      d->AddLine(ImVec2(p0.x, y0), ImVec2(p1.x, y0), IM_COL32(120, 200, 255, 60));
      std::vector<ImVec2> eqPts;
      for (std::size_t k = 0; k < curveHz.size(); ++k)
        if (curveHz[k] <= fHi)
          eqPts.emplace_back(xOfFreq(curveHz[k], p0.x, w, fLo, fHi),
                             yOfDb(curveDb[k], p0.y, h, -kEqScale, kEqScale));
      d->AddPolyline(eqPts.data(), static_cast<int>(eqPts.size()), IM_COL32(120, 200, 255, 220),
                     ImDrawFlags_None, 2.0f);
      d->AddText(ImVec2(p1.x - 130.0f, p0.y + 2.0f), IM_COL32(120, 200, 255, 200), "EQ (+/-18 dB)");
    }
    // Parametric nodes: drag = frequency/gain, wheel = Q.
    const ImVec2 m = ImGui::GetMousePos();
    if (eqMode == EqMode::Parametric && eqOn) {
      int nearest = -1;
      float nearestD = 14.0f;
      for (std::size_t i = 0; i < pbands.size(); ++i) {
        const auto& b = pbands[i];
        if (!b.on) continue;
        const float x = xOfFreq(std::clamp(static_cast<double>(b.freq), fLo, fHi), p0.x, w, fLo, fHi);
        const float y =
            yOfDb(hasGain(b.type) ? static_cast<double>(b.gain) : 0.0, p0.y, h, -kEqScale, kEqScale);
        const bool sel = static_cast<int>(i) == dragBand;
        d->AddCircleFilled(ImVec2(x, y), sel ? 7.0f : 5.5f, IM_COL32(120, 200, 255, sel ? 255 : 200));
        char num[8];
        std::snprintf(num, sizeof num, "%d", static_cast<int>(i + 1));
        d->AddText(ImVec2(x + 7.0f, y - 16.0f), IM_COL32(220, 235, 255, 230), num);
        const float dd = std::hypot(m.x - x, m.y - y);
        if (dd < nearestD) {
          nearestD = dd;
          nearest = static_cast<int>(i);
        }
      }
      if (clicked) dragBand = nearest;
      if (!active && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && !hovered) dragBand = -1;
      if (active && dragBand >= 0) {
        auto& b = pbands[static_cast<std::size_t>(dragBand)];
        b.freq = static_cast<float>(std::clamp(freqOfX(m.x, p0.x, w, fLo, fHi), 10.0, 0.45 * rate));
        if (hasGain(b.type)) {
          const double t = 1.0 - static_cast<double>((m.y - p0.y) / h);
          b.gain = static_cast<float>(std::clamp(-kEqScale + 2.0 * kEqScale * t, -18.0, 18.0));
        }
        paramsDirty = true;
      }
      const int wheelBand = dragBand >= 0 ? dragBand : nearest;
      if (hovered && wheelBand >= 0 && ImGui::GetIO().MouseWheel != 0.0f) {
        auto& b = pbands[static_cast<std::size_t>(wheelBand)];
        b.q = std::clamp(b.q * std::pow(1.15f, ImGui::GetIO().MouseWheel), 0.1f, 30.0f);
        paramsDirty = true;
      }
    }
    if (hovered) {
      const double f = freqOfX(m.x, p0.x, w, fLo, fHi);
      double lvl = -300.0;
      if (analyzer && !analyzer->spectrum().amplitude.empty()) {
        const auto& s = analyzer->spectrum();
        const auto k = static_cast<std::size_t>(f / s.binHz + 0.5);
        if (k < s.amplitude.size() && s.amplitude[k] > 0.0) lvl = 20.0 * std::log10(s.amplitude[k]);
      }
      char lv[32];
      if (lvl < static_cast<double>(dbFloor))
        std::snprintf(lv, sizeof lv, "below %.0f dBFS", static_cast<double>(dbFloor));
      else
        std::snprintf(lv, sizeof lv, "%.1f dBFS", lvl);
      char buf[160];
      std::snprintf(buf, sizeof buf, "%s  (%s)  %s%s", hzText(f).c_str(), noteText(f).c_str(), lv,
                    eqMode == EqMode::Parametric ? "\ndrag a node: freq/gain, wheel: Q" : "");
      tooltipAtMouse(buf);
    }
  }

  void drawSpectrogram(ImVec2 size) {
    ImDrawList* d = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("gram", size);
    const ImVec2 p1(p0.x + size.x, p0.y + size.y);
    if (gramTex && gramDirty) {
      SDL_UpdateTexture(gramTex, nullptr, gramPixels.data(), kGramW * 4);
      gramDirty = false;
    }
    if (gramTex)
      d->AddImage(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<std::intptr_t>(gramTex))), p0, p1);
    const double lo = 30.0, hi = std::min(20000.0, 0.5 * rate);
    for (double f : {100.0, 1000.0, 10000.0}) {
      if (f > hi) break;
      const float y = p1.y - (p1.y - p0.y) * static_cast<float>(std::log(f / lo) / std::log(hi / lo));
      d->AddLine(ImVec2(p0.x, y), ImVec2(p0.x + 8.0f, y), IM_COL32(255, 255, 255, 160));
      d->AddText(ImVec2(p0.x + 10.0f, y - 0.5f * ImGui::GetFontSize()), IM_COL32(255, 255, 255, 170),
                 bandLabel(f).c_str());
    }
    char buf[64];
    std::snprintf(buf, sizeof buf, "live, last %.1f s", static_cast<double>(kGramW) * static_cast<double>(hop) / rate);
    d->AddText(ImVec2(p1.x - ImGui::CalcTextSize(buf).x - 4.0f, p0.y + 2.0f), IM_COL32(255, 255, 255, 170), buf);
    if (ImGui::IsItemHovered()) {
      const ImVec2 m = ImGui::GetMousePos();
      const double f = lo * std::pow(hi / lo, static_cast<double>((p1.y - m.y) / size.y));
      const double ago =
          static_cast<double>((p1.x - m.x) / size.x) * kGramW * static_cast<double>(hop) / rate;
      char tip[96];
      std::snprintf(tip, sizeof tip, "%s (%s), %.2f s ago", hzText(f).c_str(), noteText(f).c_str(), ago);
      tooltipAtMouse(tip);
    }
  }

  void drawVMeter(const char* id, ImVec2 size, double v1, double v2, double lo, double hi, bool clip) {
    ImDrawList* d = ImGui::GetWindowDrawList();
    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, size);
    const ImVec2 p1(p0.x + size.x, p0.y + size.y);
    d->AddRectFilled(p0, p1, IM_COL32(14, 12, 30, 255));
    const float h = size.y - 18.0f;
    const float mid = p0.x + 0.5f * size.x;
    d->AddRectFilled(ImVec2(p0.x + 4.0f, yOfDb(v1, p0.y + 18.0f, h, lo, hi)), ImVec2(mid - 2.0f, p1.y), heat(0.55));
    d->AddRectFilled(ImVec2(mid + 2.0f, yOfDb(v2, p0.y + 18.0f, h, lo, hi)), ImVec2(p1.x - 4.0f, p1.y), heat(0.8));
    d->AddRectFilled(p0, ImVec2(p1.x, p0.y + 14.0f), clip ? IM_COL32(230, 40, 40, 255) : IM_COL32(60, 20, 20, 255));
    for (double db = lo; db <= hi; db += 12.0) {
      const float y = yOfDb(db, p0.y + 18.0f, h, lo, hi);
      d->AddLine(ImVec2(p0.x, y), ImVec2(p0.x + 3.0f, y), IM_COL32(255, 255, 255, 120));
    }
    if (ImGui::IsItemHovered()) {
      char buf[96];
      std::snprintf(buf, sizeof buf, "RMS %.1f dBFS | peak %.1f dBFS%s", v1, v2, clip ? "\nCLIPPED" : "");
      tooltipAtMouse(buf);
    }
  }

  // BS.1770 / EBU R128 readout of the output (what you hear).
  void drawLoudness() {
    auto fmt = [](double v, const char* unit) {
      char b[32];
      if (v <= kLoudnessFloor + 1.0)
        std::snprintf(b, sizeof b, "  --  %s", unit);
      else
        std::snprintf(b, sizeof b, "%6.1f %s", v, unit);
      return std::string(b);
    };
    if (!loudness) return;
    const auto& m = *loudness;
    const double tp = m.truePeakDbtp();
    ImGui::TextDisabled("Loudness (BS.1770%s):", loudnessChannels == 1 ? ", mono" : "");
    ImGui::SameLine();
    ImGui::Text("M %s   S %s   I %s   LRA %4.1f LU", fmt(m.momentaryLufs(), "LUFS").c_str(),
                fmt(m.shortTermLufs(), "LUFS").c_str(), fmt(m.integratedLufs(), "LUFS").c_str(),
                m.loudnessRangeLu());
    ImGui::SameLine();
    const bool over = tp > lim.ceilingDb + 0.05 && tp > kLoudnessFloor + 1.0;
    ImGui::TextColored(over ? ImVec4(1.0f, 0.45f, 0.35f, 1.0f) : ImVec4(0.85f, 0.85f, 0.9f, 1.0f),
                       "  TP %s", fmt(tp, "dBTP").c_str());
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("True peak (4x oversampled) of the output since the last reset.\n"
                        "Red: above the limiter ceiling. The limiter bounds sample\n"
                        "peaks; inter-sample peaks can be higher (see D24/D31).");
    ImGui::SameLine();
    if (ImGui::SmallButton("Reset meters")) resetLoudness();
  }

  // Horizontal gain-reduction bar, 0 .. -24 dB.
  static void grBar(const char* label, double grDb) {
    const float frac = static_cast<float>(std::clamp(-grDb / 24.0, 0.0, 1.0));
    char buf[48];
    std::snprintf(buf, sizeof buf, "%s %.1f dB", label, grDb);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.95f, 0.55f, 0.2f, 1.0f));
    ImGui::ProgressBar(frac, ImVec2(-1.0f, 0.0f), buf);
    ImGui::PopStyleColor();
  }

  // =========================================================== panels ==
  void eqGraphic(float width) {
    static const char* names[] = {"Flat", "Smile", "Bass boost", "Treble boost", "Vocal presence", "Cut lows"};
    static const float presets[6][10] = {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0},     {6, 5, 3, 0, -3, -3, 0, 3, 5, 6},
                                         {8, 6, 4, 1, 0, 0, 0, 0, 0, 0},     {0, 0, 0, 0, 0, 1, 3, 5, 6, 7},
                                         {-4, -3, -1, 0, 2, 4, 4, 2, 0, -1}, {-12, -9, -4, 0, 0, 0, 0, 0, 0, 0}};
    ImGui::SetNextItemWidth(width * 0.5f);
    if (ImGui::Combo("Preset", &preset, names, 6) || applyPreset) {
      applyPreset = false;
      preset = std::clamp(preset, 0, 5);
      for (std::size_t i = 0; i < gains.size(); ++i) gains[i] = i < 10 ? presets[preset][i] : 0.0f;
      paramsDirty = true;
    }
    static const char* solvers[] = {"ACGE (accurate cascade)", "Solved centres", "Plain"};
    ImGui::SetNextItemWidth(width * 0.5f);
    if (ImGui::Combo("Design", &graphicSolver, solvers, 3)) paramsDirty = true;
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("ACGE (Valimaki & Liski 2017): least-squares gains over centres AND\n"
                        "midpoints, within ~1 dB everywhere (measured <= 0.76 dB).\n"
                        "Solved centres (D25): exact at the centres, but can ripple up to\n"
                        "5 dB between them. Plain: sliders straight to the sections.");
    const auto bands = octaveBandsFor(rate);
    const std::size_t n = bands.size();
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float sw = n > 0 ? (width - spacing * static_cast<float>(n - 1)) / static_cast<float>(n) : 20.0f;
    const float sh = std::max(110.0f, ImGui::GetContentRegionAvail().y * 0.55f);
    for (std::size_t i = 0; i < n; ++i) {
      if (i > 0) ImGui::SameLine();
      ImGui::PushID(static_cast<int>(i));
      if (ImGui::VSliderFloat("##g", ImVec2(sw, sh), &gains[i], -kEqRangeDb, kEqRangeDb, "%+.0f"))
        paramsDirty = true;
      if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        gains[i] = 0.0f;
        paramsDirty = true;
      }
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s Hz: %+.1f dB (right-click: 0)", bandLabel(bands[i].center.value()).c_str(),
                          static_cast<double>(gains[i]));
      ImGui::PopID();
    }
    ImDrawList* d = ImGui::GetWindowDrawList();
    const ImVec2 lp = ImGui::GetCursorScreenPos();
    for (std::size_t i = 0; i < n; ++i) {
      const std::string lab = bandLabel(bands[i].center.value());
      const float cx = lp.x + (sw + spacing) * static_cast<float>(i) + 0.5f * sw;
      d->AddText(ImVec2(std::max(lp.x, cx - 0.5f * ImGui::CalcTextSize(lab.c_str()).x), lp.y),
                 IM_COL32(200, 200, 210, 220), lab.c_str());
    }
    ImGui::Dummy(ImVec2(width, ImGui::GetFontSize() + 2.0f));
  }

  void resetParametric() {
    pbands = {};
    pbands[0] = PBand{false, PType::LowCut, 30.0f, 0.0f, 0.707f, 1};
    pbands[1] = PBand{true, PType::LowShelf, 110.0f, 0.0f, 0.707f, 1};
    pbands[2] = PBand{true, PType::Peak, 400.0f, 0.0f, 1.0f, 1};
    pbands[3] = PBand{true, PType::Peak, 2500.0f, 0.0f, 1.0f, 1};
    pbands[4] = PBand{true, PType::HighShelf, 8000.0f, 0.0f, 0.707f, 1};
    pbands[5] = PBand{false, PType::HighCut, 18000.0f, 0.0f, 0.707f, 1};
    pbands[6] = PBand{false, PType::Notch, 60.0f, 0.0f, 8.0f, 1};
    pbands[7] = PBand{false, PType::Peak, 5000.0f, 0.0f, 2.0f, 1};
  }

  void eqParametric() {
    if (ImGui::BeginTable("pbands", 5, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 40.0f);
      ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 1.2f);
      ImGui::TableSetupColumn("Freq", ImGuiTableColumnFlags_WidthStretch, 1.3f);
      ImGui::TableSetupColumn("Gain", ImGuiTableColumnFlags_WidthStretch, 1.0f);
      ImGui::TableSetupColumn("Q / slope", ImGuiTableColumnFlags_WidthStretch, 1.1f);
      ImGui::TableHeadersRow();
      for (std::size_t i = 0; i < pbands.size(); ++i) {
        auto& b = pbands[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        char lab[8];
        std::snprintf(lab, sizeof lab, "%d", static_cast<int>(i + 1));
        if (ImGui::Checkbox(lab, &b.on)) paramsDirty = true;
        ImGui::BeginDisabled(!b.on);
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-1.0f);
        int t = static_cast<int>(b.type);
        if (ImGui::Combo("##t", &t, kPTypeNames, 7)) {
          b.type = static_cast<PType>(t);
          paramsDirty = true;
        }
        ImGui::TableSetColumnIndex(2);
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::SliderFloat("##f", &b.freq, 20.0f, 20000.0f, "%.0f Hz", ImGuiSliderFlags_Logarithmic))
          paramsDirty = true;
        ImGui::TableSetColumnIndex(3);
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::BeginDisabled(!hasGain(b.type));
        if (ImGui::SliderFloat("##g", &b.gain, -18.0f, 18.0f, "%+.1f")) paramsDirty = true;
        ImGui::EndDisabled();
        ImGui::TableSetColumnIndex(4);
        ImGui::SetNextItemWidth(-1.0f);
        if (hasSlope(b.type)) {
          static const char* slopes[] = {"12 dB/oct", "24 dB/oct", "36 dB/oct", "48 dB/oct"};
          if (ImGui::Combo("##s", &b.slope, slopes, 4)) paramsDirty = true;
        } else if (ImGui::SliderFloat("##q", &b.q, 0.1f, 30.0f, "Q %.2f", ImGuiSliderFlags_Logarithmic)) {
          paramsDirty = true;
        }
        ImGui::EndDisabled();
        ImGui::PopID();
      }
      ImGui::EndTable();
    }
    if (ImGui::Button("Reset bands")) {
      resetParametric();
      paramsDirty = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("cuts: Butterworth, exact -3 dB at the corner");
  }

  void eqMatch() {
    ImGui::TextWrapped("Match the track's long-term tonal balance to a reference recording "
                       "(third-octave bands, closed-loop design; level is not matched).");
    ImGui::BeginDisabled(busy() || e().trackFrames == 0);
    if (ImGui::Button("Choose reference WAV...")) {
      static const SDL_DialogFileFilter filters[] = {{"WAV audio", "wav;wave"}};
      SDL_ShowOpenFileDialog(&App::onRefChosen, this, window, filters, 1, nullptr, false);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", refName.empty() ? "(none)" : refName.c_str());
    ImGui::SetNextItemWidth(200.0f);
    float pct = matchAmount * 100.0f;
    if (ImGui::SliderFloat("Amount", &pct, 0.0f, 100.0f, "%.0f %%")) {
      matchAmount = pct / 100.0f;
      redesignMatch();
      paramsDirty = true;
    }
    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::SliderFloat("Ignore below peak", &matchRelFloor, 20.0f, 90.0f, "%.0f dB")) {
      redesignMatch();
      paramsDirty = true;
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Bands this far below a signal's loudest band hold no real content\n"
                        "(only window leakage in tonal material) and are left at 0 dB.");
    if (match) {
      // Wanted change per band (bars) vs the design's predicted change (line).
      ImDrawList* d = ImGui::GetWindowDrawList();
      const ImVec2 p0 = ImGui::GetCursorScreenPos();
      const ImVec2 sz(ImGui::GetContentRegionAvail().x, 130.0f);
      ImGui::InvisibleButton("matchplot", sz);
      const ImVec2 p1(p0.x + sz.x, p0.y + sz.y);
      d->AddRectFilled(p0, p1, IM_COL32(14, 12, 30, 255));
      const float y0 = yOfDb(0.0, p0.y, sz.y, -12.0, 12.0);
      d->AddLine(ImVec2(p0.x, y0), ImVec2(p1.x, y0), IM_COL32(255, 255, 255, 50));
      const std::size_t n = match->bands.size();
      const float slot = sz.x / static_cast<float>(n);
      std::vector<ImVec2> pred;
      for (std::size_t i = 0; i < n; ++i) {
        const float x0 = p0.x + slot * static_cast<float>(i) + 1.0f;
        const float y = yOfDb(match->correctionDb[i], p0.y, sz.y, -12.0, 12.0);
        d->AddRectFilled(ImVec2(x0, std::min(y, y0)), ImVec2(x0 + slot - 2.0f, std::max(y, y0)),
                         match->matched[i] ? IM_COL32(205, 55, 80, 200) : IM_COL32(90, 90, 110, 120));
        pred.emplace_back(x0 + 0.5f * slot, yOfDb(match->predictedDb[i], p0.y, sz.y, -12.0, 12.0));
      }
      d->AddPolyline(pred.data(), static_cast<int>(pred.size()), IM_COL32(252, 240, 170, 230), ImDrawFlags_None,
                     2.0f);
      d->AddText(ImVec2(p0.x + 4.0f, p0.y + 2.0f), IM_COL32(220, 220, 230, 200),
                 "bars: wanted change   line: predicted (+/-12 dB)");
    }
  }

  void panelEq(float width) {
    int m = static_cast<int>(eqMode);
    if (ImGui::RadioButton("Graphic", m == 0)) m = 0;
    ImGui::SameLine();
    if (ImGui::RadioButton("Parametric", m == 1)) m = 1;
    ImGui::SameLine();
    if (ImGui::RadioButton("Match", m == 2)) m = 2;
    if (m != static_cast<int>(eqMode)) {
      eqMode = static_cast<EqMode>(m);
      paramsDirty = true;
    }
    if (ImGui::Checkbox("EQ on", &eqOn)) paramsDirty = true;
    ImGui::SameLine();
    if (ImGui::Checkbox("Auto headroom", &autoHeadroom)) paramsDirty = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lower the output by the EQ's largest boost (exact response maximum).");
    ImGui::SetNextItemWidth(width * 0.6f);
    if (ImGui::SliderFloat("Volume", &volumeDb, -30.0f, 12.0f, "%+.1f dB")) paramsDirty = true;
    static const char* targets[] = {"Stereo", "Mid only", "Side only"};
    ImGui::SetNextItemWidth(width * 0.4f);
    if (ImGui::Combo("Apply EQ to", &eqTarget, targets, 3)) paramsDirty = true;
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Mid/side (D37): EQ the centre (M = (L+R)/2) or the width\n"
                        "(S = (L-R)/2) only. Switching fades the EQ out and back in.");
    ImGui::Separator();
    if (eqMode == EqMode::Graphic) eqGraphic(width);
    if (eqMode == EqMode::Parametric) eqParametric();
    if (eqMode == EqMode::Match) eqMatch();
    ImGui::TextDisabled("max boost %+.1f dB", eqOn ? eqMaxDb : 0.0);
    if (!eqNote.empty()) ImGui::TextDisabled("%s", eqNote.c_str());
  }

  // The limiter's mode sets its latency, so it is rebuilt (with its matched
  // dry delay) under the active stream's lock: a structural change.
  void rebuildLimiter() {
    SDL_AudioStream* s = e().mode.load() == kMic ? mic : play;
    if (s) SDL_LockAudioStream(s);
    e().truePeakMode = lim.truePeak;
    auto li = Limiter::create(LimiterParams{lim.ceilingDb, app::kLimiterLookaheadMs,
                                            lim.releaseMs, lim.truePeak},
                              SampleRateHz{rate});
    if (li.ok()) {
      e().lim = std::move(li).value();
      for (auto& d : e().limDry) d.resize(e().lim->latencyFrames());
      e().haveParams = false;
    }
    if (s) SDL_UnlockAudioStream(s);
    paramsDirty = true;
  }

  void sliderD(const char* label, double& v, float lo, float hi, const char* fmt, bool logs = false) {
    float f = static_cast<float>(v);
    if (ImGui::SliderFloat(label, &f, lo, hi, fmt, logs ? ImGuiSliderFlags_Logarithmic : 0)) {
      v = static_cast<double>(f);
      paramsDirty = true;
    }
  }

  void panelDynamics() {
    ImGui::SeparatorText("Compressor (feed-forward, soft knee, linked)");
    if (ImGui::Checkbox("On##comp", &compOn)) paramsDirty = true;
    ImGui::SameLine();
    static const char* det[] = {"Peak", "RMS"};
    int dm = comp.detector == LevelDetector::Rms ? 1 : 0;
    ImGui::SetNextItemWidth(90.0f);
    if (ImGui::Combo("Detector", &dm, det, 2)) {
      comp.detector = dm == 1 ? LevelDetector::Rms : LevelDetector::Peak;
      paramsDirty = true;
    }
    sliderD("Threshold", comp.thresholdDb, -60.0f, 0.0f, "%.1f dB");
    sliderD("Ratio", comp.ratio, 1.0f, 20.0f, "%.1f : 1", true);
    sliderD("Knee", comp.kneeDb, 0.0f, 24.0f, "%.1f dB");
    sliderD("Attack", comp.attackMs, 0.1f, 200.0f, "%.1f ms", true);
    sliderD("Release", comp.releaseMs, 5.0f, 2000.0f, "%.0f ms", true);
    if (ImGui::Checkbox("Auto makeup", &autoMakeup)) paramsDirty = true;
    if (!autoMakeup) {
      ImGui::SameLine();
      ImGui::SetNextItemWidth(160.0f);
      sliderD("Makeup", comp.makeupDb, -12.0f, 24.0f, "%+.1f dB");
    }
    grBar("GR", compOn ? static_cast<double>(e().compGrDb.load()) : 0.0);
    {
      // Static curve: output level vs input level (dB), makeup included.
      ImDrawList* d = ImGui::GetWindowDrawList();
      const float side = std::min(ImGui::GetContentRegionAvail().x, 170.0f);
      const ImVec2 p0 = ImGui::GetCursorScreenPos();
      ImGui::InvisibleButton("curve", ImVec2(side, side));
      const ImVec2 p1(p0.x + side, p0.y + side);
      d->AddRectFilled(p0, p1, IM_COL32(14, 12, 30, 255));
      // x: input -60..0 dB; y: output -60..+30 dB (room for makeup).
      const float y0db = yOfDb(0.0, p0.y, side, -60.0, 30.0);
      d->AddLine(ImVec2(p0.x, p1.y), ImVec2(p1.x, y0db), IM_COL32(255, 255, 255, 40));
      d->AddLine(ImVec2(p0.x, y0db), ImVec2(p1.x, y0db), IM_COL32(255, 255, 255, 25));
      const double mk = autoMakeup ? autoMakeupDb(comp) : comp.makeupDb;
      std::vector<ImVec2> pts;
      for (int i = 0; i <= 120; ++i) {
        const double x = -60.0 + 60.0 * i / 120.0;
        pts.emplace_back(p0.x + side * static_cast<float>((x + 60.0) / 60.0),
                         yOfDb(compressorCurveDb(comp, x) + mk, p0.y, side, -60.0, 30.0));
      }
      d->AddPolyline(pts.data(), static_cast<int>(pts.size()), IM_COL32(245, 150, 40, 255), ImDrawFlags_None, 2.0f);
      const float xt = p0.x + side * static_cast<float>((comp.thresholdDb + 60.0) / 60.0);
      d->AddLine(ImVec2(xt, p0.y), ImVec2(xt, p1.y), IM_COL32(120, 200, 255, 90));
      d->AddText(ImVec2(p0.x + 3.0f, p0.y + 2.0f), IM_COL32(220, 220, 230, 200), "out vs in (dB, 0 dB line)");
    }
    ImGui::SeparatorText("Limiter (look-ahead, ceiling guaranteed)");
    if (ImGui::Checkbox("On##lim", &limOn)) paramsDirty = true;
    ImGui::SameLine();
    ImGui::SameLine();
    if (ImGui::Checkbox("True peak", &lim.truePeak)) rebuildLimiter();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Detect on the 4x interpolated signal (BS.1770 true peak) so\n"
                        "inter-sample peaks are held near the ceiling too (measured\n"
                        "residual <= 0.2 dB). Adds %zu samples of latency.",
                        Limiter::kTruePeakExtra);
    ImGui::TextDisabled("latency %.2f ms",
                        e().lim ? static_cast<double>(e().lim->latencyFrames()) * 1e3 / rate : 0.0);
    sliderD("Ceiling", lim.ceilingDb, -12.0f, 0.0f, "%.1f dBFS");
    sliderD("Release##lim", lim.releaseMs, 5.0f, 500.0f, "%.0f ms", true);
    grBar("GR", limOn ? static_cast<double>(e().limGrDb.load()) : 0.0);
    ImGui::TextDisabled(lim.truePeak ? "True-peak mode: inter-sample peaks held within ~0.2 dB."
                                     : "Sample peaks only; inter-sample peaks can exceed it.");

    ImGui::SeparatorText("De-esser (dynamic EQ band)");
    ImGui::PushID("deess");
    if (ImGui::Checkbox("On", &deessOn)) paramsDirty = true;
    float f = static_cast<float>(deess.frequency.value());
    if (ImGui::SliderFloat("Frequency", &f, 2000.0f, 12000.0f, "%.0f Hz", ImGuiSliderFlags_Logarithmic)) {
      deess.frequency = FrequencyHz{std::min(static_cast<double>(f), 0.45 * rate)};
      paramsDirty = true;
    }
    sliderD("Threshold", deess.thresholdDb, -60.0f, 0.0f, "%.1f dB");
    sliderD("Ratio", deess.ratio, 1.0f, 20.0f, "%.1f : 1", true);
    sliderD("Range", deess.rangeDb, 0.0f, 24.0f, "%.1f dB");
    sliderD("Q", deess.q, 0.5f, 8.0f, "%.2f", true);
    grBar("Cut", deessOn ? static_cast<double>(e().deessGrDb.load()) : 0.0);
    ImGui::PopID();

    ImGui::SeparatorText("Multiband compressor (3 bands, LR4)");
    ImGui::PushID("mb");
    if (ImGui::Checkbox("On", &mbOn)) paramsDirty = true;
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderFloat("Low split", &mbLowHz, 60.0f, 600.0f, "%.0f Hz", ImGuiSliderFlags_Logarithmic);
    if (ImGui::IsItemDeactivatedAfterEdit()) rebuildMultiband();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderFloat("High split", &mbHighHz, 1000.0f, 8000.0f, "%.0f Hz", ImGuiSliderFlags_Logarithmic);
    if (ImGui::IsItemDeactivatedAfterEdit()) rebuildMultiband();
    static const char* bandNames[] = {"Low", "Mid", "High"};
    for (std::size_t b = 0; b < 3; ++b) {
      ImGui::PushID(static_cast<int>(b));
      if (ImGui::Checkbox(bandNames[b], &mb.bandOn[b])) paramsDirty = true;
      ImGui::SameLine();
      ImGui::SetNextItemWidth(110.0f);
      sliderD("Thr", mb.band[b].thresholdDb, -60.0f, 0.0f, "%.0f dB");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(90.0f);
      sliderD("Ratio", mb.band[b].ratio, 1.0f, 20.0f, "%.1f", true);
      grBar("GR", mbOn ? static_cast<double>(e().mbGrDb[b].load()) : 0.0);
      ImGui::PopID();
    }
    ImGui::TextDisabled("Bands sum to an exact all-pass when neutral (flat magnitude).");
    ImGui::PopID();
  }

  // Multiband splits are structural (filter design), rebuilt under the lock.
  void rebuildMultiband() {
    if (!(mbHighHz > 1.5f * mbLowHz)) mbHighHz = 1.6f * mbLowHz;
    SDL_AudioStream* st = e().mode.load() == kMic ? mic : play;
    if (st) SDL_LockAudioStream(st);
    e().mbLowHz = static_cast<double>(mbLowHz);
    e().mbHighHz = static_cast<double>(mbHighHz);
    MultibandParams mp = mb;
    mp.lowSplitHz = e().mbLowHz;
    mp.highSplitHz = std::min(e().mbHighHz, 0.4 * rate);
    auto m = MultibandCompressor::create(mp, SampleRateHz{rate});
    if (m.ok()) {
      e().mb = std::move(m).value();
      e().haveParams = false;
    }
    if (st) SDL_UnlockAudioStream(st);
    paramsDirty = true;
  }

  void panelTransform() {
    ImGui::TextWrapped("Offline pitch shift / time stretch of the loaded track (the original is "
                       "kept; Revert restores it).");
    ImGui::RadioButton("PSOLA (voice, solo instrument; pYIN marks)", &method, 0);
    ImGui::RadioButton("Phase vocoder, classic (tones exact)", &method, 1);
    ImGui::RadioButton("Phase vocoder, PGHI (sharper transients)", &method, 2);
    ImGui::RadioButton("Hybrid: harmonic/percussive split", &method, 3);
    ImGui::SliderFloat("Pitch", &shiftSemis, -12.0f, 12.0f, "%+.1f semitones");
    ImGui::SliderFloat("Tempo", &tempoPct, 50.0f, 200.0f, "%.0f %%");
    ImGui::BeginDisabled(busy() || original.empty());
    if (ImGui::Button("Render")) startTransform();
    ImGui::SameLine();
    if (ImGui::Button("Revert to original")) {
      auto o = original;
      if (installTrack(std::move(o), originalFrames, originalRate, originalName)) status = "Reverted: " + originalName;
    }
    ImGui::EndDisabled();
    if (busy() && job->kind == Job::Transform) ImGui::TextDisabled("rendering...");
    ImGui::Separator();
    static const char* notes[] = {
        "PSOLA moves the harmonics but keeps each period's waveform, so formants stay put "
        "(a natural voice). Needs one periodic source.",
        "Classic vocoder with phase locking: tones exact (1e-6 Hz), but attacks smear "
        "(29 % of a click's energy within 1 ms at x1.5, measured).",
        "PGHI (Prusa & Holighaus): phase integrated along time AND frequency; attacks stay "
        "compact (53 % within 1 ms at x1.5), tones within 4e-5.",
        "Hybrid (Driedger et al.): harmonic part through PGHI, percussive part through short "
        "overlap-add; on our click test it matches PGHI (77 % within 5 ms)."};
    ImGui::TextWrapped("%s", notes[std::clamp(method, 0, 3)]);
    if (method > 0)
      ImGui::TextWrapped("Any vocoder pitch shift moves the formants too (the 'chipmunk' effect).");
  }

  void panelSettings() {
    ImGui::SeparatorText("Smooth cuts");
    ImGui::SliderFloat("Crossfade", &fadeMs, 1.0f, 100.0f, "%.0f ms");
    if (ImGui::IsItemDeactivatedAfterEdit()) rebuildReader();
    static const char* curves[] = {"Linear (correlated)", "Raised cosine", "Equal power (uncorrelated)"};
    if (ImGui::Combo("Curve", &fadeCurve, curves, 3)) rebuildReader();
    ImGui::TextWrapped("Seeks, loop seams and A-B loops crossfade over this time; play/pause ramps "
                       "5 ms; EQ, compressor and limiter switches crossfade 20 ms.");
    ImGui::SeparatorText("Analyzer");
    int bpo = bandsPerOctave == 3 ? 1 : 0;
    if (ImGui::RadioButton("1/3 octave", bpo == 1)) bpo = 1;
    ImGui::SameLine();
    if (ImGui::RadioButton("octave", bpo == 0)) bpo = 0;
    if ((bpo == 1 ? 3 : 1) != bandsPerOctave) {
      bandsPerOctave = bpo == 1 ? 3 : 1;
      rebuildAnalyzer();
    }
    ImGui::SliderFloat("Floor", &dbFloor, -140.0f, -40.0f, "%.0f dB");
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      rebuildAnalyzer();
      refreshOverviewPixels();
    }
    ImGui::SeparatorText("Export");
    ImGui::Checkbox("Normalize loudness on export", &exportNormalize);
    ImGui::BeginDisabled(!exportNormalize);
    {
      const char* names[4];
      for (int i = 0; i < 4; ++i) names[i] = kExportTargets[i].name;
      ImGui::Combo("Target", &exportTarget, names, 4);
      ImGui::SliderFloat("True-peak ceiling", &exportCeilingDb, -6.0f, 0.0f, "%.1f dBTP");
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Gain to the target integrated loudness (BS.1770-5), then the\n"
                        "true-peak limiter at the ceiling if needed; the result reports\n"
                        "honestly if the target cannot be reached under the ceiling.");
    ImGui::Checkbox("Linear-phase EQ in export", &exportLinearPhase);
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("D37: the same EQ magnitude as an 8191-tap symmetric FIR (exact linear\n"
                        "phase, no phase smear; pre-ringing instead). Export only: live\n"
                        "playback stays minimum-phase. Latency is compensated.");
    ImGui::BeginDisabled(busy() || e().trackFrames == 0);
    if (ImGui::Button("Export processed WAV (24-bit stereo)...")) {
      static const SDL_DialogFileFilter filters[] = {{"WAV audio", "wav"}};
      SDL_ShowSaveFileDialog(&App::onWavChosen, this, window, filters, 1, nullptr);
    }
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("Renders the whole track through the same engine code you hear\n"
                        "(EQ, compressor, volume, limiter), aligned to the source.");
    if (!lastExport.empty()) ImGui::TextWrapped("%s", lastExport.c_str());
    ImGui::BeginDisabled(!overview);
    if (ImGui::Button("Export track spectrogram as SVG...")) {
      static const SDL_DialogFileFilter filters[] = {{"SVG image", "svg"}};
      SDL_ShowSaveFileDialog(&App::onSvgChosen, this, window, filters, 1, nullptr);
    }
    ImGui::EndDisabled();
  }

  void exportSvg(const std::string& path) {
    if (!overview) return;
    PlotStyle st;
    st.dbMin = static_cast<double>(dbFloor);
    st.dbMax = 0.0;
    st.width = 1100;
    st.height = 460;
    st.title = "Spectrogram: " + trackName;
    auto svg = svgSpectrogram(*overview, FrequencyHz{std::min(20000.0, 0.5 * rate)}, 900, 300, st);
    std::string p = path;
    if (p.size() < 4 || p.substr(p.size() - 4) != ".svg") p += ".svg";
    if (!svg.ok()) {
      status = "SVG export failed.";
      return;
    }
    auto w = writeTextFile(p, svg.value());
    status = w.ok() ? "Wrote " + p : "Cannot write " + p;
  }

  void selectReference(int index) {
    WavData w;
    if (!referenceDemo.select(index, w)) return;
    referenceView = true;
    tunerView = false;
    paramsDirty = true;
    const auto& d = referenceDemo.data;
    referenceKind = d.kind == "speech" ? 2 : d.mode == "pitch" ? 1 : 0;
    loadWav(w, d.id);
  }

  void drawReferencePlot(double position, bool chordPlot) {
    const auto& d = referenceDemo.data;
    const auto& m = referenceDemo.measured;
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 size(std::max(240.0f, ImGui::GetContentRegionAvail().x), chordPlot ? 124.0f : 150.0f);
    ImGui::InvisibleButton(chordPlot ? "reference-chords" : "reference-notes", size);
    auto* draw = ImGui::GetWindowDrawList();
    const float margin = ImGui::CalcTextSize("Fresh result").x + 22.0f;
    const float left = origin.x + margin, width = size.x - margin - 11.0f;
    auto x = [&](double t) { return left + static_cast<float>(t / d.duration) * width; };
    const ImU32 refColor = IM_COL32(105, 212, 182, 255), actualColor = IM_COL32(115, 170, 255, 255);
    draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(22, 29, 35, 255), 5);
    for (int i = 0; i <= 5; ++i) {
      const double time = d.duration * i / 5;
      char label[24]; std::snprintf(label, sizeof label, "%.1fs", time);
      draw->AddLine(ImVec2(x(time), origin.y + 6), ImVec2(x(time), origin.y + size.y - 20), IM_COL32(50, 61, 70, 255));
      draw->AddText(ImVec2(std::min(x(time), origin.x + size.x - 45), origin.y + size.y - 17), IM_COL32(180, 191, 200, 255), label);
    }
    if (chordPlot) {
      draw->AddText(ImVec2(origin.x + 7, origin.y + 14), refColor, "Reference");
      draw->AddText(ImVec2(origin.x + 7, origin.y + 47), actualColor, "Fresh result");
      draw->AddText(ImVec2(origin.x + 7, origin.y + 80), IM_COL32(200, 200, 200, 255), "Comparison");
      for (const auto& c : d.chords) {
        draw->AddRectFilled(ImVec2(x(c.start), origin.y + 11), ImVec2(x(c.end), origin.y + 35), refColor);
        if (x(c.end) - x(c.start) > 68) {
          draw->PushClipRect(ImVec2(x(c.start), origin.y + 11), ImVec2(x(c.end), origin.y + 35), true);
          draw->AddText(ImVec2(x(c.start) + 3, origin.y + 15), IM_COL32(10, 30, 25, 255), c.label.c_str());
          draw->PopClipRect();
        }
      }
      const auto& cells = referenceDemo.result.comparisons;
      for (std::size_t i = 0; i < cells.size();) {
        const auto& c = cells[i];
        double end = c.end;
        std::size_t next = i + 1;
        while (next < cells.size() && cells[next].actual == c.actual && cells[next].label == c.label && std::abs(cells[next].start - end) < 1e-8)
          end = cells[next++].end;
        draw->AddRectFilled(ImVec2(x(c.start), origin.y + 45), ImVec2(x(end) - .5f, origin.y + 68), actualColor);
        if (x(end) - x(c.start) > 36) {
          draw->PushClipRect(ImVec2(x(c.start), origin.y + 45), ImVec2(x(end), origin.y + 68), true);
          draw->AddText(ImVec2(x(c.start) + 3, origin.y + 48), IM_COL32(10, 22, 40, 255), app::reference::actualName(c).c_str());
          draw->PopClipRect();
        }
        i = next;
      }
      for (const auto& c : cells) {
        const auto color = c.expected < 0 ? IM_COL32(97, 103, 111, 255) : c.expected == c.actual ? refColor : IM_COL32(226, 104, 102, 255);
        draw->AddRectFilled(ImVec2(x(c.start), origin.y + 79), ImVec2(x(c.end) + .4f, origin.y + 95), color);
      }
    } else {
      auto y = [&](double midi) { return origin.y + 126.0f - static_cast<float>((std::clamp(midi, 36.0, 96.0) - 36) / 60) * 112.0f; };
      draw->AddText(ImVec2(origin.x + 7, origin.y + 10), refColor, "C7");
      draw->AddText(ImVec2(origin.x + 7, origin.y + 62), actualColor, "Pitch");
      draw->AddText(ImVec2(origin.x + 7, origin.y + 112), refColor, "C2");
      for (const auto& n : d.notes)
        draw->AddRectFilled(ImVec2(x(n.start), y(n.midi) - 2), ImVec2(x(n.end), y(n.midi) + 2), refColor);
      for (const auto& p : m.pitch)
        if (p.estimate.voiced) draw->AddCircleFilled(ImVec2(x(p.timeSeconds), y(69 + 12 * std::log2(p.estimate.frequencyHz / 440))), 1.5f, actualColor);
    }
    draw->AddLine(ImVec2(x(position), origin.y + 2), ImVec2(x(position), origin.y + size.y - 19), IM_COL32(255, 220, 125, 255), 2);
    if (ImGui::IsItemHovered()) {
      const double time = std::clamp(static_cast<double>((ImGui::GetIO().MousePos.x - left) / width) * d.duration, 0.0, d.duration);
      ImGui::SetTooltip("%.3f s - click to seek", time);
      if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) seek(time);
    }
  }

  void drawReferenceDemo() {
    auto& b = referenceDemo;
    app::ui::heading("Reference recordings");
    ImGui::BeginDisabled(b.busy());
    if (ImGui::Button("Open reference suite...")) {
      static const SDL_DialogFileFilter filter[] = {{"Reference suite", "tsv"}};
      SDL_ShowOpenFileDialog(&App::onSuiteChosen, this, window, filter, 1, nullptr, false);
    }
    ImGui::EndDisabled();
    ImGui::SameLine(); app::ui::caption("Expected notes and words, compared with the actual reading");
    if (ImGui::CollapsingHeader("Saved collection results")) ImGui::TextWrapped("%s", b.summary.c_str());
    if (!b.error.empty()) ImGui::TextColored(ImVec4(1, .55f, .5f, 1), "%s", b.error.c_str());
    if (b.entries.empty()) {
      ImGui::TextWrapped("Open reference-suite.tsv from the prepared audio collection. It contains real guitar recordings with chord/note labels and speech with published transcripts.");
      return;
    }
    ImGui::Separator();
    ImGui::BeginDisabled(b.busy());
    const char* kinds[] = {"Guitar chords", "Guitar pitch", "Spoken words"};
    for (int kind = 0; kind < 3; ++kind) {
      if (kind) ImGui::SameLine();
      if (ImGui::RadioButton(kinds[kind], referenceKind == kind)) {
        for (std::size_t i = 0; i < b.entries.size(); ++i) {
          const auto& entry = b.entries[i];
          const int k = entry.kind == "speech" ? 2 : entry.mode == "pitch" ? 1 : 0;
          if (kind == k) { selectReference(static_cast<int>(i)); break; }
        }
      }
    }
    ImGui::SetNextItemWidth(std::min(480.0f, ImGui::GetContentRegionAvail().x));
    if (ImGui::BeginCombo("Recording", b.selected >= 0 ? b.data.id.c_str() : "Choose a recording")) {
      for (std::size_t i = 0; i < b.entries.size(); ++i) {
        const auto& entry = b.entries[i];
        const int kind = entry.kind == "speech" ? 2 : entry.mode == "pitch" ? 1 : 0;
        if (kind == referenceKind && ImGui::Selectable(entry.id.c_str(), b.selected == static_cast<int>(i))) selectReference(static_cast<int>(i));
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine(); if (ImGui::Button("Run recognition again") && b.selected >= 0) selectReference(b.selected);
    ImGui::EndDisabled();
    if (b.selected < 0) return;
    const auto& d = b.data;
    const double position = static_cast<double>(e().shownPos.load()) / rate;
    if (ImGui::Button(e().playing.load() ? "Pause" : "Play", ImVec2(80, 0))) setPlaying(!e().playing.load());
    ImGui::SameLine(); if (ImGui::Button("Stop")) { setPlaying(false); seek(0); }
    ImGui::SameLine();
    if (ImGui::Button("Next mismatch")) {
      double target = -1;
      for (const auto& c : b.result.comparisons)
        if (c.expected >= 0 && c.expected != c.actual) { if (target < 0) target = c.start; if (c.start > position + .1) { target = c.start; break; } }
      if (target >= 0) seek(target);
    }
    ImGui::SameLine(); ImGui::Text("%.2f / %.2f s", position, d.duration);
    float pos = static_cast<float>(position);
    ImGui::SetNextItemWidth(-1);
    if (ImGui::SliderFloat("##reference-position", &pos, 0, static_cast<float>(d.duration), "%.3f s")) seek(pos);
    ImGui::TextWrapped("%s", d.stats.c_str());
    if (d.mode == "chords") ImGui::TextWrapped("MMathEn recognizes chord quality and bass locally. The timeline score below compares major/minor only.");
    if (b.busy()) ImGui::TextUnformatted("Analyzing the original WAV on a worker thread...");
    else if (b.ready && d.kind != "speech") {
      if (b.result.eligible > 0) ImGui::TextColored(ImVec4(.45f, .8f, 1, 1), "%s: %.1f%% %s on %.2f %s", b.measured.engine.c_str(), 100 * b.result.accuracy(), d.mode == "chords" ? "major/minor agreement" : "within 50 cents", b.result.eligible, d.mode == "chords" ? "supported seconds" : "eligible frames");
      else ImGui::TextUnformatted("Fresh result: not scored - no supported reference segments.");
    }
    ImGui::Separator();
    if (d.mode == "chords") {
      const app::reference::Chord* expected = nullptr;
      const app::reference::Comparison* actual = nullptr;
      for (const auto& c : d.chords) if (position >= c.start && position < c.end) expected = &c;
      for (const auto& c : b.result.comparisons) if (position >= c.start && position < c.end) actual = &c;
      ImGui::Text("Reference: %s", expected ? expected->label.c_str() : "unannotated");
      ImGui::SameLine(380 * app::ui::unit()); ImGui::Text("Fresh chord: %s", actual ? app::reference::actualName(*actual).c_str() : "uncovered / analyzing");
      if (actual) ImGui::TextColored(actual->expected < 0 ? ImVec4(.7f, .7f, .7f, 1) : actual->expected == actual->actual ? ImVec4(.5f, 1, .75f, 1) : ImVec4(1, .55f, .5f, 1), "%s", actual->expected < 0 ? "UNSCORED: chord outside the major/minor vocabulary" : actual->expected == actual->actual ? "MATCH under major/minor rules" : "MISMATCH under major/minor rules");
      drawReferencePlot(position, true);
      if (ImGui::CollapsingHeader("Compare chord shapes / notes")) {
        ImGui::TextWrapped("Example voicings in standard guitar tuning. The reference shape uses the supported major/minor reduction, so extensions may be omitted; finger positions are not measured.");
        if (ImGui::BeginTable("reference-shapes", 2)) {
          ImGui::TableNextColumn(); ImGui::TextUnformatted("Expected triad");
          ImGui::PushID("expected-shape");
          if (const auto* shape = app::tuner::shapeForState(expected ? expected->state : -1))
            app::tuner::diagram(*shape, 290 * app::ui::unit());
          else ImGui::TextUnformatted("No supported shape available");
          ImGui::PopID();
          ImGui::TableNextColumn(); ImGui::TextUnformatted("Detected triad");
          ImGui::PushID("detected-shape");
          if (const auto* shape = app::tuner::shapeForState(actual ? actual->actual : -1))
            app::tuner::diagram(*shape, 290 * app::ui::unit());
          else ImGui::TextUnformatted("No supported shape available");
          ImGui::PopID();
          ImGui::EndTable();
        }
      }
    } else if (b.ready && !b.measured.pitch.empty()) {
      const auto& points = b.measured.pitch;
      const auto it = std::lower_bound(points.begin(), points.end(), position, [](const PitchFrame& p, double t) { return p.timeSeconds < t; });
      std::size_t index = it == points.end() ? points.size() - 1 : static_cast<std::size_t>(it - points.begin());
      if (index && std::abs(points[index - 1].timeSeconds - position) < std::abs(points[index].timeSeconds - position)) --index;
      const auto& p = points[index];
      const bool covered = std::abs(p.timeSeconds - position) <= 256 / b.measured.rate;
      if (covered && p.estimate.voiced) ImGui::Text("Fresh YIN pitch: %.2f Hz (%s)", p.estimate.frequencyHz, noteText(p.estimate.frequencyHz).c_str());
      else ImGui::TextUnformatted(covered ? "Fresh YIN pitch: unvoiced" : "Fresh YIN pitch: uncovered");
      if (d.kind != "speech" && covered && d.pitch[index].hz > 0) {
        ImGui::SameLine(); ImGui::Text("| Reference %.2f Hz", d.pitch[index].hz);
        if (p.estimate.voiced) { ImGui::SameLine(); ImGui::Text("| Error %+.1f cents", 1200 * std::log2(p.estimate.frequencyHz / d.pitch[index].hz)); }
      } else { ImGui::SameLine(); ImGui::TextDisabled("| Pitch reference: not available for this window"); }
    }
    std::string sounding;
    for (const auto& n : d.notes) if (position >= n.start && position < n.end) {
      const auto name = midiToLabel(static_cast<int>(std::lround(n.midi)));
      if (!sounding.empty()) sounding += ", ";
      if (name.ok()) sounding += name.value();
    }
    if (d.kind == "guitar") {
      if (d.notes.empty()) ImGui::TextUnformatted("Individual note pitches and timings are not annotated for this recording.");
      else ImGui::TextWrapped("Annotated notes sounding: %s", sounding.empty() ? "none" : sounding.c_str());
    }
    if (!d.notes.empty() || d.kind == "speech") drawReferencePlot(position, false);
    ImGui::TextDisabled("%s", d.mode == "chords" ? "Green: all annotated notes. Red: chord mismatch. Gray: unscored. Click a plot to seek." : d.kind == "speech" ? "Blue: fresh voice-pitch estimate. No verified speech-pitch reference. Click to seek." : "Green: annotated notes. Blue: fresh voiced pitch. Only isolated note windows are scored. Click to seek.");
    if (d.kind == "speech") {
      ImGui::TextWrapped("Published transcript: %s", d.transcript.c_str());
      ImGui::TextWrapped("Whisper base.en (saved separate recognizer): %s", d.estimate.c_str());
      std::string word;
      for (const auto& w : d.words) if (position >= w.start && position < w.end) word = w.text;
      ImGui::TextColored(ImVec4(.5f, .8f, 1, 1), "Current word (estimated timing): %s", word.empty() ? "--" : word.c_str());
      if (ImGui::CollapsingHeader("Word-by-word errors")) {
        for (const auto& edit : d.edits) if (edit.operation != "match")
          ImGui::TextWrapped("%s: expected [%s], recognized [%s]", edit.operation.c_str(), edit.expected.c_str(), edit.actual.c_str());
      }
    }
    if (!d.notes.empty() && ImGui::CollapsingHeader("All reference notes / exact timings")) {
      if (ImGui::BeginTable("reference-notes", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 160))) {
        for (const char* h : {"Start (s)", "End (s)", "MIDI pitch", "Hz from MIDI", "String index"}) ImGui::TableSetupColumn(h);
        ImGui::TableHeadersRow();
        for (const auto& n : d.notes) {
          ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::Text("%.6f", n.start);
          ImGui::TableNextColumn(); ImGui::Text("%.6f", n.end);
          ImGui::TableNextColumn(); ImGui::Text("%.4f", n.midi);
          ImGui::TableNextColumn(); ImGui::Text("%.3f", 440 * std::pow(2, (n.midi - 69) / 12));
          ImGui::TableNextColumn(); ImGui::Text("%d", n.string);
        }
        ImGui::EndTable();
      }
    }
    if (ImGui::CollapsingHeader("What this test proves / sources")) {
      if (d.id.starts_with("PD-"))
        ImGui::TextWrapped("WBOSITG (2008), acoustic guitar recordings released into the public domain by their author. https://commons.wikimedia.org/wiki/File:A-major.ogg and the linked B-G recordings. The uploader names each chord; individual notes, bass inversions and precise timing are not independently annotated. Audio, source pages and file hashes are saved with the collection.");
      else if (d.kind == "guitar")
        ImGui::TextWrapped("GuitarSet 1.1.0: Xi, Bittner, Pauwels, Ye, Bello (2018), CC BY 4.0. https://zenodo.org/records/3371780. Performed chords are semi-automatic, manually verified annotations with lead-sheet roots and boundaries; not perfect physical truth.");
      else
        ImGui::TextWrapped("LibriSpeech: Panayotov, Chen, Povey, Khudanpur (2015), LibriVox readers, CC BY 4.0. https://www.openslr.org/12. Words have published transcripts; word timing and speech pitch do not have verified reference labels.");
      ImGui::TextWrapped("Chord scores use duration-weighted major/minor rules. Unsupported chords and uncovered edges receive no credit. Pitch is scored only on full windows containing one annotated note, within 50 cents; missed voices count as errors. Viterbi is offline, not a live latency test. This is a small demonstration collection, not broad accuracy certification.");
    }
  }

  void drawTransport() {
    int mode = e().mode.load();
    if (ImGui::RadioButton("Recording", mode != kMic) && mode != kFile) returnToFile();
    ImGui::SameLine();
    if (ImGui::RadioButton("Microphone", mode == kMic) && mode != kMic) {
      refreshMics();
      startMic();
    }
    mode = e().mode.load();
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    if (mode != kMic) {
      ImGui::BeginDisabled(busy());
      if (ImGui::Button("Open WAV...")) {
        static const SDL_DialogFileFilter filters[] = {{"WAV audio", "wav;wave"}};
        SDL_ShowOpenFileDialog(&App::onOpenChosen, this, window, filters, 1, nullptr, false);
      }
      ImGui::SameLine();
      if (ImGui::Button("Demo")) loadDemo();
      ImGui::EndDisabled();
      ImGui::Spacing();
      const bool playing = e().playing.load();
      ImGui::BeginDisabled(e().trackFrames == 0);
      if (ImGui::Button(playing ? "Pause" : "Play", ImVec2(64.0f, 0.0f))) setPlaying(!playing);
      ImGui::SameLine();
      if (ImGui::Button("Stop")) {
        e().playing.store(false);
        seek(0.0);
      }
      ImGui::SameLine();
      if (ImGui::Checkbox("Loop", &loop)) e().loop.store(loop);
      ImGui::SameLine();
      if (ImGui::Button("A")) {
        markA = static_cast<long long>(e().shownPos.load());
        setLoopMarks();
      }
      ImGui::SameLine();
      if (ImGui::Button("B")) {
        markB = static_cast<long long>(e().shownPos.load());
        setLoopMarks();
      }
      ImGui::SameLine();
      if (ImGui::Button("Clear A-B")) {
        markA = markB = -1;
        setLoopMarks();
      }
      ImGui::SameLine();
      ImGui::Text("%.1f / %.1f s", static_cast<double>(e().shownPos.load()) / rate, trackSeconds);
      ImGui::EndDisabled();
    } else {
      ImGui::SetNextItemWidth(300.0f);
      const std::string cur =
          micChoice == 0 ? std::string("System default") : micNames[static_cast<std::size_t>(micChoice - 1)];
      if (ImGui::BeginCombo("Device", cur.c_str())) {
        if (ImGui::Selectable("System default", micChoice == 0)) {
          micChoice = 0;
          startMic();
        }
        for (std::size_t i = 0; i < micNames.size(); ++i)
          if (ImGui::Selectable(micNames[i].c_str(), micChoice == static_cast<int>(i + 1))) {
            micChoice = static_cast<int>(i + 1);
            startMic();
          }
        ImGui::EndCombo();
      }
      ImGui::SameLine();
      if (ImGui::Button("Refresh")) refreshMics();
    }
  }

  void openBuilder() {
    if (!builderView) loopBeforeBuilder = loop;
    builderView = true; referenceView = tunerView = false; paramsDirty = true;
  }
  void leaveBuilder() {
    if (!builderView) return;
    builderView = false;
    if (loop != loopBeforeBuilder) {
      loop = loopBeforeBuilder;
      e().loop.store(loop);  // the audio thread applies it
    }
  }
  // Builder phrases play once through the normal transport.
  void drawBuilder() {
    builder.draw();
    if (builder.wantStop() && e().trackFrames > 0 && e().mode.load() == kFile) { setPlaying(false); seek(0); }
    if (auto r = builder.takeAudio()) {
      WavData w;
      w.rate = SampleRateHz{app::audition::kRate};
      w.channels = 1;
      w.bitsPerSample = 32;
      w.isFloat = true;
      w.frames = r->mono.size();
      w.samples = std::move(r->mono);
      loop = false;
      e().loop.store(false);
      loadWav(w, r->name);
      setPlaying(true);
    }
  }

  void drawHeader() {
    const float u = app::ui::unit();
    app::ui::heading("MMathEn", 22);
    ImGui::SameLine(170 * u);
    if (app::ui::button("Analyzer", !referenceView && !tunerView && !builderView)) {
      leaveBuilder(); referenceView = tunerView = false; paramsDirty = true;
    }
    ImGui::SameLine();
    if (app::ui::button("Music & tuner", tunerView)) { leaveBuilder(); referenceView = false; tunerView = true; paramsDirty = true; }
    ImGui::SameLine();
    if (app::ui::button("Player builder", builderView)) openBuilder();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Make your own player: change how an instrument is played and hear it.");
    ImGui::SameLine();
    if (app::ui::button("Reference recordings", referenceView)) {
      leaveBuilder(); referenceView = true; tunerView = false; paramsDirty = true;
      if (referenceDemo.entries.empty()) {
        const char* base = SDL_GetBasePath();
        const auto nearby = std::filesystem::path(base ? base : "") / "reference-suite.tsv";
        const auto local = std::filesystem::path("build_real_audio") / "reference-suite.tsv";
        if (std::filesystem::exists(nearby)) referenceDemo.open(nearby.string());
        else if (std::filesystem::exists(local)) referenceDemo.open(local.string());
        else {
          // Explorer launches may have a different working directory.
          for (auto parent = std::filesystem::path(base ? base : "").parent_path();
               !parent.empty() && parent != parent.parent_path(); parent = parent.parent_path()) {
            const auto suite = parent / "build_real_audio/reference-suite.tsv";
            if (std::filesystem::exists(suite)) { referenceDemo.open(suite.string()); break; }
          }
        }
      }
      if (!referenceDemo.entries.empty() && !referenceDemo.busy()) selectReference(std::max(0, referenceDemo.selected));
    }
    if (ImGui::GetContentRegionAvail().x > 1080 * u) ImGui::SameLine(ImGui::GetWindowWidth() - 350 * u);
    else ImGui::Spacing();
    const bool silent = SDL_GetAudioStreamGain(play) == 0;
    if (silent) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.50f, .23f, .11f, 1));
    if (ImGui::Button(silent ? "Unmute sound" : "Mute sound", ImVec2(130 * u, 0))) {
      output.muted = !silent;
      if (silent && output.volume == 0) output.volume = .5f;
      if (!output.apply(play)) status = std::string("Sound control failed: ") + SDL_GetError();
    }
    if (silent) ImGui::PopStyleColor();
    ImGui::SameLine(); ImGui::SetNextItemWidth(130 * u);
    float percent = output.volume * 100;
    if (ImGui::SliderFloat("##output-volume", &percent, 0, 100, "%.0f%%", ImGuiSliderFlags_AlwaysClamp)) {
      output.volume = percent / 100; output.muted = false;
      if (!output.apply(play)) status = std::string("Volume change failed: ") + SDL_GetError();
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Listening volume. Analysis and exported audio are unchanged.");
    if (silent) { ImGui::SameLine(); ImGui::TextColored(ImVec4(1, .68f, .38f, 1), "MUTED"); }
    ImGui::Separator();
  }

  static void pushPending(void* user, Pending kind, const char* const* list) {
    auto* a = static_cast<App*>(user);
    if (!list || !list[0]) return;  // error or cancelled
    std::lock_guard<std::mutex> lock(a->pendingMu);
    a->pending.emplace_back(kind, list[0]);
  }
  static void SDLCALL onOpenChosen(void* u, const char* const* l, int) { pushPending(u, Pending::Open, l); }
  static void SDLCALL onSuiteChosen(void* u, const char* const* l, int) { pushPending(u, Pending::ReferenceSuite, l); }
  static void SDLCALL onRefChosen(void* u, const char* const* l, int) { pushPending(u, Pending::Reference, l); }
  static void SDLCALL onSvgChosen(void* u, const char* const* l, int) { pushPending(u, Pending::ExportSvg, l); }
  static void SDLCALL onWavChosen(void* u, const char* const* l, int) { pushPending(u, Pending::ExportWav, l); }

  void frame(double dt) {
    std::vector<std::pair<Pending, std::string>> todo;
    {
      std::lock_guard<std::mutex> lock(pendingMu);
      todo.swap(pending);
    }
    for (const auto& item : todo) {
      if (item.first == Pending::Open) loadPath(item.second);
      if (item.first == Pending::Reference) startMatchAnalysis(item.second);
      if (item.first == Pending::ExportSvg) exportSvg(item.second);
      if (item.first == Pending::ExportWav) startExport(item.second);
      if (item.first == Pending::ReferenceSuite && !referenceDemo.busy() && referenceDemo.open(item.second)) selectReference(0);
    }
    pollJob();
    referenceDemo.poll();
    if (paramsDirty) publish();
    pumpAudio(dt);
    const ImGuiIO& io = ImGui::GetIO();
    if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Space, false) && e().trackFrames > 0 &&
        e().mode.load() == kFile)
      setPlaying(!e().playing.load());

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("mmathEn", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoBringToFrontOnFocus);
    drawHeader();
    if (referenceView) {
      drawReferenceDemo();
      ImGui::End();
      return;
    }
    if (builderView) {
      drawBuilder();
      ImGui::End();
      return;
    }
    drawTransport();
    ImGui::Separator();
    if (tunerView) {
      const auto previousRange=tunerPanel.pitchRange();
      tunerPanel.microphone = e().mode.load() == kMic;
      tunerPanel.refined = streamingTuner.refined;
      tunerPanel.draw(tune, tuneLevelDb, e().mode.load() == kMic || (e().mode.load() == kFile && e().playing.load()));
      if(previousRange!=tunerPanel.pitchRange()) {
        hist.assign(app::precisionHistorySize(rate,tunerPanel.pitchRange()),0);
        histPos=0; resetTuner();
      }
      if(!tunerPanel.music.audition.empty()) {
        const auto w=app::music::referenceTones(tunerPanel.music.audition,tunerPanel.a4);
        tunerPanel.music.audition.clear();
        if(w.frames) loadWav(w,"Music math: generated reference tones");
      }
      ImGui::End();
      return;
    }

    const float statusH = ImGui::GetFrameHeightWithSpacing();
    const float leftW = std::min(530.0f * app::ui::unit(), ImGui::GetContentRegionAvail().x * 0.42f);
    const float bodyH = ImGui::GetContentRegionAvail().y - statusH;
    ImGui::BeginChild("left", ImVec2(leftW, bodyH));
    const float meterW = 46.0f * app::ui::unit();
    ImGui::BeginChild("tabs", ImVec2(leftW - meterW - ImGui::GetStyle().ItemSpacing.x, 0.0f));
    if (ImGui::BeginTabBar("panels")) {
      auto tab = [&](const char* name, int idx) {
        return ImGui::BeginTabItem(name, nullptr, forceTab == idx ? ImGuiTabItemFlags_SetSelected : 0);
      };
      const float w = ImGui::GetContentRegionAvail().x;
      if (tab("EQ", 0)) {
        panelEq(w);
        ImGui::EndTabItem();
      }
      if (tab("Dynamics", 1)) {
        panelDynamics();
        ImGui::EndTabItem();
      }
      if (tab("Pitch/Time", 2)) {
        panelTransform();
        ImGui::EndTabItem();
      }
      if (tab("Repair", 4)) {
        panelRepair();
        ImGui::EndTabItem();
      }
      if (tab("Settings", 3)) {
        panelSettings();
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
    forceTab = -1;
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("meters", ImVec2(meterW, 0.0f));
    ImGui::TextUnformatted("dBFS");
    drawVMeter("level", ImVec2(meterW - 4.0f, std::max(60.0f, ImGui::GetContentRegionAvail().y)), meterRmsDb,
               meterPeakDb, -60.0, 0.0, now() < clipLightUntil);
    ImGui::EndChild();
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("right", ImVec2(0.0f, bodyH));
    drawLoudness();
    const float w = ImGui::GetContentRegionAvail().x;
    const float gapY = ImGui::GetStyle().ItemSpacing.y;
    float avail = ImGui::GetContentRegionAvail().y;
    if (e().mode.load() == kFile && overview) {
      drawOverview(ImVec2(w, 72.0f));
      avail -= 72.0f + gapY;
    }
    drawBars(ImVec2(w, std::max(80.0f, avail * 0.38f - gapY)));
    drawSpectrum(ImVec2(w, std::max(60.0f, avail * 0.32f - gapY)), dt);
    drawSpectrogram(ImVec2(w, std::max(60.0f, ImGui::GetContentRegionAvail().y)));
    ImGui::EndChild();

    char info[160];
    std::snprintf(info, sizeof info, "  |  %.0f Hz, Hann %zu (%.2f Hz bins), hop %zu (%.0f frames/s)", rate,
                  frameSize, rate / static_cast<double>(frameSize), hop, rate / static_cast<double>(hop));
    ImGui::TextDisabled("%s%s", status.c_str(), info);
    ImGui::End();
  }
};

}  // namespace

int main(int argc, char* argv[]) {
  std::string capturePath, openPath, matchPath, exportPath, referencePath, builderKey, playersArg, builderLoad;
  bool builderArg = false, builderPlayArg = false;
  int referenceTrack = 0, instrumentArg=0;
  double captureSeconds = 3.0, seekAt = -1.0;
  bool mute = false, useMic = false, compArg = false, transformArg = false, flatArg = false, truePeakArg = false,
       normalizeArg = false, linearArg = false, tunerArg = false, calibrationArg = false;
  int presetArg = -1, tabArg = -1, eqModeArg = -1, methodArg = -1;
  float shiftArg = 0.0f, tempoArg = 100.0f;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    const bool more = i + 1 < argc;
    if (a == "--capture" && more) {
      capturePath = argv[++i];
    } else if (a == "--reference-demo" && more) {
      referencePath = argv[++i];
    } else if (a == "--reference-track" && more) {
      referenceTrack = std::atoi(argv[++i]);
    } else if (a == "--seconds" && more) {
      captureSeconds = std::atof(argv[++i]);
    } else if (a == "--preset" && more) {
      presetArg = std::atoi(argv[++i]);
    } else if (a == "--tab" && more) {
      const std::string t = argv[++i];
      tabArg = t == "dyn" ? 1 : t == "fx" ? 2 : t == "set" ? 3 : t == "repair" ? 4 : 0;
    } else if (a == "--eqmode" && more) {
      const std::string t = argv[++i];
      eqModeArg = t == "parametric" ? 1 : t == "match" ? 2 : 0;
    } else if (a == "--seek" && more) {
      seekAt = std::atof(argv[++i]);
    } else if (a == "--match" && more) {
      matchPath = argv[++i];
    } else if (a == "--shift" && more) {
      shiftArg = static_cast<float>(std::atof(argv[++i]));
      transformArg = true;
    } else if (a == "--tempo" && more) {
      tempoArg = static_cast<float>(std::atof(argv[++i]));
      transformArg = true;
    } else if (a == "--method" && more) {
      const std::string m = argv[++i];
      methodArg = m == "pv" ? 1 : m == "pghi" ? 2 : m == "hybrid" ? 3 : 0;
    } else if (a == "--export" && more) {
      exportPath = argv[++i];
    } else if (a == "--linear") {
      linearArg = true;
    } else if (a == "--normalize") {
      normalizeArg = true;
    } else if (a == "--truepeak") {
      truePeakArg = true;
    } else if (a == "--flat") {
      flatArg = true;
    } else if (a == "--comp") {
      compArg = true;
    } else if (a == "--mic") {
      useMic = true;
    } else if (a == "--builder") {
      builderArg = true;
      if (more && argv[i + 1][0] != '-') builderKey = argv[++i];
    } else if (a == "--builder-play") {
      builderArg = builderPlayArg = true;
    } else if (a == "--players" && more) {
      playersArg = argv[++i];
    } else if (a == "--builder-load" && more) {
      builderArg = true;
      builderLoad = argv[++i];
    } else if (a == "--tuner") {
      tunerArg = true;
    } else if (a == "--piano") {
      tunerArg = true; instrumentArg=1;
    } else if (a == "--guitar") {
      tunerArg = true; instrumentArg=2;
    } else if (a == "--calibration") {
      calibrationArg = true;
    } else if (a == "--mute") {
      mute = true;
    } else {
      openPath = a;
    }
  }

  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "mmathEn", SDL_GetError(), nullptr);
    return 1;
  }
  const float scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
  const float s = scale > 0.0f ? scale : 1.0f;
  auto appPtr = std::make_unique<App>();
  App& a = *appPtr;
  a.tunerView = tunerArg;
  a.tunerPanel.instrument=instrumentArg;
  a.tunerPanel.showCalibration = calibrationArg;
  a.window = SDL_CreateWindow("MMathEn: music, piano, tuning & sound", static_cast<int>(1500 * s),
                              static_cast<int>(900 * s), SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
  if (!a.window) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "mmathEn", SDL_GetError(), nullptr);
    return 1;
  }
  a.renderer = SDL_CreateRenderer(a.window, nullptr);
  if (!a.renderer) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "mmathEn", SDL_GetError(), a.window);
    return 1;
  }
  SDL_SetRenderVSync(a.renderer, 1);
  a.gramTex = SDL_CreateTexture(a.renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, App::kGramW,
                                App::kGramH);
  if (a.gramTex) SDL_SetTextureScaleMode(a.gramTex, SDL_SCALEMODE_LINEAR);

  auto ring = SpscRing::create(std::size_t{1} << 17);
  if (!ring.ok()) return 1;
  a.e().ring = std::move(ring).value();
  if (!a.e().buildProcessors(48000.0)) return 1;
  a.resetParametric();
  const SDL_AudioSpec spec{SDL_AUDIO_F32, 2, 48000};
  a.play = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, app::playCallback, a.engine.get());
  if (!a.play) {
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "mmathEn: no audio output", SDL_GetError(), a.window);
    return 1;
  }
  a.output.muted = mute;
  if (!a.output.apply(a.play)) return 1;
  SDL_ResumeAudioStreamDevice(a.play);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  const std::filesystem::path systemFont("C:/Windows/Fonts/segoeui.ttf");
  if (std::filesystem::exists(systemFont)) io.Fonts->AddFontFromFileTTF(systemFont.string().c_str(), 16);
  else io.Fonts->AddFontDefault();
  app::ui::theme(s);
  ImGui_ImplSDL3_InitForSDLRenderer(a.window, a.renderer);
  ImGui_ImplSDLRenderer3_Init(a.renderer);

  a.setSourceRate(48000.0);
  if (presetArg >= 0) {
    a.preset = presetArg;
    a.applyPreset = true;
  }
  if (tabArg >= 0) a.forceTab = tabArg;
  if (eqModeArg >= 0) a.eqMode = static_cast<EqMode>(eqModeArg);
  if (eqModeArg == 1) {  // a visible parametric example for captures
    a.pbands[0].on = true;
    a.pbands[0].freq = 60.0f;
    a.pbands[1].gain = 4.0f;
    a.pbands[2].gain = -6.0f;
    a.pbands[2].q = 2.0f;
    a.pbands[3].gain = 3.0f;
    a.pbands[4].gain = 5.0f;
  }
  if (compArg) {
    a.compOn = true;
    a.comp.thresholdDb = -30.0;
    a.comp.ratio = 6.0;
  }
  if (flatArg) {  // every stage off: an export must equal its source
    a.eqOn = false;
    a.compOn = false;
    a.limOn = false;
  }
  a.exportNormalize = normalizeArg;
  a.exportLinearPhase = linearArg;
  if (truePeakArg) {
    a.lim.truePeak = true;
    a.rebuildLimiter();
  }
  if (methodArg >= 0) a.method = methodArg;
  a.shiftSemis = shiftArg;
  a.tempoPct = tempoArg;
  if (!referencePath.empty()) {
    a.referenceView = true;
    a.paramsDirty = true;
    if (a.referenceDemo.open(referencePath)) a.selectReference(referenceTrack);
  } else if (useMic) {
    a.refreshMics();
    a.startMic();
  } else if (!openPath.empty()) {
    a.loadPath(openPath);
  } else if (!capturePath.empty()) {
    a.loadDemo();
  }
  if (!matchPath.empty()) a.startMatchAnalysis(matchPath);
  if (builderArg) {
    a.openBuilder();
    mmathen::PlayerInstrument bi{};
    if (!playersArg.empty()) a.builder.playersPath = playersArg;
    if (!builderKey.empty() && mmathen::playerInstrumentFromKey(builderKey, bi)) a.builder.select(bi);
    if (!builderLoad.empty() && !a.builder.loadByName(builderLoad)) a.status = a.builder.loadError();
    if (builderPlayArg) a.builder.play();
  }
  bool transformPending = transformArg && !a.original.empty();

  double start = a.now(), last = start;
  bool seekDone = false, running = true;
  int rc = 0;
  while (running) {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
      ImGui_ImplSDL3_ProcessEvent(&ev);
      if (ev.type == SDL_EVENT_QUIT) running = false;
      if (ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && ev.window.windowID == SDL_GetWindowID(a.window))
        running = false;
      if (ev.type == SDL_EVENT_DROP_FILE && ev.drop.data) a.loadPath(ev.drop.data);
      if ((ev.type == SDL_EVENT_AUDIO_DEVICE_ADDED || ev.type == SDL_EVENT_AUDIO_DEVICE_REMOVED ||
           ev.type == SDL_EVENT_AUDIO_DEVICE_FORMAT_CHANGED) && ev.adevice.recording) {
        a.tunerPanel.calibration.reset();
        a.resetTuner();
      }
    }
    if (SDL_GetWindowFlags(a.window) & SDL_WINDOW_MINIMIZED) {
      SDL_Delay(10);
      a.pumpAudio(0.01);
      continue;
    }
    if (transformPending && !a.busy()) {
      a.startTransform();
      transformPending = false;
    }
    // Export after any transform, once the engine has the current settings.
    if (!exportPath.empty() && !transformPending && !a.busy() && !a.paramsDirty) {
      a.startExport(exportPath);
      exportPath.clear();
    }
    const double t = a.now();
    const double dt = std::clamp(t - last, 0.0, 0.1);
    last = t;
    if (!seekDone && seekAt >= 0.0 && t - start >= 1.0) {
      a.seek(seekAt);
      seekDone = true;
    }
    if (a.busy() || a.referenceDemo.busy() || a.builder.busy()) start = t;  // captures wait for background jobs

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    a.frame(dt);
    ImGui::Render();
    SDL_SetRenderScale(a.renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
    SDL_SetRenderDrawColor(a.renderer, 10, 9, 20, 255);
    SDL_RenderClear(a.renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), a.renderer);

    if (!capturePath.empty() && !transformPending && exportPath.empty() && !a.busy() && !a.referenceDemo.busy() && !a.builder.busy() &&
        t - start >= captureSeconds) {
      SDL_Surface* shot = SDL_RenderReadPixels(a.renderer, nullptr);
      const bool saved = shot && SDL_SavePNG(shot, capturePath.c_str());
      if (shot) SDL_DestroySurface(shot);
      const int sections = static_cast<int>(a.designSections().size());
      if (SDL_IOStream* f = SDL_IOFromFile((capturePath + ".txt").c_str(), "w")) {
        SDL_IOprintf(f,
                     "saved=%d\nstatus=%s\nrate=%.0f\ntrackSeconds=%.3f\nanalysisFrames=%llu\n"
                     "position=%llu\nclips=%llu\neqSections=%d\neqMaxDb=%.3f\ncompGrDb=%.2f\n"
                     "limGrDb=%.2f\nrmsDb=%.2f\npeakDb=%.2f\ntuneHz=%.3f\ntuneVoiced=%d\n"
                     "matchErrDb=%.3f\nintegratedLufs=%.2f\nmomentaryLufs=%.2f\ntruePeakDbtp=%.2f\n",
                     saved ? 1 : 0, a.status.c_str(), a.rate, a.trackSeconds,
                     static_cast<unsigned long long>(a.framesAnalysed),
                     static_cast<unsigned long long>(a.e().shownPos.load()),
                     static_cast<unsigned long long>(a.e().clips.load()), sections, a.eqMaxDb,
                     static_cast<double>(a.e().compGrDb.load()), static_cast<double>(a.e().limGrDb.load()),
                     a.meterRmsDb, a.meterPeakDb, a.tune.frequencyHz, a.tune.voiced ? 1 : 0,
                     a.match ? a.match->predictedErrorDb : -1.0,
                     a.loudness ? a.loudness->integratedLufs() : kLoudnessFloor,
                     a.loudness ? a.loudness->momentaryLufs() : kLoudnessFloor,
                     a.loudness ? a.loudness->truePeakDbtp() : kLoudnessFloor);
        if (a.referenceView) SDL_IOprintf(f, "referenceTrack=%s\nreferenceReady=%d\nreferenceEngine=%s\nreferenceAccuracy=%.15f\nreferenceEligible=%.15f\nreferenceError=%s\n", a.referenceDemo.data.id.c_str(), a.referenceDemo.ready ? 1 : 0, a.referenceDemo.measured.engine.c_str(), a.referenceDemo.result.accuracy(), a.referenceDemo.result.eligible, a.referenceDemo.error.c_str());
        SDL_IOprintf(f, "outputGain=%.6f\noutputMuted=%d\nsourceMode=%d\ntunerView=%d\nbuilderView=%d\n", SDL_GetAudioStreamGain(a.play), a.output.muted ? 1 : 0, a.e().mode.load(), a.tunerView ? 1 : 0, a.builderView ? 1 : 0);
        SDL_AudioSpec deviceSpec{};
        if (SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(a.play), &deviceSpec, nullptr))
          SDL_IOprintf(f, "outputFormat=%u\noutputChannels=%d\noutputRate=%d\n", static_cast<unsigned>(deviceSpec.format), deviceSpec.channels, deviceSpec.freq);
        SDL_CloseIO(f);
      }
      rc = saved ? 0 : 2;
      running = false;
    }
    SDL_RenderPresent(a.renderer);
  }

  if (a.referenceDemo.busy()) a.referenceDemo.worker.wait();
  if (a.job && a.job->thread.joinable()) a.job->thread.join();
  a.stopMic();
  SDL_DestroyAudioStream(a.play);
  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  if (a.gramTex) SDL_DestroyTexture(a.gramTex);
  if (a.overviewTex) SDL_DestroyTexture(a.overviewTex);
  SDL_DestroyRenderer(a.renderer);
  SDL_DestroyWindow(a.window);
  appPtr.reset();
  SDL_Quit();
  return rc;
}
