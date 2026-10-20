#pragma once

// Snare drum: the 14 x 5.5 in shell of yamahaTourCustomKit(), a batter and
// a snare-side head coupled by the closed cavity (CavityDrumVoice), a
// wooden stick tip, and snare strands colliding with the snare head
// (CavityDrumVoice::attachSnareWires). Constants are labelled at each line.

#include <cmath>
#include <utility>
#include <vector>

#include "cavity_drum.h"
#include "drum.h"
#include "mallet_voice.h"

namespace mmathen {

// Heads: Mylar (PET film) density 1390 kg/m^3 (DuPont Mylar product
// data, VERIFY). Batter: Remo Ambassador, 10 mil single ply (Remo
// catalogue); snare side: Remo Ambassador Snare Side, 3 mil (Remo).
inline constexpr double kMylarDensityKgPerM3 = 1390.0;  // VERIFY
inline constexpr double kSnareBatterThicknessM = 10.0 * 25.4e-6;
inline constexpr double kSnareSideThicknessM = 3.0 * 25.4e-6;
// Tuning: (0,1) of each head before cavity coupling. The snare-side head
// is usually tuned above the batter (Rossing, Science of Percussion
// Instruments, 2000, ch. 2, VERIFY ratio). TUNED to the VSCO 2 CE Snare2
// "HitSN" spectra (head peak there 236-244 Hz).
inline constexpr double kSnareBatterHz = 175.0;  // TUNED (coupled peak 212 Hz; 195 reached 228-232 Hz but the buzz rose 6 dB over the reference)
inline constexpr double kSnareSideHz = 240.0;    // TUNED
inline constexpr double kSnareHeadQ = 120.0;     // TUNED: decay with snares on (Q 40 / 80 / 120: t(-40 dB) 56 / 90 / see board)
// Stick: a 5A hickory stick weighs about 50 g; the moving mass at the tip
// is taken as 15 g (VERIFY). Wood tip on film: stiff, short contact.
inline constexpr double kSnareStickMassKg = 0.015;       // VERIFY
inline constexpr double kSnareStickStiffness = 5.0e5;    // N/m^1.5, TUNED (4e6 put a click 20 dB over the buzz: t(-20 dB) 13 ms vs 35-56 ms)
inline constexpr double kSnareStickExponent = 1.5;       // Hertz contact
inline constexpr double kSnareStrikeRadialFraction = 0.3;  // off-centre stroke, UNSOURCED

// Strands: a standard set has 20 strands (Puresound / Remo 20-strand
// 14 in sets). Moving mass per strand contact, strand resonance spread,
// contact stiffness and preload are TUNED to the VSCO Snare2 "HitSN"
// (snares on) samples; Bilbao (JASA 131, 2012) models the strands as
// stiff strings in one-sided contact and does not print one set of these
// lumped values.
[[nodiscard]] inline SnareWires snareWireSet() {
  SnareWires w;
  w.count = 20;
  w.strandMassKg = 0.3e-3;         // TUNED (0.6 g drained the head: t(-40 dB) 56 ms)
  w.lowestHz = 120.0;              // TUNED
  w.highestHz = 2400.0;            // TUNED
  w.strandQ = 3.0;                 // TUNED (Q 8 chattered on: no decay after the hit)
  w.contactStiffness = 2.0e7;      // TUNED
  w.contactExponent = 1.5;         // Hertz
  w.contactDampingPerMeterPerSecond = 0.5;  // TUNED
  w.preloadMeters = 5.0e-7;        // TUNED (2e-5 kept the strands stuck: no buzz)
  w.spanFraction = 0.6;
  return w;
}

// The strand contacts are stiff and stepped explicitly against the head;
// at 48 kHz a 2e6 N/m^1.5 contact self-oscillated (constant level after
// the hit). The voice runs at kSnareOversample times the output rate and
// averages each block of samples (boxcar decimation, as
// OversampledBowedStringVoice).
inline constexpr int kSnareOversample = 8;  // TUNED (stability)
class SnareDrumVoice final : public IVoice {
 public:
  SnareDrumVoice(CavityDrumVoice inner, SampleRateHz rate, int factor)
      : inner_(std::move(inner)), rate_(rate), factor_(factor) {}
  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }
  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(double* out, FrameCount n) override {
    using R = Result<FrameCount, QuantityError>;
    const auto count = static_cast<std::size_t>(n.value());
    if (count == 0) return R::success(n);
    if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
    buffer_.resize(count * static_cast<std::size_t>(factor_));
    auto done = inner_.renderBlock(buffer_.data(), FrameCount{buffer_.size()});
    if (!done.ok()) return R::failure(done.error());
    for (std::size_t i = 0; i < count; ++i) {
      double sum = 0.0;
      for (int k = 0; k < factor_; ++k) sum += buffer_[i * static_cast<std::size_t>(factor_) + static_cast<std::size_t>(k)];
      out[i] = sum / factor_;
    }
    return R::success(n);
  }
  void reset() override { inner_.reset(); }
  [[nodiscard]] const CavityDrumVoice& inner() const noexcept { return inner_; }

 private:
  CavityDrumVoice inner_;
  SampleRateHz rate_;
  int factor_ = 1;
  std::vector<double> buffer_;
};

// One snare hit. speed: stick speed in m/s (about 1 pp to 6 ff, VERIFY).
[[nodiscard]] inline Result<SnareDrumVoice, QuantityError> snareDrumHit(
    double speed, bool snaresOn, SampleRateHz rate, double distanceMeters = 1.0,
    const SnareWires* wires = nullptr) {
  using R = Result<SnareDrumVoice, QuantityError>;
  auto kit = yamahaTourCustomKit();
  if (!kit.ok()) return R::failure(kit.error());
  const MembraneShell& shell = kit.value().snare55;
  const double sb = kMylarDensityKgPerM3 * kSnareBatterThicknessM;
  const double ss = kMylarDensityKgPerM3 * kSnareSideThicknessM;
  auto tension = [&](double sigma, double hz) {
    const double alpha = 2.404825557695773;
    const double wave = constants::twoPi * shell.headRadius.value() * hz / alpha;
    return sigma * wave * wave;
  };
  const double tb = tension(sb, kSnareBatterHz), ts = tension(ss, kSnareSideHz);
  auto batter = tuneMembraneHead(shell, kSnareBatterHz, sb * 0.999, sb * 1.001, tb * 0.9, tb * 1.1);
  auto side = tuneMembraneHead(shell, kSnareSideHz, ss * 0.999, ss * 1.001, ts * 0.9, ts * 1.1);
  if (!batter.ok()) return R::failure(batter.error());
  if (!side.ok()) return R::failure(side.error());
  const MalletStrike stick{kSnareStrikeRadialFraction, 0.0, kSnareStickMassKg, speed,
                           kSnareStickStiffness, kSnareStickExponent, 0.0, kSnareHeadQ};
  auto voice = CavityDrumVoice::create(batter.value(), &side.value(), shell, stick, 4, 4,
                                       SampleRateHz{rate.value() * kSnareOversample},
                                       101325.0, 293.15, distanceMeters);
  if (!voice.ok()) return R::failure(voice.error());
  auto v = std::move(voice).value();
  if (snaresOn) {
    if (!v.attachSnareWires(wires != nullptr ? *wires : snareWireSet())) return R::failure(QuantityError::InvalidArgument);
  } else {
    v.enableLocalRadiation();
  }
  return R::success(SnareDrumVoice{std::move(v), rate, kSnareOversample});
}

}  // namespace mmathen
