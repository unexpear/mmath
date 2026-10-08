// Renders the waveguide voice to a WAV file. The samples are the
// stepper. No instrument is claimed, and no envelope is applied.
// Writes mmath-voice-waveguide.wav under wavOutputDirectory()
// (MMATH_WAV_DIR, or the process temporary directory) and copies it to
// /opt/cursor/artifacts only when that directory already exists.

#include <cstdio>
#include <utility>

#include <mmathen/physics/tuned_waveguide.h>
#include <mmathen/physics/wave1d.h>
#include <mmathen/render/waveguide_voice.h>
#include <mmathen/sink/wav.h>
#include <mmathen/sink/wav_path.h>

int main() {
  using namespace mmathen;
  const char* filename = "mmath-voice-waveguide.wav";
  const auto path = wavOutputPath(filename);
  auto base = idealString(DistanceMeters{0.6523}, TensionNewtons{97.344},
                          LinearDensityKgPerM{0.001});
  if (!base.ok()) return 1;
  auto loss = lossFromT60(base.value().speed, FrequencyHz{240.0},
                          DurationSeconds{3.0}, FrequencyHz{2400.0},
                          DurationSeconds{0.4});
  if (!loss.ok()) return 1;
  auto system = withLoss(base.value(), loss.value());
  if (!system.ok()) return 1;
  const SampleRateHz rate{48000.0};
  auto design = designTunedLoop(system.value(), rate, 6, 0);
  if (!design.ok()) return 1;
  auto shape = InitialShape::mode(system.value(), 1, 1.0e-3);
  if (!shape.ok()) return 1;
  auto made =
      WaveguideVoice::create(shape.value(), design.value().filters, rate, 0.2);
  if (!made.ok()) return 1;
  auto voice = std::move(made).value();
  const FrameCount frames{48000};
  auto wav = WavSink::open(path.string(), rate, frames);
  if (!wav.ok()) return 1;
  auto sink = std::move(wav).value();
  auto stats = renderVoice(voice, VoiceRenderConfig{frames, 512}, sink);
  if (!stats.ok() || !stats.value().isClean()) return 1;
  if (!sink.close().ok()) return 1;
  if (!copyWavToArtifacts(path, filename)) return 1;
  std::printf("%s frames=%llu peak=%.6f pole=%.3f Hz\n", path.string().c_str(),
              static_cast<unsigned long long>(stats.value().framesRendered),
              stats.value().peakAbs,
              design.value().partials[0].frequency.value());
  return 0;
}
