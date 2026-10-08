// Renders one bowed, struck, mallet, and blown voice to a listening WAV.
//
// The gain that brings the peak to 0.5 is playback only. Loss durations,
// hammer and mallet numbers, blowing pressures, and the far-end
// reflection magnitude are this example's arguments.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

#include <mmathen/instruments/bore_valve.h>
#include <mmathen/instruments/bowed_string.h>
#include <mmathen/instruments/piano.h>
#include <mmathen/instruments/struck_string.h>
#include <mmathen/instruments/violin.h>
#include <mmathen/physics/medium.h>
#include <mmathen/sink/wav.h>
#include <mmathen/sink/wav_path.h>

namespace {

void writeScaled(const std::vector<double>& samples, double rate, const char* filename) {
  double peak = 0.0;
  for (double sample : samples) peak = std::max(peak, std::fabs(sample));
  const double gain = peak > 0.0 ? 0.5 / peak : 0.0;
  std::vector<double> scaled(samples.size());
  for (std::size_t i = 0; i < samples.size(); ++i) scaled[i] = samples[i] * gain;
  const auto path = mmathen::wavOutputPath(filename);
  auto opened = mmathen::WavSink::open(
      path.string(), mmathen::SampleRateHz{rate},
      mmathen::FrameCount{static_cast<std::uint64_t>(samples.size())});
  if (!opened.ok()) std::exit(1);
  auto sink = std::move(opened).value();
  if (!sink.consume(scaled.data(),
                    mmathen::FrameCount{static_cast<std::uint64_t>(scaled.size())}))
    std::exit(1);
  if (!sink.close().ok()) std::exit(1);
  if (!mmathen::copyWavToArtifacts(path, filename)) std::exit(1);
  std::printf("%s peak %.6g\n", path.string().c_str(), peak);
}

}  // namespace

int main() {
  using namespace mmathen;
  const SampleRateHz rate{48000.0};
  const std::uint64_t frames = 24000;
  auto course = violin().value().courses[2];
  auto ideal = courseString(course).value();
  const double frequency = course.tuning.frequency.value();
  auto loss = lossFromT60(ideal.speed, FrequencyHz{frequency}, DurationSeconds{2.0},
                          FrequencyHz{6.0 * frequency}, DurationSeconds{0.25})
                  .value();
  const double impedance =
      course.material.linearDensity.value() *
      std::sqrt(course.tension.value() / course.material.linearDensity.value());
  const double maximum = 2.0 * impedance * 0.2 / (0.125 * 0.5);
  BowStroke stroke{0.125, 0.2, 0.15 * maximum, BowFriction{0.8, 0.3}};
  auto bow = BowedStringVoice::create(course, stroke, loss, 0.0, 4, 0, rate, 12000)
                 .value();
  auto bowed = std::vector<double>(static_cast<std::size_t>(frames));
  if (!bow.renderBlock(bowed.data(), FrameCount{frames}).ok()) std::exit(1);
  writeScaled(bowed, 48000.0, "mmath-violin-bow.wav");

  auto bank = concertGrandStringBank().value();
  const auto* spec = findPianoString(bank, "C#5");
  auto piano = tunePianoString(*spec).value();
  auto stiff = tunedPianoString(*spec).value();
  auto hammer = FeltHammerVoice::create(piano, HammerStrike{0.12, 0.008, 0.8}, loss,
                                        stiff.stiffness, 3200.0, 1.0, 0.0, 4, rate,
                                        0.3)
                    .value();
  auto struck = std::vector<double>(static_cast<std::size_t>(frames));
  if (!hammer.renderBlock(struck.data(), FrameCount{frames}).ok()) std::exit(1);
  writeScaled(struck, 48000.0, "mmath-piano-hammer.wav");

  ClarinetSpec clarinetSpec{0.45, 2.0e-4, 101325.0, 293.15, 2.0e-5, 6000.0,
                            1.0e-6, 1.0e-4, 1.0e-3, 0.01};
  auto clarinet = makeClarinet(clarinetSpec).value();
  auto reed = BoreValveVoice::fromClarinet(
                  clarinet, ValveBlow{4000.0, 12000, 0.97, 2.0e-5}, rate)
                  .value();
  auto blown = std::vector<double>(static_cast<std::size_t>(frames));
  if (!reed.renderBlock(blown.data(), FrameCount{frames}).ok()) std::exit(1);
  writeScaled(blown, 48000.0, "mmath-clarinet.wav");
  return 0;
}
