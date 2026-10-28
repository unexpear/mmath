// Renders one plucked course of each wired instrument to WAV.
//
// The T60 pair and the listen fraction are this example's arguments.
// They are not measurements stored on the instruments. The electric
// guitar file is the coil velocity. Each file is written under
// wavOutputDirectory() (MMATH_WAV_DIR, or the process temporary
// directory). A peak-scaled copy is written to /opt/cursor/artifacts
// only when that directory already exists. Writes:
//   mmath-pluck-violin.wav
//   mmath-pluck-viola.wav
//   mmath-pluck-cello.wav
//   mmath-pluck-double-bass.wav
//   mmath-pluck-harp.wav
//   mmath-pluck-harpsichord.wav
//   mmath-pluck-classical-guitar.wav
//   mmath-pluck-electric-guitar.wav

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#include <mmathen/instruments/cello.h>
#include <mmathen/instruments/double_bass.h>
#include <mmathen/instruments/electric_guitar.h>
#include <mmathen/instruments/guitar.h>
#include <mmathen/instruments/harp.h>
#include <mmathen/instruments/harpsichord.h>
#include <mmathen/instruments/plucked_string.h>
#include <mmathen/instruments/viola.h>
#include <mmathen/instruments/violin.h>
#include <mmathen/sink/wav.h>
#include <mmathen/sink/wav_path.h>

namespace {

mmathen::LossModel lossFor(const mmathen::StringCourse& course) {
  auto ideal = mmathen::courseString(course);
  const double f = course.tuning.frequency.value();
  auto loss = mmathen::lossFromT60(
      ideal.value().speed, mmathen::FrequencyHz{f}, mmathen::DurationSeconds{2.0},
      mmathen::FrequencyHz{6.0 * f}, mmathen::DurationSeconds{0.25});
  return loss.value();
}

int renderOne(const char* filename, const mmathen::StringCourse& course,
              mmathen::StringReadout readout) {
  using namespace mmathen;
  const Excitation pluck{ExcitationKind::Pluck, 0.5, 1.0e-4};
  const SampleRateHz rate{48000.0};
  auto made = PluckedStringVoice::create(course, pluck, lossFor(course), 0.0, 4,
                                         0, rate, 0.23, readout);
  if (!made.ok()) return 1;
  auto voice = std::move(made).value();
  const FrameCount frames{48000};
  const auto path = wavOutputPath(filename);
  auto wav = WavSink::open(path.string(), rate, frames);
  if (!wav.ok()) return 1;
  auto sink = std::move(wav).value();
  auto stats = renderVoice(voice, VoiceRenderConfig{frames, 512}, sink);
  if (!stats.ok() || !stats.value().isClean()) return 1;
  if (!sink.close().ok()) return 1;
  std::printf("%s frames=%llu peak=%.6g pole=%.3f Hz\n", path.string().c_str(),
              static_cast<unsigned long long>(stats.value().framesRendered),
              stats.value().peakAbs, voice.fundamentalHz());
  // Playback-only copy. The required file stays the unscaled physics.
  // Skip the copy when the artifacts directory is absent.
  const auto listening = artifactWavPath(filename);
  if (listening.empty()) return 0;
  voice.reset();
  std::vector<double> samples(static_cast<std::size_t>(frames.value()));
  if (!voice.renderBlock(samples.data(), frames).ok()) return 1;
  double peak = 0.0;
  for (double sample : samples) peak = std::max(peak, std::fabs(sample));
  if (!(peak > 0.0)) return 1;
  const double gain = 0.5 / peak;
  for (double& sample : samples) sample *= gain;
  auto opened = WavSink::open(listening.string(), rate, frames);
  if (!opened.ok()) return 1;
  auto listen = std::move(opened).value();
  if (!listen.consume(samples.data(), frames)) return 1;
  if (!listen.close().ok()) return 1;
  return 0;
}

mmathen::StringCourse tunedHarp() {
  using namespace mmathen;
  auto harp = concertPedalHarp();
  const HarpStringGeometry* string = findHarpString(harp.value(), "Cb4");
  const double frequency = makePitch("Cb4").value().frequency.value();
  const double length = string->vibratingLengthCm * 1.0e-2;
  const double tension =
      2.0e-3 * (2.0 * length * frequency) * (2.0 * length * frequency);
  return tuneHarpString(*string, 1.0, 3.0, tension * 0.5, tension * 1.5)
      .value();
}

mmathen::StringCourse tunedHarpsichord() {
  using namespace mmathen;
  auto instrument = frenchDouble();
  const HarpsichordSpeakingLength* string = findSpeakingLength(
      instrument.value(), HarpsichordChoir::LongEightFoot, "C4");
  const double frequency = makePitch("C4").value().frequency.value();
  const double length = string->vibratingLengthMm * 1.0e-3;
  const double tension =
      1.0e-3 * (2.0 * length * frequency) * (2.0 * length * frequency);
  return tuneHarpsichordString(*string, 0.5, 1.5, tension * 0.5, tension * 1.5)
      .value();
}

mmathen::StringCourse tunedClassical() {
  using namespace mmathen;
  auto guitar = classicalGuitar();
  const ClassicalGuitarString* string =
      findClassicalGuitarString(guitar.value(), "E4");
  const double length = guitar.value().vibratingLength.value();
  const double frequency = makePitch("E4").value().frequency.value();
  const double tension =
      2.0e-3 * (2.0 * length * frequency) * (2.0 * length * frequency);
  return tuneClassicalGuitarString(guitar.value(), *string, 1.0, 3.0,
                                   tension * 0.5, tension * 1.5)
      .value();
}

mmathen::StringCourse tunedElectric() {
  using namespace mmathen;
  auto guitar = electricGuitar();
  const ElectricGuitarString* string =
      findElectricGuitarString(guitar.value(), "E4");
  const double length = guitar.value().vibratingLength.value();
  const double frequency = makePitch("E4").value().frequency.value();
  const double newtons = string->tensionPounds * kPoundForceNewtons;
  const double grams =
      newtons / ((2.0 * length * frequency) * (2.0 * length * frequency)) *
      1.0e3;
  return tuneElectricGuitarString(guitar.value(), *string, grams).value();
}

}  // namespace

int main() {
  using namespace mmathen;
  if (renderOne("mmath-pluck-violin.wav", violin().value().courses[2],
                StringReadout::Displacement) != 0)
    return 1;
  if (renderOne("mmath-pluck-viola.wav", viola().value().courses[2],
                StringReadout::Displacement) != 0)
    return 1;
  if (renderOne("mmath-pluck-cello.wav", cello().value().courses[3],
                StringReadout::Displacement) != 0)
    return 1;
  if (renderOne("mmath-pluck-double-bass.wav",
                doubleBass().value().courses[3],
                StringReadout::Displacement) != 0)
    return 1;
  if (renderOne("mmath-pluck-harp.wav", tunedHarp(),
                StringReadout::Displacement) != 0)
    return 1;
  if (renderOne("mmath-pluck-harpsichord.wav", tunedHarpsichord(),
                StringReadout::Displacement) != 0)
    return 1;
  if (renderOne("mmath-pluck-classical-guitar.wav", tunedClassical(),
                StringReadout::Displacement) != 0)
    return 1;
  if (renderOne("mmath-pluck-electric-guitar.wav", tunedElectric(),
                StringReadout::CoilVelocity) != 0)
    return 1;
  return 0;
}
